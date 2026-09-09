#pragma once
#include "CoreMinimal.h"
#include "Interaction/BreakerSurvivor.h"
#include "BreakerMeridianGroundCrew.generated.h"
class ABreakerEnemy;

// Port Meridian's authored route reuses Survivor locomotion, not its quest.
UCLASS()
class RIORSEDGE_API ABreakerMeridianGroundCrew : public ABreakerSurvivor
{
    GENERATED_BODY()
public:
    ABreakerMeridianGroundCrew();
    static FName CompletionFlag();
    bool ConfigureMeridian(const TArray<FVector>& Districts,const TArray<TArray<ABreakerEnemy*>>& Pockets);
    bool IsCompleteFor(const ABreakerCharacter* Player) const;
    FText GetObjectiveText(const ABreakerCharacter* Player) const;
    // O2 PLACEHOLDER: ordinary escort route offsets; inherited speed, follow
    // radius and attempt duration remain editable Survivor tuning.
    UPROPERTY(EditAnywhere,Category="Meridian|Route") FVector ShelterOffset=FVector(-1800,0,90); // O2 PLACEHOLDER.
    UPROPERTY(EditAnywhere,Category="Meridian|Route") FVector ExtractionOffset=FVector(1600,0,90); // O2 PLACEHOLDER.
    UPROPERTY(EditAnywhere,Category="Meridian|Route") float RouteCenterHeight=90.f; // O2 PLACEHOLDER.
protected:
    virtual FName GetEscortDialogueId() const override { return TEXT("MeridianGroundCrew"); }
    virtual bool IsEscortAdmitted(const ABreakerCharacter* Player) const override;
private:
    UFUNCTION() void ObserveGuardDeaths();
    void CompleteExtraction(ABreakerSurvivor* Survivor,ABreakerCharacter* Player);
    TArray<TWeakObjectPtr<ABreakerEnemy>> Guards;
    TArray<bool> ObservedDeaths;
    bool bConfigured=false;
    bool bRosterValid=false;
    bool bCompletionClaimed=false;
};
