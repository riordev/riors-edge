#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Combat/BreakerCombatTypes.h"
#include "Combat/BreakerEnemyModifiers.h"
#include "BreakerModifierComponent.generated.h"

class ABreakerZoneActor;
class UBreakerCombatComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FBreakerModifiersChanged);

// The runtime half of the modifier system. One component, composable onto any
// ABreakerEnemy, carrying 0-3 modifiers.
//
// WHAT THIS COMPONENT MAY SEE. It takes and stores AActor* for the thing the
// owner is currently fighting, and it reads that actor's WORLD POSITION and
// nothing else. It never casts to the player character, never touches an
// attribute set that is not its owner's, and never reads a level, an item, a
// tree or a build. Every magnitude it applies comes out of
// UBreakerEnemyModifierLibrary, which cannot see a player at all. That is the
// O27 invariant made structural rather than promised, and the test suite
// asserts the library half of it directly.
//
// WHAT IT REPLICATES. The modifier list, so a client can draw the nameplate's
// marks and print the banner without asking the server what this enemy is.
// Membership, damage, spawning, blinks and reflects are SERVER ONLY.
//
// ANNOUNCEMENT IS A REQUIREMENT, NOT A NICETY. Encounter-Design §1.2's first
// test is that a modifier is identifiable from 20 m in untextured graybox
// within 1.5s. An unannounced modifier is an unfair death, not a challenge.
// The announcement is the nameplate's (O203): one drawn mark per modifier in
// the system colour, Combat/BreakerEnemyBarMath.h's MarkFor, read off
// GetModifiers every frame. This component owns no presentation — no mesh,
// no light, no colour on the body (O129) — so a modifier is a rule and a
// list entry, and nothing else has to be built for a client to see it.
UCLASS(ClassGroup=Combat, BlueprintType, meta=(BlueprintSpawnableComponent))
class RIORSEDGE_API UBreakerEnemyModifierComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UBreakerEnemyModifierComponent();
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // --- Granting ---------------------------------------------------------

    // Replaces the whole set. Illegal sets are REJECTED rather than trimmed:
    // silently dropping a modifier would make a spawner think it authored a
    // Champion when it authored a Veteran. Returns false and leaves the
    // existing set untouched.
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Enemy|Modifiers")
    bool SetModifiers(const TArray<EBreakerEnemyModifier>& NewModifiers);

    // The roll path a spawner uses: deterministic in the seed, always legal,
    // and scoped to the enemy's FAMILY so a Vestige never gets a modifier that
    // reads as tactical discipline (Assets/story-source.md §1.5).
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Enemy|Modifiers")
    TArray<EBreakerEnemyModifier> RollAndApplyModifiers(int32 Seed,
        EBreakerEnemyFamily Family = EBreakerEnemyFamily::Vestige);

    UFUNCTION(BlueprintPure, Category="Enemy|Modifiers") const TArray<EBreakerEnemyModifier>& GetModifiers() const { return Modifiers; }
    UFUNCTION(BlueprintPure, Category="Enemy|Modifiers") bool HasModifier(EBreakerEnemyModifier Modifier) const { return Modifiers.Contains(Modifier); }
    UFUNCTION(BlueprintPure, Category="Enemy|Modifiers") int32 GetModifierCount() const { return Modifiers.Num(); }
    UFUNCTION(BlueprintPure, Category="Enemy|Modifiers") FString GetBanner() const;

    UPROPERTY(BlueprintAssignable, Category="Enemy|Modifiers") FBreakerModifiersChanged OnModifiersChanged;

    // --- Owner hooks ------------------------------------------------------
    // Called by ABreakerEnemy at the four moments a modifier can change the
    // rules. Direct calls rather than delegate bindings, because Wakeful has to
    // run BEFORE the death handling it suppresses and delegate broadcast order
    // is a registration-order accident, not a contract.

    // The current target's position source. AActor* only — see the class note.
    void SetTrackedTarget(AActor* Target) { TrackedTarget = Target; }
    UFUNCTION(BlueprintPure, Category="Enemy|Modifiers") AActor* GetTrackedTarget() const { return TrackedTarget.Get(); }

    // Per-frame clocks: ward recharge, the aura pulse, the phase telegraph and
    // the blink. Called from the owner's Tick, and directly by tests, in the
    // precedent of UBreakerStatusComponent::AdvanceStatuses.
    UFUNCTION(BlueprintCallable, Category="Enemy|Modifiers")
    void AdvanceModifiers(float DeltaSeconds);

    // Cascading: the owner landed a hit at this point.
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Enemy|Modifiers")
    void NotifyAttackLanded(const FVector& ImpactLocation);

    // Wakeful. Returns true when the death is SUPPRESSED and the owner must
    // enter a downed state instead of resolving death. Consumes the revive, so
    // it can only ever return true once.
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Enemy|Modifiers")
    bool TryConsumeWakefulRevive(bool bKilledByWeakPoint);

    UFUNCTION(BlueprintPure, Category="Enemy|Modifiers") float GetWakefulReviveDelay() const;
    UFUNCTION(BlueprintPure, Category="Enemy|Modifiers") float GetWakefulReviveHealthFraction() const;

    // Volatile and Splitting. Called once the death is final.
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Enemy|Modifiers")
    void NotifyOwnerDied();

    // Releases everything this component pushed onto OTHER actors — the warding
    // aura's incoming-damage entries. Idempotent.
    UFUNCTION(BlueprintCallable, Category="Enemy|Modifiers")
    void ReleaseExternalEffects();

    // --- Live state a HUD or the owner can read -----------------------------
    // The nameplate underlines a mark while its rule is firing. Four modifiers
    // keep a state a frame can observe; these read it and nothing else.
    UFUNCTION(BlueprintPure, Category="Enemy|Modifiers") bool IsBlinking() const { return BlinkRemaining > 0.0f; }
    UFUNCTION(BlueprintPure, Category="Enemy|Modifiers") bool IsPhaseTelegraphing() const { return bPhaseTelegraphing; }
    // Same sign as TickFuse: the fuse is -1 when unlit and counts down from
    // FuseTotal while lit.
    UFUNCTION(BlueprintPure, Category="Enemy|Modifiers") bool IsFuseLit() const { return FuseRemaining > 0.0f; }
    // Up only inside one reflect call — bReflecting is the re-entrancy guard,
    // not a window — so a reader outside that call sees false.
    UFUNCTION(BlueprintPure, Category="Enemy|Modifiers") bool IsReflecting() const { return bReflecting; }
    UFUNCTION(BlueprintPure, Category="Enemy|Modifiers") bool IsAuraHolding() const { return AuraTargets.Num() > 0; }

    // Every tunable, in one authored block. O2 PLACEHOLDER throughout.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy|Modifiers") FBreakerEnemyModifierParams Params;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;

    UFUNCTION() void HandleOwnerDamaged(const FBreakerHitContext& Hit);

    // Re-applies every persistent, self-targeted consequence of the current
    // set: the ward and the speed change. Idempotent, because the chassis can
    // be rebuilt under it at any time.
    void ApplyPersistentModifiers();

    void TickWard(float DeltaSeconds);
    void TickAura(float DeltaSeconds);
    void TickPhasing(float DeltaSeconds);

    void DetonateVolatile();
    void SpawnSplits();
    void SpawnCascadeHazard(const FVector& Location);

    UPROPERTY(ReplicatedUsing=OnRep_Modifiers) TArray<EBreakerEnemyModifier> Modifiers;
    UFUNCTION() void OnRep_Modifiers();

