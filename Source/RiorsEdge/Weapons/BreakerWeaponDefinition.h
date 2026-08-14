#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Weapons/BreakerWeaponFeel.h"
#include "BreakerWeaponDefinition.generated.h"

UCLASS(BlueprintType)
class RIORSEDGE_API UBreakerWeaponDefinition : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    virtual FPrimaryAssetId GetPrimaryAssetId() const override
    {
        return FPrimaryAssetId(TEXT("WeaponDefinition"), WeaponId);
    }

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Identity") FName WeaponId = TEXT("PrototypeRifle");
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Identity") FText DisplayName;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Identity") FGameplayTagContainer WeaponTags;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Damage", meta=(ClampMin="0")) float Damage = 24.0f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Damage", meta=(ClampMin="1")) float WeakPointMultiplier = 1.75f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Damage", meta=(ClampMin="0")) float ArmorPenetration = 0.0f;
    // Falloff, rifle values (the archetype table states only its differences).
    // The gym is now an open field whose ranged enemy holds 9-19 m, so the
    // ordinary fight happens past where a small-arena curve started biting.
    // The curve keeps its shape and the archetypes keep their spread; it is
    // the SEVERITY that was tuned for a room. O2 PLACEHOLDER
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Damage", meta=(ClampMin="0")) float FalloffStart = 2800.0f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Damage", meta=(ClampMin="0")) float FalloffEnd = 7000.0f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Damage", meta=(ClampMin="0", ClampMax="1")) float MinimumFalloffMultiplier = 0.72f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Damage", meta=(ClampMin="1")) float MaximumRange = 12000.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Firing", meta=(ClampMin="1")) float RoundsPerMinute = 600.0f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Firing") bool bAutomatic = true;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Firing", meta=(ClampMin="1", ClampMax="32")) int32 PelletsPerShot = 1;
    // Burst cadence (O27 breadth). 1 means "not a burst weapon" and the whole
    // mechanic is inert, which is what every pre-existing archetype is.
    //
    // Above 1, a held trigger fires this many rounds at RoundsPerMinute and
    // then waits BurstCycleSeconds before the next burst. That gap is the whole
    // point: it is a cadence axis no other archetype has, it makes each burst
    // one decision rather than a stream, and it lets the recoil pattern be a
    // hard ladder that is fully paid off before the next burst starts.
    // Requires bAutomatic; a semi-automatic burst weapon is just a burst per
    // click, which is also legal.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Firing", meta=(ClampMin="1", ClampMax="10")) int32 ShotsPerBurst = 1;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Firing", meta=(ClampMin="0")) float BurstCycleSeconds = 0.0f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Firing", meta=(ClampMin="0")) float HipSpreadDegrees = 1.2f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Firing", meta=(ClampMin="0")) float AimSpreadDegrees = 0.25f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ammo", meta=(ClampMin="1")) int32 MagazineSize = 30;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ammo", meta=(ClampMin="0")) int32 StartingReserveAmmo = 120;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ammo", meta=(ClampMin="0")) float ReloadDuration = 1.8f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Handling", meta=(ClampMin="0")) float SwapInDuration = 0.5f;

    // How the weapon pushes back. Recoil belongs in the same table as cadence,
    // spread, and falloff so five archetypes feel like five weapons. Editable
    // on the asset; the weapon component's RecoilOverrides map wins over it.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Handling") FBreakerRecoilProfile Recoil;

    // Per-pellet chance to apply the snapshotting Bleed DoT on a damaging hit.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Status", meta=(ClampMin="0", ClampMax="1")) float BleedChance = 0.0f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Status", meta=(ClampMin="0")) float BleedDamagePerTick = 0.0f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Status", meta=(ClampMin="0")) float BleedDuration = 0.0f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Status", meta=(ClampMin="0.05")) float BleedTickInterval = 0.5f;

    // Projectile weapons spawn a rocket instead of tracing. Self-damage and
    // self-knockback rules are an open design question; rockets currently
    // ignore their instigator.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Projectile") bool bProjectile = false;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Projectile", meta=(ClampMin="100")) float ProjectileSpeed = 3000.0f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Projectile", meta=(ClampMin="0")) float ExplosionRadius = 350.0f;
};
