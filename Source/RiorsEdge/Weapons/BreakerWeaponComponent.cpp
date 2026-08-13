#include "Weapons/BreakerWeaponComponent.h"

#include "Attributes/BreakerAttributeSet.h"
#include "AbilitySystemInterface.h"
#include "AbilitySystemComponent.h"
#include "Combat/BreakerCombatComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "Weapons/BreakerRocketProjectile.h"
#include "Weapons/BreakerWeaponDefinition.h"
#include "Weapons/BreakerWeaponMath.h"

namespace
{
    UBreakerWeaponDefinition* GetPrototypeDefinition(EBreakerWeaponArchetype Archetype)
    {
        static TObjectPtr<UBreakerWeaponDefinition> Prototypes[static_cast<int32>(EBreakerWeaponArchetype::Count)];
        const int32 Index = static_cast<int32>(Archetype);
        if (!Prototypes[Index])
        {
            const FName Names[] =
            {
                TEXT("PrototypeRifleDefinition"), TEXT("PrototypeSMGDefinition"), TEXT("PrototypeSniperDefinition"),
                TEXT("PrototypeShotgunDefinition"), TEXT("PrototypeRocketDefinition")
            };
            Prototypes[Index] = NewObject<UBreakerWeaponDefinition>(GetTransientPackage(), Names[Index]);
            Prototypes[Index]->AddToRoot();
            UBreakerWeaponDefinition* Definition = Prototypes[Index];
            switch (Archetype)
            {
            case EBreakerWeaponArchetype::SMG:
                Definition->WeaponId = TEXT("SMG");
                Definition->DisplayName = FText::FromString(TEXT("SMG"));
                Definition->Damage = 13.0f;
                Definition->WeakPointMultiplier = 1.5f;
                Definition->RoundsPerMinute = 900.0f;
                Definition->bAutomatic = true;
                Definition->HipSpreadDegrees = 2.0f;
                Definition->AimSpreadDegrees = 0.9f;
                Definition->MagazineSize = 35;
                Definition->StartingReserveAmmo = 175;
                Definition->ReloadDuration = 1.5f;
                Definition->FalloffStart = 1200.0f;
                Definition->FalloffEnd = 3500.0f;
                Definition->MinimumFalloffMultiplier = 0.4f;
                Definition->MaximumRange = 6000.0f;
                Definition->SwapInDuration = 0.35f;
                break;
            case EBreakerWeaponArchetype::Sniper:
                Definition->WeaponId = TEXT("Sniper");
                Definition->DisplayName = FText::FromString(TEXT("Sniper"));
                Definition->Damage = 72.0f;
                Definition->WeakPointMultiplier = 2.0f;
                Definition->RoundsPerMinute = 150.0f;
                Definition->bAutomatic = false;
                Definition->HipSpreadDegrees = 2.0f;
                Definition->AimSpreadDegrees = 0.05f;
                Definition->MagazineSize = 8;
                Definition->StartingReserveAmmo = 40;
                Definition->ReloadDuration = 2.3f;
                Definition->FalloffStart = 3500.0f;
                Definition->FalloffEnd = 9000.0f;
                Definition->MinimumFalloffMultiplier = 0.7f;
                Definition->MaximumRange = 15000.0f;
                Definition->SwapInDuration = 0.7f;
                break;
            case EBreakerWeaponArchetype::Shotgun:
                Definition->WeaponId = TEXT("Shotgun");
                Definition->DisplayName = FText::FromString(TEXT("Shotgun"));
                Definition->Damage = 10.0f;
                Definition->WeakPointMultiplier = 1.35f;
                Definition->RoundsPerMinute = 85.0f;
                Definition->bAutomatic = false;
                Definition->PelletsPerShot = 8;
                Definition->HipSpreadDegrees = 4.5f;
                Definition->AimSpreadDegrees = 3.0f;
                Definition->MagazineSize = 8;
                Definition->StartingReserveAmmo = 40;
                Definition->ReloadDuration = 2.2f;
                Definition->FalloffStart = 800.0f;
                Definition->FalloffEnd = 2500.0f;
                Definition->MinimumFalloffMultiplier = 0.25f;
                Definition->MaximumRange = 4000.0f;
                break;
            case EBreakerWeaponArchetype::Rocket:
                Definition->WeaponId = TEXT("Rocket");
                Definition->DisplayName = FText::FromString(TEXT("Rocket Launcher"));
                Definition->Damage = 90.0f;
                Definition->WeakPointMultiplier = 1.0f;
                Definition->RoundsPerMinute = 55.0f;
                Definition->bAutomatic = false;
                Definition->HipSpreadDegrees = 0.6f;
                Definition->AimSpreadDegrees = 0.2f;
                Definition->MagazineSize = 4;
                Definition->StartingReserveAmmo = 16;
                Definition->ReloadDuration = 2.8f;
                Definition->MaximumRange = 12000.0f;
                Definition->SwapInDuration = 0.8f;
                Definition->bProjectile = true;
                Definition->ProjectileSpeed = 3200.0f;
                Definition->ExplosionRadius = 350.0f;
                break;
            default:
                Definition->DisplayName = FText::FromString(TEXT("Rifle"));
                break;
            }
        }
        return Prototypes[Index];
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
    if (GetOwner() && GetOwner()->HasAuthority())
    {
        InitializeSlotAmmunition();
        MagazineAmmo = SlotOneMagazineAmmo;
        ReserveAmmo = SlotOneReserveAmmo;
    }
}

void UBreakerWeaponComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(UBreakerWeaponComponent, MagazineAmmo);
    DOREPLIFETIME(UBreakerWeaponComponent, ReserveAmmo);
    DOREPLIFETIME(UBreakerWeaponComponent, bReloading);
    DOREPLIFETIME(UBreakerWeaponComponent, CurrentArchetype);
    DOREPLIFETIME(UBreakerWeaponComponent, CurrentSlot);
    DOREPLIFETIME(UBreakerWeaponComponent, bSwapping);
    DOREPLIFETIME(UBreakerWeaponComponent, SlotOneArchetype);
    DOREPLIFETIME(UBreakerWeaponComponent, SlotTwoArchetype);
}

