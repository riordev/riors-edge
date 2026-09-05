#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Progression/BreakerProgressionTypes.h"
#include "BreakerCharacterMovementComponent.generated.h"

// Broadcast when the character arrives on the ground faster than
// LandingHeavyFallSpeed. Presentation only (camera dip, dust, audio) — the
// weight itself is applied in C++ before this fires.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FBreakerLandingImpact, float, ImpactSpeed);
// Broadcast the instant a dash charge is consumed, carrying the horizontal
// direction the dash committed to and the horizontal speed it produced.
// Presentation only, exactly like OnLandingImpact: the velocity change itself
// has already happened in C++ when this fires, so a listener can only dress it
// (camera punch, speed lines, controller rumble) and can never change the
// movement rule. Same shape so a HUD or Blueprint can bind either without the
// movement layer knowing anything about cameras or widgets.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FBreakerDashStarted, FVector, DashDirection, float, DashSpeed);

// The ledge-traversal verbs (Part One-R): vault for the low window, mantle
// for the high one. Runtime-only — never serialized, so it may be reordered.
enum class EBreakerLedgeVerb : uint8
{
    None,
    Vault,
    Mantle,
};

// One resolved traversal: the verb the ledge's height selects and the
// clearance-checked landing point. Produced by ResolveLedgeTraversal and
// executed by the component's own custom movement mode (the pawn owns
// nothing about the rules OR the execution any more).
struct FBreakerLedgeTraversal
{
    EBreakerLedgeVerb Verb = EBreakerLedgeVerb::None;
    FVector TargetLocation = FVector::ZeroVector;
};

// Broadcast when a ledge traversal COMPLETES — never on a blocked abort —
// carrying the verb that ran. Presentation only, the OnDashStarted shape:
// the movement itself is already finished when this fires, so a listener
// (the viewmodel's exit dip, a HUD read, rumble) can only dress it. Plain
// C++ multicast rather than dynamic because EBreakerLedgeVerb is a
// runtime-only enum with no UENUM reflection, on purpose.
DECLARE_MULTICAST_DELEGATE_OneParam(FBreakerLedgeTraversalCompleted, EBreakerLedgeVerb);

UCLASS(ClassGroup=Movement, BlueprintType)
class RIORSEDGE_API UBreakerCharacterMovementComponent : public UCharacterMovementComponent
{
    GENERATED_BODY()

public:
    UBreakerCharacterMovementComponent();

    virtual float GetMaxSpeed() const override;
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
    // Weight hooks. All three are engine extension points, so the heavier fall
    // curve is integrated inside the movement sub-steps rather than patched on
    // afterwards, and it therefore survives variable frame rates and the
    // client-prediction replay path unchanged.
    virtual FVector NewFallVelocity(const FVector& InitialVelocity, const FVector& Gravity, float DeltaTime) const override;
    virtual bool DoJump(bool bReplayingMoves, float DeltaTime) override;
    virtual void ProcessLanded(const FHitResult& Hit, float remainingTime, int32 Iterations) override;
    // The ledge traversal's execution home (the custom prediction mode). The
    // old execution was pawn-side SetActorLocation in MOVE_Flying — zero
    // impact for a listen-server host and a hard rubber-band for any remote
    // client, and INVISIBLE to every sibling predicate, because "is the
    // character traversing" had no movement mode to ask about. Running the
    // smoothstep inside the movement pipeline gives it a saved-move replay
    // path and gives the siblings (viewmodel bob, Momentum's airborne
    // credit, any HUD read) one honest predicate: IsTraversingLedge().
    virtual void PhysCustom(float deltaTime, int32 Iterations) override;
    virtual void UpdateCharacterStateBeforeMovement(float DeltaSeconds) override;
    virtual FNetworkPredictionData_Client* GetPredictionData_Client() const override;
    virtual void UpdateFromCompressedFlags(uint8 Flags) override;

    UFUNCTION(BlueprintCallable, Category="Movement") void SetSprinting(bool bEnabled);
    UFUNCTION(BlueprintCallable, Category="Movement") void SetSlideRequested(bool bEnabled);
    UFUNCTION(BlueprintCallable, Category="Movement") bool TryDash(const FVector& RequestedDirection);
    // Ability-Implementation-Spec §4.3: rotates existing horizontal velocity onto
    // Direction with NO magnitude gain. Skim's verb. It owns no cooldown of its
    // own — the ability pays cost and cooldown — and it never consumes the dash
    // charge. Returns false when there is not enough horizontal speed to
    // redirect (below walk speed), so a standing player cannot use it as a
    // free reposition.
    UFUNCTION(BlueprintCallable, Category="Movement") bool TryRedirect(const FVector& Direction);
    // Tag/key-keyed temporary speed multiplier, composed multiplicatively with
    // the gear and tree multipliers (Ability-Implementation-Spec §6, the shape
    // Gunsmith's Disruptor and Swift's Overdrive both need). Lazily expired:
    // no timers, no tick cost when nothing is pushed.
    UFUNCTION(BlueprintCallable, Category="Movement") void PushSpeedMultiplier(FName Key, float Multiplier, float Duration);
    UFUNCTION(BlueprintCallable, Category="Movement") void PopSpeedMultiplier(FName Key);
    UFUNCTION(BlueprintPure, Category="Movement") float GetSpeedMultiplier() const;

