#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Combat/BreakerCombatTypes.h"
#include "Weapons/BreakerWeaponFeel.h"
#include "BreakerWeaponComponent.generated.h"

class UBreakerAttributeSet;
class UBreakerWeaponDefinition;

UENUM(BlueprintType)
enum class EBreakerWeaponArchetype : uint8
{
    Rifle,
    SMG,
    Sniper,
    Shotgun,
    Rocket,
    Count UMETA(Hidden)
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
    // Recoil pattern position of this shot: 0 is the first shot of a burst.
    // Replicated with the cosmetic event so every machine kicks identically.
    UPROPERTY(BlueprintReadOnly) int32 BurstShotIndex = 0;
    // Seed for this shot's small random recoil component.
    UPROPERTY(BlueprintReadOnly) int32 RecoilSeed = 0;
    UPROPERTY(BlueprintReadOnly) bool bAimedShot = false;
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
    UFUNCTION(BlueprintPure, Category="Weapon") FString GetArchetypeName() const;
    UFUNCTION(BlueprintPure, Category="Weapon|Debug") const FBreakerShotResult& GetLastShot() const { return LastShot; }
    UFUNCTION(BlueprintPure, Category="Weapon|Debug") float GetSecondsSinceLastShot() const;
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Weapon|Playtest") void ResetAmmunition();
    // Ammo economy (O2 placeholder): grants Fraction of each slot's
    // StartingReserveAmmo into that slot's reserve, capped at 2x starting
    // reserve so drops top a player up without making reserve meaningless.
    // Applies to the equipped weapon AND the stowed slot, so swapping is
    // never punished by an empty second gun.
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Weapon|Ammo") void AddReserveAmmoFraction(float Fraction);

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
    // Camera-relative offset of the placeholder weapon mesh, in centimetres:
    // X back toward the player, Y lateral. Presentation reads this; nothing in
    // the damage path does.
    UFUNCTION(BlueprintPure, Category="Weapon|Feel") FVector GetViewmodelLocationOffset() const;
    UFUNCTION(BlueprintPure, Category="Weapon|Feel") FRotator GetViewmodelRotationOffset() const;
    UFUNCTION(BlueprintPure, Category="Weapon|Feel") FBreakerRecoilProfile GetRecoilProfile() const { return ResolveRecoilProfile(); }

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
    bool bTriggerHeld = false;
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
    void ApplyShotFeel(const FBreakerShotResult& Shot);
    void TickRecoil(float DeltaSeconds);
    void UpdateFeelTickEnabled();
    void StoreActiveSlotAmmunition();
    void InitializeSlotAmmunition();
    void FireOnce();
    void FireProjectile(const UBreakerWeaponDefinition* Definition, const FVector& ViewLocation, const FRotator& ViewRotation, float Spread, int32 BurstIndex, int32 RecoilSeed);
    void ApplyBleedOnHit(const UBreakerWeaponDefinition* Definition, AActor* Target, const UBreakerAttributeSet* SourceAttributes);
    void FinishReload();
    void FinishSwap();
    bool CanFire() const;
    void GetViewPoint(FVector& OutLocation, FRotator& OutRotation) const;
};
