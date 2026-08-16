#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "Combat/BreakerCombatTypes.h"
#include "BreakerDeployable.generated.h"

class ABreakerEnemy;
class ABreakerZoneActor;
class UBreakerAttributeSet;
class UBreakerCombatComponent;
class UBreakerProgressionComponent;
class UBreakerScrapComponent;
class UPointLightComponent;
class UStaticMeshComponent;

// Which kind of placed object this is. NOT serialized into any save — the
// actors are session-lived world objects, never persisted — so this enum is
// append-only by convention rather than by save-compatibility law.
UENUM(BlueprintType)
enum class EBreakerDeployableType : uint8
{
    Turret,
    AmmoCrate,
    MineCluster,
    Disruptor,
    // Tank's cover panel (Class-Kits-Tank §2 T3). A deployable in every
    // mechanical sense (health pool, lifetime, one destruction path) but NOT a
    // Scrap object: it refunds nothing, and it does not count against the
    // Gunsmith density cap because the cap is a Scrap-economy rule
    // (Class-Kits-Gunsmith §2.1) and the Tank has no Scrap economy.
    AnchorPoint
};

// Why a deployable died. The FIELD TECH tier-2 rows split the one destruction
// path by cause without splitting the refund rule: Requisition (FT5) and
// Deadman (FT11) fire on EnemyDamage ONLY — "not by expiry, not by the density
// cap" is the doc's own emphasis — and Command Detonation (TK11) is the one
// cause that refunds NOTHING ("refunds nothing", also the doc's words).
UENUM(BlueprintType)
enum class EBreakerDeployableDestructionCause : uint8
{
    Expired,
    EnemyDamage,
    DensityCull,
    Exhausted,
    // TK11: manually detonated. No refund.
    Command
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FBreakerDeployableEvent, class ABreakerDeployable*, Deployable);

// ---------------------------------------------------------------------------
// THE MINIMAL DEPLOYABLE SYSTEM — O30's hole, opened (owner authorization
// 2026-08-16: "feel free to do all 5 classes").
//
// Class-Kits-Gunsmith §2 is the design contract this implements, deliberately
// at its smallest honest size: a spawned actor with its own health pool, a
// lifetime, a per-type behaviour tick, one destruction path (expiry, damage,
// density-cap cull — all identical, all refunding through the owner's Scrap
// component), a density cap of 4 total / 2 per type with destroy-oldest
// semantics, and placement validation that FAILS LOUDLY rather than silently
// relocating. Placeholder-visual via the same engine basic shapes the enemies
// use.
//
// WHAT IS DELIBERATELY ABSENT, recorded rather than implied (§2.3-§2.4):
//  * No enemy TARGETING of deployables: enemy AI acquires the player only, so
//    "enemies target deployables opportunistically" has no seam to land in.
//    Deployables still DIE to enemy fire that hits them (they carry a real
//    UBreakerCombatComponent, so a stray projectile resolves damage normally).
//  * No cast-time on placement (0.4s deploy animation): there is no animation
//    layer to hang it on. Placement is instant; the weakness §2.3 wants is
//    recorded as missing, not faked with an input lock.
//  * No data-driven trigger-condition rewrites on mines (the Tinkerer node
//    layer that consumes them is not authored this pass).
//  * The interact prompt for the Ammo Crate: the crate auto-dispenses to the
//    owner standing beside it instead of using the F-key interact chain, which
//    is owned by NPC/loot/travel precedence rules this pass must not touch.
// Every magnitude below is O2 PLACEHOLDER; numbers with a doc citation are
// transcribed, the rest are seeds.
// ---------------------------------------------------------------------------
UCLASS()
class RIORSEDGE_API ABreakerDeployable : public AActor
{
    GENERATED_BODY()

public:
    ABreakerDeployable();
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;

    // Server-side arming. ScrapCost is remembered for the 50% destruction
    // refund (0 for the Anchor Point, which is not a Scrap object).
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Deployable")
    void InitializeDeployable(EBreakerDeployableType InType, AActor* InOwnerCharacter, float InScrapCost);

