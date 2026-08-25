#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Combat/BreakerCombatTypes.h"
#include "Weapons/BreakerWeaponArchetype.h"
#include "Weapons/BreakerWeaponFeel.h"
#include "BreakerWeaponComponent.generated.h"

class UBreakerAttributeSet;
class UBreakerWeaponDefinition;
class UBreakerProgressionComponent;
class UBreakerMomentumComponent;
enum class EBreakerMomentumState : uint8;

// ---------------------------------------------------------------------------
// The composed projectile channels one trigger pull fires with (owner ruling
// 2026-08-16: Swift's identity is multishot / pierce / chain / ricochet,
// manipulated by Momentum). All four default to zero, so a character with no
// nodes, no ability window and no Momentum fires exactly the shot the
// archetype table authored — nothing changes for non-Swift builds until a
// lane, a keyed push or a momentum state grants a count.
// ---------------------------------------------------------------------------
USTRUCT(BlueprintType)
struct RIORSEDGE_API FBreakerShotChannels
{
    GENERATED_BODY()

    // Extra pellets per trigger pull, on top of the definition's
    // PelletsPerShot. Fractional: the weapon banks the fraction across pulls
    // and fires the whole pellet when it crosses 1 (a visible rhythm, not a
    // rounding loss). Extra pellets draw their spread from salted sub-streams
    // so the primary sequence never moves.
    UPROPERTY(BlueprintReadOnly) float AdditionalProjectiles = 0.0f;
    // Enemies a shot may continue THROUGH beyond its first, paying the
    // weapon's per-pierce falloff each penetration.
    UPROPERTY(BlueprintReadOnly) int32 PierceCount = 0;
    // Arcs the shot takes to a further enemy after its final target, each at
    // the weapon's chain damage fraction of the hit before it.
    UPROPERTY(BlueprintReadOnly) int32 ChainCount = 0;
    // Bounces off geometry toward the nearest enemy in line of sight.
    UPROPERTY(BlueprintReadOnly) int32 RicochetCount = 0;

    bool IsIdentity() const
    {
        return AdditionalProjectiles <= 0.0f && PierceCount <= 0 && ChainCount <= 0 && RicochetCount <= 0;
    }

    FBreakerShotChannels& operator+=(const FBreakerShotChannels& Other)
    {
        AdditionalProjectiles += Other.AdditionalProjectiles;
        PierceCount += Other.PierceCount;
        ChainCount += Other.ChainCount;
        RicochetCount += Other.RicochetCount;
        return *this;
    }
};

