#include "Abilities/BreakerGunsmithAbilities.h"

#include "AbilitySystemComponent.h"
#include "Abilities/BreakerAbilityDefinition.h"
#include "Abilities/BreakerAbilityStateComponent.h"
#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerCombatComponent.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "TimerManager.h"
#include "Weapons/BreakerWeaponComponent.h"

// ---------------------------------------------------------------------------
// G1 — SIDEARM RIG
// ---------------------------------------------------------------------------

UBreakerAbility_SidearmRig::UBreakerAbility_SidearmRig()
{
    FallbackAbilityId = TEXT("Gunsmith.SidearmRig");
    // It rewrites the owner's own damage only; window abilities predict.
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
}

FName UBreakerAbility_SidearmRig::WindowKey() { return TEXT("Window.Gunsmith.SidearmRig"); }
FName UBreakerAbility_SidearmRig::OutgoingModifierKey() { return TEXT("SidearmRig"); }

void UBreakerAbility_SidearmRig::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    ABreakerCharacter* Character = GetBreakerCharacter();
    UBreakerWeaponComponent* Weapon = Character ? Character->GetWeapon() : nullptr;
    if (!Character || !Weapon || !CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // The window's clock is SHOTS, not seconds — WindowDuration on the
    // definition is deliberately 0 and no timer is armed here. The rig ends on
    // the magazine emptying or on a reload starting, whichever comes first.
    if (UBreakerCombatComponent* Combat = Character->FindComponentByClass<UBreakerCombatComponent>())
    {
        // Flat sum stage, no expiry — the shot events own the teardown.
        Combat->PushOutgoingModifier(OutgoingModifierKey(), FlatBonusDamage, 1.0f, -1.0f);
    }
    if (UBreakerAbilityStateComponent* State = UBreakerAbilityStateComponent::FindOrAdd(Character))
    {
        // Published for the HUD with a nominal long duration; the real close
        // comes from the shot events below.
        State->StartWindow(WindowKey(), 120.0f);
    }
    // §G1's "+1 Pierce", live as of the Swift projectile pass (2026-08-16,
    // the one authorized cross-territory edit): a keyed channel bonus with no
    // expiry — the shot events below own the pop, like everything else here.
    Weapon->PushShotChannelBonus(OutgoingModifierKey(), 0.0f, PierceBonus, 0, 0);

    BoundWeapon = Weapon;
    Weapon->OnMagazineEmptied.AddDynamic(this, &UBreakerAbility_SidearmRig::HandleMagazineEmptied);
    Weapon->OnReloadChanged.AddDynamic(this, &UBreakerAbility_SidearmRig::HandleReloadChanged);
    bRigActive = true;
}

void UBreakerAbility_SidearmRig::HandleMagazineEmptied(bool bStartedFull)
{
    CloseRig();
}

void UBreakerAbility_SidearmRig::HandleReloadChanged(bool bReloading)
{
    if (bReloading) CloseRig();
}

void UBreakerAbility_SidearmRig::CloseRig()
{
    if (!bRigActive) return;
    if (CurrentActorInfo)
    {
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
    }
}

void UBreakerAbility_SidearmRig::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    // Teardown on EVERY exit: an InstancedPerActor ability is reused, and a
    // surviving binding would end the NEXT rig on this magazine's events.
    if (bRigActive)
    {
        bRigActive = false;
        if (ABreakerCharacter* Character = GetBreakerCharacter())
        {
            if (UBreakerCombatComponent* Combat = Character->FindComponentByClass<UBreakerCombatComponent>())
            {
                Combat->RemoveOutgoingModifier(OutgoingModifierKey());
            }
            if (UBreakerAbilityStateComponent* State = Character->FindComponentByClass<UBreakerAbilityStateComponent>())
            {
                State->CloseWindow(WindowKey());
            }
        }
        if (UBreakerWeaponComponent* Weapon = BoundWeapon.Get())
        {
            Weapon->PopShotChannelBonus(OutgoingModifierKey());
            Weapon->OnMagazineEmptied.RemoveDynamic(this, &UBreakerAbility_SidearmRig::HandleMagazineEmptied);
            Weapon->OnReloadChanged.RemoveDynamic(this, &UBreakerAbility_SidearmRig::HandleReloadChanged);
        }
        BoundWeapon.Reset();
    }
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

