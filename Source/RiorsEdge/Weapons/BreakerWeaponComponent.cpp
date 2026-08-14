#include "Weapons/BreakerWeaponComponent.h"

#include "Abilities/BreakerAbilityStateComponent.h"
#include "Abilities/BreakerAbility_Lead.h"
#include "Attributes/BreakerAttributeSet.h"
#include "AbilitySystemInterface.h"
#include "AbilitySystemComponent.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerStatusComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Items/BreakerEquipmentComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "Weapons/BreakerRocketProjectile.h"
#include "Weapons/BreakerWeaponDefinition.h"
#include "Weapons/BreakerWeaponFeel.h"
#include "Weapons/BreakerWeaponMath.h"

namespace
{
    // Salts the shared shot seed so the bleed roll never correlates with the
    // spread or critical rolls drawn from the same shot sequence.
    constexpr uint32 BreakerBleedSalt = 0x51ED0000u;

    // Recoil belongs in the archetype table beside cadence, spread, falloff and
    // damage, so the five weapons kick like five weapons. Every number here is
    // an O2 PLACEHOLDER. The struct's own defaults are the rifle; each case
    // states only what makes that archetype different.
    FBreakerRecoilProfile ArchetypeRecoilProfile(EBreakerWeaponArchetype Archetype)
    {
        FBreakerRecoilProfile Profile;
        switch (Archetype)
        {
        case EBreakerWeaponArchetype::SMG:
            // Buzzy: barely moves per shot, but 900 RPM stacks it fast and it
            // wanders sideways more than it climbs. Recovers quickly.
            Profile.VerticalKickDegrees = 0.26f;              // O2 PLACEHOLDER
            Profile.HorizontalKickDegrees = 0.24f;            // O2 PLACEHOLDER
            Profile.HorizontalPatternPeriod = 5;              // O2 PLACEHOLDER
            Profile.VerticalRandomFraction = 0.18f;           // O2 PLACEHOLDER
            Profile.HorizontalRandomDegrees = 0.09f;          // O2 PLACEHOLDER
            Profile.ClimbRampShots = 9.0f;                    // O2 PLACEHOLDER
            Profile.ClimbRampMultiplier = 2.0f;               // O2 PLACEHOLDER
            Profile.MaxVerticalDegrees = 8.0f;                // O2 PLACEHOLDER
            Profile.MaxHorizontalDegrees = 4.5f;              // O2 PLACEHOLDER
            Profile.AimRecoilMultiplier = 0.72f;              // O2 PLACEHOLDER
            Profile.RecoveryDelaySeconds = 0.06f;             // O2 PLACEHOLDER
            Profile.RecoveryInterpSpeed = 12.0f;              // O2 PLACEHOLDER
            Profile.RecoveryConstantDegreesPerSecond = 20.0f; // O2 PLACEHOLDER
            Profile.BloomPerShotDegrees = 0.11f;              // O2 PLACEHOLDER
            Profile.MaxBloomDegrees = 2.4f;                   // O2 PLACEHOLDER
            Profile.BloomRecoveryDegreesPerSecond = 3.0f;     // O2 PLACEHOLDER
            Profile.AimBloomMultiplier = 0.5f;                // O2 PLACEHOLDER
            Profile.BurstResetSeconds = 0.28f;                // O2 PLACEHOLDER
            Profile.ViewmodelKickUnits = 2.0f;                // O2 PLACEHOLDER
            Profile.ViewmodelKickLateralUnits = 1.0f;         // O2 PLACEHOLDER
            Profile.ViewmodelKickPitchDegrees = 1.6f;         // O2 PLACEHOLDER
            Profile.ViewmodelSpringStiffness = 320.0f;        // O2 PLACEHOLDER
            // Fastest into the sights and the least punished for running with
            // them up: the SMG is the one weapon that is allowed to be a
            // run-and-gun ADS weapon.
            Profile.AimInSeconds = 0.14f;                     // O2 PLACEHOLDER
            Profile.MoveSpreadDegrees = 0.30f;                // O2 PLACEHOLDER
            Profile.AimMoveSpreadMultiplier = 1.8f;           // O2 PLACEHOLDER
            break;
        case EBreakerWeaponArchetype::Sniper:
            // One enormous, slow kick that has to be re-aimed rather than
            // ridden. ADS cuts it hardest: the scope is the reason to use it.
            Profile.VerticalKickDegrees = 2.8f;               // O2 PLACEHOLDER
            Profile.HorizontalKickDegrees = 0.45f;            // O2 PLACEHOLDER
            Profile.HorizontalPatternPeriod = 3;              // O2 PLACEHOLDER
            Profile.VerticalRandomFraction = 0.06f;           // O2 PLACEHOLDER
            Profile.HorizontalRandomDegrees = 0.12f;          // O2 PLACEHOLDER
            Profile.ClimbRampShots = 3.0f;                    // O2 PLACEHOLDER
            Profile.ClimbRampMultiplier = 1.15f;              // O2 PLACEHOLDER
            Profile.MaxVerticalDegrees = 9.0f;                // O2 PLACEHOLDER
            Profile.MaxHorizontalDegrees = 2.5f;              // O2 PLACEHOLDER
            Profile.AimRecoilMultiplier = 0.55f;              // O2 PLACEHOLDER
            Profile.RecoveryDelaySeconds = 0.2f;              // O2 PLACEHOLDER
            Profile.RecoveryInterpSpeed = 5.0f;               // O2 PLACEHOLDER
            Profile.RecoveryConstantDegreesPerSecond = 9.0f;  // O2 PLACEHOLDER
            Profile.BloomPerShotDegrees = 0.6f;               // O2 PLACEHOLDER
            Profile.MaxBloomDegrees = 3.0f;                   // O2 PLACEHOLDER
            Profile.BloomRecoveryDegreesPerSecond = 1.6f;     // O2 PLACEHOLDER
            Profile.AimBloomMultiplier = 0.35f;               // O2 PLACEHOLDER
            Profile.BurstResetSeconds = 0.9f;                 // O2 PLACEHOLDER
            Profile.ViewmodelKickUnits = 9.0f;                // O2 PLACEHOLDER
            Profile.ViewmodelKickLateralUnits = 1.6f;         // O2 PLACEHOLDER
            Profile.ViewmodelKickPitchDegrees = 6.5f;         // O2 PLACEHOLDER
            Profile.ViewmodelSpringStiffness = 150.0f;        // O2 PLACEHOLDER
            Profile.ViewmodelSpringDamping = 17.0f;           // O2 PLACEHOLDER
            Profile.AimViewmodelMultiplier = 0.5f;            // O2 PLACEHOLDER
            // Slowest scope in the table and the harshest movement penalty:
            // the sniper is the weapon that must be PLANTED, and hip firing
            // one is deliberately close to useless.
            Profile.AimInSeconds = 0.38f;                     // O2 PLACEHOLDER
            Profile.MoveSpreadDegrees = 1.10f;                // O2 PLACEHOLDER
            Profile.AimMoveSpreadMultiplier = 3.0f;           // O2 PLACEHOLDER
            break;
        case EBreakerWeaponArchetype::Shotgun:
            // A shove. Note FirstShotSpreadMultiplier stays at 1.0: the pellet
            // cone IS the shotgun, and zeroing it would turn it into a slug.
            Profile.VerticalKickDegrees = 1.9f;               // O2 PLACEHOLDER
            Profile.HorizontalKickDegrees = 0.7f;             // O2 PLACEHOLDER
            Profile.HorizontalPatternPeriod = 4;              // O2 PLACEHOLDER
            Profile.VerticalRandomFraction = 0.14f;           // O2 PLACEHOLDER
            Profile.HorizontalRandomDegrees = 0.25f;          // O2 PLACEHOLDER
            Profile.ClimbRampShots = 4.0f;                    // O2 PLACEHOLDER
            Profile.ClimbRampMultiplier = 1.3f;               // O2 PLACEHOLDER
            Profile.MaxVerticalDegrees = 8.5f;                // O2 PLACEHOLDER
            Profile.MaxHorizontalDegrees = 3.5f;              // O2 PLACEHOLDER
            Profile.AimRecoilMultiplier = 0.75f;              // O2 PLACEHOLDER
            Profile.RecoveryDelaySeconds = 0.14f;             // O2 PLACEHOLDER
            Profile.RecoveryInterpSpeed = 6.5f;               // O2 PLACEHOLDER
            Profile.RecoveryConstantDegreesPerSecond = 13.0f; // O2 PLACEHOLDER
            Profile.FirstShotSpreadMultiplier = 1.0f;         // O2 PLACEHOLDER
            Profile.BloomPerShotDegrees = 0.4f;               // O2 PLACEHOLDER
            Profile.MaxBloomDegrees = 2.0f;                   // O2 PLACEHOLDER
            Profile.BloomRecoveryDegreesPerSecond = 2.4f;     // O2 PLACEHOLDER
            Profile.BurstResetSeconds = 0.6f;                 // O2 PLACEHOLDER
            Profile.ViewmodelKickUnits = 8.0f;                // O2 PLACEHOLDER
            Profile.ViewmodelKickLateralUnits = 1.4f;         // O2 PLACEHOLDER
            Profile.ViewmodelKickPitchDegrees = 5.2f;         // O2 PLACEHOLDER
            Profile.ViewmodelSpringStiffness = 180.0f;        // O2 PLACEHOLDER
            Profile.ViewmodelSpringDamping = 19.0f;           // O2 PLACEHOLDER
            // Smallest movement penalty in the table. Hip firing a shotgun
            // while strafing is the thing it is FOR, and its own 4.5 degree
            // pellet cone dwarfs a quarter-degree of movement anyway.
            Profile.AimInSeconds = 0.16f;                     // O2 PLACEHOLDER
            Profile.MoveSpreadDegrees = 0.25f;                // O2 PLACEHOLDER
            Profile.AimMoveSpreadMultiplier = 2.0f;           // O2 PLACEHOLDER
            break;
        case EBreakerWeaponArchetype::Rocket:
            // Heaviest single kick in the table and the slowest settle. Almost
            // no sideways component: it is mass, not muzzle climb.
            Profile.VerticalKickDegrees = 2.4f;               // O2 PLACEHOLDER
            Profile.HorizontalKickDegrees = 0.3f;             // O2 PLACEHOLDER
            Profile.HorizontalPatternPeriod = 2;              // O2 PLACEHOLDER
            Profile.VerticalRandomFraction = 0.05f;           // O2 PLACEHOLDER
            Profile.HorizontalRandomDegrees = 0.06f;          // O2 PLACEHOLDER
            Profile.ClimbRampShots = 2.0f;                    // O2 PLACEHOLDER
            Profile.ClimbRampMultiplier = 1.1f;               // O2 PLACEHOLDER
            Profile.MaxVerticalDegrees = 9.0f;                // O2 PLACEHOLDER
            Profile.MaxHorizontalDegrees = 2.0f;              // O2 PLACEHOLDER
            Profile.AimRecoilMultiplier = 0.7f;               // O2 PLACEHOLDER
            Profile.RecoveryDelaySeconds = 0.22f;             // O2 PLACEHOLDER
            Profile.RecoveryInterpSpeed = 4.5f;               // O2 PLACEHOLDER
            Profile.RecoveryConstantDegreesPerSecond = 8.0f;  // O2 PLACEHOLDER
            Profile.BloomPerShotDegrees = 0.25f;              // O2 PLACEHOLDER
            Profile.MaxBloomDegrees = 1.2f;                   // O2 PLACEHOLDER
            Profile.BloomRecoveryDegreesPerSecond = 1.2f;     // O2 PLACEHOLDER
            Profile.BurstResetSeconds = 0.8f;                 // O2 PLACEHOLDER
            Profile.ViewmodelKickUnits = 10.0f;               // O2 PLACEHOLDER
            Profile.ViewmodelKickLateralUnits = 1.2f;         // O2 PLACEHOLDER
            Profile.ViewmodelKickPitchDegrees = 6.8f;         // O2 PLACEHOLDER
            Profile.ViewmodelSpringStiffness = 140.0f;        // O2 PLACEHOLDER
            Profile.ViewmodelSpringDamping = 16.0f;           // O2 PLACEHOLDER
            Profile.AimInSeconds = 0.30f;                     // O2 PLACEHOLDER
            Profile.MoveSpreadDegrees = 0.30f;                // O2 PLACEHOLDER
            break;
        default:
            // Rifle: the struct defaults. A learnable climb with a gentle
            // sideways sway, settling in about a third of a second.
            break;
        }
        return Profile;
    }

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
                Definition->FalloffStart = 1800.0f;   // O2 PLACEHOLDER
                Definition->FalloffEnd = 4500.0f;     // O2 PLACEHOLDER
                Definition->MinimumFalloffMultiplier = 0.58f; // O2 PLACEHOLDER
                Definition->MaximumRange = 6000.0f;
                Definition->SwapInDuration = 0.35f;
                Definition->BleedChance = 0.25f;
                Definition->BleedDamagePerTick = 6.0f;
                Definition->BleedDuration = 3.0f;
                Definition->BleedTickInterval = 0.5f;
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
                // The sniper barely falls off at all: 12% across sixty metres.
                Definition->FalloffStart = 5000.0f;   // O2 PLACEHOLDER
                Definition->FalloffEnd = 11000.0f;    // O2 PLACEHOLDER
                Definition->MinimumFalloffMultiplier = 0.88f; // O2 PLACEHOLDER
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
                // Still the steepest curve in the table by a wide margin —
                // 3.5% per metre against the rifle's 0.67% — because falling
                // off hard is what a shotgun IS. It just starts a little
                // further out and bottoms out a little higher.
                Definition->FalloffStart = 1100.0f;   // O2 PLACEHOLDER
                Definition->FalloffEnd = 2800.0f;     // O2 PLACEHOLDER
                Definition->MinimumFalloffMultiplier = 0.40f; // O2 PLACEHOLDER
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
            Definition->Recoil = ArchetypeRecoilProfile(Archetype);
        }
        return Prototypes[Index];
    }
}