    UFUNCTION(BlueprintPure, Category="Deployable") EBreakerDeployableType GetDeployableType() const { return Type; }
    UFUNCTION(BlueprintPure, Category="Deployable") AActor* GetOwningCharacter() const { return OwningCharacter.Get(); }
    UFUNCTION(BlueprintPure, Category="Deployable") float GetScrapCost() const { return ScrapCost; }
    UFUNCTION(BlueprintPure, Category="Deployable") float GetRemainingLifetime() const { return LifetimeRemaining; }
    UFUNCTION(BlueprintPure, Category="Deployable") bool IsLifetimePaused() const { return bLifetimePaused; }
    UFUNCTION(BlueprintPure, Category="Deployable") int32 GetRemainingCharges() const { return ChargesRemaining; }

    // Foundry (Class-Kits-Gunsmith §3, FIELD ASSEMBLY): "never expires" is a
    // PAUSE on the lifetime clock, so the deployable still dies to damage and
    // still refunds through the one destruction path.
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Deployable")
    void SetLifetimePaused(bool bPaused) { bLifetimePaused = bPaused; }

    // Minefield (§3): invisible until it first acts. Enemy perception exclusion
    // is vacuous today — enemies do not perceive deployables at all — so the
    // visual half is the whole implementable rule.
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Deployable")
    void SetHiddenUntilAction(bool bHiddenUntilActed);

    // The ONE destruction path (§2.2): expiry, damage death, and density-cap
    // cull all come through here, and the refund is identical for all three.
    // The parameterless form stays for callers with nothing to say about cause.
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Deployable")
    void DestroyDeployable();
    // The cause-aware form the Field Tech tier-2/4 nodes read: still ONE path,
    // one refund arithmetic — the cause gates the Requisition/Deadman riders
    // and the Command no-refund rule, never the fraction.
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Deployable")
    void DestroyDeployableWithCause(EBreakerDeployableDestructionCause Cause);

    // --- Placement and the density cap (static, §2.1/§2.3) ----------------

    // 8 m along the aim ray, snapped to the nearest floor. Returns false —
    // loudly, with a log — when there is no valid floor; a failed placement
    // must cost nothing, so callers validate BEFORE committing any cost.
    static bool ResolvePlacement(UWorld* World, AActor* OwnerCharacter, const FVector& ViewLocation,
        const FVector& ViewDirection, float RangeCm, FVector& OutLocation);

    // Live deployables owned by this character (Anchor Points excluded from the
    // Scrap density count — see the enum comment).
    static int32 CountOwnedDeployables(const AActor* OwnerCharacter, int32& OutTotal, EBreakerDeployableType Type, int32& OutOfType);

    // Enforces "placing a fifth destroys the oldest and refunds it" (§2.1).
    // Called by the deployable abilities just before spawning. Oldest is
    // placement order, never remaining lifetime.
    static void EnforceDensityCapForPlacement(AActor* OwnerCharacter, EBreakerDeployableType TypeAboutToPlace);

    // FIELD ASSEMBLY's density-cap raise (base 4 -> 8 for 20s; per-type stays
    // 2, §3). Keyed per owner, expiring on world time; over-cap deployables at
    // expiry are NOT culled — the cap is checked on placement, not continuously.
    static void PushDensityCapOverride(AActor* OwnerCharacter, int32 NewTotalCap, double ExpiryWorldTime);
    static int32 TotalCapFor(const AActor* OwnerCharacter);

    static const TArray<TWeakObjectPtr<ABreakerDeployable>>& GetLiveDeployables();

    // ---- Gunsmith node rules (2026-08-16, the branch-tree pay pass) --------
    // Pure statics carry each rule's arithmetic so the suite pins them with no
    // world; the actor paths below read owner node tags/ranks live (the
    // Cleave's Edge posture), so a respec moves every rule on its next event.