    // Keystone-rewrite availability suspensions (Swift's Terminal Velocity,
    // Class-Kits.md:192). Modeled on the PushSpeedMultiplier trio above: same
    // TMap<FName, entry>, same ExpiryTime convention (Duration <= 0 means -1.0,
    // "no expiry, popped explicitly"), same mutable-map + const-prune shape.
    // They are BOOLEANS, not scales, on purpose: both are availability
    // rewrites ("unlimited dash charges" honestly means the cooldown GATE is
    // suspended, not a new charge model, per O40(a)'s final single-dash-on-
    // cooldown model; "removes the wall-ride timer" means the duration expiry
    // stops firing). A scale would require authoring a magnitude, and ruling
    // O2 forbids any agent authoring a balance value — so there is no dial
    // here, only an on/off keyed to the caller's own duration.
    UFUNCTION(BlueprintCallable, Category="Movement") void PushDashCooldownSuspension(FName Key, float Duration);
    UFUNCTION(BlueprintCallable, Category="Movement") void PopDashCooldownSuspension(FName Key);
    UFUNCTION(BlueprintPure, Category="Movement") bool IsDashCooldownSuspended() const;
    UFUNCTION(BlueprintCallable, Category="Movement") bool BeginSlide();
    UFUNCTION(BlueprintCallable, Category="Movement") void PrepareSlideJump();
    UFUNCTION(BlueprintCallable, Category="Movement") void EndSlide();

    // ---- Ledge traversal: vault and mantle (Part One-R) -------------------
    // The verbs live HERE now. The pawn's TryMantle carried 48 fused lines of
    // traces and rules with zero tests, three files from the component that
    // extracted twelve pure predicates for exactly this reason — and its
    // ceiling (150) disagreed with the grammar's MantleStepHeight (145) by
    // five hand-copied centimetres. Rules and traces are the component's;
    // the pawn keeps only the smoothstep execution.
    //
    // Runs the wall probe, the top probe, the height band and the capsule
    // clearance; writes the resolved verb and landing point. Const — resolves
    // only, changes nothing. False when no traversable ledge is ahead.
    bool ResolveLedgeTraversal(FBreakerLedgeTraversal& OutTraversal) const;

    // ---- The traversal's own movement mode --------------------------------
    // MOVE_Custom sub-mode for the vault/mantle glide. ONE published value so
    // tests and siblings never hand-copy a magic zero.
    static constexpr uint8 CustomModeLedgeTraversal = 0;

    // Resolves a traversal from the character's current pose and, on success,
    // REQUESTS it: the request is consumed inside the next movement update
    // (UpdateCharacterStateBeforeMovement), which is what lets the same
    // request replay through the saved-move stream — the server and a
    // replaying client re-run the begin inside their own simulation instead
    // of trusting a pawn-side teleport. Returns false when nothing
    // traversable is ahead or a traversal is already running.
    UFUNCTION(BlueprintCallable, Category="Movement|Ledge") bool TryBeginLedgeTraversal();
    // The one honest read every sibling predicate was missing. True from the
    // frame the traversal begins to the frame it completes or aborts.
    UFUNCTION(BlueprintPure, Category="Movement|Ledge") bool IsTraversingLedge() const
    {
        return IsLedgeTraversalMode(MovementMode, CustomMovementMode);
    }
    // The verb currently running, None outside a traversal.
    EBreakerLedgeVerb GetActiveLedgeVerb() const { return IsTraversingLedge() ? TraversalVerb : EBreakerLedgeVerb::None; }
    // The body's actual speed along the traversal glide this frame (the
    // smoothstep's derivative), for the viewmodel's stride — velocity is
    // deliberately zero during a traversal, so anything reading Velocity sees
    // a standing character, which is exactly the dead-pose defect this read
    // exists to close. Zero outside a traversal.
    UFUNCTION(BlueprintPure, Category="Movement|Ledge") float GetLedgeTraversalStrideSpeed() const;

    // Completed traversals only — a blocked abort broadcasts nothing, the
    // same rule the recorder below already follows.
    FBreakerLedgeTraversalCompleted OnLedgeTraversalCompleted;

    // Dev-only clock scale on the traversal durations (screenshot cadence is
    // 2 s and a vault crosses in 0.12 s, so a capture run slows the glide to
    // photograph the mid-traversal viewmodel). 1.0 in every real session;
    // written only by the -BreakerTraversalDemo capture instrument.
    float DevTraversalDurationScale = 1.0f;

    // THE BAND-EDGE TIEBREAK (D4, owner-ruled). Every authored ledge in the
    // project sits EXACTLY on a band edge — the kerb at 45.0 on the step
    // boundary, the risers at 145 on the mantle ceiling, dress_mound_sub at
    // 80.0 on the vault/mantle edge — because the kit's piece sizes and the
    // band edges share the same round numbers. A measured height at an exact
    // edge is a float coin-flip (transform error decides the verb), so the
    // band comparisons are biased by this epsilon: at an edge, the verb that
    // KEEPS THE LEDGE ACTIONABLE wins — the minimum admits (a 50.0 ledge
    // vaults; the engine steps only 45 and under, so refusing it would strand
    // a dead zone), the vault edge vaults (the faster verb), and the ceiling
    // admits (a 145 riser mantles). Strictly smaller than
    // LedgeMinimumHeightCm - MaxStepHeight, pinned in LedgeVerbs, so the
    // bias can never reopen the silent-step overlap. The geometry half of
    // the ruling — nudging authored tops off the edges — is GROUND-shaped
    // work done at the gym sites; this is the maths half.
    static constexpr float LedgeBandEpsilonCm = 0.5f;   // O2 PLACEHOLDER

