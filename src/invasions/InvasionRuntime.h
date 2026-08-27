#ifndef MOD_LIVING_WORLD_RUNTIME_H
#define MOD_LIVING_WORLD_RUNTIME_H

#include "Define.h"
#include "LivingWorld.h"

#include <string>
#include <vector>

namespace lw
{
enum class ActiveRuntimeState : uint8
{
    Initializing = 0,
    Running = 1,
    Completed = 2,
    Failed = 3
};

class InvasionRuntime
{
public:
    InvasionRuntime(uint64 runtimeId, uint32 invasionId,
        std::vector<InvasionStageDefinition> stages, uint64 startedAt);

    static InvasionRuntime Restore(uint64 runtimeId, uint32 invasionId,
        std::vector<InvasionStageDefinition> stages, uint32 currentStageId,
        uint64 stageStartedAt, uint64 stageEndsAt, uint64 startedAt);

    bool Start(uint64 now);
    bool Update(uint64 now);
    bool Advance(uint64 now);

    [[nodiscard]] uint64 GetRuntimeId() const;
    [[nodiscard]] uint32 GetInvasionId() const;
    [[nodiscard]] ActiveRuntimeState GetState() const;
    [[nodiscard]] InvasionStageDefinition const* GetCurrentStage() const;
    [[nodiscard]] uint64 GetStageStartedAt() const;
    [[nodiscard]] uint64 GetStageEndsAt() const;
    [[nodiscard]] uint64 GetStartedAt() const;
    [[nodiscard]] uint64 GetStageTimeRemaining(uint64 now) const;
    [[nodiscard]] std::string BuildStatusLine(uint64 now) const;

private:
    InvasionRuntime() = default;

    bool BeginCurrentStage(uint64 now);
    std::size_t FindStageIndex(uint32 stageId) const;

    uint64 _runtimeId = 0;
    uint32 _invasionId = 0;
    ActiveRuntimeState _state = ActiveRuntimeState::Initializing;
    std::vector<InvasionStageDefinition> _stages;
    std::size_t _currentStageIndex = 0;
    uint64 _stageStartedAt = 0;
    uint64 _stageEndsAt = 0;
    uint64 _startedAt = 0;
};
}

#endif
