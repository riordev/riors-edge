#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Combat/BreakerCombatTypes.h"
#include "Weapons/BreakerWeaponComponent.h"
#include "BreakerMomentumComponent.generated.h"

class UBreakerAttributeSet;
class UBreakerCharacterMovementComponent;
class UBreakerProgressionComponent;

UENUM(BlueprintType)
enum class EBreakerMomentumState : uint8
{
    Settled,
    Running,
    Redline
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FBreakerMomentumStateChanged, EBreakerMomentumState, NewState);

// Swift's Momentum loop: purposeful movement fills the class resource and
// inaction drains it. Server-authority only; inert unless the owner's
// permanent class is Swift.
UCLASS(ClassGroup=Classes, BlueprintType, meta=(BlueprintSpawnableComponent))
class RIORSEDGE_API UBreakerMomentumComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UBreakerMomentumComponent();
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
    // The whole per-frame loop, mechanically separated out of TickComponent
    // (no value or behaviour change) so it can run in an automation test with
    // no world — UActorComponent::TickComponent asserts on an unregistered
    // component, which is exactly what every test component in this suite is.
    // Same split as UBreakerManaComponent::AdvanceLoop; see that component's
    // header for the one divergence NOT unified here: Mana's AdvanceLoop also
    // polls class ownership every tick as a defensive backstop, and this
    // component does not — it relies solely on the bound OnProgressionChanged
    // delegate (RiorsEdge.Classes.ClassLockNotifiesLoop covers that path).
    void AdvanceLoop(float DeltaTime);

    UFUNCTION(BlueprintPure, Category="Momentum") EBreakerMomentumState GetMomentumState() const { return CachedState; }
    UFUNCTION(BlueprintPure, Category="Momentum") bool IsActiveForOwner() const;
    UFUNCTION(BlueprintPure, Category="Momentum") float GetMomentum() const;
    UFUNCTION(BlueprintPure, Category="Momentum") float GetMomentumFraction() const;
    // ResourceDepleted eligibility (build-math finding #3): "depleted" means
    // DRAINED past empty, which only a loop that RESTS full can be. Momentum
    // is earned by moving and decays to zero at rest — an empty bar is the
    // idle state, not a drained one — so this loop answers false and a
    // ResourceDepleted line can never read as always-on for a standing Swift.
    // Mana is the one loop that answers true; see its declaration.
    UFUNCTION(BlueprintPure, Category="Momentum") bool IsRestingStateFull() const { return false; }

    // Direct credit, mirroring UBreakerManaComponent::GrantMana(Amount,
    // bIgnoreGlobalCap=true): it bypasses the metered per-second generation
    // budget (PendingGrants/GlobalGenerationCap) entirely rather than queuing,
    // because a keystone refund is not generation and trickling it in through
    // the budget would read as a bug. Clamped to MaxClassResource by the same
    // ApplyMomentumDelta clamp every other credit path uses; non-positive
    // amounts are ignored; inert (no grant, no clamp write) for a non-Swift
    // owner via the same IsActiveForOwner() gate as the rest of this loop.
    // Observable the same way the Mana side is observable: through GetMomentum().
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Momentum") void GrantMomentum(float Amount);

    // Loop overrides (Class-Kits §1.2 ULTIMATE). A named, temporary rewrite of
    // the loop itself rather than of a magnitude: decay can be suspended and
    // generation multiplied, both against the per-second cap, which is what
    // "Momentum does not decay and all generation is doubled" actually means.
    // Lazily expired, mirroring the movement component's PushSpeedMultiplier:
    // no timers, no teardown path to get wrong if the pusher dies first.
    // DecayRateMultiplier is the loop valve's decay lane (2026-08-16): the
    // tree's ClassResourceDecay aggregate arrives through it, pushed keyed by
    // UBreakerProgressionComponent::PushLoopValveOverrides. 1.0 is neutral,
    // 2.0 doubles every decay rate, 0.0 is a legal full suspension (Reserve's
    // while-ADS line composes to exactly that) — only a NEGATIVE value is
    // meaningless, and it is loud like the generation guard. Defaulted so
    // every pre-existing caller (Overdrive, Standing Wave) is unchanged.
    UFUNCTION(BlueprintCallable, Category="Momentum|Loop") void PushLoopOverride(FName Key, bool bSuspendDecay, float GenerationMultiplier, float Duration, float DecayRateMultiplier = 1.0f);
    UFUNCTION(BlueprintCallable, Category="Momentum|Loop") void PopLoopOverride(FName Key);
    UFUNCTION(BlueprintPure, Category="Momentum|Loop") bool IsDecaySuspended() const;
    UFUNCTION(BlueprintPure, Category="Momentum|Loop") float GetGenerationMultiplier() const;
    // Product of every active override's decay-rate multiplier, composed like
    // the generation stack. Multiplies the rate DecayRateForSpeed returns.
    UFUNCTION(BlueprintPure, Category="Momentum|Loop") float GetDecayRateMultiplier() const;
    UFUNCTION(BlueprintPure, Category="Momentum|Loop") int32 GetActiveLoopOverrideCount() const;

    // ---- Frenzy rule halves (Class-Kits §1.3, LIVE 2026-08-16) ------------
    // The branch's fire-cadence rules live HERE, on the loop they pay into,
    // fed by the weapon's own delegates (OnShot / OnMagazineEmptied) and the
    // combat component's kill event. Each pure rule is a static so the
    // world-free suite pins buy-the-node-observable changes.

    // F1 Trigger Discipline: "Momentum generation from weak-point hits no
    // longer requires being airborne or sliding. R2: internal cooldown
    // 0.25s -> 0.15s." Both halves transcribed. The posture rule: satisfied
    // when the component never required posture, when the owner holds it, or
    // when the node is owned at any rank.
    static bool WeakPointPostureSatisfied(bool bRequiresAirborneOrSlide, bool bAirborneOrSliding, int32 TriggerDisciplineRank);
    // R2's shorter internal cooldown; ranks 0-1 keep the authored interval.
    static float WeakPointIntervalForRank(float BaseInterval, int32 TriggerDisciplineRank);

    // F4 Rhythm: "Every 5th consecutive hit on any target generates +8
    // Momentum, ignoring the global per-second cap. R2: every 4th. Missing
    // resets the counter." The stride per rank; 0 means the node pays nothing.
    static int32 RhythmStride(int32 RhythmRank);

    // F6 Feed: "Kills refund Momentum equal to 10% of the ability cost most
    // recently paid (R2: 20%)."
    static float FeedRefundFraction(int32 FeedRank);

    // ---- K9 Momentum Shield (Class-Kits §1.4, LIVE 2026-08-16) ------------
    // Transcribed: "While at Redline, incoming damage is reduced by an amount
    // equal to the Damage Reduction While Airborne affix value even when
    // grounded. Rewrite: it changes WHEN an existing stat applies, not its
    // magnitude." Paid through the incoming-damage chain Combat/ already
    // exposes (UBreakerCombatComponent::PushIncomingDamageModifier — the same
    // stage gear-rolled physical reduction occupies), pushed and removed from
    // this loop, which is the one place that knows the Redline band. The
    // GROUNDED gate is the node's own: airborne application is the affix's job
    // the day it exists, and applying here too would double it up then.
    // Returns the chain multiplier: 1.0 (no change) or 1 - fraction.
    static float MomentumShieldIncomingMultiplier(bool bNodeOwned, EBreakerMomentumState State, bool bGrounded, float ReductionFraction);
    // Key for this loop's entry on the owner's incoming-damage chain.
    static FName MomentumShieldModifierKey();

    // The last external spend this loop observed (an ability cost is the one
    // writer of the class resource that is not this component), for Feed and
    // for tests. 0 until a spend has been seen.
    UFUNCTION(BlueprintPure, Category="Momentum") float GetLastObservedSpend() const { return LastObservedSpend; }

    // Pure loop rules, exposed for tests and for the eventual DA_MomentumPolicy
    // asset that will own these numbers.
    static EBreakerMomentumState StateForFraction(float Fraction);
    // Overlapping overrides compose multiplicatively, like the speed stack.
    static float ComposeGenerationMultipliers(const TArray<float>& Multipliers);
    // Negative expiry means "never expires on its own".
    static bool IsLoopOverrideExpired(double ExpiryTime, double Now);
    static float GroundSpeedRate(float Speed, float ThresholdSpeed, float UpperSpeed, float RateAtThreshold, float RateAtUpper);
    static float ClampGeneration(float RequestedRate, float GlobalCap);
    static float DecayRateForSpeed(float Speed, float SettledSpeed, float ThresholdSpeed, float SettledDecay, float SlowDecay);
    // THE EXPLOIT PATCH (owner-ruled, the momentum sentence): the airborne
    // credit ticks down while airborne, refills only on genuine ground, and
    // FREEZES through a ledge traversal. The old inline rule refilled it on
    // "not airborne", and a traversal is not-airborne without being grounded
    // — so jump, mantle mid-fall, and the 3-second window came back refunded
    // (jump-mantle-fall, the recon's exploit shape). Airborne wins over the
    // traversal flag by precedence, defensively: the movement mode cannot be
    // both, but the rule must not depend on a caller knowing that.
    static float TickAirborneCredit(float Remaining, bool bAirborne, bool bTraversingLedge, float DeltaTime, float RefillSeconds);
    // Test/HUD read on the credit the exploit patch protects.
    UFUNCTION(BlueprintPure, Category="Momentum") float GetAirborneCreditRemaining() const { return AirborneCreditRemaining; }

    // Binds the attribute set directly. BeginPlay resolves it from the owner's
    // ability system; this is the same wiring for a component built outside a
    // world (the pattern UBreakerManaComponent::BindAttributes and the
    // equipment/progression components already use). AdvanceLoop's very first
    // guard is `!Attributes`, so this is what makes AdvanceLoop reachable at
    // all in an automation test — the one piece Mana already had that this
    // component did not.
    void BindAttributes(UBreakerAttributeSet* InAttributes);

    UPROPERTY(BlueprintAssignable, Category="Momentum") FBreakerMomentumStateChanged OnMomentumStateChanged;

    // O92: the generation threshold was a trap and it moves. It was 750
    // against a WalkSpeed of 700, so walking paid nothing and — the part that
    // made it a trap rather than a choice — EVERY aimed state fell under the
    // bar, because aiming multiplies move speed by 0.45 to 0.92. A Swift who
    // aimed disabled their own resource, on a permanent class, and the most
    // natural way to play the projectile class is down the sights.
    //
    // THIS EXACT BUG WAS ALREADY FIXED ONCE, THREE FILES AWAY: the retired
    // wall-ride's entry gate shipped at exactly WalkSpeed, unreachable. Same
    // shape, same file family, caught once and missed here.
    //
    // 450 is that same gate, so the three movement floors now agree — wall ride
    // 450, slide entry 550, this. It was chosen rather than tuned because
    // LOWERING ALONE CANNOT KEEP WALKING FREE: the worst aimed sprint is
    // 1100 x 0.45 = 495, so any threshold an aimed sprint clears is already
    // below the 700 walk speed. Walking therefore generates, at the floor rate
    // of 6/s against a sprint's 9.25/s, and the rate curve is what separates
    // committed movement from ambling. If walking paying at all turns out to be
    // wrong in a playtest, the other half of O92 is the answer — exempt aimed
    // states and put the threshold back above the walk ceiling — and that is a
    // change to what is measured, not to this number.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Momentum|Generation", meta=(ClampMin="0")) float GroundThresholdSpeed = 450.0f;   // O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Momentum|Generation", meta=(ClampMin="0")) float GroundUpperSpeed = 1250.0f;   // O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Momentum|Generation", meta=(ClampMin="0")) float GroundRateAtThreshold = 6.0f;   // O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Momentum|Generation", meta=(ClampMin="0")) float GroundRateAtUpperSpeed = 10.0f;   // O2 PLACEHOLDER
    // Anti-farm: running into a wall must generate nothing, so ground credit
    // requires 3.0 m of world-space displacement per second.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Momentum|Generation", meta=(ClampMin="0")) float GroundDisplacementPerSecond = 300.0f;   // O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Momentum|Generation", meta=(ClampMin="0")) float AirborneRate = 8.0f;   // O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Momentum|Generation", meta=(ClampMin="0")) float AirborneCreditSeconds = 3.0f;   // O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Momentum|Generation", meta=(ClampMin="0")) float SlideRate = 12.0f;   // O2 PLACEHOLDER
    // The wall-ride generation source retired with the verb (Part One-R).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Momentum|Generation", meta=(ClampMin="0")) float DashGrant = 10.0f;   // O2 PLACEHOLDER
    // Floor only: the real internal cooldown is the movement component's dash
    // cooldown, so refunded charges cannot be farmed.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Momentum|Generation", meta=(ClampMin="0")) float DashGrantMinimumInterval = 1.0f;   // O2 PLACEHOLDER
    // Vault and mantle COUNT AS VERBS for Momentum (owner-ruled — the
    // question this lane filed is answered and deleted): a completed
    // traversal pays a small one-shot tick, the dash grant's shape, observed
    // off the movement component's completion recorder so a blocked abort
    // pays nothing. Smaller than the dash's on purpose: a traversal has no
    // cooldown of its own, so the minimum interval below is the whole
    // anti-farm — without it, vaulting back and forth over one crate would
    // out-earn sprinting.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Momentum|Generation", meta=(ClampMin="0")) float LedgeTraversalGrant = 6.0f;   // O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Momentum|Generation", meta=(ClampMin="0")) float LedgeTraversalGrantMinimumInterval = 1.0f;   // O2 PLACEHOLDER
    // "Swift converts evasion into Momentum" — an RNG proc off the passive
    // evade layer, never a timed input (Class-Kits 1.1, O1).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Momentum|Generation", meta=(ClampMin="0")) float DodgeProcGrant = 15.0f;   // O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Momentum|Generation", meta=(ClampMin="0")) float DodgeProcInterval = 0.5f;   // O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Momentum|Generation", meta=(ClampMin="0")) float WeakPointGrant = 5.0f;   // O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Momentum|Generation", meta=(ClampMin="0")) float WeakPointInterval = 0.25f;   // O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Momentum|Generation") bool bWeakPointRequiresAirborneOrSlide = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Momentum|Generation", meta=(ClampMin="0")) float GlobalGenerationCap = 25.0f;   // O2 PLACEHOLDER

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Momentum|Decay", meta=(ClampMin="0")) float SettledSpeed = 400.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Momentum|Decay", meta=(ClampMin="0")) float SettledDecayRate = 15.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Momentum|Decay", meta=(ClampMin="0")) float SlowDecayRate = 6.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Momentum|Decay", meta=(ClampMin="0")) float DecayGraceSeconds = 1.0f;

    // K9 Momentum Shield's magnitude. O2 PLACEHOLDER: stands in for the
    // Damage Reduction While Airborne affix value the node re-sites — that
    // affix has no row in Items/BreakerAffixLibrary yet, so there is no roll
    // to read. Replaced by the equipped item's roll the day the affix exists.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Momentum|Shield", meta=(ClampMin="0", ClampMax="1")) float MomentumShieldReductionFraction = 0.25f;   // O2 PLACEHOLDER

    // Phantom Step (Core.Kinesis.PhantomStep): "a successful Dodge grants brief
    // invulnerability on a 2.0s internal cooldown". Implemented as a full-dodge
    // window pushed onto the combat component's existing passive DodgeChance —
    // the only invulnerability primitive that exists without editing Combat/.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Momentum|PhantomStep", meta=(ClampMin="0")) float PhantomStepWindowSeconds = 0.5f;  // O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Momentum|PhantomStep", meta=(ClampMin="0")) float PhantomStepCooldownSeconds = 2.0f; // Class-Kits node text

    // Public because it is the listener half of a contract that silently broke
    // once: the loop caches bIsSwift here, BeginPlay calls it exactly once, and
    // for a while nothing on the class-selection path broadcast. The suite has
    // no world, so a test cannot rely on BeginPlay to bind the delegate — it
    // binds this directly. Idempotent; calling it spuriously costs a lookup.
    UFUNCTION() void HandleProgressionChanged();

    // Public for the same worldless-suite reason as HandleProgressionChanged:
    // the suite cannot rely on BeginPlay to bind delegates, so the Frenzy
    // rule handlers are directly callable. HandleShot drives Rhythm (F4) and
    // the weak-point grant (with F1's posture rewrite); HandleMagazineEmptied
    // is Dry Fire (F5); HandleKillDealt is Feed (F6).
    UFUNCTION() void HandleShot(const FBreakerShotResult& Shot);
    UFUNCTION() void HandleMagazineEmptied(bool bStartedFull);
    UFUNCTION() void HandleKillDealt(const FBreakerHitContext& Hit);