    // THE PUBLISHED STEP HEIGHT (Part One-R's order): one number, one home.
    // The cover grammar's chest-cover reasoning and the gym's climbable
    // geometry cite 145 in comments and ABreakerGameMode::MantleStepHeight
    // carries its own copy — that copy now reads as a consumer of this one
    // (GROUND's re-point; RiorsEdge.Movement.LedgeVerbs pins the agreement
    // until it lands and after). The mantle ceiling below defaults to it.
    static constexpr float MantleStepHeightCm = 145.0f;   // O2 PLACEHOLDER

    // The completion timestamp (Part One-T): LEDGER's RecentlyMantled window
    // — Afterburn's shape, the one Velocity line a player triggers on
    // purpose — reads recency off this, exactly as Afterburn reads
    // GetLastDashTime. Only a COMPLETED traversal records; an aborted one
    // (blocked mid-move) granted nothing worth a window. KIT records, LEDGER
    // re-targets — two lanes, two commits, per the rule against authoring at
    // absent plumbing.
    UFUNCTION(BlueprintPure, Category="Movement") double GetLastLedgeTraversalTime() const { return LastLedgeTraversalEndTime; }
    void NotifyLedgeTraversalCompleted();

    // Pure rules, exposed for world-free tests (the house pattern).
    // A mantleable wall is near-vertical; a standable top is near-flat.
    static bool IsMantleableWallNormal(float ImpactNormalZ);
    static bool IsStandableTopNormal(float ImpactNormalZ);
    // Which verb a ledge height buys: below the minimum nothing (a step the
    // engine already takes), through the vault window a vault, through the
    // mantle window a mantle, above the ceiling nothing.
    static EBreakerLedgeVerb ResolveLedgeVerb(float LedgeHeightCm, float MinimumCm, float VaultMaximumCm, float MantleMaximumCm);
    // The traversal mode predicate as pure maths, so a worldless test can pin
    // it without a CharacterOwner. MOVE_Flying — the old execution's mode —
    // deliberately answers false: that mode is what made the traversal
    // invisible to every sibling system.
    static bool IsLedgeTraversalMode(uint8 InMovementMode, uint8 InCustomMode)
    {
        return InMovementMode == MOVE_Custom && InCustomMode == CustomModeLedgeTraversal;
    }
    // The glide's clock and curve, pure. Alpha is elapsed over duration,
    // clamped; the position eases with the same smoothstep the pawn's old
    // execution used, so the mode change is bit-identical in shape.
    static float LedgeTraversalAlpha(float ElapsedSeconds, float DurationSeconds);
    static FVector LedgeTraversalLocation(const FVector& Start, const FVector& Target, float Alpha);
    // The smoothstep's speed: distance x 6a(1-a) / duration. Zero at both
    // ends, peaks at 1.5x the average speed mid-glide. This is the number the
    // viewmodel's stride reads during a traversal.
    static float LedgeTraversalStrideSpeed(float DistanceCm, float DurationSeconds, float Alpha);
    UFUNCTION(BlueprintPure, Category="Movement") bool IsSprinting() const { return bWantsToSprint; }
    UFUNCTION(BlueprintPure, Category="Movement") bool IsSliding() const { return bSliding; }
    UFUNCTION(BlueprintPure, Category="Movement") bool IsSlideRequested() const { return bSlideRequested; }
    UFUNCTION(BlueprintPure, Category="Movement") float GetHorizontalSpeed() const { return Velocity.Size2D(); }
    // World time of the last consumed dash charge; class loops watch it to
    // credit a dash exactly once.
    UFUNCTION(BlueprintPure, Category="Movement") float GetLastDashTime() const { return static_cast<float>(LastDashTime); }

    // --- Composed movement stats (the locked one-additive-bucket rule) -----
    // These four used to multiply the gear multiplier by the tree multiplier,
    // which made +20% gear and +20% tree read x1.44 against a rule that says
    // x1.40. They now read the composed ATTRIBUTE, which is where both layers
    // bid into one additive Increased bucket. The attribute set is the single
    // source of truth; the additive fallback below only runs for a movement
    // component with no attribute set at all (a bare test object).
    UFUNCTION(BlueprintPure, Category="Movement|Stats") float GetComposedMoveSpeedMultiplier() const;
    UFUNCTION(BlueprintPure, Category="Movement|Stats") float GetComposedSlideSpeedMultiplier() const;
    UFUNCTION(BlueprintPure, Category="Movement|Stats") float GetComposedAirControlMultiplier() const;
    // The movement half of the hip-fire / ADS trade, and the one consumer the
    // weapons layer named at `UBreakerWeaponComponent::GetAimMoveSpeedMultiplier`.
    // Applied to the GROUNDED cap only: sliding and the boosted ceiling keep no
    // opinion, because whether an aimed slide is slowed is a movement-feel
    // ruling nobody has made. Returns exactly 1.0 with no weapon component, so
    // a bare test object and a hip-firing player are bit-identical to before.
    UFUNCTION(BlueprintPure, Category="Movement|Stats") float GetAimSpeedMultiplier() const;
    // The two grounded caps regardless of the sprint toggle, each the resting
    // figure under the same three multipliers, so the viewmodel's sprint
    // fraction (KIT-4) reads where the body actually is between the gaits.
    UFUNCTION(BlueprintPure, Category="Movement|Stats") float GetWalkSpeedCap() const;
    UFUNCTION(BlueprintPure, Category="Movement|Stats") float GetSprintSpeedCap() const;
    // Cooldown SCALE, so 0.80 is a 20% shorter dash cooldown. The attribute
    // stores the reduction (x1.20) because that is the shape an additive
    // bucket can hold; this is its reciprocal, which is what the dash wants.
    UFUNCTION(BlueprintPure, Category="Movement|Stats") float GetComposedDashCooldownMultiplier() const;