// ---------------------------------------------------------------------------
// G2 — OVERHAUL
// ---------------------------------------------------------------------------

UBreakerAbility_Overhaul::UBreakerAbility_Overhaul()
{
    FallbackAbilityId = TEXT("Gunsmith.Overhaul");
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
}

FName UBreakerAbility_Overhaul::WindowKey() { return TEXT("Window.Gunsmith.Overhaul"); }

void UBreakerAbility_Overhaul::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    const UBreakerAbilityDefinition* Definition = GetAbilityDefinition();
    ABreakerCharacter* Character = GetBreakerCharacter();
    UBreakerWeaponComponent* Weapon = Character ? Character->GetWeapon() : nullptr;
    UWorld* World = Character ? Character->GetWorld() : nullptr;
    if (!Character || !Weapon || !World || !CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // §G2: drawn on activation, up to +100% of base magazine size, at 3:1.
    const UBreakerWeaponDefinition* WeaponDefinition = Weapon->GetActiveDefinition();
    const int32 BaseMagazine = WeaponDefinition ? Weapon->GetEffectiveMagazineSize() : 0;
    const int32 DesiredDelta = FMath::FloorToInt(BaseMagazine * FMath::Max(0.0f, MaximumCapacityFraction));
    const int32 Drawn = Weapon->PushMagazineCapacityOverride(WindowKey(), DesiredDelta, ReservePerRound);

    const float Duration = Definition ? Definition->WindowDuration : 10.0f;
    if (UBreakerAbilityStateComponent* State = UBreakerAbilityStateComponent::FindOrAdd(Character))
    {
        State->StartWindow(WindowKey(), Duration);
    }

    BoundWeapon = Weapon;
    bOverhaulActive = true;
    // The pop is the settle: unspent converted rounds return to reserve at the
    // same 3:1 they were bought at, and rounds fired stay spent — the bet.
    World->GetTimerManager().SetTimer(WindowTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
    {
        CloseOverhaul();
    }), Duration, false);

    // A dry reserve draws nothing; the cast still happened (a free-cost
    // ability refused for economy reasons would read as a dead key).
    if (Drawn == 0)
    {
        UE_LOG(LogTemp, Log, TEXT("Overhaul: reserve could not pay for any conversion; the window opened empty."));
    }
}

void UBreakerAbility_Overhaul::CloseOverhaul()
{
    if (CurrentActorInfo)
    {
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
    }
}

void UBreakerAbility_Overhaul::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    if (bOverhaulActive)
    {
        bOverhaulActive = false;
        if (UBreakerWeaponComponent* Weapon = BoundWeapon.Get())
        {
            Weapon->PopMagazineCapacityOverride(WindowKey());
        }
        BoundWeapon.Reset();
        if (ABreakerCharacter* Character = GetBreakerCharacter())
        {
            if (UBreakerAbilityStateComponent* State = Character->FindComponentByClass<UBreakerAbilityStateComponent>())
            {
                State->CloseWindow(WindowKey());
            }
        }
        if (UWorld* World = GetWorld())
        {
            World->GetTimerManager().ClearTimer(WindowTimer);
        }
    }
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

// ---------------------------------------------------------------------------
// G3-G6 — THE DEPLOYABLES
// ---------------------------------------------------------------------------

UBreakerGunsmithDeployAbility::UBreakerGunsmithDeployAbility()
{
    // Spawns a server-side actor: never predicted (spec D5).
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
}

void UBreakerGunsmithDeployAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    ABreakerCharacter* Character = GetBreakerCharacter();
    UWorld* World = Character ? Character->GetWorld() : nullptr;
    if (!Character || !World)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    FVector ViewLocation = Character->GetActorLocation();
    FRotator ViewRotation = Character->GetControlRotation();
    if (const AController* Controller = Character->GetController())
    {
        Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);
    }

    // §2.3: VALIDATE BEFORE COMMIT. A failed placement costs nothing — no
    // Scrap, no activation — and it fails loudly rather than relocating.
    FVector PlaceLocation;
    if (!ABreakerDeployable::ResolvePlacement(World, Character, ViewLocation, ViewRotation.Vector(), PlacementRangeCm, PlaceLocation))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // §2.1: destroy-oldest BEFORE the new placement, so the field is never
    // blocked by its own furniture. The cull refunds through the one path.
    ABreakerDeployable::EnforceDensityCapForPlacement(Character, DeployableType);

    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    SpawnParams.Owner = Character;
    if (ABreakerDeployable* Deployable = World->SpawnActor<ABreakerDeployable>(ABreakerDeployable::StaticClass(), PlaceLocation, FRotator(0.0f, ViewRotation.Yaw, 0.0f), SpawnParams))
    {
        const UBreakerAbilityDefinition* Definition = GetAbilityDefinition();
        // The refund base is the AUTHORED cost actually paid (§1.1: 50%
        // refund, never profit — the Scrap component owns the fraction).
        Deployable->InitializeDeployable(DeployableType, Character, Definition ? Definition->ResourceCost : 0.0f);
    }

    EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

