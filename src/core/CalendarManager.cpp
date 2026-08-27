#include "CalendarManager.h"

#include "DatabaseEnv.h"
#include "Field.h"
#include "InvasionRuntimeManager.h"
#include "Log.h"
#include "QueryResult.h"
#include "TravelingEventManager.h"

#include <algorithm>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace lw
{
namespace
{
constexpr uint32 CalendarUpdateIntervalMs = 30000;

std::tm LocalTime(std::time_t value)
{
    std::tm result{};
#if defined(_WIN32)
    localtime_s(&result, &value);
#else
    localtime_r(&value, &result);
#endif
    return result;
}

std::time_t MakeLocalTime(int year, int month, int day, int hour, int minute)
{
    std::tm value{};
    value.tm_year = year - 1900;
    value.tm_mon = month - 1;
    value.tm_mday = day;
    value.tm_hour = hour;
    value.tm_min = minute;
    value.tm_sec = 0;
    value.tm_isdst = -1;
    return std::mktime(&value);
}

int DaysInMonth(int year, int month)
{
    std::time_t next = MakeLocalTime(year, month + 1, 1, 12, 0);
    std::tm nextTm = LocalTime(next);
    nextTm.tm_mday = 0;
    std::time_t last = std::mktime(&nextTm);
    return LocalTime(last).tm_mday;
}

std::time_t ResolveMonthlyAnchor(CalendarScheduleDefinition const& schedule, int year, int month)
{
    int const clampedDay = std::max(1, std::min<int>(schedule.Day, DaysInMonth(year, month)));
    std::time_t anchor = MakeLocalTime(year, month, clampedDay, schedule.Hour, schedule.Minute);
    std::tm tm = LocalTime(anchor);
    int delta = 0;

    if (schedule.RecurrenceType == CalendarRecurrenceType::MonthlyWeekdayOnOrBefore)
        delta = (tm.tm_wday - schedule.Weekday + 7) % 7;
    else
        delta = -((schedule.Weekday - tm.tm_wday + 7) % 7);

    tm.tm_mday -= delta;
    tm.tm_isdst = -1;
    return std::mktime(&tm);
}

int64 MonthlyOccurrenceKey(int year, int month)
{
    return static_cast<int64>(year) * 100 + month;
}

int64 AnnualOccurrenceKey(int year)
{
    return year;
}

std::string FormatLocal(std::time_t value)
{
    std::tm tm = LocalTime(value);
    std::ostringstream out;
    out << std::put_time(&tm, "%Y-%m-%d %H:%M");
    return out.str();
}
}

CalendarManager& CalendarManager::Instance()
{
    static CalendarManager instance;
    return instance;
}

void CalendarManager::Reset()
{
    _schedules.clear();
    _rotations.clear();
    _actions.clear();
    _updateTimerMs = CalendarUpdateIntervalMs;
    _initialized = false;
}

void CalendarManager::Initialize()
{
    Reset();
    LoadDefinitions();
    _initialized = true;
    _updateTimerMs = 1000; // Evaluate shortly after startup for catch-up/recovery.

    LOG_INFO("server.loading", "[LW Calendar] Initialized with {} enabled schedule(s).", _schedules.size());
}

void CalendarManager::Reload()
{
    Initialize();
}

void CalendarManager::LoadDefinitions()
{
    if (QueryResult result = WorldDatabase.Query(
        "SELECT `id`,`name`,`recurrence_type`,`year`,`month`,`day`,`weekday`,`hour`,`minute`,`catchup_seconds`,`rotation_offset`,`enabled` "
        "FROM `lw_calendar_schedule` WHERE `enabled` = 1 ORDER BY `id`"))
    {
        do
        {
            Field* f = result->Fetch();
            CalendarScheduleDefinition d;
            d.Id = f[0].Get<uint32>();
            d.Name = f[1].Get<std::string>();
            d.RecurrenceType = static_cast<CalendarRecurrenceType>(f[2].Get<uint8>());
            d.Year = f[3].Get<uint16>();
            d.Month = f[4].Get<uint8>();
            d.Day = f[5].Get<uint8>();
            d.Weekday = f[6].Get<uint8>();
            d.Hour = f[7].Get<uint8>();
            d.Minute = f[8].Get<uint8>();
            d.CatchupSeconds = f[9].Get<uint32>();
            d.RotationOffset = f[10].Get<int32>();
            d.Enabled = f[11].Get<uint8>() != 0;

            bool const needsMonth = d.RecurrenceType == CalendarRecurrenceType::OneTime ||
                d.RecurrenceType == CalendarRecurrenceType::Annual;
            bool const needsYear = d.RecurrenceType == CalendarRecurrenceType::OneTime;

            if (static_cast<uint8>(d.RecurrenceType) < 1 || static_cast<uint8>(d.RecurrenceType) > 4 ||
                d.Day < 1 || d.Day > 31 || d.Weekday > 6 || d.Hour > 23 || d.Minute > 59 ||
                (needsMonth && (d.Month < 1 || d.Month > 12)) || (needsYear && d.Year == 0))
            {
                LOG_ERROR("server.loading", "[LW Calendar] Schedule {} ({}) has invalid calendar fields; ignored.", d.Id, d.Name);
                continue;
            }

            _schedules.emplace(d.Id, std::move(d));
        } while (result->NextRow());
    }

    if (QueryResult result = WorldDatabase.Query(
        "SELECT `id`,`schedule_id`,`rotation_order`,`target_id_override`,`value`,`label` "
        "FROM `lw_calendar_rotation` WHERE `enabled` = 1 ORDER BY `schedule_id`,`rotation_order`,`id`"))
    {
        do
        {
            Field* f = result->Fetch();
            uint32 const scheduleId = f[1].Get<uint32>();
            if (_schedules.find(scheduleId) == _schedules.end())
                continue;

            CalendarRotationDefinition r;
            r.Id = f[0].Get<uint32>();
            r.ScheduleId = scheduleId;
            r.RotationOrder = f[2].Get<uint16>();
            r.TargetIdOverride = f[3].Get<uint32>();
            r.Value = f[4].Get<uint32>();
            r.Label = f[5].IsNull() ? std::string() : f[5].Get<std::string>();
            _rotations[scheduleId].push_back(std::move(r));
        } while (result->NextRow());
    }

    if (QueryResult result = WorldDatabase.Query(
        "SELECT `id`,`schedule_id`,`action_order`,`offset_days`,`offset_minutes`,`target_type`,`target_id`,"
        "`use_rotation_target`,`parameter1`,`parameter2`,`enabled`,`comment` "
        "FROM `lw_calendar_action` WHERE `enabled` = 1 ORDER BY `schedule_id`,`action_order`,`id`"))
    {
        do
        {
            Field* f = result->Fetch();
            uint32 const scheduleId = f[1].Get<uint32>();
            if (_schedules.find(scheduleId) == _schedules.end())
                continue;

            CalendarActionDefinition a;
            a.Id = f[0].Get<uint32>();
            a.ScheduleId = scheduleId;
            a.ActionOrder = f[2].Get<uint16>();
            a.OffsetDays = f[3].Get<int32>();
            a.OffsetMinutes = f[4].Get<int32>();
            a.TargetType = static_cast<CalendarTargetType>(f[5].Get<uint8>());
            a.TargetId = f[6].Get<uint32>();
            a.UseRotationTarget = f[7].Get<uint8>() != 0;
            a.Parameter1 = f[8].Get<uint32>();
            a.Parameter2 = f[9].Get<uint32>();
            a.Enabled = f[10].Get<uint8>() != 0;
            a.Comment = f[11].IsNull() ? std::string() : f[11].Get<std::string>();

            if (static_cast<uint8>(a.TargetType) < 1 || static_cast<uint8>(a.TargetType) > 3)
            {
                LOG_ERROR("server.loading", "[LW Calendar] Action {} has unsupported target type {}; ignored.",
                    a.Id, static_cast<uint32>(a.TargetType));
                continue;
            }

            _actions[scheduleId].push_back(std::move(a));
        } while (result->NextRow());
    }

    LOG_INFO("server.loading", "[LW Calendar] Loaded {} schedule(s), {} rotation set(s), and {} action set(s).",
        _schedules.size(), _rotations.size(), _actions.size());
}

void CalendarManager::Update(uint32 diff)
{
    if (!_initialized)
        return;

    if (_updateTimerMs > diff)
    {
        _updateTimerMs -= diff;
        return;
    }

    _updateTimerMs = CalendarUpdateIntervalMs;
    Evaluate();
}

void CalendarManager::Evaluate()
{
    std::time_t const now = std::time(nullptr);
    std::tm const nowTm = LocalTime(now);

    for (auto const& [scheduleId, schedule] : _schedules)
    {
        struct Occurrence { std::time_t Anchor = 0; int64 Key = 0; int64 Ordinal = 0; };
        std::vector<Occurrence> occurrences;

        if (schedule.RecurrenceType == CalendarRecurrenceType::OneTime)
        {
            if (schedule.Year != 0 && schedule.Month >= 1 && schedule.Month <= 12)
                occurrences.push_back({ MakeLocalTime(schedule.Year, schedule.Month, schedule.Day, schedule.Hour, schedule.Minute),
                    static_cast<int64>(schedule.Year) * 10000 + schedule.Month * 100 + schedule.Day,
                    static_cast<int64>(schedule.Year) * 12 + schedule.Month - 1 });
        }
        else if (schedule.RecurrenceType == CalendarRecurrenceType::Annual)
        {
            for (int y = nowTm.tm_year + 1900 - 1; y <= nowTm.tm_year + 1900 + 1; ++y)
                occurrences.push_back({ MakeLocalTime(y, schedule.Month, schedule.Day, schedule.Hour, schedule.Minute),
                    AnnualOccurrenceKey(y), y });
        }
        else
        {
            // Previous/current/next target month covers negative/positive action offsets around month boundaries.
            for (int monthOffset = -1; monthOffset <= 1; ++monthOffset)
            {
                int year = nowTm.tm_year + 1900;
                int month = nowTm.tm_mon + 1 + monthOffset;
                while (month < 1) { month += 12; --year; }
                while (month > 12) { month -= 12; ++year; }
                occurrences.push_back({ ResolveMonthlyAnchor(schedule, year, month),
                    MonthlyOccurrenceKey(year, month), static_cast<int64>(year) * 12 + month - 1 });
            }
        }

        auto actionItr = _actions.find(scheduleId);
        if (actionItr == _actions.end())
            continue;

        for (Occurrence const& occurrence : occurrences)
        {
            uint32 rotationTargetId = 0;
            std::string rotationLabel;
            auto rotationItr = _rotations.find(scheduleId);
            if (rotationItr != _rotations.end() && !rotationItr->second.empty())
            {
                auto const& rows = rotationItr->second;
                int64 index = occurrence.Ordinal + schedule.RotationOffset;
                int64 const size = static_cast<int64>(rows.size());
                index %= size;
                if (index < 0)
                    index += size;
                CalendarRotationDefinition const& row = rows[static_cast<std::size_t>(index)];
                rotationTargetId = row.TargetIdOverride;
                rotationLabel = row.Label;
            }

            for (CalendarActionDefinition const& action : actionItr->second)
            {
                std::time_t const due = occurrence.Anchor +
                    static_cast<std::time_t>(action.OffsetDays) * 86400 +
                    static_cast<std::time_t>(action.OffsetMinutes) * 60;

                if (due > now)
                    continue;

                uint64 const age = static_cast<uint64>(now - due);
                if (age > schedule.CatchupSeconds)
                    continue;

                if (WorldDatabase.Query(
                    "SELECT 1 FROM `lw_calendar_execution` WHERE `action_id` = {} AND `occurrence_key` = {} LIMIT 1",
                    action.Id, occurrence.Key))
                    continue;

                if (ExecuteAction(schedule, action, occurrence.Key, rotationTargetId, rotationLabel))
                {
                    WorldDatabase.Execute(
                        "INSERT INTO `lw_calendar_execution` (`action_id`,`occurrence_key`,`scheduled_at`,`executed_at`,`rotation_label`) "
                        "VALUES ({},{},FROM_UNIXTIME({}),NOW(),NULL)",
                        action.Id,
                        occurrence.Key,
                        static_cast<uint64>(due));
                }
            }
        }
    }
}

bool CalendarManager::ExecuteAction(CalendarScheduleDefinition const& schedule,
    CalendarActionDefinition const& action, int64 occurrenceKey,
    uint32 rotationTargetId, std::string const& rotationLabel)
{
    uint32 const targetId = action.UseRotationTarget && rotationTargetId != 0
        ? rotationTargetId
        : action.TargetId;

    if (targetId == 0)
    {
        LOG_ERROR("server.loading", "[LW Calendar] Schedule {} action {} resolved target id 0; action not recorded.",
            schedule.Id, action.Id);
        return false;
    }

    bool success = false;
    switch (action.TargetType)
    {
        case CalendarTargetType::StartInvasion:
            if (!_invasionsEnabled)
                return false;
            // Calendar starts the authored invasion runtime directly; its SQL stages determine duration/completion.
            success = sInvasionRuntimeMgr.StartInvasion(targetId);
            break;

        case CalendarTargetType::StartTravelingEvent:
        {
            if (!_travelersEnabled)
                return false;
            std::string error;
            TravelingEventStartResult const result = sTravelingEventMgr.Start(targetId, &error);
            success = result == TravelingEventStartResult::Started || result == TravelingEventStartResult::AlreadyActive;
            if (!success)
                LOG_ERROR("server.loading", "[LW Calendar] Traveling event {} could not start: {}.", targetId, error);
            break;
        }

        case CalendarTargetType::StopTravelingEvent:
        {
            if (!_travelersEnabled)
                return false;
            std::string error;
            success = sTravelingEventMgr.Stop(targetId, &error);
            if (!success)
                LOG_ERROR("server.loading", "[LW Calendar] Traveling event {} could not stop: {}.", targetId, error);
            break;
        }
    }

    if (success)
    {
        LOG_INFO("server.loading",
            "[LW Calendar] Executed schedule {} ({}) action {} occurrence {} -> type {}, target {}{}{}.",
            schedule.Id,
            schedule.Name,
            action.Id,
            occurrenceKey,
            static_cast<uint32>(action.TargetType),
            targetId,
            rotationLabel.empty() ? "" : ", rotation ",
            rotationLabel);
    }
    else
    {
        LOG_WARN("server.loading",
            "[LW Calendar] Schedule {} ({}) action {} occurrence {} could not execute target {} and will retry during catch-up window.",
            schedule.Id, schedule.Name, action.Id, occurrenceKey, targetId);
    }

    return success;
}


void CalendarManager::ConfigureSubsystems(bool invasionsEnabled, bool travelersEnabled)
{
    _invasionsEnabled = invasionsEnabled;
    _travelersEnabled = travelersEnabled;
}

std::string CalendarManager::BuildStatusReport() const
{
    std::ostringstream out;
    out << "\nLW calendar: " << _schedules.size() << " enabled schedule(s)"
        << (_initialized ? " [running]" : " [stopped]") << "\n";

    for (auto const& [id, schedule] : _schedules)
    {
        out << "  #" << id << " " << schedule.Name
            << " | recurrence=" << static_cast<uint32>(schedule.RecurrenceType)
            << " | actions=";
        auto ai = _actions.find(id);
        out << (ai == _actions.end() ? 0 : ai->second.size());
        auto ri = _rotations.find(id);
        if (ri != _rotations.end())
            out << " | rotation entries=" << ri->second.size();
        out << "\n";
    }

    return out.str();
}
}