    // FT8 Logistics: the Ammo Crate stops counting against the density cap.
    static bool CountsAgainstDensityCap(EBreakerDeployableType Type, bool bOwnerHasLogistics);
    // FT9 Redundancy: total cap 4 -> 5 (per-type stays 2, invariantly).
    static int32 BaseTotalCapFor(bool bHasRedundancy);
    // FT5 Requisition: the replacement discount, 10 at rank 1, 18 at rank 2.
    static float RequisitionDiscountFor(int32 Rank);
    // FT3 Second Shift: remaining lifetime after a qualifying reload — +8s
    // (R2: +14s), never past double the BASE lifetime. The 2x-base ceiling is
    // the anti-farm rule and applies to the remaining clock, so reload-cycling
    // in a corner cannot bank a permanent field.
    static float SecondShiftLifetime(int32 Rank, float BaseLifetime, float CurrentRemaining);
    // TK2 Quick Set: arm delay halved (R2: removed)...
    static float QuickSetArmDelay(int32 Rank, float BaseDelay);
    // ...and at R2 a no-delay charge triggers on a radius 1 m smaller until a
    // second has passed since it armed.
    static float QuickSetTriggerRadius(int32 Rank, float SecondsSinceArmed, float BaseRadiusCm);
    // TK4 Rearm: one charge every 6s (R2: 4s); 0 = the node is not owned.
    static float RearmInterval(int32 Rank);
    // TK7 Ordnance: 4 charges instead of 3.
    static int32 OrdnanceMineCount(bool bHasOrdnance, int32 BaseCount);
    // TK7's anti-explosion clause: charges detonating within 1s of the same
    // cluster's previous detonation are ONE damage instance for procs.
    static float OrdnanceProcCoefficient(bool bHasOrdnance, double Now, double LastDetonationTime);
    // TK9 Patience: armed and untriggered for 10s = triggers harder.
    static bool PatienceQualifies(float ArmedUntriggeredSeconds);

    // FT5 Requisition's replacement window: a pending same-type discount keyed
    // per owner, registered by the enemy-destruction path, consumed by the
    // deploy ability that spends it. Expires 8s after the destruction.
    static void RegisterReplacementCredit(AActor* OwnerCharacter, EBreakerDeployableType Type, float Discount, double ExpiryWorldTime);
    static float PendingReplacementDiscount(const AActor* OwnerCharacter, EBreakerDeployableType Type, double Now);
    static void ConsumeReplacementCredit(AActor* OwnerCharacter, EBreakerDeployableType Type);

    // TK11 Command Detonation: detonates every ARMED live charge across the
    // owner's Mine Clusters at once and destroys the emptied clusters through
    // the Command cause (no refund). Returns how many charges detonated.
    static int32 CommandDetonateOwnedMines(AActor* OwnerCharacter);

    UPROPERTY(BlueprintAssignable, Category="Deployable") FBreakerDeployableEvent OnDeployableDestroyed;

    // --- Density (Class-Kits-Gunsmith §2.1, transcribed) -------------------
    static constexpr int32 BaseTotalDensityCap = 4;   // O2 PLACEHOLDER (§2.1: "4 active total")
    static constexpr int32 PerTypeDensityCap = 2;     // O2 PLACEHOLDER (§2.1: "2 of any one type"; nothing may raise it)

    // --- Shared tuning -----------------------------------------------------
    // Lifetimes per §2.2's table; assigned per type in InitializeDeployable.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Deployable", meta=(ClampMin="0")) float LifetimeRemaining = 30.0f;   // O2 PLACEHOLDER

