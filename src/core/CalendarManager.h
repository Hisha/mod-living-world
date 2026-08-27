#ifndef MOD_LIVING_WORLD_CALENDAR_MANAGER_H
#define MOD_LIVING_WORLD_CALENDAR_MANAGER_H

#include "Define.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace lw
{
enum class CalendarRecurrenceType : uint8
{
    OneTime = 1,
    Annual = 2,
    MonthlyWeekdayOnOrBefore = 3,
    MonthlyWeekdayOnOrAfter = 4
};

enum class CalendarTargetType : uint8
{
    StartInvasion = 1,
    StartTravelingEvent = 2,
    StopTravelingEvent = 3
};

struct CalendarScheduleDefinition
{
    uint32 Id = 0;
    std::string Name;
    CalendarRecurrenceType RecurrenceType = CalendarRecurrenceType::OneTime;
    uint16 Year = 0;
    uint8 Month = 0;
    uint8 Day = 1;
    uint8 Weekday = 0; // 0 Sunday .. 6 Saturday, matching std::tm.
    uint8 Hour = 0;
    uint8 Minute = 0;
    uint32 CatchupSeconds = 86400;
    int32 RotationOffset = 0;
    bool Enabled = true;
};

struct CalendarRotationDefinition
{
    uint32 Id = 0;
    uint32 ScheduleId = 0;
    uint16 RotationOrder = 0;
    uint32 TargetIdOverride = 0;
    uint32 Value = 0;
    std::string Label;
};

struct CalendarActionDefinition
{
    uint32 Id = 0;
    uint32 ScheduleId = 0;
    uint16 ActionOrder = 0;
    int32 OffsetDays = 0;
    int32 OffsetMinutes = 0;
    CalendarTargetType TargetType = CalendarTargetType::StartInvasion;
    uint32 TargetId = 0;
    bool UseRotationTarget = false;
    uint32 Parameter1 = 0;
    uint32 Parameter2 = 0;
    bool Enabled = true;
    std::string Comment;
};

class CalendarManager
{
public:
    static CalendarManager& Instance();

    void Initialize();
    void Reset();
    void Reload();
    void Update(uint32 diff);
    void ConfigureSubsystems(bool invasionsEnabled, bool travelersEnabled);

    std::string BuildStatusReport() const;

private:
    CalendarManager() = default;

    void LoadDefinitions();
    void Evaluate();
    bool ExecuteAction(CalendarScheduleDefinition const& schedule,
        CalendarActionDefinition const& action, int64 occurrenceKey,
        uint32 rotationTargetId, std::string const& rotationLabel);

    uint32 _updateTimerMs = 0;
    bool _initialized = false;
    bool _invasionsEnabled = true;
    bool _travelersEnabled = true;
    std::unordered_map<uint32, CalendarScheduleDefinition> _schedules;
    std::unordered_map<uint32, std::vector<CalendarRotationDefinition>> _rotations;
    std::unordered_map<uint32, std::vector<CalendarActionDefinition>> _actions;
};
}

#define sLwCalendarMgr lw::CalendarManager::Instance()

#endif
