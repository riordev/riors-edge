#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "BreakerDeployable.generated.h"

class ABreakerZoneActor;
class UBreakerAttributeSet;
class UBreakerCombatComponent;
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
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Deployable")
    void DestroyDeployable();

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

    // --- Anchor Point (Class-Kits-Tank §2 T3) ------------------------------
    // 2.5 m wide x 2 m tall; health 20% of the Tank's maximum health; 12s.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Deployable|AnchorPoint", meta=(ClampMin="0")) float AnchorWidthCm = 250.0f;     // §T3
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Deployable|AnchorPoint", meta=(ClampMin="0")) float AnchorHeightCm = 200.0f;    // §T3
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Deployable|AnchorPoint", meta=(ClampMin="0", ClampMax="1")) float AnchorHealthFraction = 0.2f;   // §T3: placeholder 20% of max health

    UFUNCTION() void HandleCombatDeath();
    UFUNCTION() void HandleZoneOccupantEntered(AActor* Occupant);
    UFUNCTION() void HandleZoneOccupantExited(AActor* Occupant);

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
        bool bLive = true;
    };
    TArray<FMineCharge> Mines;

    // Enemies currently slowed by this Disruptor, so EndPlay can restore them.
    TArray<TWeakObjectPtr<AActor>> SlowedEnemies;

    static int32 NextPlacementSerial;
};
