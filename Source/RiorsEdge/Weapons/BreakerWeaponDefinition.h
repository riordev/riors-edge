#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
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
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Damage", meta=(ClampMin="0")) float FalloffStart = 2000.0f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Damage", meta=(ClampMin="0")) float FalloffEnd = 6000.0f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Damage", meta=(ClampMin="0", ClampMax="1")) float MinimumFalloffMultiplier = 0.55f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Damage", meta=(ClampMin="1")) float MaximumRange = 12000.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Firing", meta=(ClampMin="1")) float RoundsPerMinute = 600.0f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Firing") bool bAutomatic = true;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Firing", meta=(ClampMin="1", ClampMax="32")) int32 PelletsPerShot = 1;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Firing", meta=(ClampMin="0")) float HipSpreadDegrees = 1.2f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Firing", meta=(ClampMin="0")) float AimSpreadDegrees = 0.25f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ammo", meta=(ClampMin="1")) int32 MagazineSize = 30;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ammo", meta=(ClampMin="0")) int32 StartingReserveAmmo = 120;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ammo", meta=(ClampMin="0")) float ReloadDuration = 1.8f;
};
