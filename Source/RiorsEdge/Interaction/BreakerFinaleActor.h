#pragma once
#include "CoreMinimal.h"
#include "Interaction/BreakerNPC.h"
#include "BreakerFinaleActor.generated.h"
class ABreakerCharacter;
UCLASS()
class RIORSEDGE_API ABreakerFinaleActor : public ABreakerNPC
{
    GENERATED_BODY()
public:
    ABreakerFinaleActor();
    virtual void BeginPlay() override;
    void ConfigureFragment(int32 PocketCount);
    void ConfigureDevice();
    void SetPocketCleared(int32 PocketIndex, bool bCleared = true);
    bool AreAllPocketsCleared() const;
    bool TryRecoverFragment(ABreakerCharacter* Player);
    bool TryChooseFinale(ABreakerCharacter* Player, bool bSeal);
    static bool TryMeetAlternate(ABreakerNPC* NPC, ABreakerCharacter* Player);
    bool IsFragment() const { return !bDevice; }
    bool IsDevice() const { return bDevice; }
private:
    bool IsEligibleInteractor(ABreakerCharacter* Player) const;
    void RefreshPresentation();
    bool bDevice = false;
    TArray<bool> ClearedPockets;
};