UBreakerWeaponComponent::UBreakerWeaponComponent()
{
    // Ticks only while recoil, bloom, or the viewmodel spring have work left;
    // UpdateFeelTickEnabled switches it off again the moment everything is at
    // rest, so an idle weapon still costs nothing.
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = false;
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

int32 UBreakerWeaponComponent::GetEquippedItemLevel() const
{
    // The loadout slot number and the equipment slot correspond POSITIONALLY:
    // weapon slot 1 is the Primary equipment slot, slot 2 the Secondary. That
    // is the only correspondence that exists today — an FBreakerItemInstance
    // carries no weapon archetype, so which of the five guns a Primary item IS
    // remains unanswered and is deliberately not invented here. Item level is
    // the part the two layers already agree on, so item level is what crosses.
    const AActor* Owner = GetOwner();
    const UBreakerEquipmentComponent* Equipment = Owner ? Owner->FindComponentByClass<UBreakerEquipmentComponent>() : nullptr;
    if (Equipment)
    {
        const EBreakerEquipSlot Slot = (CurrentSlot == 2) ? EBreakerEquipSlot::Secondary : EBreakerEquipSlot::Primary;
        FBreakerItemInstance Item;
        if (Equipment->GetEquippedItem(Slot, Item) && Item.IsValid())
        {
            return FMath::Max(1, Item.ItemLevel);
        }
    }
    // Nothing equipped: the archetype table's own level. A clean clone with no
    // loadout is a level-1 weapon, which is a scalar of exactly 1.0.
    return FMath::Max(1, UnequippedItemLevel);
}

float UBreakerWeaponComponent::GetItemLevelDamageScalar() const
{
    return FBreakerWeaponMath::ItemLevelDamageScalar(GetEquippedItemLevel(), ItemLevelDamageGrowth);
}

float UBreakerWeaponComponent::GetScaledBaseDamage() const
{
    const UBreakerWeaponDefinition* Definition = ResolveDefinition();
    if (!Definition) return 0.0f;
    return FBreakerWeaponMath::WeaponBaseDamage(Definition->Damage, GetEquippedItemLevel(), ItemLevelDamageGrowth);
}

FBreakerRecoilProfile UBreakerWeaponComponent::ResolveRecoilProfile() const
{
    // Component override (editor-tunable per instance, no recompile) beats the
    // definition asset, which beats the archetype fallback table.
    FBreakerRecoilProfile Profile;
    if (const FBreakerRecoilProfile* Override = RecoilOverrides.Find(CurrentArchetype))
    {
        Profile = *Override;
    }
    else if (const UBreakerWeaponDefinition* Definition = ResolveDefinition())
    {
        Profile = Definition->Recoil;
    }
    else
    {
        Profile = ArchetypeRecoilProfile(CurrentArchetype);
    }

    // A single global trim over aim kick only. Recovery, bloom, and viewmodel
    // keep their authored values so scaling the kick cannot desynchronise the
    // settle from the climb.
    const float Scale = FMath::Max(0.0f, RecoilScale);
    if (!FMath::IsNearlyEqual(Scale, 1.0f))
    {
        Profile.VerticalKickDegrees *= Scale;
        Profile.HorizontalKickDegrees *= Scale;
        Profile.HorizontalRandomDegrees *= Scale;
    }
    return Profile;
}

float UBreakerWeaponComponent::GetAimAlpha() const
{
    if (!bAiming) return 0.0f;
    const float AimIn = ResolveRecoilProfile().AimInSeconds;
    if (AimIn <= 0.0f || !GetWorld()) return 1.0f;
    return FMath::Clamp(static_cast<float>(GetWorld()->GetTimeSeconds() - AimStartTime) / AimIn, 0.0f, 1.0f);
}

float UBreakerWeaponComponent::GetSpeedFraction() const
{
    if (!GetOwner() || MoveSpreadReferenceSpeed <= 0.0f) return 0.0f;
    return FMath::Clamp(static_cast<float>(GetOwner()->GetVelocity().Size2D()) / MoveSpreadReferenceSpeed, 0.0f, 1.0f);
}

float UBreakerWeaponComponent::GetMovementSpreadDegrees() const
{
    return FBreakerWeaponFeel::MovementSpreadDegrees(ResolveRecoilProfile(), GetSpeedFraction(), GetAimAlpha());
}

float UBreakerWeaponComponent::GetNextShotSpreadDegrees() const
{
    const UBreakerWeaponDefinition* Definition = ResolveDefinition();
    if (!Definition) return 0.0f;
    const float Alpha = GetAimAlpha();
    // Partway into ADS is partway to the aimed cone, not the whole thing.
    const float BaseSpread = FMath::Lerp(Definition->HipSpreadDegrees, Definition->AimSpreadDegrees, Alpha);
    const FBreakerRecoilProfile Profile = ResolveRecoilProfile();
    const float Movement = FBreakerWeaponFeel::MovementSpreadDegrees(Profile, GetSpeedFraction(), Alpha);
    return FBreakerWeaponFeel::EffectiveSpreadDegrees(Profile, BaseSpread, BloomDegrees, BurstShotIndex, Movement);
}

FVector UBreakerWeaponComponent::GetViewmodelLocationOffset() const
{
    // -X is toward the player: the weapon is driven back into the shoulder.
    return FVector(-Viewmodel.BackOffset, Viewmodel.LateralOffset, 0.0f);
}

FRotator UBreakerWeaponComponent::GetViewmodelRotationOffset() const
{
    // Positive pitch on a camera-relative component points the muzzle up.
    return FRotator(Viewmodel.PitchOffset, 0.0f, 0.0f);
}

void UBreakerWeaponComponent::ResetWeaponFeel()
{
    RecoilPitchAccumulated = 0.0f;
    RecoilYawAccumulated = 0.0f;
    RecoveryDelayRemaining = 0.0f;
    BloomDegrees = 0.0f;
    BurstShotIndex = 0;
    bHasAppliedControlRotation = false;
    Viewmodel = FBreakerViewmodelState();
    UpdateFeelTickEnabled();
}

void UBreakerWeaponComponent::UpdateFeelTickEnabled()
{
    const bool bBusy = RecoilPitchAccumulated != 0.0f || RecoilYawAccumulated != 0.0f
        || BloomDegrees > 0.0f || !Viewmodel.IsAtRest();
    SetComponentTickEnabled(bBusy);
}

void UBreakerWeaponComponent::ApplyShotFeel(const FBreakerShotResult& Shot)
{
    // The shot carries the ADS progress it was fired at, so a round loosed
    // halfway into the sights kicks halfway between hip and aimed — on every
    // machine, not just the shooter's.
    const FBreakerRecoilProfile Profile = FBreakerWeaponFeel::ProfileAtAimAlpha(ResolveRecoilProfile(), Shot.AimAlpha);
    const bool bAimedKick = Shot.AimAlpha > 0.0f;
    const FBreakerRecoilKick Kick = FBreakerWeaponFeel::ComputeShotKick(Profile, Shot.BurstShotIndex, Shot.RecoilSeed, bAimedKick);

    if (bViewmodelKickEnabled)
    {
        FBreakerWeaponFeel::AddViewmodelKick(Profile, Viewmodel, Kick.YawDegrees, bAimedKick);
    }

    // Recoil moves the aim, and only the aim, and only for the player who is
    // actually looking through this weapon.
    APawn* Pawn = Cast<APawn>(GetOwner());
    AController* OwningController = Pawn ? Pawn->GetController() : nullptr;
    if (bRecoilEnabled && Pawn && OwningController && Pawn->IsLocallyControlled())
    {
        const FBreakerRecoilKick Applied = FBreakerWeaponFeel::AccumulateKick(Profile, Kick, RecoilPitchAccumulated, RecoilYawAccumulated);

        const FRotator Current = OwningController->GetControlRotation();
        const float CurrentPitch = FRotator::NormalizeAxis(Current.Pitch);
        // Never let the kick drive the view through vertical; give back to the
        // settle budget whatever the clamp refused, so recovery stays exact.
        const float ClampedPitch = FMath::Clamp(CurrentPitch + Applied.PitchDegrees, -89.0f, 89.0f);
        const float ActualPitchDelta = ClampedPitch - CurrentPitch;
        RecoilPitchAccumulated -= (Applied.PitchDegrees - ActualPitchDelta);

        FRotator Kicked = Current;
        Kicked.Pitch = ClampedPitch;
        Kicked.Yaw = Current.Yaw + Applied.YawDegrees;
        OwningController->SetControlRotation(Kicked);
        LastAppliedControlRotation = OwningController->GetControlRotation();
        bHasAppliedControlRotation = true;
        RecoveryDelayRemaining = FMath::Max(RecoveryDelayRemaining, Profile.RecoveryDelaySeconds);
    }

    UpdateFeelTickEnabled();
}

void UBreakerWeaponComponent::TickComponent(float DeltaSeconds, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaSeconds, TickType, ThisTickFunction);
    TickRecoil(DeltaSeconds);
}

