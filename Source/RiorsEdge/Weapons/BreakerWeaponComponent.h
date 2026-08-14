#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Combat/BreakerCombatTypes.h"
#include "Weapons/BreakerWeaponArchetype.h"
#include "Weapons/BreakerWeaponFeel.h"
#include "BreakerWeaponComponent.generated.h"

class UBreakerAttributeSet;
class UBreakerWeaponDefinition;


// ---------------------------------------------------------------------------
// One pellet's worth of a shot.
//
// WHY THIS EXISTS. `FBreakerShotResult` carried ONE impact for a whole spread,
// which meant the shot contract could not answer "where did the shotgun
// actually land". Two consequences were recorded in the code: the tracer
// renderer had nothing per-pellet to draw, so the shotgun deliberately drew no
// streak at all, and anything wanting a per-pellet reading (impact sparks,
// future per-pellet numbers, per-pellet procs) had to guess.
//
// This is ADDITIVE. Every single-impact field on FBreakerShotResult keeps the
// exact meaning it had before — see the back-compatibility note there — so the
// HUD, the telemetry and the damage-numbers path are untouched by it.
// ---------------------------------------------------------------------------
USTRUCT(BlueprintType)
struct RIORSEDGE_API FBreakerPelletImpact
{
    GENERATED_BODY()

    // False for a pellet that hit nothing. Missing pellets are still RECORDED,
    // because where a pellet went when it missed is exactly the information a
    // spread of tracers is made of.
    UPROPERTY(BlueprintReadOnly) bool bHit = false;
    UPROPERTY(BlueprintReadOnly) bool bWeakPoint = false;
    // Where this pellet's ray ended: the impact when it hit, the end of its
    // maximum range when it did not. Always finite, always usable as a draw
    // target.
    UPROPERTY(BlueprintReadOnly) FVector End = FVector::ZeroVector;
    UPROPERTY(BlueprintReadOnly) TObjectPtr<AActor> HitActor = nullptr;
};

USTRUCT(BlueprintType)
struct RIORSEDGE_API FBreakerShotResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) bool bFired = false;
    // ---- Single-impact accessors, UNCHANGED --------------------------------
    // These are the pre-per-pellet contract and they keep their exact previous
    // semantics: bHit/bWeakPoint are the OR across the spread, ImpactPoint /
    // HitActor / TraceEnd are the last pellet that landed, and DamageResult is
    // the spread's summed damage. Every existing consumer (the HUD's damage
    // numbers and impact spark, the Mana component's per-shot generation, the
    // playtest telemetry) reads only these, and none of them changed.
    UPROPERTY(BlueprintReadOnly) bool bHit = false;
    UPROPERTY(BlueprintReadOnly) bool bWeakPoint = false;
    UPROPERTY(BlueprintReadOnly) FVector TraceStart = FVector::ZeroVector;
    UPROPERTY(BlueprintReadOnly) FVector TraceEnd = FVector::ZeroVector;
    UPROPERTY(BlueprintReadOnly) FVector ImpactPoint = FVector::ZeroVector;
    UPROPERTY(BlueprintReadOnly) TObjectPtr<AActor> HitActor = nullptr;
    UPROPERTY(BlueprintReadOnly) FBreakerDamageResult DamageResult;

    // ---- Per-pellet record, ADDITIVE ---------------------------------------
    // One entry per pellet of the trigger pull, in fire order, hits and misses
    // alike. A single-projectile weapon records exactly one entry, so consumers
    // never need a special case for "is this a shotgun"; a projectile weapon
    // records none, because it put a real actor in the world instead.
    //
    // Rides the cosmetic multicast with the rest of the struct. The definition
    // clamps PelletsPerShot to 32, which bounds the payload.
    UPROPERTY(BlueprintReadOnly) TArray<FBreakerPelletImpact> Pellets;

    // How many pellets this trigger pull put in the air (0 for a projectile).
    int32 GetPelletCount() const { return Pellets.Num(); }
    // How many of them landed. Equals GetPelletCount() minus the misses; the
    // legacy bHit is exactly (GetLandedPelletCount() > 0).
    int32 GetLandedPelletCount() const
    {
        int32 Landed = 0;
        for (const FBreakerPelletImpact& Pellet : Pellets) { if (Pellet.bHit) ++Landed; }
        return Landed;
    }
    // Recoil pattern position of this shot: 0 is the first shot of a burst.
    // Replicated with the cosmetic event so every machine kicks identically.
    UPROPERTY(BlueprintReadOnly) int32 BurstShotIndex = 0;
    // Seed for this shot's small random recoil component.
    UPROPERTY(BlueprintReadOnly) int32 RecoilSeed = 0;
    UPROPERTY(BlueprintReadOnly) bool bAimedShot = false;
    // How far into ADS the weapon was when this round left it, 0 (hip) to 1
    // (fully sighted). Carried with the cosmetic event so every machine
    // reproduces the same partial-ADS kick.
    UPROPERTY(BlueprintReadOnly) float AimAlpha = 0.0f;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FBreakerShotEvent, const FBreakerShotResult&, Shot);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FBreakerAmmoEvent, int32, MagazineAmmo, int32, ReserveAmmo);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FBreakerReloadEvent, bool, bReloading);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FBreakerSwapEvent, bool, bSwapping, int32, SlotNumber);

