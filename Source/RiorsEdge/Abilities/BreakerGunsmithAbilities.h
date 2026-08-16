#pragma once

#include "CoreMinimal.h"
#include "Abilities/BreakerGameplayAbility.h"
#include "Combat/BreakerDeployable.h"
#include "BreakerGunsmithAbilities.generated.h"

class UBreakerWeaponComponent;

// ---------------------------------------------------------------------------
// THE GUNSMITH KIT (Class-Kits-Gunsmith §3), built to PLAYABLE as O2
// placeholders under the owner's 2026-08-16 authorization ("feel free to do
// all 5 classes"). The class's defining ergonomic — two clocks — is carried by
// the DEFINITIONS, not restated here: the two Armory abilities cost nothing
// and carry cooldowns; the four deployables cost Scrap and carry none.
//
// Costs and cooldowns live on the fallback registry rows
// (Abilities/BreakerAbilityDefinition.cpp), quoted from the kit doc. Every
// magnitude in THIS file is O2 PLACEHOLDER; doc-seeded numbers cite their row.
// ---------------------------------------------------------------------------

// G1 Sidearm Rig (§3 G1, starter, Armory): the next magazine's worth of shots
// deals bonus FLAT damage and gains +1 Pierce. Counted in SHOTS, not seconds —
// it ends when the magazine empties or on reload, whichever comes first.
//
// NEAREST HONEST NOTES, recorded rather than hidden:
//  * The flat bonus lands through the outgoing-modifier chain's FlatBonus,
//    which is the flat-sum stage the doc demands — but the chain applies to
//    ALL outgoing damage while live, not only weapon rounds. The window's
//    shot-counted lifetime keeps that leak small; a per-shot modifier hook
//    (spec §4.5 FBreakerPendingShotModifier) does not exist to be reused.
//  * +1 Pierce is NOT implemented: no pierce mechanic exists anywhere on the
//    weapon path yet, so there is no ceiling to stack into. Recorded absent.
UCLASS()
class RIORSEDGE_API UBreakerAbility_SidearmRig : public UBreakerGameplayAbility
{
    GENERATED_BODY()

public:
    UBreakerAbility_SidearmRig();

    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
    virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

    static FName WindowKey();
    static FName OutgoingModifierKey();

    // O2 PLACEHOLDER: §G1 authors "bonus flat damage" with no magnitude. Flat
    // bucket, before the additive Increased bucket, so it cannot double-dip.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="SidearmRig", meta=(ClampMin="0")) float FlatBonusDamage = 8.0f;   // O2 PLACEHOLDER

private:
    UFUNCTION() void HandleMagazineEmptied(bool bStartedFull);
    UFUNCTION() void HandleReloadChanged(bool bReloading);
    void CloseRig();

    TWeakObjectPtr<UBreakerWeaponComponent> BoundWeapon;
    bool bRigActive = false;
};

// G2 Overhaul (§3 G2, Armory): for 10s, reserve ammunition converts into
// magazine capacity at 3 reserve : 1 magazine round, drawn on activation up to
// +100% of base magazine size; the unspent remainder settles back on pop. A
// bet that trades sustain for burst. The window duration is the definition's
// WindowDuration (10s, §G2); the conversion mechanics live on the weapon's
// PushMagazineCapacityOverride — the exact hook §G2 names as missing.
UCLASS()
class RIORSEDGE_API UBreakerAbility_Overhaul : public UBreakerGameplayAbility
{
    GENERATED_BODY()

public:
    UBreakerAbility_Overhaul();

    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
    virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

    static FName WindowKey();

    // §G2: seed 3 reserve : 1 magazine round.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Overhaul", meta=(ClampMin="1")) int32 ReservePerRound = 3;   // O2 PLACEHOLDER (§G2 seed)
    // §G2: drawn up to a cap of +100% of base magazine size.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Overhaul", meta=(ClampMin="0")) float MaximumCapacityFraction = 1.0f;   // O2 PLACEHOLDER (§G2 seed)

private:
    void CloseOverhaul();

    FTimerHandle WindowTimer;
    TWeakObjectPtr<UBreakerWeaponComponent> BoundWeapon;
    bool bOverhaulActive = false;
};