void UBreakerWeaponComponent::TickRecoil(float DeltaSeconds)
{
    if (DeltaSeconds <= 0.0f) return;
    const FBreakerRecoilProfile Profile = ResolveRecoilProfile();

    BloomDegrees = FBreakerWeaponFeel::BloomAfterTime(Profile, BloomDegrees, DeltaSeconds);
    FBreakerWeaponFeel::IntegrateViewmodel(Profile, Viewmodel, DeltaSeconds);

    APawn* Pawn = Cast<APawn>(GetOwner());
    AController* OwningController = Pawn ? Pawn->GetController() : nullptr;
    if (OwningController && Pawn->IsLocallyControlled() && (RecoilPitchAccumulated != 0.0f || RecoilYawAccumulated != 0.0f))
    {
        FRotator Current = OwningController->GetControlRotation();
        if (bHasAppliedControlRotation)
        {
            // Aim movement that opposes the kick is the player compensating.
            // Spend the settle budget on it rather than shoving the view the
            // same distance again once the burst ends.
            const float PlayerPitchDelta = FRotator::NormalizeAxis(Current.Pitch - LastAppliedControlRotation.Pitch);
            const float PlayerYawDelta = FRotator::NormalizeAxis(Current.Yaw - LastAppliedControlRotation.Yaw);
            RecoilPitchAccumulated = FBreakerWeaponFeel::ConsumeCompensation(RecoilPitchAccumulated, PlayerPitchDelta);
            RecoilYawAccumulated = FBreakerWeaponFeel::ConsumeCompensation(RecoilYawAccumulated, PlayerYawDelta);
        }

        if (RecoveryDelayRemaining > 0.0f)
        {
            RecoveryDelayRemaining = FMath::Max(0.0f, RecoveryDelayRemaining - DeltaSeconds);
        }
        else
        {
            const float NewPitch = FBreakerWeaponFeel::RecoverAxis(Profile, RecoilPitchAccumulated, DeltaSeconds);
            const float NewYaw = FBreakerWeaponFeel::RecoverAxis(Profile, RecoilYawAccumulated, DeltaSeconds);
            Current.Pitch = FRotator::NormalizeAxis(Current.Pitch) + (NewPitch - RecoilPitchAccumulated);
            Current.Yaw = Current.Yaw + (NewYaw - RecoilYawAccumulated);
            RecoilPitchAccumulated = NewPitch;
            RecoilYawAccumulated = NewYaw;
            OwningController->SetControlRotation(Current);
        }
        LastAppliedControlRotation = OwningController->GetControlRotation();
        bHasAppliedControlRotation = true;
    }
    else if (!OwningController)
    {
        // No aim to move: drop the budget rather than banking a kick that
        // would be handed back the next time a controller appears.
        RecoilPitchAccumulated = 0.0f;
        RecoilYawAccumulated = 0.0f;
    }

    UpdateFeelTickEnabled();
}

