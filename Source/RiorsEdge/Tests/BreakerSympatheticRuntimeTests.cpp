#include "Misc/AutomationTest.h"
#include "Tests/BreakerCastTestHelpers.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Abilities/BreakerSupportAbilities.h"
#include "Abilities/BreakerAbilityDefinition.h"
#include "Abilities/BreakerAbilityStateComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Classes/BreakerChargeComponent.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerStatusComponent.h"
#include "Combat/BreakerEntropy.h"
#include "Combat/BreakerZoneActor.h"
#include "Abilities/BreakerAbility_Rot.h"
#include "Components/SphereComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Items/BreakerEquipmentComponent.h"
#include "Items/BreakerLootLibrary.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionLibrary.h"
#include "Progression/BreakerProgressionTree.h"
#include "Progression/BreakerExperience.h"
#include "Save/BreakerMissionContent.h"
#include "TimerManager.h"
#include "Weapons/BreakerWeaponComponent.h"
#include "Weapons/BreakerWeaponDefinition.h"
#include "Weapons/BreakerRocketProjectile.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerSympatheticRuntimeTest, "RiorsEdge.Abilities.SympatheticRuntime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerSympatheticRuntimeTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Init;
    Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!TestNotNull(TEXT("isolated attunement world"), World)) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL());
    const uint64 InitialFrame = GFrameCounter;
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); GFrameCounter = InitialFrame; };
    auto MakePlayer = [&](FVector Location, EBreakerClassId Class)
    {
        FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        auto* Player = World->SpawnActor<ABreakerCharacter>(Location, FRotator::ZeroRotator, Spawn);
        if (!Player) return Player;
        auto* ASC = Player->GetAbilitySystemComponent();
        ASC->InitAbilityActorInfo(Player, Player); ASC->AddAttributeSetSubobject(Player->GetAttributes());
        Player->GetCombat()->BindAttributes(Player->GetAttributes());
        Player->GetProgression()->BindAttributes(Player->GetAttributes());
        Player->GetEquipment()->BindAttributes(Player->GetAttributes());
        TestTrue(TEXT("real permanent class choice"), Player->GetProgression()->ChoosePermanentClassById(Class));
        Player->GetBreakerMovement()->SetComponentTickEnabled(false);
        Player->FindComponentByClass<UBreakerChargeComponent>()->BindAttributes(Player->GetAttributes());
        Player->GetAttributes()->ApplyClassResource(100);
        return Player;
    };
    auto* Source = MakePlayer(FVector(0, 200, 100), EBreakerClassId::Support);
    auto* Other = MakePlayer(FVector(0, -200, 100), EBreakerClassId::Support);
    auto* Ally = MakePlayer(FVector(0, 0, 100), EBreakerClassId::Caster);
    if (!Source || !Other || !Ally) return false;
    auto Advance = [&](int32 Steps)
    {
        for (int32 I = 0; I < Steps; ++I)
        {
            TArray<UBreakerAbilityStateComponent*> States;
            for (TActorIterator<ABreakerCharacter> It(World); It; ++It)
                if (auto* State = It->FindComponentByClass<UBreakerAbilityStateComponent>())
                { State->SetComponentTickEnabled(false); States.Add(State); }
            ++GFrameCounter; World->Tick(LEVELTICK_All, .05f);
            for (auto* State : States) State->AdvanceTime(.05f);
            if (!World->GetTimerManager().HasBeenTickedThisFrame()) World->GetTimerManager().Tick(.05f);
        }
    };
    auto Purchase = [&](ABreakerCharacter* Player, int32 Rank)
    {
        auto* Progression = Player->GetProgression();
        // Restored authored campaign completion, never extra Doctrine points.
        Progression->AwardExperience(UBreakerExperienceLibrary::TotalXpToReachLevel(50, FBreakerExperienceCurve()));
        FBreakerQuestFlagSet Flags;
        for (const auto& Mission : UBreakerMissionLibrary::GetMissions())
            for (const auto& Beat : Mission.Beats)
                for (FName Flag : UBreakerMissionLibrary::BeatCompletionFlags(Beat)) Flags.Add(Flag);
        Flags.Add(TEXT("Quest.Finale.Seal")); Progression->SettleDoctrineEntitlement(Flags);
        FText Failure;
        auto* Tree = UBreakerProgressionLibrary::GetSupportConductorTree();
        for (int32 I = 0; I < 2; ++I)
            if (!TestTrue(TEXT("purchase real Section prerequisite and tier investment"), Progression->PurchaseNode(Tree, TEXT("Support.Conductor.Section"), Failure))) return false;
        for (int32 I = 0; I < Rank; ++I)
            if (!TestTrue(TEXT("purchase actual Attunement rank"), Progression->PurchaseNode(Tree, TEXT("Support.Conductor.Attunement"), Failure))) return false;
        Player->FindComponentByClass<UBreakerChargeComponent>()->BindAttributes(Player->GetAttributes());
        return true;
    };
    auto CastBuff = [&](ABreakerCharacter* Player, TSubclassOf<UBreakerGameplayAbility> Ability)
    {
        Player->GetAttributes()->ApplyClassResource(100);
        auto* ASC = Player->GetAbilitySystemComponent();
        // Reuse the same isolated recipient rig across lifecycle cases. Clear
        // only this ability's cooldown; cooldown behavior is not this fixture.
        FGameplayTagContainer Cooldown;
        Cooldown.AddTag(Ability->GetDefaultObject<UBreakerGameplayAbility>()->GetAbilityDefinition()->CooldownTag);
        ASC->RemoveActiveEffectsWithGrantedTags(Cooldown);
        const auto Handle = ASC->GiveAbility(FGameplayAbilitySpec(Ability, 1));
        TestTrue(TEXT("native buff activation commits"), ASC->TryActivateAbility(Handle));
        BreakerResolvePendingCast(World, Player);
        return Handle;
    };
    auto FinishSympathetic = [&](ABreakerCharacter* Player)
    {
        FText Failure;
        auto* P = Player->GetProgression();
        auto* Tree = UBreakerProgressionLibrary::GetSupportConductorTree();
        for (int32 I = 0; I < 2; ++I)
            if (!TestTrue(TEXT("real tier-four investment"), P->PurchaseNode(Tree, TEXT("Support.Conductor.Sustain"), Failure))) return false;
        if (!TestTrue(TEXT("real Sympathetic purchase"), P->PurchaseNode(Tree, TEXT("Support.Conductor.SympatheticResonance"), Failure))) return false;
        TestEqual(TEXT("complete build spends exactly eight earned Doctrine"), P->GetUnspentPoints(EBreakerPointCurrency::DoctrinePoints), 0);
        return true;
    };
    if (!Purchase(Source, 2) || !Purchase(Other, 2)) return false;
    Advance(1);
    auto FirstCast = CastBuff(Source, UBreakerAbility_Cadence::StaticClass());
    auto* AllyState = Ally->FindComponentByClass<UBreakerAbilityStateComponent>();
    if (!TestNotNull(TEXT("actual ally state"), AllyState)) return false;
    TestFalse(TEXT("Attunement alone grants no Sympathetic"), AllyState->HasActiveSympatheticAttunement());
    if (!FinishSympathetic(Source) || !FinishSympathetic(Other)) return false;
    TestTrue(TEXT("purchased node consumes an actual maintained buff"), AllyState->HasActiveSympatheticAttunement());

    struct FTarget { AActor* Actor; UBreakerCombatComponent* Combat; UBreakerStatusComponent* Status; UBreakerAttributeSet* Health; };
    auto MakeTarget = [&](FVector Location, float MaxHealth = 1000)
    {
        FTarget Target{ World->SpawnActor<AActor>(), nullptr, nullptr };
        auto* Body = NewObject<USphereComponent>(Target.Actor); Target.Actor->AddInstanceComponent(Body); Target.Actor->SetRootComponent(Body);
        Body->SetSphereRadius(40); Body->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        Body->SetCollisionResponseToAllChannels(ECR_Ignore); Body->SetCollisionResponseToChannel(ECC_GameTraceChannel2, ECR_Block); Body->RegisterComponent();
        Target.Actor->SetActorLocation(Location);
        Target.Combat = NewObject<UBreakerCombatComponent>(Target.Actor); Target.Actor->AddInstanceComponent(Target.Combat); Target.Combat->RegisterComponent();
        auto* Health = NewObject<UBreakerAttributeSet>(Target.Actor); Health->ApplyMaxHealth(MaxHealth); Health->ApplyHealth(MaxHealth); Target.Combat->BindAttributes(Health); Target.Health = Health;
        Target.Status = NewObject<UBreakerStatusComponent>(Target.Actor); Target.Actor->AddInstanceComponent(Target.Status); Target.Status->RegisterComponent();
        Target.Status->BeginPlay(); Target.Status->SetComponentTickEnabled(false);
        return Target;
    };
    auto Snapshot = [&](ABreakerCharacter* Player, float Damage = 10, float Proc = 1)
    {
        FBreakerDamageRequest Hit; Hit.BaseDamage = Damage; Hit.ProcCoefficient = Proc;
        Hit.Element = EBreakerElement::Entropy; Hit.ElementalFraction = 1; Hit.bCanCritical = false; Hit.SetInstigator(Player);
        Player->FindComponentByClass<UBreakerAbilityStateComponent>()->SnapshotSympatheticEntropy(Hit);
        return Hit;
    };
    const auto Hit = Snapshot(Ally);
    TestEqual(TEXT("shipped flat buildup"), Hit.ElementBuildupFlat, 2.0f);
    TestEqual(TEXT("shipped fade time"), Hit.ElementBuildupFadeSeconds, 4.0f);
    const auto SecondCast = CastBuff(Other, UBreakerAbility_Cadence::StaticClass());
    TestEqual(TEXT("overlapping buffs cannot multiply bonus"), Snapshot(Ally).ElementBuildupFlat, Hit.ElementBuildupFlat);
    Other->GetAbilitySystemComponent()->CancelAbilityHandle(SecondCast);
    TestTrue(TEXT("other maintained source remains eligible"), AllyState->HasActiveSympatheticAttunement());

    auto Low = MakeTarget(FVector(4000, 0, 0)), High = MakeTarget(FVector(4200, 0, 0));
    Low.Combat->ReceiveDamage(Snapshot(Ally, 10));
    High.Combat->ReceiveDamage(Snapshot(Ally, 40));
    TestEqual(TEXT("low-damage bonus is flat"), Low.Status->GetEntropyBuildup() - 10, 2.0f);
    TestEqual(TEXT("high-damage bonus is identical"), High.Status->GetEntropyBuildup() - 40, 2.0f);
    auto Zero = MakeTarget(FVector(4400, 0, 0));
    Zero.Combat->PushIncomingDamageModifier(TEXT("FullMitigationFixture"), 0);
    TestEqual(TEXT("actual fully mitigated hit deals zero"), Zero.Combat->ReceiveDamage(Hit).HealthDamage, 0.0f);
    TestEqual(TEXT("landed zero-damage hit still earns only flat buildup"), Zero.Status->GetEntropyBuildup(), 2.0f);
    auto Resist = MakeTarget(FVector(4600, 0, 0)); Resist.Status->EntropyResistancePercent = 50;
    const auto Resisted = Resist.Combat->ReceiveDamage(Snapshot(Ally, 10, .5f));
    TestEqual(TEXT("resistance never changes hit damage"), Resisted.HealthDamage, 10.0f);
    TestEqual(TEXT("proc and resistance weight ordinary and flat buildup once"), Resist.Status->GetEntropyBuildup(), 3.0f);

    auto Refused = MakeTarget(FVector(4800, 0, 0));
    Refused.Combat->DodgeChance = 1; TestTrue(TEXT("actual dodge"), Refused.Combat->ReceiveDamage(Hit).bDodged);
    Refused.Combat->DodgeChance = 0;
    Refused.Status->GrantStatusImmunity(1); Refused.Combat->ReceiveDamage(Hit); Refused.Status->AdvanceStatuses(1);
    Refused.Combat->ReceiveDamage(Snapshot(Ally, 0)); Refused.Combat->ReceiveDamage(Snapshot(Ally, 10, 0));
    auto Dot = Hit; Dot.bIsDamageOverTime = true; Refused.Combat->ReceiveDamage(Dot);
    TestEqual(TEXT("dodge immunity zero raw zero proc and DoT refuse bonus"), Refused.Status->GetEntropyBuildup(), 0.0f);

    // Each attacker owns the lifetime of their protected contribution.
    auto Mixed = MakeTarget(FVector(5000, 0, 0));
    Mixed.Combat->ReceiveDamage(Hit); Mixed.Status->AdvanceStatuses(3);
    auto Plain = Hit; Plain.ElementBuildupFlat = 0; Plain.ElementBuildupFadeSeconds = 0;
    Mixed.Combat->ReceiveDamage(Plain);
    Mixed.Combat->ReceiveDamage(Snapshot(Source));
    Mixed.Status->AdvanceStatuses(3);
    TestEqual(TEXT("older ally contribution fades independently of newer hits"), Mixed.Status->GetEntropyBuildup(), 28.0f, .001f);
    Mixed.Status->AdvanceStatuses(1);
    TestEqual(TEXT("ordinary buildup clears at four seconds while protected amounts fade"), Mixed.Status->GetEntropyBuildup(), 15.0f, .001f);
    Mixed.Status->AdvanceStatuses(4);
    TestEqual(TEXT("every protected contribution eventually clears"), Mixed.Status->GetEntropyBuildup(), 0.0f);
    auto Whole = BreakerBuildup::Advance({12, 4, 4}, 6);
    BreakerBuildup::FDecayState Split{12, 4, 4};
    for (int32 I = 0; I < 120; ++I) Split = BreakerBuildup::Advance(Split, .05f);
    TestEqual(TEXT("fade is independent of frame partition"), Split.Amount, Whole.Amount, .001f);

    auto Trigger = MakeTarget(FVector(5200, 0, 0));
    for (int32 I = 0; I < 9; ++I) Trigger.Combat->ReceiveDamage(Hit);
    const auto Rot = FGameplayTag::RequestGameplayTag(TEXT("Status.Rot"));
    TestTrue(TEXT("bonus reaches real Rot threshold sooner"), Trigger.Status->HasStatus(Rot));
    if (!TestEqual(TEXT("one earned Rot"), Trigger.Status->GetActiveStatuses().Num(), 1)) return false;
    TestEqual(TEXT("bonus never enters Rot's damage snapshot"), Trigger.Status->GetActiveStatuses()[0].Spec.BaseDamagePerTick * 8, 5.0f);
    TestEqual(TEXT("threshold consumes every buildup cohort"), Trigger.Status->GetEntropyBuildup(), 0.0f);
    Trigger.Combat->ReceiveDamage(Hit);
    TestEqual(TEXT("active Rot refuses further buildup"), Trigger.Status->GetEntropyBuildup(), 0.0f);
    Trigger.Status->AdvanceStatuses(4);
    TestEqual(TEXT("Rot ticks cannot create bonus buildup"), Trigger.Status->GetEntropyBuildup(), 0.0f);

    // Exercise the actual weapon trace, rather than only a snapshotted request.
    auto ShotTarget = MakeTarget(FVector::ZeroVector);
    auto* Weapon = Ally->GetWeapon();
    Weapon->WeaponDefinition = DuplicateObject<UBreakerWeaponDefinition>(Weapon->GetActiveDefinition(), Weapon);
    Weapon->WeaponDefinition->HipSpreadDegrees = 0; Weapon->WeaponDefinition->AimSpreadDegrees = 0;
    Weapon->WeaponDefinition->Recoil.BloomPerShotDegrees = 0; Weapon->WeaponDefinition->BleedChance = 0;
    FVector Eye; FRotator Aim; Ally->GetActorEyesViewPoint(Eye, Aim); ShotTarget.Actor->SetActorLocation(Eye + Aim.Vector() * 500);
    Ally->GetAbilitySystemComponent()->SetNumericAttributeBase(UBreakerAttributeSet::GetCriticalChanceAttribute(), 0);
    const float BeforeShot = ShotTarget.Health->GetHealth();
    Weapon->ResetAmmunition(); const int32 Ammo = Weapon->GetMagazineAmmo(); Weapon->StartFire(); Weapon->StopFire();
    TestEqual(TEXT("actual weapon spends one round"), Weapon->GetMagazineAmmo(), Ammo - 1);
    const float ActualDamage = BeforeShot - ShotTarget.Health->GetHealth();
    TestTrue(TEXT("actual hitscan lands"), ActualDamage > 0);
    TestEqual(TEXT("actual hitscan carries flat buildup payload"), ShotTarget.Status->GetEntropyBuildup(), ActualDamage + 2, .001f);

    // Entropy ability casts capture the same node without converting physical abilities.
    CastBuff(Ally, UBreakerAbility_Rot::StaticClass());
    ABreakerZoneActor* Zone = nullptr;
    for (TActorIterator<ABreakerZoneActor> It(World); It; ++It) if (It->GetZoneInstigator() == Ally) { Zone = *It; break; }
    if (!TestNotNull(TEXT("real buffed Caster creates Rot zone"), Zone)) return false;
    ShotTarget.Actor->SetActorLocation(Zone->GetActorLocation());
    auto* ShotBody = Cast<USphereComponent>(ShotTarget.Actor->GetRootComponent());
    ShotBody->SetCollisionObjectType(ECC_Pawn); ShotBody->SetCollisionResponseToAllChannels(ECR_Block);
    Source->GetAbilitySystemComponent()->CancelAbilityHandle(FirstCast);
    TestFalse(TEXT("cancellation removes current Sympathetic eligibility"), AllyState->HasActiveSympatheticAttunement());
    const float BeforeZone = ShotTarget.Status->GetEntropyBuildup(), BeforeZoneHealth = ShotTarget.Health->GetHealth();
    Zone->AdvanceZone(.5f);
    const float ZoneDamage = BeforeZoneHealth - ShotTarget.Health->GetHealth();
    TestTrue(TEXT("actual zone tick lands"), ZoneDamage > 0);
    TestEqual(TEXT("actual cast retains snapshotted bonus after buff cancellation"), ShotTarget.Status->GetEntropyBuildup() - BeforeZone, ZoneDamage + 2, .001f);

    FirstCast = CastBuff(Source, UBreakerAbility_Cadence::StaticClass());
    FBreakerItemInstance RocketItem;
    bool bRolledRocket = false;
    for (int32 Seed = 1; Seed <= 1000 && !bRolledRocket; ++Seed)
    {
        RocketItem = UBreakerLootLibrary::RollItem(TEXT("Sympathetic.Runtime"), EBreakerEquipSlot::Primary, EBreakerItemRarity::Standard, 1, Seed);
        bRolledRocket = RocketItem.WeaponArchetype == EBreakerWeaponArchetype::Rocket;
    }
    if (!TestTrue(TEXT("real level-one rocket roll"), bRolledRocket)) return false;
    for (auto& Affix : RocketItem.Affixes) Affix.Value = 0; // Isolate node from gear riders.
    TestTrue(TEXT("actual rocket equip"), Ally->GetEquipment()->EquipItem(RocketItem));
    Weapon->WeaponDefinition = nullptr; Weapon->SyncArchetypesToEquipment(); Weapon->EquipSlot(1);
    const auto* Active = Weapon->GetActiveDefinition();
    Advance(FMath::CeilToInt((FMath::Max(Active->SwapInDuration / Weapon->GetSwapSpeedMultiplier(),
        60.0f / FMath::Max(1.0f, Weapon->GetEffectiveRoundsPerMinute(Active))) + .1f) / .05f));
    Weapon->ResetAmmunition(); Weapon->StartFire(); Weapon->StopFire();
    ABreakerRocketProjectile* Rocket = nullptr;
    for (TActorIterator<ABreakerRocketProjectile> It(World); It; ++It) if (!It->HasExploded()) { Rocket = *It; break; }
    if (!TestNotNull(TEXT("real Sympathetic rocket launch"), Rocket)) return false;
    if (auto* Movement = Rocket->FindComponentByClass<UProjectileMovementComponent>()) { Movement->StopMovementImmediately(); Movement->Deactivate(); }
    Rocket->SetActorEnableCollision(false);
    auto RocketTarget = MakeTarget(FVector(6000, 0, 0), 10000);
    Source->GetAbilitySystemComponent()->CancelAbilityHandle(FirstCast);
    TestFalse(TEXT("current node is gone before projectile impact"), AllyState->HasActiveSympatheticAttunement());
    const float BeforeRocket = RocketTarget.Health->GetHealth(); Rocket->Explode(RocketTarget.Actor->GetActorLocation());
    const float RocketDamage = BeforeRocket - RocketTarget.Health->GetHealth();
    TestTrue(TEXT("actual rocket explosion lands"), RocketDamage > 0);
    TestEqual(TEXT("rocket retains firing-time flat payload"), RocketTarget.Status->GetEntropyBuildup(), RocketDamage + 2, .001f);
    FirstCast = CastBuff(Source, UBreakerAbility_Cadence::StaticClass());
    Ally->SetActorLocation(FVector(0, 3000, 100)); Advance(2);
    TestEqual(TEXT("rank two Attunement tail remains"), AllyState->GetWeaponEntropyConversionFraction(), 1.0f);
    TestFalse(TEXT("Attunement-only tail grants no Sympathetic"), AllyState->HasActiveSympatheticAttunement());
    Ally->SetActorLocation(FVector(0, 0, 100)); Advance(2);
    TestTrue(TEXT("actual reentry restores node"), AllyState->HasActiveSympatheticAttunement());
    FText Failure;
    TestTrue(TEXT("real source Forge respec"), Source->GetProgression()->RespecAtForge(EBreakerPointCurrency::DoctrinePoints, true, Failure));
    TestFalse(TEXT("respec immediately removes eligibility"), AllyState->HasActiveSympatheticAttunement());
    FBreakerDamageRequest Lethal; Lethal.BaseDamage = 100000; Lethal.bCanCritical = false;
    Low.Combat->ReceiveDamage(Lethal);
    TestEqual(TEXT("target death clears protected buildup"), Low.Status->GetEntropyBuildup(), 0.0f);
    return true;
}
#endif