const UBreakerWeaponDefinition* UBreakerWeaponComponent::ResolveDefinition() const
{
    return WeaponDefinition ? WeaponDefinition.Get() : GetPrototypeDefinition(CurrentArchetype);
}

void UBreakerWeaponComponent::EquipArchetype(EBreakerWeaponArchetype NewArchetype)
{
    if (CurrentArchetype == NewArchetype) return;
    StopFire();
    if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(ReloadTimer);
    CurrentArchetype = NewArchetype;
    bReloading = false;
    const UBreakerWeaponDefinition* Definition = ResolveDefinition();
    MagazineAmmo = Definition ? Definition->MagazineSize : 0;
    ReserveAmmo = Definition ? Definition->StartingReserveAmmo : 0;
    OnReloadChanged.Broadcast(false);
    OnAmmoChanged.Broadcast(MagazineAmmo, ReserveAmmo);
}

void UBreakerWeaponComponent::InitializeSlotAmmunition()
{
    if (SlotOneMagazineAmmo < 0)
    {
        const UBreakerWeaponDefinition* SlotOne = GetPrototypeDefinition(SlotOneArchetype);
        SlotOneMagazineAmmo = SlotOne->MagazineSize;
        SlotOneReserveAmmo = SlotOne->StartingReserveAmmo;
    }
    if (SlotTwoMagazineAmmo < 0)
    {
        const UBreakerWeaponDefinition* SlotTwo = GetPrototypeDefinition(SlotTwoArchetype);
        SlotTwoMagazineAmmo = SlotTwo->MagazineSize;
        SlotTwoReserveAmmo = SlotTwo->StartingReserveAmmo;
    }
}

void UBreakerWeaponComponent::StoreActiveSlotAmmunition()
{
    if (CurrentSlot == 1)
    {
        SlotOneMagazineAmmo = MagazineAmmo;
        SlotOneReserveAmmo = ReserveAmmo;
    }
    else
    {
        SlotTwoMagazineAmmo = MagazineAmmo;
        SlotTwoReserveAmmo = ReserveAmmo;
    }
}