void UBreakerWeaponComponent::EquipArchetype(EBreakerWeaponArchetype NewArchetype)
{
    if (CurrentArchetype == NewArchetype) return;
    StopFire();
    if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(ReloadTimer);
    CurrentArchetype = NewArchetype;
    bReloading = false;
    // A different weapon starts its pattern from zero.
    BurstShotIndex = 0;
    BloomDegrees = 0.0f;
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
    // The incoming weapon starts its pattern from zero; any kick still in the
    // air keeps settling, because the aim it moved is still the player's aim.
    BurstShotIndex = 0;
    BloomDegrees = 0.0f;
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
        BurstShotIndex = 0;
        BloomDegrees = 0.0f;
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
    // Guarded like every other timer touch in this component: an owned but
    // worldless component (a test fixture, a CDO) has no timer manager, and
    // StopFire was the one place that assumed it did.
    if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(AutomaticFireTimer);
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

void UBreakerWeaponComponent::SetAimingInternal(bool bNewAiming)
{
    // Only a fresh press restarts the ramp; re-asserting an aim already held
    // must not hand the player a second aim-in window.
    if (bNewAiming && !bAiming)
    {
        AimStartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
    }
    bAiming = bNewAiming;
}

void UBreakerWeaponComponent::SetAiming(bool bNewAiming)
{
    SetAimingInternal(bNewAiming);
    if (GetOwner() && !GetOwner()->HasAuthority())
    {
        ServerSetAiming(bNewAiming);
    }
}

void UBreakerWeaponComponent::ServerStartFire_Implementation() { StartFire(); }
void UBreakerWeaponComponent::ServerStopFire_Implementation() { StopFire(); }
void UBreakerWeaponComponent::ServerStartReload_Implementation() { StartReload(); }
void UBreakerWeaponComponent::ServerSetAiming_Implementation(bool bNewAiming) { SetAimingInternal(bNewAiming); }

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

    const FBreakerRecoilProfile RecoilProfile = ResolveRecoilProfile();
    // A burst is a run of shots with no meaningful gap. Let the trigger rest
    // and the weapon is dead accurate again, its pattern back at shot zero:
    // that is the whole reward for trigger discipline.
    const double IdleSeconds = GetWorld()->GetTimeSeconds() - LastShotTime;
    if (IdleSeconds > RecoilProfile.BurstResetSeconds)
    {
        BurstShotIndex = 0;
        BloomDegrees = 0.0f;
    }

    LastShotTime = GetWorld()->GetTimeSeconds();
    --MagazineAmmo;
    OnAmmoChanged.Broadcast(MagazineAmmo, ReserveAmmo);

    FVector ViewLocation;
    FRotator ViewRotation;
    GetViewPoint(ViewLocation, ViewRotation);
    // ADS tightens the cone twice over: the definition's aimed spread is the
    // floor, and bloom grows more slowly on top of it. Both are now bought
    // over AimInSeconds rather than granted on the button down, and both are
    // paid for in movement — a moving aimed shot is wider than a moving hip
    // shot, which is the whole reason to ever leave the sights.
    const float ShotAimAlpha = GetAimAlpha();
    const FBreakerRecoilProfile AimedProfile = FBreakerWeaponFeel::ProfileAtAimAlpha(RecoilProfile, ShotAimAlpha);
    const float BaseSpread = FMath::Lerp(Definition->HipSpreadDegrees, Definition->AimSpreadDegrees, ShotAimAlpha);
    const float MovementSpread = FBreakerWeaponFeel::MovementSpreadDegrees(AimedProfile, GetSpeedFraction(), ShotAimAlpha);
    const float Spread = FBreakerWeaponFeel::EffectiveSpreadDegrees(AimedProfile, BaseSpread, BloomDegrees, BurstShotIndex, MovementSpread);

    // Recoil state for this shot, resolved before the pellets so the cosmetic
    // event can carry it to every machine and they all kick identically.
    const int32 FiredBurstIndex = BurstShotIndex;
    const int32 RecoilSeed = static_cast<int32>(HashCombine(GetTypeHash(GetOwner()), static_cast<uint32>(ShotSequence + 1)));
    ++BurstShotIndex;
    BloomDegrees = FBreakerWeaponFeel::BloomAfterShot(AimedProfile, BloomDegrees, ShotAimAlpha > 0.0f);
    UpdateFeelTickEnabled();

    if (Definition->bProjectile)
    {
        FireProjectile(Definition, ViewLocation, ViewRotation, Spread, FiredBurstIndex, RecoilSeed, ShotAimAlpha);
        if (MagazineAmmo <= 0 && ReserveAmmo > 0) StartReload();
        return;
    }

    FBreakerShotResult Shot;
    Shot.bFired = true;
    Shot.BurstShotIndex = FiredBurstIndex;
    Shot.RecoilSeed = RecoilSeed;
    Shot.bAimedShot = bAiming;
    Shot.AimAlpha = ShotAimAlpha;
    Shot.TraceStart = ViewLocation;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(BreakerWeaponTrace), true, GetOwner());

    // Lead's mark, resolved once per shot rather than once per pellet: the mark
    // cannot change between the pellets of a single trigger pull. A mark with
    // no remaining time reads as no mark at all.
    const UBreakerAbilityStateComponent* AbilityState = GetOwner() ? GetOwner()->FindComponentByClass<UBreakerAbilityStateComponent>() : nullptr;
    const AActor* MarkedTarget = (AbilityState && AbilityState->GetMarkRemaining() > 0.0f) ? AbilityState->GetMarkedTarget() : nullptr;
    const float LeadMinimumRangeCm = UBreakerAbility_Lead::DefaultMinimumRangeCm();

    // Item level, read once per trigger pull. Every pellet of a shotgun blast
    // and any bleed those pellets apply share this one reading, so a shot can
    // never straddle an equipment change.
    const float LevelScalar = GetItemLevelDamageScalar();
    const float ScaledBaseDamage = FMath::Max(0.0f, Definition->Damage) * LevelScalar;

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
        // The geometric hit, the forgiveness halo around it, or Lead's mark.
        const bool bPelletWeakPoint = ResolveWeakPointHit(Hit, ViewLocation, Direction)
            // Lead (Class-Kits §1.2 S6): shots that hit the mark from beyond
            // the range gate are weak-point hits regardless of impact
            // location. The gate is the ability's own rule, called here rather
            // than reimplemented.
            || UBreakerAbility_Lead::ShouldTreatAsWeakPoint(
                MarkedTarget != nullptr && Hit.GetActor() == MarkedTarget, Hit.Distance, LeadMinimumRangeCm);
        Shot.bWeakPoint |= bPelletWeakPoint;

        if (UBreakerCombatComponent* TargetCombat = Hit.GetActor() ? Hit.GetActor()->FindComponentByClass<UBreakerCombatComponent>() : nullptr)
        {
            const UBreakerAttributeSet* SourceAttributes = nullptr;
            if (const IAbilitySystemInterface* AbilityOwner = Cast<IAbilitySystemInterface>(GetOwner()))
            {
                if (const UAbilitySystemComponent* ASC = AbilityOwner->GetAbilitySystemComponent()) SourceAttributes = ASC->GetSet<UBreakerAttributeSet>();
            }
            FBreakerDamageRequest Damage;
            // The multiplicand: archetype base carried up the item-level curve,
            // then falloff. Per pellet, exactly as before.
            Damage.BaseDamage = ScaledBaseDamage * FBreakerWeaponMath::DamageMultiplierAtDistance(Definition, Hit.Distance);
            Damage.DamageFamily = EBreakerDamageFamily::Physical;
            Damage.WeakPointMultiplier = Definition->WeakPointMultiplier;
            Damage.ArmorPenetration = Definition->ArmorPenetration;
            Damage.bWeakPointHit = bPelletWeakPoint;
            Damage.CriticalChance = SourceAttributes ? SourceAttributes->GetCriticalChance() : 0.05f;
            Damage.CriticalMultiplier = SourceAttributes ? SourceAttributes->GetCriticalMultiplier() : 1.5f;
            // ONE number. Gear's Weapon Damage affix and every skill node that
            // raises damage are already summed into the DamageMultiplier
            // attribute's single additive Increased bucket; multiplying gear in
            // separately here is what used to break the locked rule.
            Damage.SourceDamageMultiplier = SourceAttributes ? SourceAttributes->GetDamageMultiplier() : 1.0f;
            Damage.RandomSeed = HashCombine(GetTypeHash(GetOwner()), ShotSequence);
            Damage.SourceLocation = GetOwner()->GetActorLocation();
            Damage.bHasSourceLocation = true;
            Damage.SetInstigator(GetOwner());
            // Outgoing modifiers compose on the shooter's own component before
            // the request leaves the weapon.
            if (UBreakerCombatComponent* OwnerCombat = GetOwner()->FindComponentByClass<UBreakerCombatComponent>())
            {
                OwnerCombat->ApplyOutgoingModifiers(Damage);
            }
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

            ApplyBleedOnHit(Definition, Hit.GetActor(), SourceAttributes, LevelScalar);
        }
    }
    MulticastShotCosmetics(Shot);

    if (MagazineAmmo <= 0 && ReserveAmmo > 0) StartReload();
}