    // --- Turret (Class-Kits-Gunsmith §3 G3) --------------------------------
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Deployable|Turret", meta=(ClampMin="0")) float TurretRangeCm = 1800.0f;         // O2 PLACEHOLDER (§G3: seed 18 m)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Deployable|Turret", meta=(ClampMin="0.05")) float TurretFireInterval = 1.0f;    // O2 PLACEHOLDER (§G3 names a fixed cadence, no seed)
    // Fraction of the owner's SCALED weapon base damage per turret round, so
    // §1.3's "all damage is the player's damage" is literal and the turret
    // rides gear depth exactly as the player's own rounds do (O35).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Deployable|Turret", meta=(ClampMin="0")) float TurretDamageCoefficient = 0.6f;  // O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Deployable|Turret", meta=(ClampMin="0")) float TurretProcCoefficient = 0.5f;    // §G3: proc coefficient 0.5
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Deployable|Turret", meta=(ClampMin="1")) float TurretHealth = 150.0f;           // O2 PLACEHOLDER

    // --- Ammo Crate (§3 G4) ------------------------------------------------
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Deployable|Crate", meta=(ClampMin="1")) int32 CrateCharges = 4;                 // §G4: seed 4 interact charges
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Deployable|Crate", meta=(ClampMin="0", ClampMax="1")) float CrateReserveFraction = 0.4f;   // §G4: seed 40% of base reserve
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Deployable|Crate", meta=(ClampMin="0")) float CrateUseRadiusCm = 250.0f;        // O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Deployable|Crate", meta=(ClampMin="0.1")) float CrateUseInterval = 1.0f;        // O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Deployable|Crate", meta=(ClampMin="1")) float CrateHealth = 100.0f;             // O2 PLACEHOLDER

    // --- Mine Cluster (§3 G5) ----------------------------------------------
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Deployable|Mines", meta=(ClampMin="1")) int32 MineCount = 3;                    // §G5: 3 charges, one placement
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Deployable|Mines", meta=(ClampMin="0")) float MineSpreadCm = 200.0f;            // §G5: seed 2 m spread
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Deployable|Mines", meta=(ClampMin="0")) float MineArmDelay = 1.0f;              // §G5: seed 1.0s
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Deployable|Mines", meta=(ClampMin="0")) float MineTriggerRadiusCm = 250.0f;     // §G5: seed 2.5 m
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Deployable|Mines", meta=(ClampMin="0")) float MineBlastRadiusCm = 300.0f;       // O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Deployable|Mines", meta=(ClampMin="0")) float MineDamageCoefficient = 1.2f;     // O2 PLACEHOLDER (fraction of owner's scaled weapon base)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Deployable|Mines", meta=(ClampMin="0", ClampMax="1")) float MineEdgeDamageFraction = 0.35f;   // O2 PLACEHOLDER (the rocket's falloff shape reused)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Deployable|Mines", meta=(ClampMin="1")) float MineClusterHealth = 60.0f;        // O2 PLACEHOLDER

    // --- Disruptor (§3 G6) -------------------------------------------------
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Deployable|Disruptor", meta=(ClampMin="0")) float DisruptorRadiusCm = 600.0f;   // §G6: seed 6 m
    // FLAT, never percentage (§G6, the Rot/boss-cap protection). "Strips more"
    // than Rot's 40, per the combat component's own comment.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Deployable|Disruptor", meta=(ClampMin="0")) float DisruptorArmorReduction = 60.0f;   // O2 PLACEHOLDER
    // Applied through ABreakerEnemy::ApplyModifierMovementProfile against the
    // AUTHORED base speed on zone entry and restored (1.0) on exit. KNOWN
    // LIMITATION, recorded: an enemy whose speed the modifier layer also
    // rewrote (Fleetfoot) is restored to 1.0x on exit, not to its modifier
    // speed — there is no keyed slow primitive on enemies to compose with.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Deployable|Disruptor", meta=(ClampMin="0", ClampMax="1")) float DisruptorSlowMultiplier = 0.55f;   // O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Deployable|Disruptor", meta=(ClampMin="1")) float DisruptorHealth = 100.0f;     // O2 PLACEHOLDER

    // --- Node-rule tuning (all O2 PLACEHOLDER unless doc-cited) ------------
    // FT3: "within their radius" — the doc authors no number; near = this.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Deployable|Nodes", meta=(ClampMin="0")) float SecondShiftRadiusCm = 900.0f;   // O2 PLACEHOLDER
    // FT6: the health a crate charge restores at rank 1 (rank 2 doubles it).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Deployable|Nodes", meta=(ClampMin="0")) float ForemanHealPerCharge = 15.0f;   // O2 PLACEHOLDER
    // FT7: LOS grace, doc-seeded ("seed 1.2s of grace").
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Deployable|Nodes", meta=(ClampMin="0")) float TurretLOSGraceSeconds = 1.2f;   // §FT7 seed
    // FT11: the Deadman detonation, as a fraction of the owner's scaled weapon
    // base (the §1.3 rule every deployable damage number obeys).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Deployable|Nodes", meta=(ClampMin="0")) float DeadmanDamageCoefficient = 1.0f;   // O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Deployable|Nodes", meta=(ClampMin="0")) float DeadmanBlastRadiusCm = 300.0f;   // O2 PLACEHOLDER
    // TK3: the line-of-sight trigger's reach ("within its range" — no seed).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Deployable|Nodes", meta=(ClampMin="0")) float TripwireTriggerRangeCm = 900.0f;   // O2 PLACEHOLDER
    // TK8 Interdiction: how much the Disruptor stretches enemy wind-ups begun
    // inside it, through the enemy's keyed telegraph seam. ClampMin 1 is the
    // node's own law — "Delays — never cancels" — and a value below 1 would
    // SHORTEN a telegraph, which Encounter-Design §0 forbids outright.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Deployable|Nodes", meta=(ClampMin="1")) float InterdictionWindupMultiplier = 1.6f;   // O2 PLACEHOLDER

    // --- Anchor Point (Class-Kits-Tank §2 T3) ------------------------------
    // 2.5 m wide x 2 m tall; health 20% of the Tank's maximum health; 12s.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Deployable|AnchorPoint", meta=(ClampMin="0")) float AnchorWidthCm = 250.0f;     // §T3
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Deployable|AnchorPoint", meta=(ClampMin="0")) float AnchorHeightCm = 200.0f;    // §T3
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Deployable|AnchorPoint", meta=(ClampMin="0", ClampMax="1")) float AnchorHealthFraction = 0.2f;   // §T3: placeholder 20% of max health

    UFUNCTION() void HandleCombatDeath();
    UFUNCTION() void HandleZoneOccupantEntered(AActor* Occupant);
    UFUNCTION() void HandleZoneOccupantExited(AActor* Occupant);
    // FT3 Second Shift: the owner's reload completing near this deployable.
    UFUNCTION() void HandleOwnerReloadCompleted(bool bAnyRoundFired);
    // FT2 Overwatch: tracks the enemy the owner last damaged (turrets only).
    UFUNCTION() void HandleOwnerHitDealt(const FBreakerHitContext& Hit);
    // TK5 Attrition Field: the owner's kill, position-checked against the
    // Disruptor's field (Disruptors only).
    UFUNCTION() void HandleOwnerKillDealt(const FBreakerHitContext& Hit);