    // --- Jump budget (ruling O25) ------------------------------------------
    // Two jumps for everyone, a third for Swift. Recomputed from the PERMANENT
    // class on OnProgressionChanged and polled as a backstop, so a dev class
    // swap can never strand a non-Swift character with three jumps.
    UFUNCTION(BlueprintPure, Category="Movement|Jump") int32 GetGrantedJumpCount() const { return GrantedJumpCount; }
    UFUNCTION(BlueprintCallable, Category="Movement|Jump") void RefreshJumpGrant();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Grounded Movement", meta=(ClampMin="0")) float WalkSpeed = 700.0f;   // O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Grounded Movement", meta=(ClampMin="0")) float SprintSpeed = 1100.0f;   // O2 PLACEHOLDER

    // --- Weight (owner report: "movement should be less floaty") ---------
    // Everything in this category is new; the OLD behaviour is "no curve at
    // all", i.e. a symmetric arc under a flat GravityScale. Set
    // FallGravityMultiplier and ApexGravityMultiplier to 1.0, MaxFallSpeed to
    // 0, JumpCutMultiplier to 1.0 and LandingMinimumSpeedScale to 1.0 and the
    // component behaves exactly as it did before this pass.

    // Gravity multiplier applied once the character is falling faster than
    // ApexBandSpeed downward. A jump that falls faster than it rises is the
    // single strongest "this body has mass" cue; a symmetric arc is the
    // classic floaty read. OLD: 1.0 (no fall multiplier existed).
    //
    // EASED (owner: "gravity needs to be tuned down just a little bit... needs
    // to make the character slightly more floaty"). This is the dial the
    // GravityScale comment in the constructor named for exactly this report,
    // and it is named rather than GravityScale because the rise has already
    // been walked back to 1.38 — a hair over its original 1.35 — while the
    // DESCENT still ran at 1.80x on top of it. Floatiness and heaviness live in
    // different halves of the arc, so the half that is still heavy is the half
    // that moves. Apex height is untouched by construction (the rise is
    // untouched); airtime goes 0.90 -> 0.93 s and the landing arrives at
    // 871 cm/s instead of 939, still under LandingHeavyFallSpeed so routine
    // jumping stays untaxed. ONE value moved this pass, so the next report
    // attributes cleanly (O26: this executes an owner request, it does not open
    // a movement pass). If it is STILL heavy, the next dial is
    // LandingMinimumSpeedScale to 1.0, which deletes the landing cost outright.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Weight", meta=(ClampMin="0.1")) float FallGravityMultiplier = 1.55f; // WAS 1.80f AT THE WEIGHT PASS (1.0 = no curve at all) — O2 PLACEHOLDER
    // Gravity multiplier exactly at the apex, where vertical velocity is near
    // zero and hang time is felt directly. Blended into 1.0 on the way up and
    // into FallGravityMultiplier on the way down, so the curve is continuous.
    // OLD: 1.0 (no apex treatment existed).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Weight", meta=(ClampMin="0.1")) float ApexGravityMultiplier = 1.50f;   // O2 PLACEHOLDER
    // Half-width of the apex band, in cm/s of vertical speed. OLD: n/a.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Weight", meta=(ClampMin="1")) float ApexBandSpeed = 220.0f;   // O2 PLACEHOLDER
    // Terminal velocity, so the heavier fall curve cannot turn a long drop into
    // a bullet. Binds well before the physics volume's 4000. 0 disables.
    // OLD: 0 (only the volume's 4000 cm/s applied).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Weight", meta=(ClampMin="0")) float MaxFallSpeed = 2400.0f;   // O2 PLACEHOLDER
    // Variable jump height: releasing jump while still rising scales the
    // remaining rise by this. Authority over the arc reads as control rather
    // than drift. 1.0 restores the old fixed-height jump. OLD: 1.0.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Weight", meta=(ClampMin="0", ClampMax="1")) float JumpCutMultiplier = 0.55f;   // O2 PLACEHOLDER
    // Below this rise speed a cut is not worth the discontinuity. OLD: n/a.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Weight", meta=(ClampMin="0")) float JumpCutMinimumRiseSpeed = 50.0f;   // O2 PLACEHOLDER
    // Written onto ACharacter::JumpMaxHoldTime at BeginPlay. The engine clears
    // bPressedJump one frame after the press when this is 0, which leaves the
    // movement layer with no way to see a release; this window keeps the flag
    // alive long enough to detect one. It must comfortably exceed the rise
    // time (~0.45 s) or a held jump would read as a release and self-cut. The
    // engine's own hold-to-rise is suppressed in DoJump. OLD: 0 (engine
    // default; the character never authored it).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Weight", meta=(ClampMin="0")) float JumpHoldWindow = 0.60f;   // O2 PLACEHOLDER
    // Landing impact. Below LandingHeavyFallSpeed a landing costs nothing (a
    // full-height jump lands at about 920 cm/s, so ordinary jumping is never
    // taxed); from there to LandingMaxFallSpeed the horizontal speed kept on
    // arrival ramps down to LandingMinimumSpeedScale. A queued slide owns its
    // own landing and is exempt. OLD: no landing behaviour whatsoever.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Weight", meta=(ClampMin="0")) float LandingHeavyFallSpeed = 950.0f;   // O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Weight", meta=(ClampMin="1")) float LandingMaxFallSpeed = 2400.0f;   // O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Weight", meta=(ClampMin="0", ClampMax="1")) float LandingMinimumSpeedScale = 0.78f;   // O2 PLACEHOLDER

    // --- Jump budget (ruling O25) ------------------------------------------
    // "TWO JUMPS are base kit for everyone and Swift innately unlocks a third
    // later — innate to the class, not a tree purchase." Before this the count
    // was a single constant on ABreakerCharacter and the third jump did not
    // exist in any form. The MECHANISM is what this change delivers; the
    // numbers below are placeholders.

    // Base kit, every class, every level. Written onto
    // ACharacter::JumpMaxCount, so this is the one authority on the budget —
    // a Blueprint override of JumpMaxCount is deliberately overwritten, because
    // O25 is a rule and not a per-Blueprint preference.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Jump", meta=(ClampMin="1")) int32 BaseJumpCount = 2;
    // Master switch. False restores exactly the pre-O25 behaviour (two jumps
    // for everyone including Swift) with no other change.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Jump") bool bSwiftThirdJumpEnabled = true;
    // O2 PLACEHOLDER — THE THRESHOLD IS STILL AWAITING AN OWNER RULING, and the
    // number below is NOT that ruling.
    //
    // WHY IT IS 1 (owner: "i never could do a 3rd jump"). It was 20, and the
    // grant was not broken — it was UNREACHABLE BY CONSTRUCTION. This gate
    // reads `FBreakerProgressionState::CharacterLevel`, and nothing in the
    // project writes that field: it is declared with a default of 1, there is
    // no XP loop, no level-up path, and a repository-wide search for an
    // assignment to it returns the declaration and nothing else. A feature
    // gated behind a stat that no code moves is not a late unlock, it is a
    // disabled feature with a plausible-looking excuse — and the excuse is what
    // made it survive a green suite and a playtest.
    //
    // The honest repair is not to quietly pick a smaller number that happens to
    // clear today's level. It is that the gate must key off something that
    // actually moves, and until an XP loop exists nothing does — so the gate
    // DEFAULTS TO REACHABLE. Raise it again on the same day CharacterLevel
    // starts moving, and not before; RefreshJumpGrant logs once if this is ever
    // set above a level the game can reach, so the failure cannot be silent a
    // second time. `RiorsEdge.Movement.JumpGrantMatrix` asserts the reachable
    // case specifically: Swift gets three and every other class gets two in the
    // progression state the game ACTUALLY runs in, not in a hypothetical one.
    //
    // Still free: no resource cost, no cooldown, no point spend (O25 makes it
    // innate to the class, not a purchase).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Jump", meta=(ClampMin="1", ClampMax="50")) int32 SwiftThirdJumpUnlockLevel = 1; // RULED, not residue: O47 (2026-08-16) — level 1 permanently, base class identity. (History: shipped at 20 when nothing wrote CharacterLevel.)
    // O2 PLACEHOLDER. The third jump must not read as "the second jump again".
    // It blends horizontal velocity toward the current input direction while
    // PRESERVING its magnitude, so it is a course correction, not a speed
    // source — Swift's identity is redirection (Skim is the same verb) and
    // Master 5.4 forbids self-acceleration. 0 makes the third jump identical
    // to the second; 1 would snap it fully onto input, which reads as a dash.
    // Deliberately restrained: O26 says movement gets no further dedicated
    // passes, so this executes O25 and adds nothing else.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Jump", meta=(ClampMin="0", ClampMax="1")) float SwiftThirdJumpRedirectAlpha = 0.55f;

    // Forgiving Source-style air steering: turns existing momentum toward
    // input without increasing its magnitude or allowing a free reversal.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Air Movement", meta=(ClampMin="0")) float AirSteerRate = 4.2f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Air Movement", meta=(ClampMin="-1", ClampMax="1")) float AirSteerMinimumAlignment = -0.2f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dash", meta=(ClampMin="0")) float DashSpeedFloor = 1500.0f;   // O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dash", meta=(ClampMin="0")) float DashSpeedBonus = 200.0f;   // O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dash", meta=(ClampMin="0")) float DashVerticalFloor = 80.0f;   // O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dash", meta=(ClampMin="0")) float DashCooldown = 4.0f;   // O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dash", meta=(ClampMin="0")) float MomentumHardCap = 4200.0f;   // O2 PLACEHOLDER
    // D1(a), owner-ruled (the momentum sentence): when the boosted ceiling's
    // conditions break — input released, collision slow, hard direction
    // break — speed above the current cap BLEEDS over this window instead of
    // the ceiling vanishing in one frame. The felt case is the direction
    // break: turning hard after a dash used to confiscate every cm/s above
    // the grounded cap within two frames, because the engine brakes an
    // over-max velocity to MaxSpeed almost instantly and the ceiling IS
    // MaxSpeed. 0 restores the old instant cut exactly.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dash", meta=(ClampMin="0")) float AboveCapDecaySeconds = 0.5f;   // O2 PLACEHOLDER

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Slide", meta=(ClampMin="0")) float SlideEntrySpeed = 550.0f;   // O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Slide", meta=(ClampMin="0")) float SlideEntryBoost = 120.0f;   // O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Slide", meta=(ClampMin="0")) float SlideEntryBoostDuration = 0.35f;   // O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Slide", meta=(ClampMin="0")) float SlideBoostCooldown = 1.2f;   // O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Slide", meta=(ClampMin="0")) float SlideExitSpeed = 450.0f;   // O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Slide", meta=(ClampMin="0")) float SlideGroundFriction = 1.2f;   // O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Slide", meta=(ClampMin="0")) float SlideBrakingDeceleration = 350.0f;   // O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Slide", meta=(ClampMin="0")) float SlideSlopeAcceleration = 900.0f;   // O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Slide", meta=(ClampMin="0")) float SlideMaxDuration = 1.0f;   // O2 PLACEHOLDER
    // D1(b), owner-ruled: the fraction of the slide's horizontal speed a
    // slide-jump carries into the air. It was silently 1.0 — full
    // conservation — which made slide-jump strictly better than the vault at
    // the vault's own crate (the recon's finding); the toll is what buys the
    // verb choice back. 1.0 restores the old behaviour exactly.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Slide", meta=(ClampMin="0", ClampMax="1")) float SlideJumpSpeedConservation = 0.70f;   // O2 PLACEHOLDER

    // Short wall ride: preserves traversal flow without generating speed or
    // replacing combat.
    // ---- Ledge traversal parameters (Part One-R), all O2 PLACEHOLDER ------
    // The vault window is [minimum, vault max]; the mantle window is
    // (vault max, mantle max]. The mantle ceiling defaults to the published
    // MantleStepHeightCm so the grammar and the verb cannot drift apart.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Movement|Ledge", meta=(ClampMin="0")) float MantleReachCm = 90.0f;               // O2 PLACEHOLDER
    // 50, not the design's 35 (Part One-V): the engine steps 45 silently, so
    // a 35-45 ledge resolved as a vault the player could never trigger — the
    // bottom quarter of the window was dead and no pure test could see it.
    // The invariant LedgeMinimumHeightCm > MaxStepHeight is pinned in
    // RiorsEdge.Movement.LedgeVerbs.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Movement|Ledge", meta=(ClampMin="0")) float LedgeMinimumHeightCm = 50.0f;        // O2 PLACEHOLDER
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Movement|Ledge", meta=(ClampMin="0")) float VaultMaximumHeightCm = 80.0f;        // O2 PLACEHOLDER
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Movement|Ledge", meta=(ClampMin="0")) float MantleMaximumHeightCm = MantleStepHeightCm;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Movement|Ledge", meta=(ClampMin="0.05")) float MantleDurationSeconds = 0.20f;    // O2 PLACEHOLDER
    // A vault is FAST — the whole point of the low window is not breaking
    // stride; the duration gap is what makes the two verbs read differently.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Movement|Ledge", meta=(ClampMin="0.05")) float VaultDurationSeconds = 0.12f;     // O2 PLACEHOLDER

    // Wall ride and wall jump RETIRED (Part One-R, owner ruling): the owner
    // asked for vault and mantle instead, wall-jump's whole parameter family
    // was the ride's, and the grammar the levels validate against never
    // assumed either. The serialized things they touched stay by their own
    // rules: EBreakerBuildCondition::WallRiding retires in place
    // (Progression/), and the owner's WallRideDamage affix lines are his.
    UPROPERTY(BlueprintAssignable, Category="Movement|Weight") FBreakerLandingImpact OnLandingImpact;
    UPROPERTY(BlueprintAssignable, Category="Movement|Dash") FBreakerDashStarted OnDashStarted;

    // Pure rules, exposed for world-free tests.
    // A redirect is legal only when there is real horizontal speed to turn.
    static bool CanRedirect(float HorizontalSpeed, float MinimumSpeed);
    // Rotates HorizontalVelocity onto Direction. The output magnitude is the
    // input magnitude, floored at MinimumSpeed so a redirect never dead-stops,
    // and never above the input magnitude (Master 5.4: no self-acceleration).
    static FVector RedirectHorizontalVelocity(const FVector& HorizontalVelocity, const FVector& Direction, float MinimumSpeed);
    // Multiplicative composition of the active temporary multipliers.
    static float ComposeSpeedMultipliers(const TArray<float>& Multipliers);
    // The expiry rule shared by both keystone suspension chains, factored out
    // as a pure predicate for exactly the reason the ledge predicates and the
    // weight curve below are pure: PushSpeedMultiplier's own expiry has no
    // world-free test anywhere in this suite (NewObject() has no World, so a
    // Duration > 0 push there never actually reaches its ExpiryTime branch),
    // so copying that pattern verbatim would leave suspension expiry equally
    // untestable without a live world. This predicate is the one piece of the
    // prune loop that does NOT need a UWorld, so it can be asserted directly.
    // A negative ExpiryTime never expires; otherwise active strictly before
    // Now, matching PruneSpeedMultipliers' own remove condition exactly.
    static bool IsSuspensionActive(double ExpiryTime, double Now);

    // The locked aggregation rule, expressed over two 1.0-based layer
    // multipliers that are each already a single additive bucket: the shared
    // bucket is the SUM of the two fractional parts, so +20% gear and +20% tree
    // is x1.40. Multiplying them (x1.44) is the bug this replaces — the same
    // bug class the damage pass fixed when it deleted GearWeaponDamageMultiplier.
    // Only used when there is no attribute set to read from; the composed
    // attribute is the real source of truth and produces the same number.
    static float ComposeAdditiveMultiplier(float LayerA, float LayerB);

    // Jump budget as a pure rule (ruling O25). Two for everyone, three for
    // Swift at or past the unlock level. Swapping AWAY from Swift returns two,
    // which is the case that must never be missed: a dev class swap that left
    // three jumps behind would be a permanent illegal grant.
    static int32 ResolveJumpCount(EBreakerClassId PermanentClass, int32 CharacterLevel, int32 BaseCount, bool bThirdJumpEnabled, int32 UnlockLevel);
    // Rotates HorizontalVelocity partway toward Direction, magnitude preserved
    // exactly. Alpha 0 is a no-op, 1 is a full redirect. A zero or vertical
    // direction leaves the velocity alone.
    static FVector BlendHorizontalVelocity(const FVector& HorizontalVelocity, const FVector& Direction, float Alpha);

    // --- The momentum sentence's pure rules (D1) ---------------------------
    // The bleed rate a broken boost decays at: the gap between the ceiling
    // and the resting cap, spread over the window. Zero gap is zero rate; a
    // zero window is the old instant cut, encoded as an unpayable rate.
    static float BoostedCeilingBleedRate(float Ceiling, float RestingCap, float WindowSeconds);
    // One bleed step. Returns the decayed ceiling, or 0 — the existing
    // "no boost" sentinel — the moment it reaches the resting cap, at which
    // point GetMaxSpeed's max() owns the answer again.
    static float StepBoostedCeilingBleed(float Ceiling, float RestingCap, float RatePerSecond, float DeltaTime);
    // The slide-jump's conserved launch velocity: horizontal only, scaled by
    // the conservation fraction, clamped so a misauthored value can never
    // manufacture speed (Master 5.4).
    static FVector SlideJumpConservedVelocity(const FVector& HorizontalVelocity, float ConservationFraction);

    // --- Weight rules, pure maths so they can be tested without a world ---
    // Gravity multiplier as a continuous function of vertical velocity:
    // 1.0 while rising outside the apex band, ApexMultiplier at zero vertical
    // velocity, FallMultiplier once falling outside the band, linear between.
    // Continuity matters: a step change in gravity at the apex is visible as a
    // hitch in the camera.
    static float ComputeGravityMultiplier(float VerticalVelocity, float ApexBand, float ApexMultiplier, float FallMultiplier);
    // Terminal velocity, applied downward only; rising velocity is never
    // touched (a dash or wall jump must keep its impulse).
    static float ClampFallSpeed(float VerticalVelocity, float MaxDownwardSpeed);
    // Variable jump height. Only a rise is cut, so this can never accelerate a
    // fall and can never turn a descent into a boost.
    static float ApplyJumpCut(float VerticalVelocity, float CutMultiplier, float MinimumRiseSpeed);
    // Fraction of horizontal speed kept on a landing of the given downward
    // impact speed. 1.0 up to HeavyFallSpeed, then a linear ramp to
    // MinimumScale at MaxFallSpeed, clamped.
    static float LandingSpeedScale(float ImpactSpeed, float HeavyFallSpeed, float MaxImpactSpeed, float MinimumScale);

