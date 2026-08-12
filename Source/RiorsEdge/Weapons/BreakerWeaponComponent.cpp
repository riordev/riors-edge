#include "Weapons/BreakerWeaponComponent.h"

#include "Attributes/BreakerAttributeSet.h"
#include "AbilitySystemInterface.h"
#include "AbilitySystemComponent.h"
#include "Combat/BreakerCombatComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "Weapons/BreakerWeaponDefinition.h"
#include "Weapons/BreakerWeaponMath.h"

namespace
{
    UBreakerWeaponDefinition* GetPrototypeDefinition()
    {
        static TObjectPtr<UBreakerWeaponDefinition> Prototype;
        if (!Prototype)
        {
            Prototype = NewObject<UBreakerWeaponDefinition>(GetTransientPackage(), TEXT("PrototypeRifleDefinition"));
            Prototype->AddToRoot();
        }
        return Prototype;
    }
}

UBreakerWeaponComponent::UBreakerWeaponComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(true);
}

void UBreakerWeaponComponent::BeginPlay()
{
    Super::BeginPlay();
    const UBreakerWeaponDefinition* Definition = ResolveDefinition();
    if (GetOwner() && GetOwner()->HasAuthority() && Definition)
    {
        MagazineAmmo = Definition->MagazineSize;
        ReserveAmmo = Definition->StartingReserveAmmo;
    }
}

void UBreakerWeaponComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(UBreakerWeaponComponent, MagazineAmmo);
    DOREPLIFETIME(UBreakerWeaponComponent, ReserveAmmo);
    DOREPLIFETIME(UBreakerWeaponComponent, bReloading);
}

const UBreakerWeaponDefinition* UBreakerWeaponComponent::ResolveDefinition() const
{
    return WeaponDefinition ? WeaponDefinition.Get() : GetPrototypeDefinition();
}

void UBreakerWeaponComponent::StartFire()
{
    if (!GetOwner() || !GetOwner()->HasAuthority())
    {
        ServerStartFire();
        return;
    }
    bTriggerHeld = true;
    FireOnce();
    const UBreakerWeaponDefinition* Definition = ResolveDefinition();
    if (Definition && Definition->bAutomatic)
    {
        GetWorld()->GetTimerManager().SetTimer(AutomaticFireTimer, this, &ThisClass::FireOnce,
            FBreakerWeaponMath::FireInterval(Definition->RoundsPerMinute), true);
    }
}

void UBreakerWeaponComponent::StopFire()
{
    if (!GetOwner() || !GetOwner()->HasAuthority())
    {
        ServerStopFire();
        return;
    }
    bTriggerHeld = false;
    GetWorld()->GetTimerManager().ClearTimer(AutomaticFireTimer);
}

void UBreakerWeaponComponent::StartReload()
{
    if (!GetOwner() || !GetOwner()->HasAuthority())
    {
        ServerStartReload();
        return;
    }
    const UBreakerWeaponDefinition* Definition = ResolveDefinition();
    if (!Definition || bReloading || MagazineAmmo >= Definition->MagazineSize || ReserveAmmo <= 0) return;
    StopFire();
    bReloading = true;
    OnReloadChanged.Broadcast(true);
    GetWorld()->GetTimerManager().SetTimer(ReloadTimer, this, &ThisClass::FinishReload, Definition->ReloadDuration, false);
}

void UBreakerWeaponComponent::SetAiming(bool bNewAiming)
{
    bAiming = bNewAiming;
    if (GetOwner() && !GetOwner()->HasAuthority())
    {
        ServerSetAiming(bNewAiming);
    }
}

void UBreakerWeaponComponent::ServerStartFire_Implementation() { StartFire(); }
void UBreakerWeaponComponent::ServerStopFire_Implementation() { StopFire(); }
void UBreakerWeaponComponent::ServerStartReload_Implementation() { StartReload(); }
void UBreakerWeaponComponent::ServerSetAiming_Implementation(bool bNewAiming) { bAiming = bNewAiming; }

bool UBreakerWeaponComponent::CanFire() const
{
    const UBreakerWeaponDefinition* Definition = ResolveDefinition();
    if (!Definition || bReloading || MagazineAmmo <= 0 || !GetWorld()) return false;
    return GetWorld()->GetTimeSeconds() - LastShotTime + UE_KINDA_SMALL_NUMBER >= FBreakerWeaponMath::FireInterval(Definition->RoundsPerMinute);
}