// Shared base for the four deployable abilities (§3 G3-G6): validate placement
// FIRST so a failed placement costs nothing (§2.3), enforce the density cap
// with destroy-oldest semantics (§2.1), commit the Scrap cost through the
// ordinary GAS cost effect, then spawn. No cooldown — Scrap is the cooldown.
UCLASS(Abstract)
class RIORSEDGE_API UBreakerGunsmithDeployAbility : public UBreakerGameplayAbility
{
    GENERATED_BODY()

public:
    UBreakerGunsmithDeployAbility();

    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

    // §2.3: seed 8 m along the aim ray.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Deploy", meta=(ClampMin="0")) float PlacementRangeCm = 800.0f;   // §2.3 seed

protected:
    EBreakerDeployableType DeployableType = EBreakerDeployableType::Turret;
};

// G3 Turret (§3 G3, starter, Field Tech): 40 Scrap, autonomous emplacement,
// 30s. Consistent, never optimal.
UCLASS()
class RIORSEDGE_API UBreakerAbility_Turret : public UBreakerGunsmithDeployAbility
{
    GENERATED_BODY()
public:
    UBreakerAbility_Turret();
};

// G4 Ammo Crate (§3 G4, Field Tech): 30 Scrap, reserve ammunition on a charge
// pool, fully self-usable at full value.
UCLASS()
class RIORSEDGE_API UBreakerAbility_AmmoCrate : public UBreakerGunsmithDeployAbility
{
    GENERATED_BODY()
public:
    UBreakerAbility_AmmoCrate();
};

// G5 Mine Cluster (§3 G5, Tinkerer): 35 Scrap, three proximity charges on an
// arm delay. ONE placement against the density cap, not three.
UCLASS()
class RIORSEDGE_API UBreakerAbility_MineCluster : public UBreakerGunsmithDeployAbility
{
    GENERATED_BODY()
public:
    UBreakerAbility_MineCluster();
};

// G6 Disruptor (§3 G6, Tinkerer): 45 Scrap, a field that slows and strips a
// FLAT amount of armour. The class's only crowd control.
UCLASS()
class RIORSEDGE_API UBreakerAbility_Disruptor : public UBreakerGunsmithDeployAbility
{
    GENERATED_BODY()
public:
    UBreakerAbility_Disruptor();
};

// FIELD ASSEMBLY (§3 ultimate): 100 Scrap (full bar), no cooldown. Deploys
// every unlocked deployable type at once and raises the density cap to 8 for
// 20s (per-type stays 2).
//
// "Unlocked" means NODE-PURCHASED, and the Gunsmith branch trees are
// deliberately not authored this pass (kits-playable scope) — so with no grant
// nodes in existence the whole kit counts as unlocked, and this comment is the
// tripwire: when the granting nodes land, this must start reading node
// purchases or it silently over-delivers.
UCLASS()
class RIORSEDGE_API UBreakerAbility_FieldAssembly : public UBreakerGameplayAbility
{
    GENERATED_BODY()

public:
    UBreakerAbility_FieldAssembly();

    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
    virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

    static FName WindowKey();
    static FName MachinistModifierKey();

    // §3: the window raises the TOTAL cap to 8; per-type stays 2, invariantly.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="FieldAssembly", meta=(ClampMin="4")) int32 RaisedDensityCap = 8;   // §3
    // Machinist's per-type mapping table is authored as a SHAPE by the doc and
    // explicitly not its magnitudes. Two of the four entries are implemented
    // (Turret -> a flat weapon-damage rider; Ammo Crate -> periodic reserve
    // regeneration); the Mine Cluster on-kill detonation and Disruptor aura
    // entries are recorded ABSENT rather than faked.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="FieldAssembly", meta=(ClampMin="0")) float MachinistFlatDamageFraction = 0.3f;   // O2 PLACEHOLDER (fraction of scaled weapon base)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="FieldAssembly", meta=(ClampMin="0.5")) float MachinistReservePulseSeconds = 5.0f;   // O2 PLACEHOLDER
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="FieldAssembly", meta=(ClampMin="0", ClampMax="1")) float MachinistReservePulseFraction = 0.1f;   // O2 PLACEHOLDER

private:
    void HandleMachinistPulse();
    void CloseAssembly();

    FTimerHandle WindowTimer;
    FTimerHandle MachinistPulseTimer;
    bool bAssemblyActive = false;
};