void UBreakerWeaponComponent::EquipSlot(int32 SlotNumber)
{
    SlotNumber = FMath::Clamp(SlotNumber, 1, 2);
    if (GetOwner() && !GetOwner()->HasAuthority())
    {
        ServerEquipSlot(SlotNumber);
        return;
    }
    if (CurrentSlot == SlotNumber) return;

    StopFire();
    if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(ReloadTimer);
    InitializeSlotAmmunition();
    StoreActiveSlotAmmunition();
    CurrentSlot = SlotNumber;
    CurrentArchetype = CurrentSlot == 1 ? SlotOneArchetype : SlotTwoArchetype;
    MagazineAmmo = CurrentSlot == 1 ? SlotOneMagazineAmmo : SlotTwoMagazineAmmo;
    ReserveAmmo = CurrentSlot == 1 ? SlotOneReserveAmmo : SlotTwoReserveAmmo;
    bReloading = false;
    OnReloadChanged.Broadcast(false);
    OnAmmoChanged.Broadcast(MagazineAmmo, ReserveAmmo);

    // Swap tempo: the incoming weapon is unusable for its SwapInDuration.
    // Swap speed affixes will scale this window; on-swap-in damage windows
    // read GetSecondsSinceSwapIn once it closes.
    const UBreakerWeaponDefinition* Incoming = ResolveDefinition();
    const float SwapDuration = Incoming ? Incoming->SwapInDuration : 0.5f;
    bSwapping = true;
    OnSwapChanged.Broadcast(true, CurrentSlot);
    if (SwapDuration > 0.0f && GetWorld())
    {
        GetWorld()->GetTimerManager().SetTimer(SwapTimer, this, &ThisClass::FinishSwap, SwapDuration, false);
    }
    else
    {
        FinishSwap();
    }
}

void UBreakerWeaponComponent::FinishSwap()
{
    bSwapping = false;
    LastSwapInTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
    OnSwapChanged.Broadcast(false, CurrentSlot);
}

float UBreakerWeaponComponent::GetSecondsSinceSwapIn() const
{
    return GetWorld() ? static_cast<float>(GetWorld()->GetTimeSeconds() - LastSwapInTime) : BIG_NUMBER;
}

void UBreakerWeaponComponent::ServerEquipSlot_Implementation(int32 SlotNumber)
{
    EquipSlot(SlotNumber);
}

void UBreakerWeaponComponent::SetSlotArchetype(int32 SlotNumber, EBreakerWeaponArchetype NewArchetype)
{
    SlotNumber = FMath::Clamp(SlotNumber, 1, 2);
    if (GetOwner() && !GetOwner()->HasAuthority())
    {
        ServerSetSlotArchetype(SlotNumber, NewArchetype);
        return;
    }
    if (GetSlotArchetype(SlotNumber) == NewArchetype) return;

    const UBreakerWeaponDefinition* Definition = GetPrototypeDefinition(NewArchetype);
    if (SlotNumber == 1)
    {
        SlotOneArchetype = NewArchetype;
        SlotOneMagazineAmmo = Definition->MagazineSize;
        SlotOneReserveAmmo = Definition->StartingReserveAmmo;
    }
    else
    {
        SlotTwoArchetype = NewArchetype;
        SlotTwoMagazineAmmo = Definition->MagazineSize;
        SlotTwoReserveAmmo = Definition->StartingReserveAmmo;
    }
    if (CurrentSlot == SlotNumber)
    {
        StopFire();
        if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(ReloadTimer);
        bReloading = false;
        CurrentArchetype = NewArchetype;
        MagazineAmmo = Definition->MagazineSize;
        ReserveAmmo = Definition->StartingReserveAmmo;
        OnReloadChanged.Broadcast(false);
        OnAmmoChanged.Broadcast(MagazineAmmo, ReserveAmmo);
    }
}

void UBreakerWeaponComponent::ServerSetSlotArchetype_Implementation(int32 SlotNumber, EBreakerWeaponArchetype NewArchetype)
{
    SetSlotArchetype(SlotNumber, NewArchetype);
}

