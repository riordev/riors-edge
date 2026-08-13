#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Combat/BreakerCombatTypes.h"
#include "BreakerWeaponComponent.generated.h"

class UBreakerWeaponDefinition;

UENUM(BlueprintType)
enum class EBreakerWeaponArchetype : uint8
{
    Rifle,
    Scattergun,
    Marksman
};

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
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FBreakerSwapEvent, bool, bSwapping, int32, SlotNumber);

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
    UFUNCTION(BlueprintCallable, Category="Weapon") void EquipArchetype(EBreakerWeaponArchetype NewArchetype);
    UFUNCTION(BlueprintCallable, Category="Weapon") void EquipSlot(int32 SlotNumber);
    UFUNCTION(BlueprintPure, Category="Weapon") bool IsReloading() const { return bReloading; }
    UFUNCTION(BlueprintPure, Category="Weapon") bool IsAiming() const { return bAiming; }
    UFUNCTION(BlueprintPure, Category="Weapon") int32 GetMagazineAmmo() const { return MagazineAmmo; }
    UFUNCTION(BlueprintPure, Category="Weapon") int32 GetReserveAmmo() const { return ReserveAmmo; }
    UFUNCTION(BlueprintPure, Category="Weapon") const UBreakerWeaponDefinition* GetDefinition() const { return WeaponDefinition; }
    UFUNCTION(BlueprintPure, Category="Weapon") EBreakerWeaponArchetype GetArchetype() const { return CurrentArchetype; }
    UFUNCTION(BlueprintPure, Category="Weapon") int32 GetCurrentSlot() const { return CurrentSlot; }
    UFUNCTION(BlueprintPure, Category="Weapon") bool IsSwapping() const { return bSwapping; }
    // Seconds since the last swap completed. Secondary "damage on swap-in"
    // affixes read this to decide whether their window is open.
    UFUNCTION(BlueprintPure, Category="Weapon") float GetSecondsSinceSwapIn() const;
    UFUNCTION(BlueprintPure, Category="Weapon") FString GetArchetypeName() const;
    UFUNCTION(BlueprintPure, Category="Weapon|Debug") const FBreakerShotResult& GetLastShot() const { return LastShot; }
    UFUNCTION(BlueprintPure, Category="Weapon|Debug") float GetSecondsSinceLastShot() const;
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Weapon|Playtest") void ResetAmmunition();

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
    int32 SlotOneMagazineAmmo = -1;
    int32 SlotOneReserveAmmo = -1;
    int32 SlotTwoMagazineAmmo = -1;
    int32 SlotTwoReserveAmmo = -1;
    bool bAiming = false;
    bool bTriggerHeld = false;
    int32 ShotSequence = 0;
    double LastShotTime = -1000.0;
    FTimerHandle AutomaticFireTimer;
    FTimerHandle ReloadTimer;
    FTimerHandle SwapTimer;
    double LastSwapInTime = -1000.0;
    FBreakerShotResult LastShot;
    double LastCosmeticShotTime = -1000.0;

    const UBreakerWeaponDefinition* ResolveDefinition() const;
    void StoreActiveSlotAmmunition();
    void InitializeSlotAmmunition();
    void FireOnce();
    void FinishReload();
    void FinishSwap();
    bool CanFire() const;
    void GetViewPoint(FVector& OutLocation, FRotator& OutRotation) const;
};