bool UBreakerWeaponComponent::ResolveWeakPointHit(const FHitResult& Hit, const FVector& RayOrigin, const FVector& RayDirection) const
{
    // The exact geometric hit always counts and costs nothing to check.
    if (Hit.GetComponent() && Hit.GetComponent()->ComponentHasTag(TEXT("WeakPoint"))) return true;
    if (WeakPointToleranceCm <= 0.0f) return false;

    const AActor* HitActor = Hit.GetActor();
    if (!HitActor) return false;

    // Only a round that already hit THIS actor may be upgraded. The halo is
    // forgiveness for imprecise aim on a target you hit, never a second chance
    // at a target you missed, and never a reason to reward shooting the wall
    // in front of an enemy's head.
    for (const UActorComponent* Component : HitActor->GetComponents())
    {
        const UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Component);
        if (!Primitive || !Primitive->ComponentHasTag(TEXT("WeakPoint"))) continue;
        // A weak point whose collision is off (a corpse, a phase) is not a
        // weak point; the halo must not outlive the thing it surrounds.
        if (!Primitive->IsCollisionEnabled()) continue;
        if (FBreakerWeaponMath::IsWithinWeakPointTolerance(RayOrigin, RayDirection,
            Primitive->GetComponentLocation(), Primitive->Bounds.SphereRadius, WeakPointToleranceCm))
        {
            return true;
        }
    }
    return false;
}