private:
    UFUNCTION() void HandleDamageReceived(const FBreakerDamageResult& Result);

    UBreakerCharacterMovementComponent* GetBreakerMovement() const;
    bool IsInSafeZone() const;
    void ApplyMomentumDelta(float Delta);
    // Spend observation (Feed's producer half). Every write this component
    // makes goes through ApplyMomentumDelta, so any DECREASE of the class
    // resource between our own writes is an external spend — and ability
    // costs are the one external writer that decreases it. Called before
    // every internal write and at the top of the loop; records the drop in
    // LastObservedSpend and re-baselines the cache either way.
    void ObserveExternalSpend();
    // Rank of a Frenzy node on the cached progression component, 0 without one.
    int32 GetFrenzyNodeRank(FName NodeId) const;
    void RefreshState();
    void TryPhantomStep();
    // K9's push/remove bookkeeping: keeps the owner's incoming-damage chain in
    // step with the multiplier above. Called from AdvanceLoop once posture is
    // known, and with bGrounded=false on any path that must tear it down.
    void UpdateMomentumShield(bool bGrounded);

    struct FLoopOverrideEntry
    {
        bool bSuspendDecay = false;
        float GenerationMultiplier = 1.0f;
        // Scales the decay rate; 0 is a legal suspension, see PushLoopOverride.
        float DecayRateMultiplier = 1.0f;
        // Negative = no expiry; popped explicitly.
        double ExpiryTime = -1.0;
    };
    // Mutable: the pure reads below are const and are the natural place to drop
    // expired entries, which is what "lazy expiry" means here.
    mutable TMap<FName, FLoopOverrideEntry> LoopOverrides;
    void PruneLoopOverrides() const;

    UPROPERTY() TObjectPtr<UBreakerAttributeSet> Attributes;
    mutable TWeakObjectPtr<UBreakerCharacterMovementComponent> CachedMovement;
    TWeakObjectPtr<UBreakerProgressionComponent> CachedProgression;

    bool bIsSwift = false;
    EBreakerMomentumState CachedState = EBreakerMomentumState::Settled;
    FVector LastLocation = FVector::ZeroVector;
    bool bHasLastLocation = false;
    float AirborneCreditRemaining = 0.0f;
    float SettledElapsed = 0.0f;
    float PendingGrants = 0.0f;
    double LastDashGrantTime = -1000.0;
    double LastWeakPointGrantTime = -1000.0;
    double LastDodgeGrantTime = -1000.0;
    double LastPhantomStepTime = -1000.0;
    double LastObservedDashTime = -1000.0;
    // The traversal grant's observation pair, the dash pair's exact shape:
    // the movement recorder's timestamp last seen, and the last time a grant
    // was actually paid (the anti-farm interval reads this one).
    double LastObservedTraversalTime = -1000.0;
    double LastTraversalGrantTime = -1000.0;

    // ---- Frenzy rule-half state (Class-Kits §1.3) -------------------------
    // Rhythm's consecutive-hit counter (F4). Hitscan only: a rocket's shot
    // record carries no pellets and neither advances nor resets the count —
    // launching a rocket is not "missing".
    int32 ConsecutiveHits = 0;
    // Feed's spend observation: the class resource as this component last
    // left (or saw) it, and the size of the last external drop. Negative
    // baseline means "not yet observed".
    float LastKnownResource = -1.0f;
    float LastObservedSpend = 0.0f;

    // K9 Momentum Shield: whether this loop currently holds an entry on the
    // owner's incoming-damage chain, so push and remove happen exactly on the
    // Redline-grounded edges rather than every tick.
    bool bMomentumShieldPushed = false;
};