private:
    UBreakerCombatComponent* OwnerCombat() const;
    float OwnerMaxHealth() const;
    float OwnerAttackDamage() const;

    TWeakObjectPtr<AActor> TrackedTarget;
    // Everyone currently carrying this enemy's aura entry, so the entry is
    // released when they leave the radius or when this enemy dies. Weak: an
    // ally that dies mid-aura must not be kept alive by the buff list.
    TArray<TWeakObjectPtr<AActor>> AuraTargets;
    TArray<TWeakObjectPtr<ABreakerZoneActor>> LiveHazards;

    float AuraPulseTimer = 0.0f;
    float PhaseTimer = 0.0f;
    float PhaseTelegraphRemaining = 0.0f;
    float BlinkRemaining = 0.0f;
    bool bPhaseTelegraphing = false;
    float FuseRemaining = -1.0f;
    float FuseTotal = 0.0f;
    // Who popped this Volatile, snapshotted on the death frame rather than
    // read at detonation: the fuse outlives the death by design, and the
    // answer must be the killing blow's, not whatever touched the corpse
    // last. Weak — an attacker who dies inside the fuse credits nobody (O245).
    TWeakObjectPtr<AActor> VolatileCreditTo = nullptr;
    bool bWakefulSpent = false;
    // Re-entrancy guard for the reflect. Belt and braces: the reflect is dealt
    // as TrueDamage and this component only ever reflects Physical/Elemental,
    // so a reflect can never reflect a reflect even without the flag.
    bool bReflecting = false;
    bool bExternalEffectsReleased = false;
};