void UBreakerWeaponComponent::ApplyBleedOnHit(const UBreakerWeaponDefinition* Definition, AActor* Target, const UBreakerAttributeSet* SourceAttributes, float LevelScalar)
{
    if (!Definition || !Target || Definition->BleedChance <= 0.0f || Definition->BleedDamagePerTick <= 0.0f || Definition->BleedDuration <= 0.0f) return;
    UBreakerStatusComponent* Status = Target->FindComponentByClass<UBreakerStatusComponent>();
    if (!Status) return;

    // Same seed material as the pellet damage, salted so bleed and critical
    // rolls stay independent while remaining reproducible on the server.
    FRandomStream Stream(static_cast<int32>(HashCombine(HashCombine(GetTypeHash(GetOwner()), ShotSequence), BreakerBleedSalt)));
    if (Stream.FRand() > Definition->BleedChance) return;

    FBreakerStatusApplicationSpec Spec;
    Spec.StatusTag = FGameplayTag::RequestGameplayTag(TEXT("Status.Bleed"), false);
    // The DoT's base is a weapon base damage number like any other, so it rides
    // the same item-level curve. Leaving it flat would make bleed a smaller and
    // smaller fraction of an SMG's output as the game went on, which is a
    // silent nerf to the archetype's identity rather than a design choice.
    Spec.BaseDamagePerTick = Definition->BleedDamagePerTick * FMath::Max(0.0f, LevelScalar);
    Spec.Duration = Definition->BleedDuration;
    Spec.TickInterval = FMath::Max(0.05f, Definition->BleedTickInterval);
    Spec.Snapshot.SourcePower = SourceAttributes ? SourceAttributes->GetDamageMultiplier() : 1.0f;
    Spec.Snapshot.CriticalChance = SourceAttributes ? SourceAttributes->GetCriticalChance() : 0.05f;
    Spec.Snapshot.CriticalMultiplier = SourceAttributes ? SourceAttributes->GetCriticalMultiplier() : 1.5f;
    Spec.Snapshot.DamageOverTimeMultiplier = SourceAttributes ? SourceAttributes->GetDamageOverTimeMultiplier() : 1.0f;
    // The critical result is rolled once at application; every tick of this
    // application then crits or does not for its whole lifetime.
    Spec.Snapshot.bRolledCritical = Stream.FRand() < Spec.Snapshot.CriticalChance;
    Status->ApplyStatus(Spec, EBreakerDamageFamily::Physical, GetOwner());
}

