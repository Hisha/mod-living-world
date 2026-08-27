#ifndef MOD_LIVING_WORLD_CREATURE_TEMPLATE_MANAGER_H
#define MOD_LIVING_WORLD_CREATURE_TEMPLATE_MANAGER_H

#include "Define.h"

#include <unordered_map>

namespace lw
{
class LwCreatureTemplateManager
{
public:
    static LwCreatureTemplateManager& Instance();

    // Must run before AzerothCore loads creature_template into ObjectMgr.
    bool MaterializeStartupTemplates();

    [[nodiscard]] uint32 ResolveEntry(uint32 lwTemplateId) const;
    [[nodiscard]] uint32 GetMappedTemplateCount() const;

private:
    LwCreatureTemplateManager() = default;

    bool RetireInactiveMappings();
    bool MaterializeEnabledDefinitions();
    uint32 AllocateEntry() const;
    bool MaterializeDefinition(uint32 lwTemplateId, uint32 allocatedEntry);
    void LoadMappings();

    std::unordered_map<uint32, uint32> _entryByLwTemplate;
};
}

#define sLwCreatureTemplateMgr lw::LwCreatureTemplateManager::Instance()

#endif
