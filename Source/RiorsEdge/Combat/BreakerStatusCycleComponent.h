#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Combat/BreakerCombatTypes.h"
#include "BreakerStatusCycleComponent.generated.h"

class UBreakerProgressionComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FBreakerCycleChanged, FGameplayTag, NextStatusTag);

// One authored entry in the cycle: a status this character can apply, and the
// payload applied when the cycle lands on it. Data rather than code because
// Class-Kits §2.5 is explicit that every Multispell node is authored against
// "distinct status types" and never against named elements — so no code path
// here may branch on Bleed or Poison.
USTRUCT(BlueprintType)
struct RIORSEDGE_API FBreakerCycleEntry
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) FBreakerStatusApplicationSpec Spec;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EBreakerDamageFamily DamageFamily = EBreakerDamageFamily::Physical;
    // Element positions feed the actual impact's buildup; Spec is preview identity only.
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EBreakerElement Element = EBreakerElement::None;
    // Shown on the HUD cycle readout (SI-4). O2 PLACEHOLDER text.
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FText DisplayName;
};

// The status cycle behind C5 Fracture (Ability-Implementation-Spec §5.5).
//
// "The caster's available status types" is DERIVED, not authored per ability:
// Bleed and Poison are direct physical statuses; Entropy earns Rot through buildup.
// Future elements extend this list only when their actual delivery path exists.
// and the order must be deterministic — the HUD previews the next position, and
// a preview that can lie is worse than no preview.
//
// DEVIATION FROM SPEC: the spec places this at
// Classes/BreakerStatusCycleComponent.h. It lives under Combat/ because the
// cycle is a status concept and Combat/ owns statuses; Classes/ owns resource
// loops. Same reasoning, and the same kind of note, as
// Abilities/BreakerMeleeSweep.h. The API is the spec's.
UCLASS(ClassGroup=Combat, BlueprintType, meta=(BlueprintSpawnableComponent))
class RIORSEDGE_API UBreakerStatusCycleComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UBreakerStatusCycleComponent();
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    bool CanPreviewAhead() const { return bPreviewAhead; }

    // Finds the component on an actor, adding it if absent. Same
    // zero-setup pattern as UBreakerAbilityStateComponent::FindOrAdd: an
    // ability must not be dead because a component was never attached in a
    // Blueprint nobody has authored yet.
    static UBreakerStatusCycleComponent* FindOrAdd(AActor* Owner);

    // The status the NEXT cast will apply. Lookahead 1 is the position after
    // that (MS2 R2 previews one ahead).
    UFUNCTION(BlueprintPure, Category="Combat|Cycle") FGameplayTag PeekNext(int32 Lookahead = 0) const;
    UFUNCTION(BlueprintPure, Category="Combat|Cycle") FBreakerCycleEntry PeekNextEntry(int32 Lookahead = 0) const;

    // Consumes the current position and returns what it was, moving the cursor
    // on. Returns an invalid tag when the cycle is empty — an empty cycle is a
    // content failure, and returning the first status by accident would hide it.
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Combat|Cycle") FGameplayTag AdvanceCycle();

    // MS2 R1: the cycle advances on HIT rather than on cast, so a missed cast
    // does not waste a position. The ability asks this before deciding when to
    // call AdvanceCycle.
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Combat|Cycle") void SetAdvanceOnHit(bool bOnHit);
    UFUNCTION(BlueprintPure, Category="Combat|Cycle") bool GetAdvanceOnHit() const { return bAdvanceOnHit; }

    UFUNCTION(BlueprintPure, Category="Combat|Cycle") TArray<FGameplayTag> GetAvailableStatusTypes() const;
    UFUNCTION(BlueprintPure, Category="Combat|Cycle") int32 GetCycleLength() const { return AvailableStatuses.Num(); }
    UFUNCTION(BlueprintPure, Category="Combat|Cycle") int32 GetCursor() const { return Cursor; }

    // Grows the cycle as the character gains status types. Idempotent by tag:
    // Siphon granting Void twice must not make Void come round twice as often.
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Combat|Cycle") void AddStatusType(const FBreakerCycleEntry& Entry);
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Combat|Cycle") void RemoveStatusType(FGameplayTag StatusTag);

    UPROPERTY(BlueprintAssignable, Category="Combat|Cycle") FBreakerCycleChanged OnCycleChanged;

    // Starter Bleed/Poison/Entropy remains three positions. Unlocking Siphon
    // appends Void once; its Erased tag is preview identity, never a free status.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing=OnRep_Cycle, Category="Combat|Cycle") TArray<FBreakerCycleEntry> AvailableStatuses;

private:
    void SeedDefaultCycle();
    void BindProgression();
    TWeakObjectPtr<UBreakerProgressionComponent> BoundProgression;
    bool bOwnsSiphonEntry = false;
    UFUNCTION() void OnRep_Cycle();
    UFUNCTION() void SyncProgression();

    UPROPERTY(ReplicatedUsing=OnRep_Cycle) int32 Cursor = 0;
    UPROPERTY(Replicated) bool bAdvanceOnHit = false;
    UPROPERTY(ReplicatedUsing=OnRep_Cycle) bool bPreviewAhead = false;
    bool bSeeded = false;
};
