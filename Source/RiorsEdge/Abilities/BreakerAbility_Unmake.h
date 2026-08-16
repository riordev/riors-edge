#pragma once

#include "CoreMinimal.h"
#include "Abilities/BreakerCasterAbility.h"
#include "Combat/BreakerStatusComponent.h"
#include "BreakerAbility_Unmake.generated.h"

class ABreakerCharacter;

// One Cascade ear per status-bearing actor. The status layer's application
// event is FBreakerActiveStatus only — it does not say WHO it fired on — so
// each binding needs an object that remembers its target. This is that object:
// a weak target, a weak caster, and one handler. Created by Unmake when the
// Cascade window opens, unbound and discarded when it closes.
UCLASS()
class RIORSEDGE_API UBreakerCascadeEchoListener : public UObject
{
    GENERATED_BODY()

public:
    UPROPERTY() TWeakObjectPtr<AActor> Target;
    UPROPERTY() TWeakObjectPtr<ABreakerCharacter> Caster;

    UFUNCTION() void HandleStatusApplied(const FBreakerActiveStatus& Status);
};

// UNMAKE — the Caster ultimate (Class-Kits §2.2, Ability-Implementation-Spec
// §5.7): 80 Mana, no cooldown. "For 6 seconds, all Caster abilities cost 0
// Mana, and Mana generation is suspended. The bar's remaining value at cast
// time is irrelevant; the window is fixed."
//
// Keystone rewrites (spec D1, resolved from the owner's tag container):
//   Edgework  — Cleave loses its animation lock and Closequarter loses its
//               range limit within line of sight. BUILT, both halves: each
//               ability reads the keystone tag AND the live Unmake window
//               (the two-part gate; the tag alone is permanent from node
//               purchase and shipped exactly that bug once, D10).
//   Long Dark — 12s duration, abilities cost 50% instead of 0%. Fully
//               parametric: both numbers are variant-row fields, no branch.
//   Cascade   — every status application by this Caster during the window
//               also applies the next status in Fracture's cycle to the same
//               target, at proc coefficient 0. BUILT below: the window binds a
//               listener to every status-bearing actor's application event and
//               echoes through the caster's own status cycle. Proc coefficient
//               0 on the echo is the load-bearing termination (Master 7.10.1):
//               the listener ignores applications carrying 0, so an echo can
//               never echo. KNOWN LIMIT: actors spawned after the window opens
//               are not bound and do not echo until the next Unmake.
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

    // Pure rule, Cascade: does this application echo? All four legs or
    // nothing — the keystone must be owned, the window must be live, the
    // application must be the caster's own, and the application must itself
    // carry a positive proc coefficient (the echo's own 0 is what stops
    // echo-of-echo recursion, Master 7.10.1).
    UFUNCTION(BlueprintPure, Category="Unmake")
    static bool ShouldCascadeEcho(bool bCascadeHeld, bool bWindowActive, bool bInstigatedByCaster, float AppliedProcCoefficient);

    // Pure rule, Cascade: the echoed application is the cycle entry's own spec
    // riding the item-level scalar (Fracture's rule, O35), with its proc
    // coefficient forced to 0. Everything else is untouched — the echo is the
    // next cycle status, not a new status.
    UFUNCTION(BlueprintPure, Category="Unmake")
    static FBreakerStatusApplicationSpec MakeCascadeEchoSpec(FBreakerStatusApplicationSpec CycleSpec, float DamageScalar);

private:
    void BeginCascadeListening(UWorld* World, ABreakerCharacter* Character);
    void EndCascadeListening();

    UPROPERTY() TArray<TObjectPtr<UBreakerCascadeEchoListener>> CascadeListeners;

    FTimerHandle WindowTimer;
};