void UBreakerWeaponComponent::FireProjectile(const UBreakerWeaponDefinition* Definition, const FVector& ViewLocation, const FRotator& ViewRotation, float Spread, int32 BurstIndex, int32 RecoilSeed, float ShotAimAlpha)
{
    const FVector Direction = FBreakerWeaponMath::ApplyConeSpread(ViewRotation.Vector(), Spread, ++ShotSequence);

    const UBreakerAttributeSet* SourceAttributes = nullptr;
    if (const IAbilitySystemInterface* AbilityOwner = Cast<IAbilitySystemInterface>(GetOwner()))
    {
        if (const UAbilitySystemComponent* ASC = AbilityOwner->GetAbilitySystemComponent()) SourceAttributes = ASC->GetSet<UBreakerAttributeSet>();
    }
    FBreakerDamageRequest Damage;
    // Same multiplicand as the hitscan path: the rocket's payload is a weapon
    // base damage number and scales with item level identically.
    Damage.BaseDamage = FMath::Max(0.0f, Definition->Damage) * GetItemLevelDamageScalar();
    Damage.DamageFamily = EBreakerDamageFamily::Physical;
    Damage.WeakPointMultiplier = 1.0f;
    Damage.ArmorPenetration = Definition->ArmorPenetration;
    Damage.CriticalChance = SourceAttributes ? SourceAttributes->GetCriticalChance() : 0.05f;
    Damage.CriticalMultiplier = SourceAttributes ? SourceAttributes->GetCriticalMultiplier() : 1.5f;
    // Same single composed number as the hitscan path.
    Damage.SourceDamageMultiplier = SourceAttributes ? SourceAttributes->GetDamageMultiplier() : 1.0f;
    Damage.RandomSeed = HashCombine(GetTypeHash(GetOwner()), ShotSequence);
    Damage.SetInstigator(GetOwner());
    // The rocket carries an already-composed request; modifiers active at the
    // moment of firing are the ones that count, not those at detonation.
    if (UBreakerCombatComponent* OwnerCombat = GetOwner() ? GetOwner()->FindComponentByClass<UBreakerCombatComponent>() : nullptr)
    {
        OwnerCombat->ApplyOutgoingModifiers(Damage);
    }

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
    Shot.BurstShotIndex = BurstIndex;
    Shot.RecoilSeed = RecoilSeed;
    Shot.bAimedShot = bAiming;
    Shot.AimAlpha = ShotAimAlpha;
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

FVector UBreakerWeaponComponent::GetVisualMuzzleLocation() const
{
    FVector ViewLocation;
    FRotator ViewRotation;
    GetViewPoint(ViewLocation, ViewRotation);
    const FVector Offset = bAiming ? AimedMuzzleViewOffset : MuzzleViewOffset;
    return ViewLocation + ViewRotation.RotateVector(Offset);
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
    // Feel runs on the cosmetic path, after the trace has already been
    // resolved: the round goes where the player was aiming when they pulled,
    // and the kick then moves the aim for the shot after it.
    if (Shot.bFired) ApplyShotFeel(Shot);
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
    ResetWeaponFeel();
    OnReloadChanged.Broadcast(false);
    OnAmmoChanged.Broadcast(MagazineAmmo, ReserveAmmo);
}

void UBreakerWeaponComponent::AddReserveAmmoFraction(float Fraction)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || Fraction <= 0.0f) return;
    InitializeSlotAmmunition();

    // O2 placeholder: cap at 2x StartingReserveAmmo. Enough headroom that a
    // good streak banks a cushion, tight enough that reserve still matters.
    const float ReserveCapMultiplier = 2.0f;

    auto GrantToSlot = [this, Fraction, ReserveCapMultiplier](EBreakerWeaponArchetype Archetype, int32& SlotReserve)
    {
        const UBreakerWeaponDefinition* Definition = GetPrototypeDefinition(Archetype);
        if (!Definition) return;
        const int32 Starting = Definition->StartingReserveAmmo;
        // Round up so small fractions on low-reserve weapons (rocket: 16)
        // still grant at least one round.
        const int32 Granted = FMath::CeilToInt(Starting * Fraction);
        const int32 Cap = FMath::CeilToInt(Starting * ReserveCapMultiplier);
        SlotReserve = FMath::Min(SlotReserve + Granted, Cap);
    };

    // The equipped slot's live counters are the source of truth; sync them
    // into slot storage first so nothing is lost.
    StoreActiveSlotAmmunition();
    GrantToSlot(SlotOneArchetype, SlotOneReserveAmmo);
    GrantToSlot(SlotTwoArchetype, SlotTwoReserveAmmo);
    ReserveAmmo = CurrentSlot == 1 ? SlotOneReserveAmmo : SlotTwoReserveAmmo;
    OnAmmoChanged.Broadcast(MagazineAmmo, ReserveAmmo);
}

void UBreakerWeaponComponent::OnRep_Ammo() { OnAmmoChanged.Broadcast(MagazineAmmo, ReserveAmmo); }
void UBreakerWeaponComponent::OnRep_Reloading() { OnReloadChanged.Broadcast(bReloading); }
void UBreakerWeaponComponent::OnRep_Swapping()
{
    if (!bSwapping) LastSwapInTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
    OnSwapChanged.Broadcast(bSwapping, CurrentSlot);
}
