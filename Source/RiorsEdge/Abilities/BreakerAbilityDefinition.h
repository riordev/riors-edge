#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Progression/BreakerProgressionTypes.h"
#include "BreakerAbilityDefinition.generated.h"

class UGameplayAbility;

// Data contract for one class ability (Ability-Implementation-Spec SI-10).
// All tuning lives here, never in the ability's C++.
UCLASS(BlueprintType)
class RIORSEDGE_API UBreakerAbilityDefinition : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    virtual FPrimaryAssetId GetPrimaryAssetId() const override
    {
        return FPrimaryAssetId(TEXT("AbilityDefinition"), AbilityId);
    }

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Identity") FName AbilityId = NAME_None;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Identity") EBreakerClassId ClassId = EBreakerClassId::None;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Identity") FText DisplayName;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Identity") FText Description;
    // Which loadout slot this ability is designed for. Ultimates may only be
    // equipped in the Ultimate slot; class abilities may sit in either of the two.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Identity") EBreakerAbilitySlot SlotAffinity = EBreakerAbilitySlot::ClassAbilityOne;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Identity") FGameplayTag AbilityTag;
    // Cooldown tracking tag, granted by the cooldown GameplayEffect. Empty when
    // the ability is purely cost-gated (Class-Kits §0.3: "Mana *is* the cooldown").
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Identity") FGameplayTag CooldownTag;

    // Null while an ability is designed but not yet implemented. The ability
    // component records the slot as unimplemented rather than granting nothing
    // silently.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Implementation") TSubclassOf<UGameplayAbility> AbilityClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cost", meta=(ClampMin="0")) float ResourceCost = 0.0f;
    // 0 = no cooldown at all (cost-gated). The HUD must distinguish this from a
    // cooldown that happens to be ready.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cost", meta=(ClampMin="0")) float CooldownSeconds = 0.0f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cost", meta=(ClampMin="0")) float WindowDuration = 0.0f;

    UFUNCTION(BlueprintPure, Category="Abilities") float GetResourceCost() const { return ResourceCost; }
    UFUNCTION(BlueprintPure, Category="Abilities") float GetCooldownSeconds() const { return CooldownSeconds; }
    UFUNCTION(BlueprintPure, Category="Abilities") bool HasCooldown() const { return CooldownSeconds > 0.0f; }
    UFUNCTION(BlueprintPure, Category="Abilities") bool IsImplemented() const { return AbilityClass != nullptr; }
    UFUNCTION(BlueprintPure, Category="Abilities") bool IsUltimate() const { return SlotAffinity == EBreakerAbilitySlot::Ultimate; }
    UFUNCTION(BlueprintPure, Category="Abilities") bool CanOccupySlot(EBreakerAbilitySlot Slot) const;

    // Zero-setup C++ fallback registry, matching the weapon prototype
    // convention: the ability layer is playable before any Data Asset exists.
    // Shipped Data Assets take precedence once authored.
    static UBreakerAbilityDefinition* FindFallback(FName AbilityId);
    static const TArray<UBreakerAbilityDefinition*>& GetFallbackRegistry();
    // Default Swift loadout used until a class definition supplies a catalogue.
    static FName DefaultAbilityIdForSlot(EBreakerClassId ClassId, EBreakerAbilitySlot Slot);
};
