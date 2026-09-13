#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Containers/Ticker.h"
#include "Combat/BreakerCombatTypes.h"
#include "BreakerFieldTrialRecorder.generated.h"

// Opt-in observations of ordinary play. Never advances quests or grants power.
UCLASS()
class UBreakerFieldTrialRecorder : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    void RecordInterruptedCast();
private:
    bool Sample(float DeltaSeconds);
    void UnbindPlayer();
    UFUNCTION() void RecordDeath();
    UFUNCTION() void RecordKill(const FBreakerHitContext& Hit);
    TWeakObjectPtr<class ABreakerCharacter> Player;
    FTSTicker::FDelegateHandle Ticker;
    FString OutputPath;
    double StartedAt = 0;
    int32 Deaths = 0;
    int32 Kills = 0;
    int32 InterruptedCasts = 0;
};
