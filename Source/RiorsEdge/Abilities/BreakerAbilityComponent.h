#pragma once

#include "CoreMinimal.h"
#include "GameplayAbilitySpec.h"
#include "Components/ActorComponent.h"
#include "Progression/BreakerProgressionTypes.h"
#include "BreakerAbilityComponent.generated.h"

class UAbilitySystemComponent;
class UBreakerAbilityDefinition;
class UBreakerProgressionComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FBreakerAbilitySlotChanged, EBreakerAbilitySlot, Slot, FName, AbilityId);
// Fires only when a slot actually activated. Polling cannot substitute: an
// activation is instantaneous, and abilities like Skim leave no lasting state
// for the HUD to notice on a later frame.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FBreakerAbilityActivated, EBreakerAbilitySlot, Slot);

// Why a selection was refused. An enum rather than a bool so a UI can say
// WHICH rule was broken without parsing localised text, and so the rule set is
// testable without a world, a UI, or a save.
UENUM(BlueprintType)
enum class EBreakerAbilitySelectionResult : uint8
{
    Allowed,
    // The character has not locked a class yet, so nothing is grantable.
    NoClassChosen,
    // No ability with that id exists in the registry — a stale save, a typo in
    // content, or a UI passing a display name where an id belongs.
    UnknownAbility,
    // The id is real but belongs to another class.
    WrongClass,
    // Real, and this class's, but it cannot occupy this slot: an ultimate in a
    // class-ability slot, or a class ability in the ultimate slot.
    WrongSlot,
    // Already sitting in one of the other slots. Two copies of one ability is
    // not a build, it is a bug the player can create for themselves.
    AlreadyEquipped,
    // O100: real, this class's, fits the slot, and the character has not bought
    // it yet. APPENDED AT THE END, like every other enum in this project that
    // content or a save could be holding a value from.
    //
    // Before this, PreviewSelection could not see the unlock rule at all, so a
    // locked ability previewed as fine and the true refusal only appeared after
    // the click. PreviewSelection now CALLS
    // UBreakerProgressionComponent::IsAbilityUnlocked rather than restating it
    // — progression owns the unlock rules, and a second copy here is precisely
    // the drift TryEquipAbility's "ONE writer" comment warns about.
    NotUnlocked
};

// One granted loadout slot. AbilityId is recorded even when the ability has no
// implementation yet, so the HUD can label a designed-but-unbuilt slot instead
// of showing an empty one.
USTRUCT(BlueprintType)
struct RIORSEDGE_API FBreakerGrantedAbility
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) FName AbilityId = NAME_None;
    UPROPERTY(BlueprintReadOnly) TObjectPtr<UBreakerAbilityDefinition> Definition = nullptr;
    UPROPERTY(BlueprintReadOnly) bool bImplemented = false;
    FGameplayAbilitySpecHandle Handle;
};