private:
    struct FSpeedMultiplierEntry
    {
        float Multiplier = 1.0f;
        // Negative = no expiry; popped explicitly.
        double ExpiryTime = -1.0;
    };
    // Mutable: GetMaxSpeed() is const and is the natural place to drop expired
    // entries, which is what "lazy expiry" means here.
    mutable TMap<FName, FSpeedMultiplierEntry> SpeedMultipliers;
    void PruneSpeedMultipliers() const;

    // Keystone suspension entries carry no payload beyond the expiry itself —
    // they are pure booleans (see the O2 comment at the public API above).
    struct FSuspensionEntry
    {
        // Negative = no expiry; popped explicitly. Same convention as
        // FSpeedMultiplierEntry::ExpiryTime.
        double ExpiryTime = -1.0;
    };
    mutable TMap<FName, FSuspensionEntry> DashCooldownSuspensions;
    void PruneDashCooldownSuspensions() const;

    // The composed attribute set is the source of truth for the four movement
    // stats above. Equipment and progression are still resolved, but only as
    // the fallback for a component with no attribute set (a bare test object).
    class UBreakerAttributeSet* GetAttributes() const;
    class UBreakerEquipmentComponent* GetEquipment() const;
    class UBreakerProgressionComponent* GetProgression() const;
    mutable TWeakObjectPtr<class UBreakerAttributeSet> CachedAttributes;
    mutable TWeakObjectPtr<class UBreakerEquipmentComponent> CachedEquipment;
    mutable TWeakObjectPtr<class UBreakerProgressionComponent> CachedProgression;
    // WalkSpeed is authored HERE, not on the attribute set, so the composed
    // MoveSpeed attribute would otherwise be a number close to the walk speed
    // but never equal to it. Published once, as soon as the ability system has
    // actually registered the set (which is after this component's BeginPlay).
    bool bPublishedMoveSpeedBase = false;
    void PublishMoveSpeedBase();

    // O25 jump budget. ObservedClass/ObservedLevel are the poll backstop, in
    // the precedent of UBreakerManaComponent::AdvanceLoop: DevForceClass does
    // broadcast today, but a grant that can outlive its class is exactly the
    // kind of thing that must not depend on one caller remembering to.
    UFUNCTION() void HandleProgressionChanged();
    int32 GrantedJumpCount = 2;
    EBreakerClassId ObservedClass = EBreakerClassId::None;
    int32 ObservedLevel = 0;
    bool bBoundProgression = false;
    // One-shot, so the "unreachable gate" failure can never be silent again.
    // The third jump shipped gated on level 20 with nothing in the project
    // writing CharacterLevel, and the only symptom was an owner saying "i never
    // could do a 3rd jump" — no warning, no failing test, no log line.
    bool bWarnedUnreachableThirdJump = false;
    // Applies the third jump's course correction, if this jump is one.
    void ApplyBonusJumpRedirect();

    // ---- Ledge traversal execution state ----------------------------------
    // The saved-move classes below capture and restore this whole block, so a
    // replayed move mid-glide continues from the right point instead of
    // restarting the traversal.
    friend class FBreakerSavedMove_Character;
    // The jump key's request, one-shot, carried to the server in the
    // compressed-flags byte (FLAG_Custom_0) exactly as bWantsToCrouch rides
    // its own flag. Consumed by UpdateCharacterStateBeforeMovement.
    bool bWantsLedgeTraversal = false;
    // The client-side resolve from TryBeginLedgeTraversal, used same-frame so
    // the initiating machine begins from exactly what it resolved. The server
    // and a replaying client have no pending copy and re-resolve from their
    // own authoritative pose — deterministic given the same pose, and the
    // ordinary movement correction owns any residual divergence.
    FBreakerLedgeTraversal PendingTraversal;
    bool bHasPendingTraversal = false;
    FVector TraversalStart = FVector::ZeroVector;
    FVector TraversalTarget = FVector::ZeroVector;
    // Horizontal velocity carried in, restored on a COMPLETED exit; a blocked
    // abort exits dead (the old execution's rule, kept).
    FVector TraversalExitVelocity = FVector::ZeroVector;
    float TraversalElapsed = 0.0f;
    float TraversalDuration = 0.2f;
    EBreakerLedgeVerb TraversalVerb = EBreakerLedgeVerb::None;
    void BeginLedgeTraversal(const FBreakerLedgeTraversal& Traversal);
    void FinishLedgeTraversal(bool bCompleted);
    void PhysLedgeTraversal(float DeltaTime, int32 Iterations);

    bool bWantsToSprint = false;
    bool bSlideRequested = false;
    bool bSlideRequestConsumed = false;
    bool bSliding = false;
    double LastDashTime = -1000.0;
    // Sentinel matches LastDashTime's: "never" is far in the past, so recency
    // reads need no separate has-ever flag.
    double LastLedgeTraversalEndTime = -1000.0;
    // The grounded cap as GetMaxSpeed computes it, factored so the bleed and
    // the cap read one expression and cannot drift.
    float GetGroundedSpeedCap() const;
    // Captured once when the boost's conditions first break, so the bleed is
    // LINEAR over the authored window rather than an asymptote. Reset to 0
    // whenever the boost re-arms.
    float BoostedCeilingBleedRatePerSecond = 0.0f;

    // ---- Dev speed-trace instrument (-BreakerMoveTrace) -------------------
    // The capture harness cannot press movement keys, so the component
    // drives itself: a scripted sprint / dash / hard-turn / slide-jump run
    // with a rate-limited speed log, read out of the session log. Inert
    // without the switch; player-controlled pawns only.
    bool bMoveTraceArmed = false;
    double MoveTraceStartTime = -1.0;
    double LastMoveTraceLogTime = -1000.0;
    int32 MoveTraceStep = 0;
    FVector MoveTraceForward = FVector::ZeroVector;
    FVector MoveTraceGlanceDir = FVector::ZeroVector;
    void TickMoveTrace();
    double LastSlideBoostTime = -1000.0;
    float BoostedSpeedCeiling = 0.0f;
    float SavedGroundFriction = 0.0f;
    float SavedBrakingDeceleration = 0.0f;
    float SlideElapsed = 0.0f;
    float SlideEntryBoostRemaining = 0.0f;
    // True between a real jump impulse and the moment the cut is spent (or the
    // jump ends). Only DoJump arms it, which is what keeps the slide jump
    // and the dash's vertical floor — neither of which runs through the jump
    // state machine — from being cut by a stray key release.
    bool bJumpCutArmed = false;
    void ApplyAirSteering(float DeltaTime);
};