UCLASS(ClassGroup=Weapons, BlueprintType, meta=(BlueprintSpawnableComponent))
class RIORSEDGE_API UBreakerWeaponComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UBreakerWeaponComponent();
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaSeconds, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UFUNCTION(BlueprintCallable, Category="Weapon") void StartFire();
    UFUNCTION(BlueprintCallable, Category="Weapon") void StopFire();
    UFUNCTION(BlueprintCallable, Category="Weapon") void StartReload();
    UFUNCTION(BlueprintCallable, Category="Weapon") void SetAiming(bool bNewAiming);
    UFUNCTION(BlueprintCallable, Category="Weapon") void EquipArchetype(EBreakerWeaponArchetype NewArchetype);
    UFUNCTION(BlueprintCallable, Category="Weapon") void EquipSlot(int32 SlotNumber);
    // Assigns which archetype a loadout slot carries; resets that slot's
    // stored ammunition to the new weapon's defaults.
    UFUNCTION(BlueprintCallable, Category="Weapon") void SetSlotArchetype(int32 SlotNumber, EBreakerWeaponArchetype NewArchetype);
    UFUNCTION(BlueprintPure, Category="Weapon") EBreakerWeaponArchetype GetSlotArchetype(int32 SlotNumber) const { return SlotNumber == 1 ? SlotOneArchetype : SlotTwoArchetype; }
    UFUNCTION(BlueprintPure, Category="Weapon") bool IsReloading() const { return bReloading; }
    UFUNCTION(BlueprintPure, Category="Weapon") bool IsAiming() const { return bAiming; }
    UFUNCTION(BlueprintPure, Category="Weapon") int32 GetMagazineAmmo() const { return MagazineAmmo; }
    UFUNCTION(BlueprintPure, Category="Weapon") int32 GetReserveAmmo() const { return ReserveAmmo; }
    UFUNCTION(BlueprintPure, Category="Weapon") const UBreakerWeaponDefinition* GetDefinition() const { return WeaponDefinition; }
    // The definition actually in use: the authored asset when one is set,
    // otherwise the archetype's code-driven prototype.
    UFUNCTION(BlueprintPure, Category="Weapon") const UBreakerWeaponDefinition* GetActiveDefinition() const { return ResolveDefinition(); }
    UFUNCTION(BlueprintPure, Category="Weapon") EBreakerWeaponArchetype GetArchetype() const { return CurrentArchetype; }
    UFUNCTION(BlueprintPure, Category="Weapon") int32 GetCurrentSlot() const { return CurrentSlot; }
    UFUNCTION(BlueprintPure, Category="Weapon") bool IsSwapping() const { return bSwapping; }
    // Seconds since the last swap completed. Secondary "damage on swap-in"
    // affixes read this to decide whether their window is open.
    UFUNCTION(BlueprintPure, Category="Weapon") float GetSecondsSinceSwapIn() const;
    // Reads the equipped Primary/Secondary items and arms their archetypes.
    // Call after anything that changes equipment; an empty slot is left alone.
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Weapon") void SyncArchetypesToEquipment();
    UFUNCTION(BlueprintPure, Category="Weapon") FString GetArchetypeName() const;
    // Composed cadence. The Fire Rate affix (Weapon.FireRate, the line the SMG
    // leans toward) reaches gameplay through these two and nowhere else.
    UFUNCTION(BlueprintPure, Category="Weapon|Damage") float GetFireRateMultiplier() const;
    UFUNCTION(BlueprintPure, Category="Weapon|Damage") float GetEffectiveRoundsPerMinute(const UBreakerWeaponDefinition* Definition) const;
    UFUNCTION(BlueprintPure, Category="Weapon|Debug") const FBreakerShotResult& GetLastShot() const { return LastShot; }
    UFUNCTION(BlueprintPure, Category="Weapon|Debug") float GetSecondsSinceLastShot() const;
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Weapon|Playtest") void ResetAmmunition();
    // Ammo economy (O2 placeholder): grants Fraction of each slot's
    // StartingReserveAmmo into that slot's reserve, capped at 2x starting
    // reserve so drops top a player up without making reserve meaningless.
    // Applies to the equipped weapon AND the stowed slot, so swapping is
    // never punished by an empty second gun.
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Weapon|Ammo") void AddReserveAmmoFraction(float Fraction);

    // ---- Item level -> base damage ----------------------------------------
    // Power-Curve.md §3. Base weapon damage is the multiplicand every affix,
    // node and crit multiplies; before this it was an archetype constant, so an
    // item level 1 weapon and an item level 50 weapon hit identically and the
    // primary power axis of the genre did not exist.
    //
    // This layer owns the MULTIPLICAND ONLY. The additive Increased bucket, the
    // More product and crit are the attribute set's, and nothing here touches
    // fire rate, magazine or reserve.

    // `w` in WeaponBase(ilvl) = ArchetypeBase * (1 + w)^(ilvl - 1), as a
    // fraction per level. Chosen equal to the monster health growth `g` so a
    // baseline build holds a roughly constant TTK across the game; set it above
    // `g` and baseline TTK falls with level, below and the game outruns the
    // player. 0 restores the flat pre-curve behaviour for A/B. O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Weapon|Damage", meta=(ClampMin="0", ClampMax="1"))
    float ItemLevelDamageGrowth = 0.09f;

    // What a weapon with nothing equipped in its slot represents. 1, so the
    // scalar is exactly 1.0 and the zero-setup gym — a clean clone with no
    // loadout — plays on the archetype numbers exactly as authored. Any other
    // choice would silently rebalance every measured TTK. O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Weapon|Damage", meta=(ClampMin="1"))
    int32 UnequippedItemLevel = 1;

    // Item level of the item equipped in the loadout slot this weapon is in
    // (slot 1 -> Primary, slot 2 -> Secondary), or UnequippedItemLevel when
    // there is no equipment component or that slot is empty.
    UFUNCTION(BlueprintPure, Category="Weapon|Damage") int32 GetEquippedItemLevel() const;
    // (1 + w)^(ilvl - 1) for the weapon as it stands right now.
    UFUNCTION(BlueprintPure, Category="Weapon|Damage") float GetItemLevelDamageScalar() const;
    // The number every damage path in this component uses as its base: the
    // active definition's Damage, scaled to the equipped item level. For a
    // multi-pellet weapon this is still PER PELLET, exactly as Damage was.
    UFUNCTION(BlueprintPure, Category="Weapon|Damage") float GetScaledBaseDamage() const;

    // ---- Weapon feel -------------------------------------------------------
    // Recoil moves the AIM, never the bullet relative to the aim: the trace
    // already follows the controller's view rotation, so the crosshair and the
    // round move together, always.

    UFUNCTION(BlueprintPure, Category="Weapon|Feel") float GetRecoilPitchDegrees() const { return RecoilPitchAccumulated; }
    UFUNCTION(BlueprintPure, Category="Weapon|Feel") float GetRecoilYawDegrees() const { return RecoilYawAccumulated; }
    // Current extra cone half-angle from sustained fire. The HUD may widen the
    // crosshair by this; it is the honest number the trace uses.
    UFUNCTION(BlueprintPure, Category="Weapon|Feel") float GetBloomDegrees() const { return BloomDegrees; }
    // Cone half-angle the NEXT shot would use, ADS and first-shot accuracy
    // included.
    UFUNCTION(BlueprintPure, Category="Weapon|Feel") float GetNextShotSpreadDegrees() const;
    UFUNCTION(BlueprintPure, Category="Weapon|Feel") int32 GetBurstShotIndex() const { return BurstShotIndex; }
    // How far into ADS the weapon is, 0 (hip) to 1 (fully sighted). Ramps over
    // the profile's AimInSeconds while the aim button is held and drops to 0
    // the instant it is released: committing to sights takes time, abandoning
    // them does not.
    UFUNCTION(BlueprintPure, Category="Weapon|Feel") float GetAimAlpha() const;
    // Extra cone the owner is currently paying for being in motion. Aimed
    // movement costs AimMoveSpreadMultiplier times as much as hip movement.
    UFUNCTION(BlueprintPure, Category="Weapon|Feel") float GetMovementSpreadDegrees() const;
    // Camera-relative offset of the placeholder weapon mesh, in centimetres:
    // X back toward the player, Y lateral. Presentation reads this; nothing in
    // the damage path does.
    UFUNCTION(BlueprintPure, Category="Weapon|Feel") FVector GetViewmodelLocationOffset() const;
    UFUNCTION(BlueprintPure, Category="Weapon|Feel") FRotator GetViewmodelRotationOffset() const;
    UFUNCTION(BlueprintPure, Category="Weapon|Feel") FBreakerRecoilProfile GetRecoilProfile() const { return ResolveRecoilProfile(); }

    // ---- The ADS mobility bill: the weapons half ---------------------------
    //
    // THE GAP, STATED EXACTLY. ADS is supposed to cost time and mobility. It
    // charges the time (AimInSeconds) and it charges cone while moving
    // (MoveSpreadDegrees x AimMoveSpreadMultiplier), but it has never charged
    // MOVEMENT SPEED, because `Source/RiorsEdge/Movement/` has no aim awareness
    // whatsoever: `UBreakerCharacterMovementComponent::GetMaxSpeed()` composes
    // walk/sprint speed with the gear multiplier and the temporary-multiplier
    // stack, and nothing in that chain has ever heard of the weapon.
    //
    // This side is now closed. The archetype authors its own aimed speed
    // penalty (`FBreakerRecoilProfile::AimMoveSpeedMultiplier`), the weapon
    // composes it against live ADS progress so snapping to sights does not
    // instantly bolt the player to the floor, and it is published here as one
    // query with no world side effects.
    //
    // WHAT IS STILL MISSING, on the OTHER side of the boundary and owned by
    // whoever holds Movement/: exactly one consumer.
    // `UBreakerCharacterMovementComponent::GetMaxSpeed()` must multiply its
    // grounded cap by `GetAimMoveSpeedMultiplier()` from the owner's weapon
    // component (or the character must push it as a keyed temporary multiplier
    // on aim state changes). Until that lands this returns an honest number
    // that nobody reads, which is a visible gap rather than a silent one.
    // Sliding and the boosted-speed ceiling deliberately have no opinion here;
    // whether an aimed slide is slowed is a movement-feel ruling, not a weapon
    // one.
    UFUNCTION(BlueprintPure, Category="Weapon|Aim")
    float GetAimMoveSpeedMultiplier() const;
    // The fully-sighted penalty this archetype authors, ignoring current aim
    // state. The loadout screen and tuning read this; movement wants the
    // composed one above.
    UFUNCTION(BlueprintPure, Category="Weapon|Aim")
    float GetArchetypeAimMoveSpeedMultiplier() const { return ResolveRecoilProfile().AimMoveSpeedMultiplier; }

    // ---- Presentation ------------------------------------------------------
    // VISUAL ONLY, and deliberately NOT where the trace starts.
    //
    // The trace begins at the camera (GetViewPoint) and that is load-bearing:
    // the feel layer's tested invariant is that recoil moves the aim and the
    // round follows the aim, so the round always lands on the crosshair. This
    // accessor exists so the tracer LINE can be drawn from the gun instead of
    // from the middle of the player's face. Visual origin and trace origin
    // differing is standard practice in every first-person shooter; the two
    // converge at the impact point, which is the only place they must agree.
    //
    // Nothing in the damage path may call this.
    UFUNCTION(BlueprintPure, Category="Weapon|Presentation") FVector GetVisualMuzzleLocation() const;
    // Camera-space muzzle offset in centimetres: X forward, Y right, Z up.
    // The default matches the placeholder weapon assembly on ABreakerCharacter
    // (visual at +48 fwd / +18 right / -18 up, barrel a further +31.5 fwd once
    // the parent scale is applied, half its own length again to the tip).
    // O2 PLACEHOLDER — replace with a socket lookup when authored arms land.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Weapon|Presentation")
    FVector MuzzleViewOffset = FVector(95.0f, 18.0f, -18.0f);
    // Aiming pulls the gun under the crosshair, so the muzzle comes with it.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Weapon|Presentation")
    FVector AimedMuzzleViewOffset = FVector(95.0f, 2.0f, -6.0f);
    // Clears kick, bloom, and viewmodel state without touching the aim.
    UFUNCTION(BlueprintCallable, Category="Weapon|Feel") void ResetWeaponFeel();

    // Master switch, so the owner can A/B the whole layer in the editor.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Weapon|Feel") bool bRecoilEnabled = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Weapon|Feel") bool bViewmodelKickEnabled = true;
    // Global trim over every archetype's kick, for fast whole-game tuning.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Weapon|Feel", meta=(ClampMin="0", ClampMax="4")) float RecoilScale = 1.0f;
    // Per-archetype override, editable on the component instance so the owner
    // can tune every weapon's feel in the editor without a recompile. An entry
    // here wins over the definition's own profile.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Weapon|Feel") TMap<EBreakerWeaponArchetype, FBreakerRecoilProfile> RecoilOverrides;
    // Ground speed at which a weapon pays its full MoveSpreadDegrees. Below
    // this the penalty scales down linearly; standing still costs nothing.
    // Set near walk rather than sprint so "planted" means actually planted.
    // O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Weapon|Feel", meta=(ClampMin="1")) float MoveSpreadReferenceSpeed = 600.0f;

    // ---- Weak-point forgiveness -------------------------------------------
    // The enemy head is a 20 cm sphere and the shot is a zero-radius line, so
    // weak-point acceptance is a hard binary with no felt edge: the round that
    // clips the ear and the round that misses the shoulder read identically to
    // the player. This is a world-space halo added around any component tagged
    // WeakPoint on the actor that was hit — the same physical generosity at
    // 5 m and at 50 m. It never creates a hit; it only upgrades a hit that
    // already landed on that actor.
    //
    // THE knob for "weak points feel stingy". 0 restores the exact old
    // geometric test. O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Weapon|Feel", meta=(ClampMin="0")) float WeakPointToleranceCm = 14.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon") TObjectPtr<UBreakerWeaponDefinition> WeaponDefinition;
    UPROPERTY(BlueprintAssignable, Category="Weapon") FBreakerShotEvent OnShot;
    UPROPERTY(BlueprintAssignable, Category="Weapon") FBreakerAmmoEvent OnAmmoChanged;
    UPROPERTY(BlueprintAssignable, Category="Weapon") FBreakerReloadEvent OnReloadChanged;
    UPROPERTY(BlueprintAssignable, Category="Weapon") FBreakerSwapEvent OnSwapChanged;

