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
