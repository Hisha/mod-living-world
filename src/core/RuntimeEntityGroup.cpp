#include "RuntimeEntityGroup.h"

#include <algorithm>

namespace lw
{
RuntimeEntityGroupManager& RuntimeEntityGroupManager::Instance()
{
    static RuntimeEntityGroupManager instance;
    return instance;
}

void RuntimeEntityGroupManager::Reset()
{
    _groups.clear();
    _groupsByRuntime.clear();
    _nextGroupId = 1;
}

uint64 RuntimeEntityGroupManager::NextGroupId()
{
    return _nextGroupId++;
}

RuntimeEntityGroup& RuntimeEntityGroupManager::CreateGroup(uint64 runtimeId, uint32 spawnGroupId)
{
    RuntimeEntityGroup group;
    group.Id = NextGroupId();
    group.RuntimeId = runtimeId;
    group.SpawnGroupId = spawnGroupId;

    uint64 id = group.Id;
    auto [itr, inserted] = _groups.emplace(id, std::move(group));
    (void)inserted;

    _groupsByRuntime[runtimeId].push_back(id);
    return itr->second;
}

RuntimeEntityGroup* RuntimeEntityGroupManager::GetGroup(uint64 runtimeGroupId)
{
    auto itr = _groups.find(runtimeGroupId);
    return itr == _groups.end() ? nullptr : &itr->second;
}

RuntimeEntityGroup const* RuntimeEntityGroupManager::GetGroup(uint64 runtimeGroupId) const
{
    auto itr = _groups.find(runtimeGroupId);
    return itr == _groups.end() ? nullptr : &itr->second;
}

std::vector<uint64> RuntimeEntityGroupManager::GetGroupsForRuntime(uint64 runtimeId) const
{
    auto itr = _groupsByRuntime.find(runtimeId);
    if (itr == _groupsByRuntime.end())
    {
        return {};
    }

    return itr->second;
}

std::vector<uint64> RuntimeEntityGroupManager::GetAllGroupIds() const
{
    std::vector<uint64> ids;
    ids.reserve(_groups.size());
    for (auto const& [id, group] : _groups)
    {
        (void)group;
        ids.push_back(id);
    }
    return ids;
}

RuntimeEntityGroup* RuntimeEntityGroupManager::FindLatestGroup(uint64 runtimeId, uint32 spawnGroupId)
{
    auto itr = _groupsByRuntime.find(runtimeId);
    if (itr == _groupsByRuntime.end())
    {
        return nullptr;
    }

    for (auto groupItr = itr->second.rbegin(); groupItr != itr->second.rend(); ++groupItr)
    {
        RuntimeEntityGroup* group = GetGroup(*groupItr);
        if (group && group->SpawnGroupId == spawnGroupId)
        {
            return group;
        }
    }

    return nullptr;
}

bool RuntimeEntityGroupManager::RemoveGroup(uint64 runtimeGroupId)
{
    auto groupItr = _groups.find(runtimeGroupId);
    if (groupItr == _groups.end())
    {
        return false;
    }

    uint64 runtimeId = groupItr->second.RuntimeId;
    _groups.erase(groupItr);

    auto runtimeItr = _groupsByRuntime.find(runtimeId);
    if (runtimeItr != _groupsByRuntime.end())
    {
        std::vector<uint64>& ids = runtimeItr->second;
        ids.erase(std::remove(ids.begin(), ids.end(), runtimeGroupId), ids.end());

        if (ids.empty())
        {
            _groupsByRuntime.erase(runtimeItr);
        }
    }

    return true;
}

void RuntimeEntityGroupManager::RemoveRuntime(uint64 runtimeId)
{
    auto itr = _groupsByRuntime.find(runtimeId);
    if (itr == _groupsByRuntime.end())
    {
        return;
    }

    for (uint64 groupId : itr->second)
    {
        _groups.erase(groupId);
    }

    _groupsByRuntime.erase(itr);
}
}