// SI-1/SI-2: the single seam between progression and GAS. Owns granting,
// revoking, slot activation, and the cooldown/cost queries the HUD reads.
// Granting is server-authoritative; queries are safe anywhere.
UCLASS(ClassGroup=Abilities, BlueprintType, meta=(BlueprintSpawnableComponent))
class RIORSEDGE_API UBreakerAbilityComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UBreakerAbilityComponent();
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // Reconciles granted specs against the owner's ability loadout. Revoke then
    // grant, never the reverse: a re-equip of the same ability would otherwise
    // momentarily double-grant and clobber the cooldown effect.
    UFUNCTION(BlueprintCallable, Category="Abilities") void RefreshGrants();

    UFUNCTION(BlueprintCallable, Category="Abilities") bool TryActivateSlot(EBreakerAbilitySlot Slot);
    UFUNCTION(BlueprintPure, Category="Abilities") FName GetAbilityIdForSlot(EBreakerAbilitySlot Slot) const;
    UFUNCTION(BlueprintPure, Category="Abilities") UBreakerAbilityDefinition* GetDefinitionForSlot(EBreakerAbilitySlot Slot) const;
    UFUNCTION(BlueprintPure, Category="Abilities") bool IsSlotImplemented(EBreakerAbilitySlot Slot) const;
    UFUNCTION(BlueprintPure, Category="Abilities") bool IsSlotGranted(EBreakerAbilitySlot Slot) const;
    UFUNCTION(BlueprintPure, Category="Abilities") float GetCooldownRemaining(EBreakerAbilitySlot Slot) const;
    UFUNCTION(BlueprintPure, Category="Abilities") float GetCooldownDuration(EBreakerAbilitySlot Slot) const;
    // 0 cooldown seconds means the ability is cost-gated, not "ready".
    UFUNCTION(BlueprintPure, Category="Abilities") bool SlotHasCooldown(EBreakerAbilitySlot Slot) const;
    UFUNCTION(BlueprintPure, Category="Abilities") float GetCost(EBreakerAbilitySlot Slot) const;
    UFUNCTION(BlueprintPure, Category="Abilities") float GetResourceCostForSlot(EBreakerAbilitySlot Slot) const { return GetCost(Slot); }
    UFUNCTION(BlueprintPure, Category="Abilities") bool CanAffordSlot(EBreakerAbilitySlot Slot) const;
    UFUNCTION(BlueprintPure, Category="Abilities") int32 GetActiveCooldownCount() const;
    UFUNCTION(BlueprintPure, Category="Abilities") int32 GetGrantedCount() const;

    UPROPERTY(BlueprintAssignable, Category="Abilities") FBreakerAbilitySlotChanged OnSlotChanged;
    UPROPERTY(BlueprintAssignable, Category="Abilities") FBreakerAbilityActivated OnAbilityActivated;

    // How often the component re-checks the loadout for changes. Cheap: it
    // compares three FNames and the class id.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Abilities", meta=(ClampMin="0.05")) float LoadoutPollInterval = 0.5f;

    // Pure rule, exposed for tests: resolve a loadout id for a slot, falling
    // back to the class default when the loadout has nothing equipped.
    static UBreakerAbilityDefinition* ResolveDefinition(EBreakerClassId ClassId, EBreakerAbilitySlot Slot, FName EquippedId);

    // --- Ability selection (the player's two abilities + one ultimate) -----
    //
    // WHAT A UI MUST CALL, in order. This component is the whole public
    // surface; nothing outside it needs to know about the registry, the
    // fallback table, or the progression state's loadout struct.
    //
    //   1. GetSelectableAbilityIds(Slot)  -> the choices to draw for that slot
    //   2. GetEquippedAbilityId(Slot)     -> which one to draw as selected
    //   3. PreviewSelection(Slot, Id)     -> greying out, on hover/repaint,
    //                                        with no side effects at all
    //   4. TryEquipAbility(Slot, Id, Out) -> commit; returns false with a
    //                                        player-facing reason in Out
    //
    // TryEquipAbility does NOT write the loadout itself. It validates, then
    // delegates the write to UBreakerProgressionComponent::EquipAbility so
    // there stays exactly ONE writer of the progression state — which is also
    // what makes the choice persist, since the loadout is a field of
    // FBreakerProgressionState and UBreakerSaveGame stores that struct whole.
    // A second writer here would save correctly and diverge from the unlock
    // rules, which is the worse of the two failure modes.
    //
    // Equipping is a locked PROGRESSION decision, not a per-fight toggle
    // (CONTEXT.md locked decisions: "characters equip two class abilities and
    // one ultimate"); this API is deliberately not a quick-swap.
    UFUNCTION(BlueprintPure, Category="Abilities|Selection")
    TArray<FName> GetSelectableAbilityIds(EBreakerAbilitySlot Slot) const;

    // The id the PLAYER chose for this slot, which is not always the id that is
    // granted: an empty or stale choice falls back to the class default, and
    // GetAbilityIdForSlot reports that resolved answer. A picker must show the
    // choice, not the fallback, or the player cannot tell "I picked Cleave"
    // from "Cleave is what you get when you pick nothing".
    UFUNCTION(BlueprintPure, Category="Abilities|Selection")
    FName GetEquippedAbilityId(EBreakerAbilitySlot Slot) const;

    // Side-effect-free "would this be allowed?". Safe to call every frame.
    UFUNCTION(BlueprintPure, Category="Abilities|Selection")
    EBreakerAbilitySelectionResult PreviewSelection(EBreakerAbilitySlot Slot, FName AbilityId) const;

    // Commits the choice. Server-authoritative, like every other grant path
    // here. Returns false and fills OutFailureReason with player-facing text on
    // any refusal, including one that comes back from progression's own unlock
    // rules — those are not restated here, because a second copy of "is this
    // unlocked" would drift.
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Abilities|Selection")
    bool TryEquipAbility(EBreakerAbilitySlot Slot, FName AbilityId, FText& OutFailureReason);

    // The pure rule behind PreviewSelection, world-free and testable: the
    // precedent of BreakerRangedBehavior.h and BreakerMonsterChassis.h. Takes
    // the loadout as three ids rather than reading it, so the whole selection
    // rule set can be exercised with no components at all.
    static EBreakerAbilitySelectionResult ValidateSelection(
        EBreakerClassId ClassId, EBreakerAbilitySlot Slot, FName AbilityId,
        FName EquippedOne, FName EquippedTwo, FName EquippedUltimate);

    // Player-facing text for a refusal. Localised in one place so the UI never
    // authors its own copy of a rule's wording.
    static FText DescribeSelectionResult(EBreakerAbilitySelectionResult Result);

protected:
    UFUNCTION(Server, Reliable) void ServerActivateSlot(EBreakerAbilitySlot Slot);

private:
    UAbilitySystemComponent* GetAbilitySystem() const;
    UBreakerProgressionComponent* GetProgression() const;
    bool BuildLoadoutSignature(FString& OutSignature) const;

    UPROPERTY() TMap<EBreakerAbilitySlot, FBreakerGrantedAbility> GrantedBySlot;

    mutable TWeakObjectPtr<UAbilitySystemComponent> CachedAbilitySystem;
    mutable TWeakObjectPtr<UBreakerProgressionComponent> CachedProgression;
    FString CachedLoadoutSignature;
    float PollElapsed = 0.0f;
};