UBreakerAbility_Turret::UBreakerAbility_Turret()
{
    FallbackAbilityId = TEXT("Gunsmith.Turret");
    DeployableType = EBreakerDeployableType::Turret;
}

UBreakerAbility_AmmoCrate::UBreakerAbility_AmmoCrate()
{
    FallbackAbilityId = TEXT("Gunsmith.AmmoCrate");
    DeployableType = EBreakerDeployableType::AmmoCrate;
}

UBreakerAbility_MineCluster::UBreakerAbility_MineCluster()
{
    FallbackAbilityId = TEXT("Gunsmith.MineCluster");
    DeployableType = EBreakerDeployableType::MineCluster;
}

UBreakerAbility_Disruptor::UBreakerAbility_Disruptor()
{
    FallbackAbilityId = TEXT("Gunsmith.Disruptor");
    DeployableType = EBreakerDeployableType::Disruptor;
}

// ---------------------------------------------------------------------------
// ULTIMATE — FIELD ASSEMBLY
// ---------------------------------------------------------------------------

UBreakerAbility_FieldAssembly::UBreakerAbility_FieldAssembly()
{
    FallbackAbilityId = TEXT("Gunsmith.FieldAssembly");
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
}

FName UBreakerAbility_FieldAssembly::WindowKey() { return TEXT("Window.Gunsmith.FieldAssembly"); }
FName UBreakerAbility_FieldAssembly::MachinistModifierKey() { return TEXT("FieldAssembly.Machinist"); }

