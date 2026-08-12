#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Combat/BreakerCombatTypes.h"
#include "BreakerWeaponComponent.generated.h"

class UBreakerWeaponDefinition;

USTRUCT(BlueprintType)
struct RIORSEDGE_API FBreakerShotResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) bool bFired = false;
    UPROPERTY(BlueprintReadOnly) bool bHit = false;
    UPROPERTY(BlueprintReadOnly) bool bWeakPoint = false;
    UPROPERTY(BlueprintReadOnly) FVector TraceStart = FVector::ZeroVector;
    UPROPERTY(BlueprintReadOnly) FVector TraceEnd = FVector::ZeroVector;
    UPROPERTY(BlueprintReadOnly) FVector ImpactPoint = FVector::ZeroVector;
    UPROPERTY(BlueprintReadOnly) TObjectPtr<AActor> HitActor = nullptr;
    UPROPERTY(BlueprintReadOnly) FBreakerDamageResult DamageResult;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FBreakerShotEvent, const FBreakerShotResult&, Shot);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FBreakerAmmoEvent, int32, MagazineAmmo, int32, ReserveAmmo);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FBreakerReloadEvent, bool, bReloading);

UCLASS(ClassGroup=Weapons, BlueprintType, meta=(BlueprintSpawnableComponent))
class RIORSEDGE_API UBreakerWeaponComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UBreakerWeaponComponent();
    virtual void BeginPlay() override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UFUNCTION(BlueprintCallable, Category="Weapon") void StartFire();
    UFUNCTION(BlueprintCallable, Category="Weapon") void StopFire();
    UFUNCTION(BlueprintCallable, Category="Weapon") void StartReload();
    UFUNCTION(BlueprintCallable, Category="Weapon") void SetAiming(bool bNewAiming);
    UFUNCTION(BlueprintPure, Category="Weapon") bool IsReloading() const { return bReloading; }
    UFUNCTION(BlueprintPure, Category="Weapon") bool IsAiming() const { return bAiming; }
    UFUNCTION(BlueprintPure, Category="Weapon") int32 GetMagazineAmmo() const { return MagazineAmmo; }
    UFUNCTION(BlueprintPure, Category="Weapon") int32 GetReserveAmmo() const { return ReserveAmmo; }
    UFUNCTION(BlueprintPure, Category="Weapon") const UBreakerWeaponDefinition* GetDefinition() const { return WeaponDefinition; }
    UFUNCTION(BlueprintPure, Category="Weapon|Debug") const FBreakerShotResult& GetLastShot() const { return LastShot; }
    UFUNCTION(BlueprintPure, Category="Weapon|Debug") float GetSecondsSinceLastShot() const;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon") TObjectPtr<UBreakerWeaponDefinition> WeaponDefinition;
    UPROPERTY(BlueprintAssignable, Category="Weapon") FBreakerShotEvent OnShot;
    UPROPERTY(BlueprintAssignable, Category="Weapon") FBreakerAmmoEvent OnAmmoChanged;
    UPROPERTY(BlueprintAssignable, Category="Weapon") FBreakerReloadEvent OnReloadChanged;

protected:
    UFUNCTION(Server, Reliable) void ServerStartFire();
    UFUNCTION(Server, Reliable) void ServerStopFire();
    UFUNCTION(Server, Reliable) void ServerStartReload();
    UFUNCTION(Server, Reliable) void ServerSetAiming(bool bNewAiming);
    UFUNCTION(NetMulticast, Unreliable) void MulticastShotCosmetics(const FBreakerShotResult& Shot);
    UFUNCTION() void OnRep_Ammo();
    UFUNCTION() void OnRep_Reloading();

private:
    UPROPERTY(ReplicatedUsing=OnRep_Ammo) int32 MagazineAmmo = 0;
    UPROPERTY(ReplicatedUsing=OnRep_Ammo) int32 ReserveAmmo = 0;
    UPROPERTY(ReplicatedUsing=OnRep_Reloading) bool bReloading = false;
    bool bAiming = false;
    bool bTriggerHeld = false;
    int32 ShotSequence = 0;
    double LastShotTime = -1000.0;
    FTimerHandle AutomaticFireTimer;
    FTimerHandle ReloadTimer;
    FBreakerShotResult LastShot;
    double LastCosmeticShotTime = -1000.0;

    const UBreakerWeaponDefinition* ResolveDefinition() const;
    void FireOnce();
    void FinishReload();
    bool CanFire() const;
    void GetViewPoint(FVector& OutLocation, FRotator& OutRotation) const;
};