protected:
    virtual void BeginPlay() override;

    void TickTurret(float DeltaSeconds);
    void TickAmmoCrate(float DeltaSeconds);
    void TickMineCluster(float DeltaSeconds);
    void DetonateMine(int32 MineIndex);
    void BuildPlaceholderVisual();
    void MarkActed();
    // Owner's scaled weapon base damage (O35), 0 without a weapon component.
    float OwnerWeaponBaseDamage() const;
    // Node reads off the owner (null-safe: no progression = no node, every
    // authored behaviour bit-identical).
    const UBreakerProgressionComponent* OwnerProgression() const;
    UBreakerScrapComponent* OwnerScrap() const;
    int32 OwnerNodeRank(FName NodeId) const;
    bool OwnerHasNodeTag(const FGameplayTag& Tag) const;
    // FT11's Deadman blast (and nothing else's): radial, enemies only, through
    // the one damage pipeline, crediting the owner. Never chains — it damages
    // enemies, not deployables, so a second generation cannot exist.
    void DetonateRadialBlast(const FVector& Center, float DamageCoefficient, float RadiusCm, float ProcCoefficient);

    UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Root;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> BodyVisual;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> TopVisual;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UPointLightComponent> Glow;
    // Real damage intake: enemy projectiles resolve against any actor carrying
    // a combat component, so a deployable dies to stray fire through the same
    // pipeline as everything else rather than through a bespoke health float.
    UPROPERTY(VisibleAnywhere) TObjectPtr<UBreakerCombatComponent> Combat;
    UPROPERTY() TObjectPtr<UBreakerAttributeSet> Attributes;
    UPROPERTY() TObjectPtr<ABreakerZoneActor> DisruptorZone;

