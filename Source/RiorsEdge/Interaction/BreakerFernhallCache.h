#pragma once
#include "CoreMinimal.h"
#include "Interaction/BreakerNPC.h"
#include "BreakerFernhallCache.generated.h"
class ABreakerEnemy;
class ABreakerCharacter;
UCLASS()
class RIORSEDGE_API ABreakerFernhallCache : public ABreakerNPC
{
    GENERATED_BODY()
public:
    ABreakerFernhallCache();
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    void Configure(int32 AreaLevel, const TArray<ABreakerEnemy*>& Pocket, int32 ExpectedMembers);
    bool TryOpen(ABreakerCharacter* Player);
    bool IsOpened() const { return bOpened; }
    bool IsPocketCleared() const { return bCleared; }
    FText GetCachePrompt() const;
private:
    UFUNCTION() void ObserveGuardDeaths();
    TArray<TWeakObjectPtr<ABreakerEnemy>> Guards;
    TArray<bool> ObservedDeaths;
    bool bConfigured = false;
    bool bRosterValid = false;
    int32 ItemLevel = 1;
    UPROPERTY(Replicated) bool bOpened = false;
    UPROPERTY(Replicated) bool bCleared = false;
};
