#include "Abilities/BreakerGunsmithAbilities.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Abilities/BreakerAbilityDefinition.h"
#include "Abilities/BreakerAbilityStateComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerDamageLibrary.h"
#include "Combat/BreakerEnemy.h"
#include "Combat/BreakerZoneActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Controller.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionLibrary.h"
#include "TimerManager.h"
#include "Weapons/BreakerWeaponComponent.h"

namespace BreakerGunsmithAbilityLocal
{
    // Prefixed for the unity build (the house rule).
    const UBreakerProgressionComponent* BreakerOwnerProgression(const AActor* OwnerActor)
    {
        return OwnerActor ? OwnerActor->FindComponentByClass<UBreakerProgressionComponent>() : nullptr;
    }
    int32 BreakerOwnerNodeRank(const AActor* OwnerActor, const TCHAR* NodeId)
    {
        const UBreakerProgressionComponent* Progression = BreakerOwnerProgression(OwnerActor);
        return Progression ? Progression->GetNodeRank(FName(NodeId), EBreakerPointCurrency::ClassPoints) : 0;
    }
    bool BreakerOwnerHasTag(const AActor* OwnerActor, const FGameplayTag& Tag)
    {
        const UBreakerProgressionComponent* Progression = BreakerOwnerProgression(OwnerActor);
        return Progression && Progression->HasNodeTag(Tag);
    }
}

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
    // AR8 Rig Discipline: the window is measured in SHOTS — a budget of one
    // effective magazine, counted down on the weapon's own shot event, and it
    // survives exactly one reload. Bound only when owned so a bare rig's event
    // surface is bit-identical to before the node existed.
    ShotsRemaining = 0;
    ReloadsSurvived = 0;
    if (OwnerHasNodeTag(BreakerNodeTags::Node_AR_RigDiscipline.GetTag()))
    {
        ShotsRemaining = Weapon->GetEffectiveMagazineSize();
        Weapon->OnShot.AddDynamic(this, &UBreakerAbility_SidearmRig::HandleShotFired);
    }
    bRigActive = true;
}

bool UBreakerAbility_SidearmRig::OwnerHasNodeTag(const FGameplayTag& Tag) const
{
    return BreakerGunsmithAbilityLocal::BreakerOwnerHasTag(GetBreakerCharacter(), Tag);
}

bool UBreakerAbility_SidearmRig::WindowClosesOnMagazineEmptied(bool bHasLastRound, bool bHasRigDiscipline)
{
    // AR5: "Sidearm Rig's window does not end on that round." AR8 replaces the
    // magazine boundary with the shot budget entirely.
    return !bHasLastRound && !bHasRigDiscipline;
}

bool UBreakerAbility_SidearmRig::WindowClosesOnReloadStart(bool bHasRigDiscipline, int32 InReloadsSurvived)
{
    // AR8: "it persists across ONE reload" — the second reload still ends it.
    return !bHasRigDiscipline || InReloadsSurvived >= 1;
}

float UBreakerAbility_SidearmRig::ColdBarrelShave(int32 Rank)
{
    if (Rank >= 2) return 2.5f;   // §AR6 R2, transcribed
    if (Rank == 1) return 1.5f;   // §AR6, transcribed
    return 0.0f;
}

void UBreakerAbility_SidearmRig::ShaveCooldownForEmptyReload(AActor* OwnerActor, int32 Rank)
{
    const float Shave = ColdBarrelShave(Rank);
    if (!OwnerActor || Shave <= 0.0f) return;
    const IAbilitySystemInterface* AbilityOwner = Cast<IAbilitySystemInterface>(OwnerActor);
    UAbilitySystemComponent* ASC = AbilityOwner ? AbilityOwner->GetAbilitySystemComponent() : nullptr;
    if (!ASC) return;
    // The rig's cooldown is an ordinary duration effect carrying the rig's
    // cooldown tag (requested by string, the keystone posture — the tag is the
    // ability registry's file-local). Winding its start time back is the one
    // engine seam that shortens a LIVE cooldown without restating its length.
    const FGameplayTag CooldownTag = FGameplayTag::RequestGameplayTag(TEXT("Cooldown.Class.Gunsmith.SidearmRig"), false);
    if (!CooldownTag.IsValid()) return;
    const FGameplayEffectQuery Query = FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(FGameplayTagContainer(CooldownTag));
    for (const FActiveGameplayEffectHandle& Handle : ASC->GetActiveEffects(Query))
    {
        ASC->ModifyActiveEffectStartTime(Handle, -Shave);
    }
}

