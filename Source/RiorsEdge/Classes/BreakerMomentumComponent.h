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
    UFUNCTION(BlueprintCallable, Category="Momentum|Loop") void PushLoopOverride(FName Key, bool bSuspendDecay, float GenerationMultiplier, float Duration);
    UFUNCTION(BlueprintCallable, Category="Momentum|Loop") void PopLoopOverride(FName Key);
    UFUNCTION(BlueprintPure, Category="Momentum|Loop") bool IsDecaySuspended() const;
    UFUNCTION(BlueprintPure, Category="Momentum|Loop") float GetGenerationMultiplier() const;
    UFUNCTION(BlueprintPure, Category="Momentum|Loop") int32 GetActiveLoopOverrideCount() const;

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

    // Binds the attribute set directly. BeginPlay resolves it from the owner's
    // ability system; this is the same wiring for a component built outside a
    // world (the pattern UBreakerManaComponent::BindAttributes and the
    // equipment/progression components already use). AdvanceLoop's very first
    // guard is `!Attributes`, so this is what makes AdvanceLoop reachable at
    // all in an automation test — the one piece Mana already had that this
    // component did not.
    void BindAttributes(UBreakerAttributeSet* InAttributes);

    UPROPERTY(BlueprintAssignable, Category="Momentum") FBreakerMomentumStateChanged OnMomentumStateChanged;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Momentum|Generation", meta=(ClampMin="0")) float GroundThresholdSpeed = 750.0f;   // O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Momentum|Generation", meta=(ClampMin="0")) float GroundUpperSpeed = 1250.0f;   // O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Momentum|Generation", meta=(ClampMin="0")) float GroundRateAtThreshold = 6.0f;   // O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Momentum|Generation", meta=(ClampMin="0")) float GroundRateAtUpperSpeed = 10.0f;   // O2 PLACEHOLDER
    // Anti-farm: running into a wall must generate nothing, so ground credit
    // requires 3.0 m of world-space displacement per second.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Momentum|Generation", meta=(ClampMin="0")) float GroundDisplacementPerSecond = 300.0f;   // O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Momentum|Generation", meta=(ClampMin="0")) float AirborneRate = 8.0f;   // O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Momentum|Generation", meta=(ClampMin="0")) float AirborneCreditSeconds = 3.0f;   // O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Momentum|Generation", meta=(ClampMin="0")) float SlideRate = 12.0f;   // O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Momentum|Generation", meta=(ClampMin="0")) float WallRideRate = 10.0f;   // O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Momentum|Generation", meta=(ClampMin="0")) float WallRideCreditSeconds = 0.85f;   // O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Momentum|Generation", meta=(ClampMin="0")) float DashGrant = 10.0f;   // O2 PLACEHOLDER
    // Floor only: the real internal cooldown is the movement component's dash
    // cooldown, so refunded charges cannot be farmed.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Momentum|Generation", meta=(ClampMin="0")) float DashGrantMinimumInterval = 1.0f;   // O2 PLACEHOLDER
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

private:
    UFUNCTION() void HandleShot(const FBreakerShotResult& Shot);
    UFUNCTION() void HandleDamageReceived(const FBreakerDamageResult& Result);

    UBreakerCharacterMovementComponent* GetBreakerMovement() const;
    bool IsInSafeZone() const;
    void ApplyMomentumDelta(float Delta);
    void RefreshState();
    void TryPhantomStep();

    struct FLoopOverrideEntry
    {
        bool bSuspendDecay = false;
        float GenerationMultiplier = 1.0f;
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
    float WallRideCreditRemaining = 0.0f;
    float SettledElapsed = 0.0f;
    float PendingGrants = 0.0f;
    double LastDashGrantTime = -1000.0;
    double LastWeakPointGrantTime = -1000.0;
    double LastDodgeGrantTime = -1000.0;
    double LastPhantomStepTime = -1000.0;
    double LastObservedDashTime = -1000.0;
};