FString UBreakerWeaponComponent::GetArchetypeName() const
{
    switch (CurrentArchetype)
    {
        case EBreakerWeaponArchetype::SMG: return TEXT("SMG");
        case EBreakerWeaponArchetype::Sniper: return TEXT("SNIPER");
        case EBreakerWeaponArchetype::Shotgun: return TEXT("SHOTGUN");
        case EBreakerWeaponArchetype::Rocket: return TEXT("ROCKET");
        default: return TEXT("RIFLE");
    }
}

void UBreakerWeaponComponent::StartFire()
{
    if (!GetOwner()) return;
    if (!GetOwner()->HasAuthority())
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
    if (!GetOwner()) return;
    if (!GetOwner()->HasAuthority())
    {
        ServerStopFire();
        return;
    }
    bTriggerHeld = false;
    GetWorld()->GetTimerManager().ClearTimer(AutomaticFireTimer);
}

void UBreakerWeaponComponent::StartReload()
{
    if (!GetOwner()) return;
    if (!GetOwner()->HasAuthority())
    {
        ServerStartReload();
        return;
    }
    const UBreakerWeaponDefinition* Definition = ResolveDefinition();
    if (!Definition || bReloading || bSwapping || MagazineAmmo >= Definition->MagazineSize || ReserveAmmo <= 0) return;
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
    if (!Definition || bReloading || bSwapping || MagazineAmmo <= 0 || !GetWorld()) return false;
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

    if (Definition->bProjectile)
    {
        FireProjectile(Definition, ViewLocation, ViewRotation, Spread);
        if (MagazineAmmo <= 0 && ReserveAmmo > 0) StartReload();
        return;
    }

    FBreakerShotResult Shot;
    Shot.bFired = true;
    Shot.TraceStart = ViewLocation;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(BreakerWeaponTrace), true, GetOwner());
    const int32 PelletCount = FMath::Max(1, Definition->PelletsPerShot);
    for (int32 PelletIndex = 0; PelletIndex < PelletCount; ++PelletIndex)
    {
        const FVector Direction = FBreakerWeaponMath::ApplyConeSpread(ViewRotation.Vector(), Spread, ++ShotSequence);
        const FVector PelletEnd = ViewLocation + Direction * Definition->MaximumRange;
        if (PelletIndex == 0) Shot.TraceEnd = PelletEnd;
        FHitResult Hit;
        if (!GetWorld()->LineTraceSingleByChannel(Hit, ViewLocation, PelletEnd, ECC_GameTraceChannel2, Params)) continue;

        Shot.bHit = true;
        Shot.HitActor = Hit.GetActor();
        Shot.ImpactPoint = Hit.ImpactPoint;
        Shot.TraceEnd = Hit.ImpactPoint;
        const bool bPelletWeakPoint = Hit.GetComponent() && Hit.GetComponent()->ComponentHasTag(TEXT("WeakPoint"));
        Shot.bWeakPoint |= bPelletWeakPoint;

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
            Damage.bWeakPointHit = bPelletWeakPoint;
            Damage.CriticalChance = SourceAttributes ? SourceAttributes->GetCriticalChance() : 0.05f;
            Damage.CriticalMultiplier = SourceAttributes ? SourceAttributes->GetCriticalMultiplier() : 1.5f;
            Damage.SourceDamageMultiplier = SourceAttributes ? SourceAttributes->GetDamageMultiplier() : 1.0f;
            Damage.RandomSeed = HashCombine(GetTypeHash(GetOwner()), ShotSequence);
            Damage.SourceLocation = GetOwner()->GetActorLocation();
            Damage.bHasSourceLocation = true;
            const FBreakerDamageResult PelletDamage = TargetCombat->ReceiveDamage(Damage);
            Shot.DamageResult.RawDamage += PelletDamage.RawDamage;
            Shot.DamageResult.MitigatedDamage += PelletDamage.MitigatedDamage;
            Shot.DamageResult.ShieldDamage += PelletDamage.ShieldDamage;
            Shot.DamageResult.HealthDamage += PelletDamage.HealthDamage;
            Shot.DamageResult.RemainingShield = PelletDamage.RemainingShield;
            Shot.DamageResult.RemainingHealth = PelletDamage.RemainingHealth;
            Shot.DamageResult.bCritical |= PelletDamage.bCritical;
            Shot.DamageResult.bWeakPoint |= PelletDamage.bWeakPoint;
            Shot.DamageResult.bShieldBroken |= PelletDamage.bShieldBroken;
            Shot.DamageResult.bKilled |= PelletDamage.bKilled;
        }
    }
    MulticastShotCosmetics(Shot);

    if (MagazineAmmo <= 0 && ReserveAmmo > 0) StartReload();
}

