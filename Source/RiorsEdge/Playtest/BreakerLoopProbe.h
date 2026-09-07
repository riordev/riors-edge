#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Containers/Ticker.h"
#include "BreakerLoopProbe.generated.h"

// Opt-in standalone integration probe. Uses real map loads and isolated disk
// saves; it accelerates kills, so it measures wiring, never combat pacing.
UCLASS()
class UBreakerLoopProbe : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
private:
    bool TickProbe(float DeltaSeconds);
    bool Finish(bool bPassed, const TCHAR* Reason);
    bool SelectTravel(class ABreakerCharacter* Player, FName Destination);
    bool TickActTwo(class ABreakerCharacter* Player, class ABreakerGameMode* Mode);
    bool SelectDialogueFlag(class ABreakerCharacter* Player, FName Flag);
    bool bActTwo = false;
    bool bSawMarshal = false;
    int32 BreachMaximumWave = 0;
    FName LastCampaignBeat;
    FTSTicker::FDelegateHandle Ticker;
    TWeakObjectPtr<UWorld> DepartedWorld;
    double StartedAt = 0;
    int32 Stage = 0;
    int32 PaidXp = 0;
    int32 PaidGlass = 0;
    int32 KilledEnemies = 0;
    int32 CompletionCount = 0;
    int32 XpBeforePurse = 0;
    int32 GlassBeforePurse = 0;
    bool bWatchingCompletion = false;
    bool bSawHoldfast = false;
    bool bContactPhotoTaken = false;
    bool bMarshalPhotoTaken = false;
    int32 PhotoDelay = 0;
    FString PendingPhoto;
};
