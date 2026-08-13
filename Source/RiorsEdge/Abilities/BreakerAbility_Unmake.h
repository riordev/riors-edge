#pragma once

#include "CoreMinimal.h"
#include "Abilities/BreakerCasterAbility.h"
#include "BreakerAbility_Unmake.generated.h"

// UNMAKE — the Caster ultimate (Class-Kits §2.2, Ability-Implementation-Spec
// §5.7): 80 Mana, no cooldown. "For 6 seconds, all Caster abilities cost 0
// Mana, and Mana generation is suspended. The bar's remaining value at cast
// time is irrelevant; the window is fixed."
//
// Keystone rewrites (spec D1, resolved from the owner's tag container):
//   Edgework  — Cleave loses its animation lock and Closequarter loses its
//               range limit within line of sight. IMPLEMENTED FOR CLEAVE ONLY:
//               UBreakerAbility_Cleave reads the keystone tag directly. The
//               Closequarter half needs a line-of-sight range override and is
//               NOT built.
//   Long Dark — 12s duration, abilities cost 50% instead of 0%. Fully
//               parametric: both numbers are variant-row fields, no branch.
//   Cascade   — every status application also applies the next status in
//               Fracture's cycle at proc coefficient 0. NOT BUILT: Fracture
//               (C5) and the status-cycle component do not exist.
//
// Both unbuilt halves are registered as variant rows so the selector is real
// and the gap is visible, rather than a keystone that silently does nothing.
UCLASS()
class RIORSEDGE_API UBreakerAbility_Unmake : public UBreakerCasterAbility
{
    GENERATED_BODY()

public:
    UBreakerAbility_Unmake();

    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
    virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

    // Suspension key on the Mana component, shared so activate and teardown
    // cannot drift.
    static FName GenerationSuspensionKey();

    // Pure rule: the cost scalar a variant row imposes on other Caster
    // abilities, clamped to something sane. An unauthored row must not make the
    // whole class free forever.
    UFUNCTION(BlueprintPure, Category="Unmake")
    static float ResolveCostScalar(float VariantCostMultiplier);

    // Pure rule: the ultimate's window length, falling back to the definition's
    // own duration when a row authors none.
    UFUNCTION(BlueprintPure, Category="Unmake")
    static float ResolveDuration(float VariantDuration, float DefinitionDuration);

private:
    FTimerHandle WindowTimer;
};