void UBreakerWeaponComponent::FireOnce()
{
    if (!CanFire()) return;
    const UBreakerWeaponDefinition* Definition = ResolveDefinition();
    if (!Definition) return;

    LastShotTime = GetWorld()->GetTimeSeconds();
    --MagazineAmmo;
    OnAmmoChanged.Broadcast(MagazineAmmo, ReserveAmmo);

    FVector ViewLocation;
    FRotator ViewRotation;
    GetViewPoint(ViewLocation, ViewRotation);
    const float Spread = bAiming ? Definition->AimSpreadDegrees : Definition->HipSpreadDegrees;
    const FVector Direction = FBreakerWeaponMath::ApplyConeSpread(ViewRotation.Vector(), Spread, ++ShotSequence);

    FBreakerShotResult Shot;
    Shot.bFired = true;
    Shot.TraceStart = ViewLocation;
    Shot.TraceEnd = ViewLocation + Direction * Definition->MaximumRange;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(BreakerWeaponTrace), true, GetOwner());
    FHitResult Hit;
    if (GetWorld()->LineTraceSingleByChannel(Hit, Shot.TraceStart, Shot.TraceEnd, ECC_GameTraceChannel2, Params))
    {
        Shot.bHit = true;
        Shot.HitActor = Hit.GetActor();
        Shot.ImpactPoint = Hit.ImpactPoint;
        Shot.TraceEnd = Hit.ImpactPoint;
        Shot.bWeakPoint = Hit.GetComponent() && Hit.GetComponent()->ComponentHasTag(TEXT("WeakPoint"));

        if (UBreakerCombatComponent* TargetCombat = Hit.GetActor() ? Hit.GetActor()->FindComponentByClass<UBreakerCombatComponent>() : nullptr)
        {
            const UBreakerAttributeSet* SourceAttributes = nullptr;
            if (const IAbilitySystemInterface* AbilityOwner = Cast<IAbilitySystemInterface>(GetOwner()))
            {
                if (const UAbilitySystemComponent* ASC = AbilityOwner->GetAbilitySystemComponent()) SourceAttributes = ASC->GetSet<UBreakerAttributeSet>();
            }
            FBreakerDamageRequest Damage;
            Damage.BaseDamage = Definition->Damage * FBreakerWeaponMath::DamageMultiplierAtDistance(Definition, Hit.Distance);
            Damage.DamageFamily = EBreakerDamageFamily::Physical;
            Damage.WeakPointMultiplier = Definition->WeakPointMultiplier;
            Damage.ArmorPenetration = Definition->ArmorPenetration;
            Damage.bWeakPointHit = Shot.bWeakPoint;
            Damage.CriticalChance = SourceAttributes ? SourceAttributes->GetCriticalChance() : 0.05f;
            Damage.CriticalMultiplier = SourceAttributes ? SourceAttributes->GetCriticalMultiplier() : 1.5f;
            Damage.SourceDamageMultiplier = SourceAttributes ? SourceAttributes->GetDamageMultiplier() : 1.0f;
            Damage.RandomSeed = HashCombine(GetTypeHash(GetOwner()), ShotSequence);
            Shot.DamageResult = TargetCombat->ReceiveDamage(Damage);
        }
    }
    MulticastShotCosmetics(Shot);

    if (MagazineAmmo <= 0 && ReserveAmmo > 0) StartReload();
}

void UBreakerWeaponComponent::FinishReload()
{
    const UBreakerWeaponDefinition* Definition = ResolveDefinition();
    if (!Definition) return;
    const int32 Needed = FMath::Max(0, Definition->MagazineSize - MagazineAmmo);
    const int32 Loaded = FMath::Min(Needed, ReserveAmmo);
    MagazineAmmo += Loaded;
    ReserveAmmo -= Loaded;
    bReloading = false;
    OnReloadChanged.Broadcast(false);
    OnAmmoChanged.Broadcast(MagazineAmmo, ReserveAmmo);
}

void UBreakerWeaponComponent::GetViewPoint(FVector& OutLocation, FRotator& OutRotation) const
{
    if (const APawn* Pawn = Cast<APawn>(GetOwner()))
    {
        if (const AController* Controller = Pawn->GetController())
        {
            Controller->GetPlayerViewPoint(OutLocation, OutRotation);
            return;
        }
    }
    if (GetOwner()) GetOwner()->GetActorEyesViewPoint(OutLocation, OutRotation);
}

void UBreakerWeaponComponent::MulticastShotCosmetics_Implementation(const FBreakerShotResult& Shot)
{
    OnShot.Broadcast(Shot);
}

void UBreakerWeaponComponent::OnRep_Ammo() { OnAmmoChanged.Broadcast(MagazineAmmo, ReserveAmmo); }
void UBreakerWeaponComponent::OnRep_Reloading() { OnReloadChanged.Broadcast(bReloading); }