// One secondary leg of a shot: a pierce continuation, a chain arc, or a
// ricochet bounce. ADDITIVE beside FBreakerPelletImpact for the same reason
// that struct exists — presentation needs a segment (start AND end) to draw an
// arc, and the per-pellet record's one-entry-per-pellet contract must not be
// disturbed by legs that are not pellets.
USTRUCT(BlueprintType)
struct RIORSEDGE_API FBreakerSecondaryImpact
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) FVector Start = FVector::ZeroVector;
    UPROPERTY(BlueprintReadOnly) FVector End = FVector::ZeroVector;
    UPROPERTY(BlueprintReadOnly) bool bHit = false;
    UPROPERTY(BlueprintReadOnly) TObjectPtr<AActor> HitActor = nullptr;
};


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
    // ---- Secondary legs, ADDITIVE ------------------------------------------
    // Pierce continuations, chain arcs and ricochet bounces, in resolution
    // order. Empty for every shot fired by a build with the channels at zero,
    // so nothing that predates the channels ever sees one. Each leg carries
    // its own start, because an arc's tracer cannot be reconstructed from the
    // pellet's muzzle.
    UPROPERTY(BlueprintReadOnly) TArray<FBreakerSecondaryImpact> SecondaryImpacts;

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
// The two magazine-economy events Scrap generation needs (Class-Kits-Gunsmith
// §1.1; the kit doc ranks OnMagazineEmptied as its top missing hook). Both
// carry their anti-farm clause as the parameter, so a listener CANNOT credit
// the un-earned case by omission:
//  * ReloadCompleted's bAnyRoundFired is false for a top-off of a magazine
//    nothing has left (rounds only ever leave by firing, so "the reload loaded
//    anything" and "a round was fired since the magazine was last full" are
//    the same fact);
//  * MagazineEmptied fires on the LAST ROUND LEAVING the magazine — never on
//    the reload — and bStartedFull is true only when the magazine was full at
//    the start of this fire cycle, so topping off at 1/30 and firing one round
//    does not re-arm the dump bonus.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FBreakerReloadCompletedEvent, bool, bAnyRoundFired);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FBreakerMagazineEmptiedEvent, bool, bStartedFull);

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
    // The whole trigger pull: GetScaledBaseDamage() times the active
    // definition's PelletsPerShot. THIS is the number a melee/blast
    // weapon-coefficient means by "weapon damage" — the per-pellet base reads
    // a shotgun at 10 while a sniper reads 72, so "1.5x weapon damage" on a
    // shotgun Tank swung feather-soft, a 7.2x thematic inversion (audit F1,
    // Build-Profiles-2026-08-16). Weapon ROUNDS keep the per-pellet accessor;
    // nothing in the fire path changes.
    //
    // What a 1.0-coefficient swing reads per archetype at item level 1:
    //   Rifle 24, SMG 13, Sniper 72, Shotgun 80 (10 x 8 pellets), Rocket 90,
    //   Burst Rifle 29, Machinegun 11, Sidearm 21.
    // The shotgun becomes the heavy swing, which is the theme. The burst
    // rifle is deliberately PER ROUND (29): a trigger pull puts ONE round in
    // the air — the burst is a cadence fact, not a payload fact — so pellet
    // count is the only per-pull payload multiplier there is.
    UFUNCTION(BlueprintPure, Category="Weapon|Damage") float GetScaledFullBlastDamage() const;

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

    // ---- Point-blank range treatment (keystone rewrite support) -----------
    // Standing Wave's Overdrive variant (Class-Kits line 193): "converts the
    // frozen [Momentum] value into weapon range... shots behave as if fired
    // at point-blank regardless of distance." This is the RANGE half only —
    // it forces the falloff multiplier to 1.0 regardless of Hit.Distance
    // while at least one entry is active. It changes nothing else about the
    // shot path: spread, pellets, the trace, recoil, and the aim/trace
    // contract are untouched. There is no projectile-speed concept on this
    // component for the other half of that sentence to reach (rockets carry
    // no falloff at all today, see FireProjectile) — that half is left
    // uncovered rather than invented.
    //
    // Keyed, duration-expiring, lazily pruned: the same shape as
    // UBreakerCharacterMovementComponent::PushSpeedMultiplier. Duration <= 0
    // means no expiry, popped explicitly instead.
    UFUNCTION(BlueprintCallable, Category="Weapon|Damage") void PushRangeTreatmentOverride(FName Key, float Duration);
    UFUNCTION(BlueprintCallable, Category="Weapon|Damage") void PopRangeTreatmentOverride(FName Key);
    UFUNCTION(BlueprintPure, Category="Weapon|Damage") bool IsRangeTreatmentOverridden() const;

    // ---- Projectile channels (owner ruling 2026-08-16) --------------------
    // Swift's redesign: "multishot, pierce, chain, ricochet, movement,
    // manipulation of projectiles with your momentum". The channels compose
    // from three sources, in this order:
    //   1. the progression tree's Flat lanes (FBreakerNodeStats — the
    //      ProjectileCount / Pierce / ChainCount / RicochetCount stat targets),
    //   2. keyed pushes from ability windows (Sidearm Rig's +1 Pierce is the
    //      first caller), same lazy-expiry shape as PushRangeTreatmentOverride,
    //   3. the owner's Momentum STATE, Swift-gated by the momentum component's
    //      own IsActiveForOwner — see MomentumChannelBonus for the table.
    // Hitscan only: the rocket puts a real actor in the world and is untouched.
    UFUNCTION(BlueprintPure, Category="Weapon|Channels") FBreakerShotChannels GetShotChannels() const;
    // Keyed additive channel bonus. Re-pushing a key replaces rather than
    // stacks; Duration <= 0 means no expiry, popped explicitly.
    UFUNCTION(BlueprintCallable, Category="Weapon|Channels") void PushShotChannelBonus(FName Key, float AdditionalProjectiles, int32 PierceBonus, int32 ChainBonus, int32 RicochetBonus, float Duration = -1.0f);
    UFUNCTION(BlueprintCallable, Category="Weapon|Channels") void PopShotChannelBonus(FName Key);

    // The momentum coupling table, the identity mechanic: Momentum STATE
    // manipulates projectiles. Pure and static so the suite pins the table
    // with no world. Every magnitude below is O2 PLACEHOLDER, chosen to be
    // PERCEPTIBLE (a pierce the player cannot see is dead content):
    //   Running:  +1 Pierce            — moving fast makes rounds punch through.
    //   Redline:  +1 Pierce, +1 Chain  — held speed makes them arc onward too.
    //   Airborne: +0.5 projectile      — a second pellet every other shot
    //             (owner ruling 2026-08-16: halved from +1; the full double is
    //             bought back through Swift.Kinetic.AirWork's +0.5 airborne).
    //   Sliding:  +0.5 projectile      — a second pellet every other shot.
    // Airborne/sliding multishot requires at least Running: the coupling is
    // Momentum manipulating projectiles, not posture doing it for free.
    static FBreakerShotChannels MomentumChannelBonus(EBreakerMomentumState State, bool bAirborne, bool bSliding);

    // ---- Channel tunables, all O2 PLACEHOLDER -----------------------------
    // Damage multiplier applied per target PENETRATED: the second enemy in a
    // line takes 70% of the first, the third 49%. Overpenetration (Marksman
    // M10, by tag) skips the step after a killing hit.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Weapon|Channels", meta=(ClampMin="0", ClampMax="1")) float PierceDamageFalloff = 0.70f;   // O2 PLACEHOLDER
    // Chain arcs reach this far from the struck enemy...
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Weapon|Channels", meta=(ClampMin="0")) float ChainRadiusCm = 1200.0f;   // O2 PLACEHOLDER
    // ...and each arc deals this fraction of the hit before it.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Weapon|Channels", meta=(ClampMin="0", ClampMax="1")) float ChainDamageMultiplier = 0.50f;   // O2 PLACEHOLDER
    // Ricochet seeks the nearest enemy in line of sight within this radius of
    // the geometry impact. Base value matches nothing in the kit doc — the
    // doc's 12 m / 20 m figures belong to Swift.Marksman.Angle's two ranks
    // (Class-Kits §1.5 M4, transcribed at the read site), which OVERRIDE this
    // when owned.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Weapon|Channels", meta=(ClampMin="0")) float RicochetSeekRadiusCm = 800.0f;   // O2 PLACEHOLDER
    // A bounced round arrives at this fraction of the pellet's current damage.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Weapon|Channels", meta=(ClampMin="0", ClampMax="1")) float RicochetDamageMultiplier = 0.65f;   // O2 PLACEHOLDER

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
    // Server-side magazine-economy events (see the delegate comments above).
    // UBreakerScrapComponent's NotifyReloadCompleted / NotifyMagazineEmptied
    // are the intended listeners; ABreakerCharacter wires them.
    UPROPERTY(BlueprintAssignable, Category="Weapon") FBreakerReloadCompletedEvent OnReloadCompleted;
    UPROPERTY(BlueprintAssignable, Category="Weapon") FBreakerMagazineEmptiedEvent OnMagazineEmptied;

    // ---- Magazine capacity override (Class-Kits-Gunsmith §3 G2) -----------
    // The hook Overhaul's own doc names as MISSING ("PushMagazineCapacityOverride
    // ... with reserve debited on push and settled on pop", spec §6 G2). Keyed
    // and additive: while any entries are live the effective magazine size is
    // base plus their sum, a reload completing during the window fills to the
    // overridden capacity, and a reload completing after pop fills to base.
    // ReservePerRound > 0 is the CONVERSION form the spec describes: the push
    // immediately draws up to DeltaRounds into the magazine, debiting
    // ReservePerRound reserve per round drawn (G2's seed is 3:1), and raises
    // capacity by exactly what was drawn. ReservePerRound == 0 raises capacity
    // only and moves no rounds. Returns the rounds actually drawn.
    //
    // NEGATIVE DeltaRounds is the SHRINK form (AR10 Overpressure: "capacity
    // converts into reserve instead"). The delta is clamped so the effective
    // magazine never drops below 1 (ClampMagazineCapacityDelta), rounds the
    // shrunk capacity can no longer hold move magazine -> reserve 1:1 on the
    // push — they are real rounds changing pockets, not a conversion, so no
    // ratio applies and nothing is owed on the pop: the pop simply restores
    // capacity and the player reloads into the headroom. ReservePerRound is
    // ignored for a shrink. Returns the (negative) capacity delta actually
    // applied, 0 when nothing could shrink.
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Weapon|Magazine")
    int32 PushMagazineCapacityOverride(FName Key, int32 DeltaRounds, int32 ReservePerRound = 0);
    // The pop settles the UNSPENT remainder back: converted rounds still in
    // the magazine above the restored capacity are removed and refunded to
    // reserve at the same ratio they were bought at. Spent rounds refund
    // nothing — the bet is that you fire them (G2: "if the fight ends before
    // the window does you gave up reserve for nothing" applies only to the
    // rounds you FIRED; the unspent conversion settles back).
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Weapon|Magazine")
    void PopMagazineCapacityOverride(FName Key);
    // The active definition's MagazineSize plus every live override delta,
    // floored at 1.
    UFUNCTION(BlueprintPure, Category="Weapon|Magazine")
    int32 GetEffectiveMagazineSize() const;

    // ---- Gunsmith / Tank weapon-half node rules (2026-08-16) --------------
    // The weapon-side consumers of the Armory/Bastion tags the tree already
    // publishes. Every one is bit-identical when the node is unowned; the
    // pure halves live on FBreakerWeaponMath and are pinned world-free.

    // AR3 Chambered: true while the free post-reload round is armed — set by
    // a completing reload when the node is owned, spent by the next shot,
    // dropped by anything that changes which weapon is in the hands.
    UFUNCTION(BlueprintPure, Category="Weapon|Nodes")
    bool IsChamberedRoundArmed() const { return bChamberedRoundArmed; }

    // B7 Emplacement: whether spread currently reads as stationary — node
    // owned and the owner within the Grit layer's own anchor-near radius.
    // GetSpeedFraction (the one movement-spread input) reads this, so the
    // fired cone, the predicted cone and the HUD crosshair agree.
    UFUNCTION(BlueprintPure, Category="Weapon|Nodes")
    bool IsSpreadReadingStationary() const;

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
    // True while the current fire cycle began from a genuinely full magazine —
    // the MagazineEmptied event's anti-farm clause (see the delegate comment).
    bool bFireCycleStartedFull = false;
    // AR3 Chambered: the free round a completed reload armed (node-gated at
    // the arm site). Server fire-path state, cleared by every path that
    // changes which weapon is in the hands.
    bool bChamberedRoundArmed = false;
    // True while resolving the trigger pull that chambered the magazine's
    // final round — set beside the ammo debit, read by SubmitWeaponDamage for
    // Core.Volley.LastRound's crit guarantee. Hitscan resolution happens in
    // the same call stack as the pull, which is the scope this flag is honest
    // over; a projectile in flight resolves on impact and does not read it.
    bool bFiringFinalMagazineRound = false;
    // AR5's once-per-magazine guard: with the dump threshold at 1 the event
    // fires while a round is still chambered, and this stops the actual last
    // round re-firing it. Cleared where the magazine refills.
    bool bMagazineDumpBroadcastThisCycle = false;
    // Node-tag read on the owner's progression component, the same question
    // the Marksman rules ask by rank. False with no component.
    bool OwnerHasNodeTag(const FGameplayTag& Tag) const;
    // Keyed additive deltas over the definition's MagazineSize. No expiry: an
    // ability window owns its pop, exactly like the incoming-modifier chain.
    struct FMagazineCapacityOverrideEntry
    {
        // Capacity delta this entry contributes (== the rounds drawn, for a
        // conversion push).
        int32 DeltaRounds = 0;
        // Reserve paid per drawn round, so the pop can settle at the same rate.
        int32 ReservePerRound = 0;
    };
    TMap<FName, FMagazineCapacityOverrideEntry> MagazineCapacityOverrides;

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

    // Same entry shape as UBreakerCharacterMovementComponent::FSpeedMultiplierEntry,
    // minus the magnitude this override has no use for: presence is the signal.
    struct FRangeTreatmentOverrideEntry
    {
        // Negative = no expiry; popped explicitly.
        double ExpiryTime = -1.0;
    };
    // Mutable: IsRangeTreatmentOverridden() is const and is the natural place
    // to drop expired entries, matching the movement/momentum lazy-expiry pattern.
    mutable TMap<FName, FRangeTreatmentOverrideEntry> RangeTreatmentOverrides;
    void PruneRangeTreatmentOverrides() const;

    // ---- Projectile channel state -----------------------------------------
    // The banked sub-pellet fraction from AdditionalProjectiles. Server-side
    // fire-path state, like ShotSequence; resets with the rest of nothing —
    // deliberately, so a +0.5 build's every-other-shot rhythm survives reloads
    // and swaps rather than restarting on each.
    float MultishotAccumulator = 0.0f;
    struct FShotChannelBonusEntry
    {
        FBreakerShotChannels Channels;
        // Negative = no expiry; popped explicitly.
        double ExpiryTime = -1.0;
    };
    // Mutable for the same lazy-expiry-in-const-read reason as
    // RangeTreatmentOverrides above.
    mutable TMap<FName, FShotChannelBonusEntry> ShotChannelBonuses;
    void PruneShotChannelBonuses() const;
    // One pellet's full resolution: the pierce loop, the ricochet bounce and
    // the chain arcs. Writes impacts and damage into Shot/Pellet and returns
    // how many enemies the pellet pierced BEYOND its first hit, so FireOnce
    // can pay Pierce Discipline's Momentum once per trigger pull. Lives beside
    // FireOnce rather than inside it so the base pellet bookkeeping above it
    // stays readable. PelletSeed is the seed the pellet's spread was drawn
    // with; every secondary draw is salted off it.
    int32 ResolvePelletImpacts(const UBreakerWeaponDefinition* Definition, const FVector& ViewLocation, const FVector& Direction,
        const FBreakerShotChannels& Channels, float ScaledBaseDamage, const UBreakerAttributeSet* SourceAttributes,
        const AActor* MarkedTarget, float LeadMinimumRangeCm, float LevelScalar, int32 PelletSeed,
        FBreakerShotResult& Shot, struct FBreakerPelletImpact& Pellet);
    // Nearest legal enemy (combat component, alive, unstruck, line of sight)
    // to Origin within RadiusCm. The world query half of chain/ricochet.
    AActor* FindNearestChainTarget(const FVector& Origin, float RadiusCm, const TArray<const AActor*>& ExcludedActors) const;
    // Builds and submits one weapon damage request; shared by the base hit,
    // pierce continuations, chain arcs and ricochet legs so a stat can never
    // apply to some of them and not others.
    FBreakerDamageResult SubmitWeaponDamage(const UBreakerWeaponDefinition* Definition, class UBreakerCombatComponent* TargetCombat,
        const UBreakerAttributeSet* SourceAttributes, float BaseDamage, float DistanceFromMuzzle, bool bWeakPoint,
        float ArmorPenetrationOverride, const FVector& ImpactPoint, int32 DamageSeed,
        // O104: a weak point GRANTED by a rule rather than earned by hitting one
        // takes the weak-point multiplier instead of crit, not as well as it.
        // Defaulted false so the legs that cannot carry a mark -- chain arcs --
        // keep their existing behaviour without restating it.
        bool bWeakPointIsGranted = false);

    // ---- Marksman / Frenzy rule-half state (Class-Kits §1.3 / §1.5) --------
    // Server-side timestamps of recent trigger pulls, pruned to Loaded's 2s
    // window on write. Exists only to answer "how many shots left this weapon
    // in the 2s before this reload began" (§1.3 F2).
    TArray<double> RecentShotTimes;
    // Free rounds the reload currently in flight owes (Loaded, §1.3 F2).
    // Captured at reload START — the Redline check and the 2s window are both
    // read the moment the player commits to the reload — and settled in
    // FinishReload before reserve is drawn, so the refund is paid in saved
    // reserve rather than vanishing into an already-full magazine.
    int32 PendingLoadedRefundRounds = 0;
    // Ledger's once-per-mark bookkeeping (§1.5 M3: the refund pays when the
    // mark CONNECTS, not per shot into it). A mark is "new" when its target
    // changed or its remaining time jumped UP past the value recorded at the
    // last refund — a re-cast refreshes the window, time only ever runs down.
    TWeakObjectPtr<const AActor> LedgerRefundedTarget;
    float LedgerRefundedMarkRemaining = -1.0f;
    // Rank of a class-point node on the owner, 0 with no progression component.
    int32 GetClassNodeRank(FName NodeId) const;
    // Steady's posture read (§1.5 M2's R2 clause needs airborne).
    bool IsOwnerAirborne() const;

    const UBreakerWeaponDefinition* ResolveDefinition() const;
    FBreakerRecoilProfile ResolveRecoilProfile() const;
    // The owner's tree aggregate, or null with no progression component — the
    // read behind the weapon-handling lanes (WeaponSpread, RecoilRecovery,
    // StatusChance). Live like GetShotChannels: a respec changes the next
    // shot, not the next equip.
    const struct FBreakerNodeStats* GetOwnerNodeStats() const;
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
    // SeedBasis is the hit's own draw seed (the base pellet's ShotSequence
    // value, or a secondary leg's salted seed), so a pierced or chained hit's
    // bleed roll neither collides with the primary sequence nor repeats it.
    void ApplyBleedOnHit(const UBreakerWeaponDefinition* Definition, AActor* Target, const UBreakerAttributeSet* SourceAttributes, float LevelScalar, int32 SeedBasis);
    void FinishReload();
    void FinishSwap();
    bool CanFire() const;
    void GetViewPoint(FVector& OutLocation, FRotator& OutRotation) const;
};