protected:
    UFUNCTION(Server, Reliable) void ServerStartFire();
    UFUNCTION(Server, Reliable) void ServerStopFire();
    UFUNCTION(Server, Reliable) void ServerStartReload();
    UFUNCTION(Server, Reliable) void ServerSetAiming(bool bNewAiming);
    UFUNCTION(Server, Reliable) void ServerEquipSlot(int32 SlotNumber);
    UFUNCTION(Server, Reliable) void ServerSetSlotArchetype(int32 SlotNumber, EBreakerWeaponArchetype NewArchetype);
    UFUNCTION(NetMulticast, Unreliable) void MulticastShotCosmetics(const FBreakerShotResult& Shot);
    UFUNCTION() void OnRep_Ammo();
    UFUNCTION() void OnRep_Reloading();
    UFUNCTION() void OnRep_Swapping();

private:
    UPROPERTY(ReplicatedUsing=OnRep_Ammo) int32 MagazineAmmo = 0;
    UPROPERTY(ReplicatedUsing=OnRep_Ammo) int32 ReserveAmmo = 0;
    UPROPERTY(ReplicatedUsing=OnRep_Reloading) bool bReloading = false;
    UPROPERTY(ReplicatedUsing=OnRep_Swapping) bool bSwapping = false;
    UPROPERTY(Replicated) EBreakerWeaponArchetype CurrentArchetype = EBreakerWeaponArchetype::Rifle;
    UPROPERTY(Replicated) int32 CurrentSlot = 1;
    UPROPERTY(Replicated) EBreakerWeaponArchetype SlotOneArchetype = EBreakerWeaponArchetype::Rifle;
    UPROPERTY(Replicated) EBreakerWeaponArchetype SlotTwoArchetype = EBreakerWeaponArchetype::Shotgun;
    int32 SlotOneMagazineAmmo = -1;
    int32 SlotOneReserveAmmo = -1;
    int32 SlotTwoMagazineAmmo = -1;
    int32 SlotTwoReserveAmmo = -1;
    bool bAiming = false;
    // When the aim button went down. ADS benefits ramp from here.
    double AimStartTime = -1000.0;
    bool bTriggerHeld = false;
    // Rounds fired since this fire-burst began, for ShotsPerBurst weapons only.
    // Deliberately NOT the same counter as BurstShotIndex below: that one is
    // the RECOIL pattern's position and resets on trigger rest, this one is the
    // magazine cadence's position and resets at the end of each burst cycle.
    int32 RoundsInFireBurst = 0;
    int32 ShotSequence = 0;
    double LastShotTime = -1000.0;
    FTimerHandle AutomaticFireTimer;
    FTimerHandle ReloadTimer;
    FTimerHandle SwapTimer;
    double LastSwapInTime = -1000.0;
    FBreakerShotResult LastShot;
    double LastCosmeticShotTime = -1000.0;

    // Weapon feel state. RecoilPitch/YawAccumulated is the settle BUDGET: the
    // degrees this component added to the control rotation and has not yet
    // given back. Recovery only ever returns what is in the budget.
    float RecoilPitchAccumulated = 0.0f;
    float RecoilYawAccumulated = 0.0f;
    float RecoveryDelayRemaining = 0.0f;
    float BloomDegrees = 0.0f;
    int32 BurstShotIndex = 0;
    // The control rotation as we last left it, so manual aim movement between
    // ticks can be told apart from our own kick and recovery.
    FRotator LastAppliedControlRotation = FRotator::ZeroRotator;
    bool bHasAppliedControlRotation = false;
    FBreakerViewmodelState Viewmodel;

    const UBreakerWeaponDefinition* ResolveDefinition() const;
    FBreakerRecoilProfile ResolveRecoilProfile() const;
    // Owner ground speed over MoveSpreadReferenceSpeed, clamped to [0,1].
    float GetSpeedFraction() const;
    void SetAimingInternal(bool bNewAiming);
    // True when this pellet should be paid as a weak point: the geometric hit,
    // Lead's marked-target rule, or the forgiveness halo.
    bool ResolveWeakPointHit(const struct FHitResult& Hit, const FVector& RayOrigin, const FVector& RayDirection) const;
    void ApplyShotFeel(const FBreakerShotResult& Shot);
    void TickRecoil(float DeltaSeconds);
    void UpdateFeelTickEnabled();
    void StoreActiveSlotAmmunition();
    void InitializeSlotAmmunition();
    // Returns whether a round actually left the weapon, which the burst chain
    // needs so a shot refused by the reload/swap/cadence gates does not count
    // against the burst.
    bool FireOnce();
    // Timer entry point: UE timers need a void() signature.
    void FireOnceTimer();
    // Burst-cadence weapons only. One-shot timer chain rather than the
    // repeating timer the other automatics use, because the interval alternates
    // between the in-burst fire interval and the between-burst cycle gap.
    // Non-burst weapons keep the repeating timer exactly as before, so their
    // cadence cannot drift by a callback's worth per shot.
    void AdvanceBurstFire();
    void ScheduleBurstFire(float DelaySeconds);
    void FireProjectile(const UBreakerWeaponDefinition* Definition, const FVector& ViewLocation, const FRotator& ViewRotation, float Spread, int32 BurstIndex, int32 RecoilSeed, float ShotAimAlpha);
    // LevelScalar is resolved once per trigger pull and passed down, so every
    // pellet and the bleed it may apply share one item-level reading.
    void ApplyBleedOnHit(const UBreakerWeaponDefinition* Definition, AActor* Target, const UBreakerAttributeSet* SourceAttributes, float LevelScalar);
    void FinishReload();
    void FinishSwap();
    bool CanFire() const;
    void GetViewPoint(FVector& OutLocation, FRotator& OutRotation) const;
};
