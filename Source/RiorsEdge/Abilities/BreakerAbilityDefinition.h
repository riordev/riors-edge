#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Abilities/GameplayAbility.h"  // TSubclassOf<UGameplayAbility> below needs the complete type
#include "GameplayTagContainer.h"
#include "Progression/BreakerProgressionTypes.h"
#include "BreakerAbilityDefinition.generated.h"

class UGameplayAbility;

// One keystone variant row (Ability-Implementation-Spec D1). A branch keystone
// grants a passive GE carrying KeystoneTag; the ultimate resolves the matching
// row at activation. The row holds only the *parametric* deltas — the
// behavioral differences that are not expressible as parameters (Bloodrhythm's
// hit-timeout exit, Terminal Velocity's dash-charge availability) live as named
// C++ branches in the ability, guarded by this row's tag. That split is D1's
// whole point.
USTRUCT(BlueprintType)
struct RIORSEDGE_API FBreakerAbilityVariant
{
    GENERATED_BODY()

    // Empty = the base row, used when the owner holds no keystone.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Variant") FGameplayTag KeystoneTag;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Variant") FText VariantName;
    // <= 0 means "inherit the definition's WindowDuration".
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Variant", meta=(ClampMin="0")) float WindowDuration = 0.0f;
    // Temporary movement speed multiplier applied for the window.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Variant", meta=(ClampMin="0")) float SpeedMultiplier = 1.0f;
    // Seconds without a landed hit before the window self-terminates. <= 0
    // disables the check (Bloodrhythm is the only user today).
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Variant", meta=(ClampMin="0")) float HitTimeoutSeconds = 0.0f;
    // What the window does to the price of the OWNER'S OTHER abilities while it
    // is open. 1.0 = unchanged; Caster's Unmake authors 0.0 (free casts) and
    // 0.5 under the Long Dark keystone (Class-Kits §2.2). Ignored by windows
    // that do not rewrite costs.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Variant", meta=(ClampMin="0")) float AbilityCostMultiplier = 1.0f;
};

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

    // Index 0 is the base row when authored. Empty on non-ultimates.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Variants") TArray<FBreakerAbilityVariant> Variants;

    // Spec D1 selector: at most one keystone can be held (Class-Kits §0.2), so
    // this is a lookup, not a merge. Falls back to the base row, and to a
    // default-constructed row when nothing is authored.
    UFUNCTION(BlueprintPure, Category="Abilities") FBreakerAbilityVariant ResolveVariant(const FGameplayTagContainer& OwnerTags) const;

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
    // The DEFAULT loadout for a class — what a character is handed before they
    // choose. It is not the catalogue and it is not the only answer: an
    // equipped id on the progression state wins over it in ResolveDefinition,
    // and GetClassAbilityIds below is what a player actually picks from.
    //
    // This distinction was invisible for a while and cost the project real
    // content: with six Caster abilities implemented against three keys, the
    // default table was the only thing anything read, so exactly one of the
    // five class abilities was reachable at a time and moving Rot into slot two
    // made Closequarter dead content.
    static FName DefaultAbilityIdForSlot(EBreakerClassId ClassId, EBreakerAbilitySlot Slot);

    // THE CATALOGUE: every ability this class grants that can occupy this slot,
    // in registry order. Derived from the registry rather than from a
    // hand-maintained list, so an ability cannot be implemented, registered,
    // and still be unpickable — which is precisely the state the Caster kit was
    // in. A UI enumerates this to build its picker.
    static TArray<UBreakerAbilityDefinition*> GetClassAbilities(EBreakerClassId ClassId, EBreakerAbilitySlot Slot);
    // Id-only form, for callers that only need to name the choices.
    static TArray<FName> GetClassAbilityIds(EBreakerClassId ClassId, EBreakerAbilitySlot Slot);
    // Does this class grant this ability at all, for any slot? The first gate a
    // selection has to pass: a Swift may not equip Cleave however the id got
    // into the request.
    static bool ClassGrantsAbility(EBreakerClassId ClassId, FName AbilityId);
};