void UBreakerAbility_FieldAssembly::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    const UBreakerAbilityDefinition* Definition = GetAbilityDefinition();
    ABreakerCharacter* Character = GetBreakerCharacter();
    UWorld* World = Character ? Character->GetWorld() : nullptr;
    const float Threshold = Definition ? Definition->ResourceCost : 100.0f;
    // An ultimate is all-or-nothing, the Overdrive precedent.
    if (!Character || !World || GetCurrentClassResource() < Threshold || !CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    FGameplayTagContainer OwnerTags;
    if (const UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr)
    {
        ASC->GetOwnedGameplayTags(OwnerTags);
    }
    const FBreakerAbilityVariant Variant = Definition ? Definition->ResolveVariant(OwnerTags) : FBreakerAbilityVariant();
    const float Duration = Variant.WindowDuration > 0.0f ? Variant.WindowDuration : 20.0f;

    if (UBreakerAbilityStateComponent* State = UBreakerAbilityStateComponent::FindOrAdd(Character))
    {
        State->StartWindow(WindowKey(), Duration);
    }
    bAssemblyActive = true;
    World->GetTimerManager().SetTimer(WindowTimer, FTimerDelegate::CreateWeakLambda(this, [this]() { CloseAssembly(); }), Duration, false);

    const bool bMachinist = Variant.KeystoneTag == FGameplayTag::RequestGameplayTag(TEXT("Keystone.Gunsmith.Machinist"), false);
    const bool bFoundry = Variant.KeystoneTag == FGameplayTag::RequestGameplayTag(TEXT("Keystone.Gunsmith.Foundry"), false);
    const bool bMinefield = Variant.KeystoneTag == FGameplayTag::RequestGameplayTag(TEXT("Keystone.Gunsmith.Minefield"), false);

    if (bMachinist)
    {
        // MACHINIST places NOTHING: every unlocked type's effect applies to
        // the player instead. The doc authors the mapping's SHAPE and not its
        // magnitudes; two of four entries are implemented (see the header).
        float FlatRider = 0.0f;
        if (const UBreakerWeaponComponent* Weapon = Character->GetWeapon())
        {
            FlatRider = Weapon->GetScaledBaseDamage() * MachinistFlatDamageFraction;
        }
        if (UBreakerCombatComponent* Combat = Character->FindComponentByClass<UBreakerCombatComponent>())
        {
            Combat->PushOutgoingModifier(MachinistModifierKey(), FlatRider, 1.0f, Duration);
        }
        World->GetTimerManager().SetTimer(MachinistPulseTimer, this, &UBreakerAbility_FieldAssembly::HandleMachinistPulse,
            FMath::Max(0.5f, MachinistReservePulseSeconds), /*bLoop=*/true);
        return;
    }

    // Base / Foundry / Minefield: one free mass placement of every unlocked
    // type at valid positions around the player, then the raised density cap
    // for the window. With no granting nodes authored yet, "unlocked" is the
    // whole kit (see the header's tripwire comment).
    ABreakerDeployable::PushDensityCapOverride(Character, RaisedDensityCap, World->GetTimeSeconds() + Duration);

    const EBreakerDeployableType Types[] = {
        EBreakerDeployableType::Turret, EBreakerDeployableType::AmmoCrate,
        EBreakerDeployableType::MineCluster, EBreakerDeployableType::Disruptor };
    const float RingRadius = 300.0f;   // O2 PLACEHOLDER: "valid positions around the player"
    int32 Placed = 0;
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(Types); ++Index)
    {
        const float Angle = (2.0f * PI / UE_ARRAY_COUNT(Types)) * Index;
        const FVector Direction = FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f);
        FVector PlaceLocation;
        // Aim from chest height outward; a spot with no floor is skipped
        // rather than failing the whole ultimate.
        const FVector Origin = Character->GetActorLocation() + FVector(0.0f, 0.0f, 50.0f);
        if (!ABreakerDeployable::ResolvePlacement(World, Character, Origin, Direction, RingRadius, PlaceLocation)) continue;

        ABreakerDeployable::EnforceDensityCapForPlacement(Character, Types[Index]);

        FActorSpawnParameters SpawnParams;
        SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        SpawnParams.Owner = Character;
        if (ABreakerDeployable* Deployable = World->SpawnActor<ABreakerDeployable>(ABreakerDeployable::StaticClass(), PlaceLocation, FRotator(0.0f, Character->GetActorRotation().Yaw, 0.0f), SpawnParams))
        {
            // Placed at NO individual Scrap cost (§3) — and therefore with a
            // zero refund base, or the free placements would mint Scrap on
            // expiry. The economy stays one-directional.
            Deployable->InitializeDeployable(Types[Index], Character, 0.0f);
            if (bFoundry) Deployable->SetLifetimePaused(true);      // permanent but bounded (§3)
            if (bMinefield) Deployable->SetHiddenUntilAction(true); // the ambush ultimate (§3)
            ++Placed;
        }
    }
    if (Placed == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("Field Assembly: no valid floor anywhere around the player; the cap was still raised for the window."));
    }
}

void UBreakerAbility_FieldAssembly::HandleMachinistPulse()
{
    // The Ammo Crate mapping entry: continuous reserve regeneration.
    if (ABreakerCharacter* Character = GetBreakerCharacter())
    {
        if (UBreakerWeaponComponent* Weapon = Character->GetWeapon())
        {
            Weapon->AddReserveAmmoFraction(MachinistReservePulseFraction);
        }
    }
}

void UBreakerAbility_FieldAssembly::CloseAssembly()
{
    if (CurrentActorInfo)
    {
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
    }
}

void UBreakerAbility_FieldAssembly::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    if (bAssemblyActive)
    {
        bAssemblyActive = false;
        if (ABreakerCharacter* Character = GetBreakerCharacter())
        {
            if (UBreakerCombatComponent* Combat = Character->FindComponentByClass<UBreakerCombatComponent>())
            {
                Combat->RemoveOutgoingModifier(MachinistModifierKey());
            }
            if (UBreakerAbilityStateComponent* State = Character->FindComponentByClass<UBreakerAbilityStateComponent>())
            {
                State->CloseWindow(WindowKey());
            }
        }
        if (UWorld* World = GetWorld())
        {
            World->GetTimerManager().ClearTimer(WindowTimer);
            World->GetTimerManager().ClearTimer(MachinistPulseTimer);
        }
    }
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