void UBreakerAbility_SidearmRig::HandleMagazineEmptied(bool bStartedFull)
{
    if (WindowClosesOnMagazineEmptied(
        OwnerHasNodeTag(BreakerNodeTags::Node_AR_LastRound.GetTag()),
        OwnerHasNodeTag(BreakerNodeTags::Node_AR_RigDiscipline.GetTag())))
    {
        CloseRig();
    }
}

void UBreakerAbility_SidearmRig::HandleReloadChanged(bool bReloading)
{
    if (!bReloading) return;
    if (WindowClosesOnReloadStart(OwnerHasNodeTag(BreakerNodeTags::Node_AR_RigDiscipline.GetTag()), ReloadsSurvived))
    {
        CloseRig();
        return;
    }
    ++ReloadsSurvived;
}

void UBreakerAbility_SidearmRig::HandleShotFired(const FBreakerShotResult& Shot)
{
    // Only bound under Rig Discipline; a refused shot never broadcasts, so
    // every event here is a real round leaving the rig's budget.
    if (--ShotsRemaining <= 0)
    {
        CloseRig();
    }
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
            Weapon->OnShot.RemoveDynamic(this, &UBreakerAbility_SidearmRig::HandleShotFired);
        }
        BoundWeapon.Reset();
        ShotsRemaining = 0;
        ReloadsSurvived = 0;
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
FName UBreakerAbility_Overhaul::TailKey() { return TEXT("Window.Gunsmith.Overhaul.Tail"); }

int32 UBreakerAbility_Overhaul::BenchWorkTailRounds(int32 DrawnRounds)
{
    // §AR7: "at half the converted capacity", floor division — a one-round
    // window has no tail, which reads correctly as "nothing left to taper".
    return FMath::Max(0, DrawnRounds / 2);
}

bool UBreakerAbility_Overhaul::OwnerHasNodeTag(const FGameplayTag& Tag) const
{
    return BreakerGunsmithAbilityLocal::BreakerOwnerHasTag(GetBreakerCharacter(), Tag);
}

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

    // A re-cast supersedes a pending Bench Work tail: the new window IS the
    // conversion now, and a tail under it would double-count the old one.
    CancelBenchWorkTail();

    int32 Drawn = 0;
    bOverpressureActive = OwnerHasNodeTag(BreakerNodeTags::Node_AR_Overpressure.GetTag());
    if (bOverpressureActive)
    {
        // AR10 Overpressure: the bet reverses. No reserve is drawn into the
        // magazine; the doc's literal capacity SHRINK now lands — the
        // weapon's capacity hook accepts a negative delta (the 2026-08-16
        // weapon-half pay pass closed the seam this note used to record as
        // missing): the magazine shrinks for the window, rounds it can no
        // longer hold settle to reserve 1:1 on the weapon, and the same
        // WindowKey pop that settles the base bet restores the capacity.
        // On top of the mechanical settle, the window credits reserve up
        // front and every shot fired inside it restores a little more.
        const int32 DesiredShrink = FMath::FloorToInt(Weapon->GetEffectiveMagazineSize() * FMath::Clamp(OverpressureCapacityShrinkFraction, 0.0f, 1.0f));
        if (DesiredShrink > 0)
        {
            Weapon->PushMagazineCapacityOverride(WindowKey(), -DesiredShrink);
        }
        Weapon->AddReserveAmmoFraction(OverpressureReserveGrantFraction);
        Weapon->OnShot.AddDynamic(this, &UBreakerAbility_Overhaul::HandleOverpressureShot);
    }
    else
    {
        // §G2: drawn on activation, up to +100% of base magazine size, at 3:1.
        const UBreakerWeaponDefinition* WeaponDefinition = Weapon->GetActiveDefinition();
        const int32 BaseMagazine = WeaponDefinition ? Weapon->GetEffectiveMagazineSize() : 0;
        const int32 DesiredDelta = FMath::FloorToInt(BaseMagazine * FMath::Max(0.0f, MaximumCapacityFraction));
        Drawn = Weapon->PushMagazineCapacityOverride(WindowKey(), DesiredDelta, ReservePerRound);
    }
    LastDrawnRounds = Drawn;

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
    if (Drawn == 0 && !bOverpressureActive)
    {
        UE_LOG(LogTemp, Log, TEXT("Overhaul: reserve could not pay for any conversion; the window opened empty."));
    }
}

void UBreakerAbility_Overhaul::HandleOverpressureShot(const FBreakerShotResult& Shot)
{
    // AR10's second clause: every shot in the window restores a portion of
    // reserve. Only bound while an Overpressure window is live.
    if (UBreakerWeaponComponent* Weapon = BoundWeapon.Get())
    {
        Weapon->AddReserveAmmoFraction(OverpressurePerShotReserveFraction);
    }
}