// ---------------------------------------------------------------------------
// The saved-move pass (the custom prediction mode's other half). The request
// bit rides FLAG_Custom_0 so the server consumes the SAME one-shot the client
// consumed, inside its own simulation of the same move; the glide state is
// captured and restored so a client replaying corrected moves mid-traversal
// continues the glide instead of restarting it. Graded honestly: this is the
// standard sprint-flag pattern applied to a one-shot, exercised today only by
// the listen-server host path (no remote client exists to playtest against),
// and pinned by the worldless suite at the pure-rule level.
// ---------------------------------------------------------------------------
class FBreakerSavedMove_Character : public FSavedMove_Character
{
public:
    using Super = FSavedMove_Character;

    uint8 bSavedWantsLedgeTraversal : 1;
    FVector SavedTraversalStart = FVector::ZeroVector;
    FVector SavedTraversalTarget = FVector::ZeroVector;
    FVector SavedTraversalExitVelocity = FVector::ZeroVector;
    float SavedTraversalElapsed = 0.0f;
    float SavedTraversalDuration = 0.2f;
    EBreakerLedgeVerb SavedTraversalVerb = EBreakerLedgeVerb::None;

    FBreakerSavedMove_Character() : bSavedWantsLedgeTraversal(false) {}
    virtual void Clear() override;
    virtual uint8 GetCompressedFlags() const override;
    virtual bool CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* InCharacter, float MaxDelta) const override;
    virtual void SetMoveFor(ACharacter* C, float InDeltaTime, FVector const& NewAccel, FNetworkPredictionData_Client_Character& ClientData) override;
    virtual void PrepMoveFor(ACharacter* C) override;
};

class FBreakerNetworkPredictionData_Client_Character : public FNetworkPredictionData_Client_Character
{
public:
    explicit FBreakerNetworkPredictionData_Client_Character(const UCharacterMovementComponent& ClientMovement)
        : FNetworkPredictionData_Client_Character(ClientMovement) {}
    virtual FSavedMovePtr AllocateNewMove() override
    {
        return FSavedMovePtr(new FBreakerSavedMove_Character());
    }
};