void UBreakerWeaponComponent::FireProjectile(const UBreakerWeaponDefinition* Definition, const FVector& ViewLocation, const FRotator& ViewRotation, float Spread)
{
    const FVector Direction = FBreakerWeaponMath::ApplyConeSpread(ViewRotation.Vector(), Spread, ++ShotSequence);

    const UBreakerAttributeSet* SourceAttributes = nullptr;
    if (const IAbilitySystemInterface* AbilityOwner = Cast<IAbilitySystemInterface>(GetOwner()))
    {
        if (const UAbilitySystemComponent* ASC = AbilityOwner->GetAbilitySystemComponent()) SourceAttributes = ASC->GetSet<UBreakerAttributeSet>();
    }
    FBreakerDamageRequest Damage;
    Damage.BaseDamage = Definition->Damage;
    Damage.DamageFamily = EBreakerDamageFamily::Physical;
    Damage.WeakPointMultiplier = 1.0f;
    Damage.ArmorPenetration = Definition->ArmorPenetration;
    Damage.CriticalChance = SourceAttributes ? SourceAttributes->GetCriticalChance() : 0.05f;
    Damage.CriticalMultiplier = SourceAttributes ? SourceAttributes->GetCriticalMultiplier() : 1.5f;
    Damage.SourceDamageMultiplier = SourceAttributes ? SourceAttributes->GetDamageMultiplier() : 1.0f;
    Damage.RandomSeed = HashCombine(GetTypeHash(GetOwner()), ShotSequence);

    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    Params.Owner = GetOwner();
    Params.Instigator = Cast<APawn>(GetOwner());
    // Spawn ahead of the view so the rocket clears the shooter's capsule.
    const FVector SpawnLocation = ViewLocation + Direction * 80.0f;
    if (ABreakerRocketProjectile* Rocket = GetWorld()->SpawnActor<ABreakerRocketProjectile>(ABreakerRocketProjectile::StaticClass(), SpawnLocation, Direction.Rotation(), Params))
    {
        Rocket->InitializeRocket(Damage, Definition->ProjectileSpeed, Definition->ExplosionRadius);
    }

    FBreakerShotResult Shot;
    Shot.bFired = true;
    Shot.TraceStart = ViewLocation;
    Shot.TraceEnd = SpawnLocation + Direction * 400.0f;
    MulticastShotCosmetics(Shot);
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
    LastShot = Shot;
    LastCosmeticShotTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
    OnShot.Broadcast(Shot);
}

float UBreakerWeaponComponent::GetSecondsSinceLastShot() const
{
    return GetWorld() ? static_cast<float>(GetWorld()->GetTimeSeconds() - LastCosmeticShotTime) : BIG_NUMBER;
}

void UBreakerWeaponComponent::ResetAmmunition()
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;
    StopFire();
    GetWorld()->GetTimerManager().ClearTimer(ReloadTimer);
    SlotOneMagazineAmmo = -1;
    SlotTwoMagazineAmmo = -1;
    InitializeSlotAmmunition();
    MagazineAmmo = CurrentSlot == 1 ? SlotOneMagazineAmmo : SlotTwoMagazineAmmo;
    ReserveAmmo = CurrentSlot == 1 ? SlotOneReserveAmmo : SlotTwoReserveAmmo;
    bReloading = false;
    OnReloadChanged.Broadcast(false);
    OnAmmoChanged.Broadcast(MagazineAmmo, ReserveAmmo);
}

void UBreakerWeaponComponent::OnRep_Ammo() { OnAmmoChanged.Broadcast(MagazineAmmo, ReserveAmmo); }
void UBreakerWeaponComponent::OnRep_Reloading() { OnReloadChanged.Broadcast(bReloading); }
void UBreakerWeaponComponent::OnRep_Swapping()
{
    if (!bSwapping) LastSwapInTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
    OnSwapChanged.Broadcast(bSwapping, CurrentSlot);
}