void UBreakerAbility_Overhaul::HandleTailReloadCompleted(bool bAnyRoundFired)
{
    UBreakerWeaponComponent* Weapon = BoundWeapon.Get();
    if (!Weapon)
    {
        CancelBenchWorkTail();
        return;
    }
    if (TailState == EBenchWorkTail::Armed)
    {
        // §AR7: the conversion applies to THE NEXT MAGAZINE LOADED after the
        // window ends, at half strength — a fresh half-size conversion push
        // riding the magazine this reload just filled.
        const int32 TailRounds = BenchWorkTailRounds(LastDrawnRounds);
        if (TailRounds <= 0 || Weapon->PushMagazineCapacityOverride(TailKey(), TailRounds, ReservePerRound) <= 0)
        {
            CancelBenchWorkTail();
            return;
        }
        TailState = EBenchWorkTail::Active;
        return;
    }
    if (TailState == EBenchWorkTail::Active)
    {
        // The tail magazine has been reloaded away; the pop settles the
        // unspent remainder exactly as the window's own pop does.
        CancelBenchWorkTail();
    }
}

void UBreakerAbility_Overhaul::CancelBenchWorkTail()
{
    if (UBreakerWeaponComponent* Weapon = BoundWeapon.Get())
    {
        if (TailState == EBenchWorkTail::Active)
        {
            Weapon->PopMagazineCapacityOverride(TailKey());
        }
        if (TailState != EBenchWorkTail::None)
        {
            Weapon->OnReloadCompleted.RemoveDynamic(this, &UBreakerAbility_Overhaul::HandleTailReloadCompleted);
        }
    }
    TailState = EBenchWorkTail::None;
    // The tail was the only thing keeping the binding alive after the window.
    if (!bOverhaulActive)
    {
        BoundWeapon.Reset();
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
        bool bTailArmed = false;
        if (UBreakerWeaponComponent* Weapon = BoundWeapon.Get())
        {
            Weapon->PopMagazineCapacityOverride(WindowKey());
            if (bOverpressureActive)
            {
                Weapon->OnShot.RemoveDynamic(this, &UBreakerAbility_Overhaul::HandleOverpressureShot);
            }
            // AR7 Bench Work: the window that actually converted arms a tail —
            // the NEXT magazine loaded gets a half-strength conversion. A
            // cancelled window is a cliff by choice and gets none.
            if (!bWasCancelled && !bOverpressureActive
                && BenchWorkTailRounds(LastDrawnRounds) > 0
                && OwnerHasNodeTag(BreakerNodeTags::Node_AR_BenchWork.GetTag()))
            {
                Weapon->OnReloadCompleted.AddDynamic(this, &UBreakerAbility_Overhaul::HandleTailReloadCompleted);
                TailState = EBenchWorkTail::Armed;
                bTailArmed = true;
            }
        }
        bOverpressureActive = false;
        // The tail keeps the weapon binding alive past the window; everything
        // else tears down now.
        if (!bTailArmed)
        {
            BoundWeapon.Reset();
        }
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

bool UBreakerGunsmithDeployAbility::IsTinkererDeployable(EBreakerDeployableType Type)
{
    // §TK1's scope: the trap half of the kit. Turret and crate are Field
    // Tech's; the Anchor Point is not even a Scrap object.
    return Type == EBreakerDeployableType::MineCluster || Type == EBreakerDeployableType::Disruptor;
}

float UBreakerGunsmithDeployAbility::EffectiveDeployCost(float BaseCost, EBreakerDeployableType Type, EBreakerScrapState State, int32 CheapWorkRank, float ReplacementDiscount)
{
    float Cost = FMath::Max(0.0f, BaseCost);
    // TK1 Cheap Work: 10 less (R2: 18) while Dry, Tinkerer deployables only,
    // to a floor of 10 — rescues the broke, never subsidises the rich.
    if (CheapWorkRank > 0 && IsTinkererDeployable(Type) && State == EBreakerScrapState::Dry)
    {
        const float Discount = CheapWorkRank >= 2 ? 18.0f : 10.0f;   // §TK1, transcribed
        Cost = FMath::Max(10.0f, Cost - Discount);
    }
    // FT5 Requisition: the replacement placed within 8s costs 10 (R2: 18)
    // less. Applies after Cheap Work's floor and never below free.
    return FMath::Max(0.0f, Cost - FMath::Max(0.0f, ReplacementDiscount));
}

float UBreakerGunsmithDeployAbility::GetResourceCost() const
{
    using namespace BreakerGunsmithAbilityLocal;
    const float BaseCost = Super::GetResourceCost();
    const ABreakerCharacter* Character = GetBreakerCharacter();
    if (!Character) return BaseCost;

    EBreakerScrapState State = EBreakerScrapState::Dry;
    if (const UBreakerScrapComponent* Scrap = Character->FindComponentByClass<UBreakerScrapComponent>())
    {
        State = Scrap->GetScrapState();
    }
    const int32 CheapWorkRank = BreakerOwnerNodeRank(Character, TEXT("Gunsmith.Tinkerer.CheapWork"));
    const UWorld* World = Character->GetWorld();
    const float ReplacementDiscount = ABreakerDeployable::PendingReplacementDiscount(
        Character, DeployableType, World ? World->GetTimeSeconds() : 0.0);
    return EffectiveDeployCost(BaseCost, DeployableType, State, CheapWorkRank, ReplacementDiscount);
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
        // The refund base is the cost ACTUALLY PAID — the live GetResourceCost,
        // discounts included, so a Cheap Work placement cannot refund more than
        // it cost (§1.1: refund, never profit).
        Deployable->InitializeDeployable(DeployableType, Character, GetResourceCost());
        // FT5: the discount is a one-placement credit; the placement spent it.
        ABreakerDeployable::ConsumeReplacementCredit(Character, DeployableType);
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

void UBreakerAbility_MineCluster::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    // TK11 Command Detonation: at the per-type cap ("no charges left to
    // place" — a fifth cluster could only cull your own field), re-activation
    // becomes the detonator: every armed charge fires at once, no placement,
    // no cost, no refund. With armed charges absent (all spent or still
    // arming), the input falls through to the ordinary placement.
    ABreakerCharacter* Character = GetBreakerCharacter();
    if (Character && BreakerGunsmithAbilityLocal::BreakerOwnerHasTag(Character, BreakerNodeTags::Node_TK_CommandDetonation.GetTag()))
    {
        int32 Total = 0;
        int32 OfType = 0;
        ABreakerDeployable::CountOwnedDeployables(Character, Total, EBreakerDeployableType::MineCluster, OfType);
        if (OfType >= ABreakerDeployable::PerTypeDensityCap)
        {
            if (ABreakerDeployable::CommandDetonateOwnedMines(Character) > 0)
            {
                EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
                return;
            }
        }
    }
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
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
        // magnitudes; all four entries are implemented (see the header).
        float FlatRider = 0.0f;
        if (const UBreakerWeaponComponent* Weapon = Character->GetWeapon())
        {
            FlatRider = Weapon->GetScaledBaseDamage() * MachinistFlatDamageFraction;
        }
        if (UBreakerCombatComponent* Combat = Character->FindComponentByClass<UBreakerCombatComponent>())
        {
            // Turret entry: the flat weapon-damage rider.
            Combat->PushOutgoingModifier(MachinistModifierKey(), FlatRider, 1.0f, Duration);
            // Mine Cluster entry: kills during the window detonate radially at
            // the victim ("Mine Cluster becomes an on-kill radial detonation").
            Combat->OnKillDealt.AddDynamic(this, &UBreakerAbility_FieldAssembly::HandleMachinistKill);
            bMachinistActive = true;
        }
        // Ammo Crate entry: continuous reserve regeneration.
        World->GetTimerManager().SetTimer(MachinistPulseTimer, this, &UBreakerAbility_FieldAssembly::HandleMachinistPulse,
            FMath::Max(0.5f, MachinistReservePulseSeconds), /*bLoop=*/true);
        // Disruptor entry: "Disruptor becomes an aura on the player" — the
        // same Disruptor-tagged zone the deployable spawns, riding the player
        // (the Wellspring follow seam), so slow and flat armour strip apply by
        // the exact rules the placed field uses, anti-stack tag included.
        {
            FBreakerZoneSpec AuraSpec;
            AuraSpec.ZoneTag = FGameplayTag::RequestGameplayTag(TEXT("Zone.Gunsmith.Disruptor"), false);
            AuraSpec.RadiusCm = 600.0f;    // the Disruptor's own seed (§G6)
            AuraSpec.Duration = Duration;
            AuraSpec.TickInterval = 1.0f;
            AuraSpec.FlatArmorReduction = 60.0f;   // O2 PLACEHOLDER, the deployable's own seed
            AuraSpec.ZoneColor = FLinearColor(0.95f, 0.55f, 0.10f);
            FActorSpawnParameters AuraParams;
            AuraParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            AuraParams.Owner = Character;
            MachinistAura = World->SpawnActor<ABreakerZoneActor>(ABreakerZoneActor::StaticClass(), Character->GetActorLocation(), FRotator::ZeroRotator, AuraParams);
            if (MachinistAura)
            {
                MachinistAura->ConfigureZone(AuraSpec, Character);
                MachinistAura->SetFollowActor(Character);
            }
        }
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

void UBreakerAbility_FieldAssembly::HandleMachinistKill(const FBreakerHitContext& Hit)
{
    // The Mine Cluster mapping entry: the kill detonates at the victim, at the
    // owner's scaled weapon base and the turret's proc discipline (0.5), so
    // the window cannot become a proc engine. Enemies only; radial falloff to
    // the mine's own edge fraction is skipped — a small flat blast reads more
    // honestly at aura size than a re-derived falloff curve.
    // Anti-recursion (the MS4/Cascade pattern): a kill landed BY a detonation
    // must not detonate again — one generation, then it stops.
    static bool bBreakerMachinistDetonating = false;
    if (bBreakerMachinistDetonating) return;

    ABreakerCharacter* Character = GetBreakerCharacter();
    UWorld* World = Character ? Character->GetWorld() : nullptr;
    const AActor* Victim = Hit.Target.Get();
    if (!Character || !World || !Victim) return;
    const UBreakerWeaponComponent* Weapon = Character->GetWeapon();
    const float BaseDamage = (Weapon ? Weapon->GetScaledBaseDamage() : 0.0f) * MachinistDetonationCoefficient;
    if (BaseDamage <= 0.0f) return;
    TGuardValue<bool> DetonationGuard(bBreakerMachinistDetonating, true);

    const UBreakerAttributeSet* OwnerAttributes = Character->GetAttributes();
    const FVector Center = Victim->GetActorLocation();
    for (TActorIterator<ABreakerEnemy> It(World); It; ++It)
    {
        ABreakerEnemy* Candidate = *It;
        if (!Candidate || Candidate == Victim) continue;
        UBreakerCombatComponent* CandidateCombat = Candidate->FindComponentByClass<UBreakerCombatComponent>();
        if (!CandidateCombat || CandidateCombat->IsDead()) continue;
        if (FVector::DistSquared(Center, Candidate->GetActorLocation()) > MachinistDetonationRadiusCm * MachinistDetonationRadiusCm) continue;

        FBreakerDamageRequest Damage;
        Damage.BaseDamage = BaseDamage;
        Damage.DamageFamily = EBreakerDamageFamily::Physical;
        Damage.ProcCoefficient = 0.5f;   // the turret's own discipline (§G3)
        Damage.CriticalChance = OwnerAttributes ? OwnerAttributes->GetCriticalChance() : UBreakerAttributeSet::DefaultCriticalChance;
        Damage.CriticalMultiplier = OwnerAttributes ? OwnerAttributes->GetCriticalMultiplier() : UBreakerAttributeSet::DefaultCriticalMultiplier;
        UBreakerDamageLibrary::FillSourcePools(OwnerAttributes, EBreakerDamageDelivery::Ability, Damage);
        Damage.SourceLocation = Center;
        Damage.bHasSourceLocation = true;
        Damage.SetInstigator(Character);
        // O34: every outgoing submission composes the modifier chain, so a
        // live window More counts inside the one ceiling (the Fracture bug's
        // rule, enforced by AbilitySubmissionConformance).
        if (UBreakerCombatComponent* OwnerCombat = Character ? Character->FindComponentByClass<UBreakerCombatComponent>() : nullptr)
        {
            OwnerCombat->ApplyOutgoingModifiers(Damage);
        }
        CandidateCombat->ReceiveDamage(Damage);
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
                if (bMachinistActive)
                {
                    Combat->OnKillDealt.RemoveDynamic(this, &UBreakerAbility_FieldAssembly::HandleMachinistKill);
                }
            }
            if (UBreakerAbilityStateComponent* State = Character->FindComponentByClass<UBreakerAbilityStateComponent>())
            {
                State->CloseWindow(WindowKey());
            }
        }
        bMachinistActive = false;
        if (MachinistAura && !MachinistAura->IsActorBeingDestroyed())
        {
            MachinistAura->Destroy();
        }
        MachinistAura = nullptr;
        if (UWorld* World = GetWorld())
        {
            World->GetTimerManager().ClearTimer(WindowTimer);
            World->GetTimerManager().ClearTimer(MachinistPulseTimer);
        }
    }
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