private:
    EBreakerDeployableType Type = EBreakerDeployableType::Turret;
    TWeakObjectPtr<AActor> OwningCharacter;
    float ScrapCost = 0.0f;
    bool bLifetimePaused = false;
    bool bHiddenUntilAction = false;
    bool bHasActed = false;
    bool bDestroyed = false;
    int32 ChargesRemaining = 0;
    int32 PlacementSerial = 0;
    double LastTurretShotTime = -1000.0;
    double LastCrateUseTime = -1000.0;

    struct FMineCharge
    {
        FVector Location = FVector::ZeroVector;
        float ArmRemaining = 0.0f;
        // TK2/TK9: how long this charge has been armed and untriggered.
        float SecondsSinceArmed = 0.0f;
        bool bLive = true;
    };
    TArray<FMineCharge> Mines;

    // Enemies currently slowed by this Disruptor, so EndPlay can restore them.
    TArray<TWeakObjectPtr<AActor>> SlowedEnemies;

    // ---- Node-rule state ---------------------------------------------------
    // Authored (pre-extension) lifetime: FT3's 2x ceiling and TK9's Disruptor
    // age both measure against the base, never the extended clock.
    float BaseLifetime = 0.0f;
    // Seconds since placement (lifetime pauses do not stop age).
    float AgeSeconds = 0.0f;
    // FT3: one extension per reload per deployable is natural (one event per
    // reload); nothing else needed.
    // FT2: the enemy the owner most recently damaged. KNOWN LIMITATION,
    // recorded: deployable damage is attributed to the owner (SI-8), so a
    // turret's own hits also move this — "the target YOU last damaged" reads
    // slightly sticky. The alternative (a per-source split on OnHitDealt) is
    // combat-owner territory.
    TWeakObjectPtr<ABreakerEnemy> LastOwnerDamagedEnemy;
    // FT7/FT2-R2/FT10: current turret target and its LOS-grace clock.
    TWeakObjectPtr<ABreakerEnemy> CurrentTurretTarget;
    double TurretLOSLostTime = -1000.0;
    // FT10 (and FT2 R2): the next turret shot skips the cadence gate.
    bool bTurretFreeShotPending = false;
    // FT6: half-rate consumption toggle while the interactor's reserve is full.
    bool bForemanSkipCharge = false;
    // TK4: time until the next rearm while the cluster sits empty.
    float RearmAccumulator = 0.0f;
    // TK7: the same-cluster 1s proc-merge window.
    double LastMineDetonationTime = -1000.0;
    // TK9 (Disruptor half): the double strip pays on the FIRST entry only.
    bool bAnyEnemyEnteredField = false;
    // Enemies carrying this deployable's Patience armour strip, keyed for the
    // pop on exit/death.
    TArray<TWeakObjectPtr<AActor>> PatienceStruckEnemies;
    // Bound-owner bookkeeping so EndPlay unbinds exactly what Initialize bound.
    TWeakObjectPtr<UBreakerCombatComponent> BoundOwnerCombat;
    TWeakObjectPtr<class UBreakerWeaponComponent> BoundOwnerWeapon;

    static int32 NextPlacementSerial;
};
