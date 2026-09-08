#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Abilities/BreakerTankAbilities.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Classes/BreakerGritComponent.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerDamageLibrary.h"
#include "Combat/BreakerEnemy.h"
#include "Combat/BreakerRangedEnemy.h"
#include "Components/BoxComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Progression/BreakerClassDefinition.h"
#include "Progression/BreakerExperience.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionNode.h"
#include "Progression/BreakerProgressionTree.h"
#include "Weapons/BreakerWeaponComponent.h"
#include "Weapons/BreakerWeaponDefinition.h"
#include "TimerManager.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerCoreControlRuntimeTest, "RiorsEdge.Combat.CoreControlRuntime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerCoreControlRuntimeTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Init; Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!World) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); World->InitializeActorsForPlay(FURL());
    const uint64 InitialFrame = GFrameCounter;
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); GFrameCounter = InitialFrame; };
    auto* Player = World->SpawnActor<ABreakerCharacter>(); if (!Player) return false;
    Player->SetActorTickEnabled(false); auto* Move = Player->GetBreakerMovement(); Move->SetComponentTickEnabled(false); Move->bRunPhysicsWithNoController = true;
    auto* Attr = Player->GetAttributes(); auto* ASC = Player->GetAbilitySystemComponent();
    ASC->InitAbilityActorInfo(Player, Player); ASC->AddAttributeSetSubobject(Attr);
    Player->GetCombat()->BindAttributes(Attr); auto* Progression = Player->GetProgression(); Progression->BindAttributes(Attr);
    if (!Progression->ChoosePermanentClassById(EBreakerClassId::Tank)) return false;
    auto* Definition = DuplicateObject<UBreakerClassDefinition>(Progression->ClassDefinition, Player);
    auto* Tree = NewObject<UBreakerProgressionTree>(Definition); Tree->TreeId = TEXT("Test.Core.Control"); Tree->Currency = EBreakerPointCurrency::CorePoints;
    Definition->BranchTrees.Add(Tree); Progression->ClassDefinition = Definition;
    for (const TCHAR* Id : { TEXT("Lockstep"), TEXT("Shockwave"), TEXT("Interrupt"), TEXT("Numbers") })
    {
        auto* Node = NewObject<UBreakerProgressionNode>(Tree); Node->NodeId = Id; Node->Currency = Tree->Currency;
        if (FString(Id) != TEXT("Numbers")) Node->GrantedTags.AddTag(FGameplayTag::RequestGameplayTag(FName(FString(TEXT("Progression.Node.Core.Control.")) + Id)));
        else
        {
            FBreakerNodeEffect Duration; Duration.StatTarget = EBreakerNodeStatTarget::StaggerDuration;
            Duration.StatBucket = EBreakerNodeStatBucket::IncreasedPercent; Duration.ValuePerRank = 24; Node->Effects.Add(Duration);
            auto Resistance = Duration; Resistance.StatTarget = EBreakerNodeStatTarget::EnemyStaggerResistanceReduction; Resistance.ValuePerRank = 18; Node->Effects.Add(Resistance);
        }
        Tree->Nodes.Add(Node);
    }
    Progression->AwardExperience(UBreakerExperienceLibrary::TotalXpToReachLevel(6, Progression->ExperienceCurve));
    FText Reason;
    auto Buy = [&](const TCHAR* Id) { return TestTrue(TEXT("Core purchase spends level-earned points"), Progression->PurchaseNode(Tree, Id, Reason)); };
    auto Enemy = [&](FVector Location)
    {
        auto* E = World->SpawnActor<ABreakerEnemy>(Location, FRotator::ZeroRotator); E->SetActorTickEnabled(false);
        auto* EA = Cast<UBreakerAttributeSet>(E->GetDefaultSubobjectByName(TEXT("Attributes")));
        auto* EASC = E->GetAbilitySystemComponent(); EASC->InitAbilityActorInfo(E, E); EASC->AddAttributeSetSubobject(EA);
        EA->ApplyMaxHealth(100000); EA->ApplyHealth(100000);
        EASC->SetNumericAttributeBase(UBreakerAttributeSet::GetArmorAttribute(), 0);
        auto* C = E->FindComponentByClass<UBreakerCombatComponent>(); C->BindAttributes(EA); C->SetComponentTickEnabled(false);
        return E;
    };
    auto* Primary = Enemy(FVector(450,0,100)); auto* PrimaryCombat = Primary->FindComponentByClass<UBreakerCombatComponent>();
    Primary->bStaggerImmune = true;
    TestFalse(TEXT("Unowned source cannot bypass enemy immunity"), PrimaryCombat->ApplyStaggerFrom(Player, 1));
    if (!Buy(TEXT("Lockstep"))) return false;
    PrimaryCombat->StaggerResistance = 1;
    TestFalse(TEXT("Lockstep alone does not bypass numeric resistance"), PrimaryCombat->ApplyStaggerFrom(Player, 1));
    PrimaryCombat->StaggerResistance = .5f;
    TestTrue(TEXT("Owned Lockstep bypasses enemy boolean immunity"), PrimaryCombat->ApplyStaggerFrom(Player, 1));
    TestEqual(TEXT("Immunity halves resistance-adjusted duration"), PrimaryCombat->GetStaggerRemaining(), .25f, .001f);
    PrimaryCombat->RestoreVitals();
    Primary->bStaggerImmune = false; PrimaryCombat->GrantStaggerImmunity(10);
    TestTrue(TEXT("Owned Lockstep also bypasses timed enemy immunity"), PrimaryCombat->ApplyStaggerFrom(Player, 1));
    PrimaryCombat->RestoreVitals(); Primary->bStaggerImmune = true;
    Player->GetCombat()->GrantStaggerImmunity(10);
    TestFalse(TEXT("Player landing protection cannot be bypassed"), Player->GetCombat()->ApplyStaggerFrom(Player, 1));
    Player->GetCombat()->RestoreVitals();
    if (!Buy(TEXT("Numbers")) || !Buy(TEXT("Shockwave")) || !Buy(TEXT("Interrupt"))) return false;
    auto* Secondary = Enemy(FVector(700,0,100)); auto* SecondaryCombat = Secondary->FindComponentByClass<UBreakerCombatComponent>();
    auto* Third = Enemy(FVector(950,0,100)); auto* ThirdCombat = Third->FindComponentByClass<UBreakerCombatComponent>();
    auto* Floor = World->SpawnActor<AActor>(); auto* Ground = NewObject<UBoxComponent>(Floor);
    Floor->AddInstanceComponent(Ground); Floor->SetRootComponent(Ground); Ground->SetBoxExtent(FVector(2000,2000,10));
    Ground->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics); Ground->SetCollisionResponseToAllChannels(ECR_Block); Ground->RegisterComponent();
    Floor->SetActorLocation(FVector(0,0,-10));
    auto* Grit = Player->GetGrit(); Grit->BindAttributes(Attr); Grit->SetComponentTickEnabled(false);
    Grit->SetInCombat(true);
    for (int32 Second = 0; Second < 70; ++Second) { Grit->SetEnemyInProximity(true); Grit->AdvanceLoop(1); }
    const auto Plunge = ASC->GiveAbility(FGameplayAbilitySpec(UBreakerAbility_GroundZero::StaticClass(), 1));
    Player->TeleportTo(FVector(0,0,400), FRotator::ZeroRotator, false, true);
    Move->SetMovementMode(MOVE_Falling); Move->Velocity = FVector(0,0,-100);
    const float Before = Attr->GetClassResource();
    if (!TestTrue(TEXT("Actual paid Ground Zero starts"), ASC->TryActivateAbility(Plunge))) return false;
    TestTrue(TEXT("Native plunge consumes earned Grit"), Attr->GetClassResource() < Before);
    for (int32 Step = 0; Step < 30 && Move->IsFalling(); ++Step)
    {
        ++GFrameCounter; World->Tick(LEVELTICK_All, .05f);
        if (!World->GetTimerManager().HasBeenTickedThisFrame()) World->GetTimerManager().Tick(.05f);
        Move->PerformMovement(.05f);
    }
    if (!TestTrue(TEXT("Actual physical landing occurs"), Move->IsMovingOnGround())) return false;
    TestTrue(TEXT("Paid landing bypasses primary immunity"), PrimaryCombat->IsStaggered());
    const float Duration = 1.5f * 1.24f * .68f * .5f;
    TestTrue(TEXT("Duration and resistance compose once on actual landing"), PrimaryCombat->GetStaggerRemaining() > Duration - .11f
        && PrimaryCombat->GetStaggerRemaining() <= Duration + .001f);
    TestTrue(TEXT("Shockwave reaches nearest enemy outside slam radius"), SecondaryCombat->IsStaggered());
    TestFalse(TEXT("Secondary stagger cannot chain to third enemy"), ThirdCombat->IsStaggered());
    TestEqual(TEXT("Secondary receives source duration once"), SecondaryCombat->GetStaggerRemaining(), 1.5f * 1.24f, .11f);
    // A real fired rifle exercises Interrupt's target-side Increased composition.
    Player->SetActorLocation(FVector(0,0,100)); FVector Eye; FRotator Aim; Player->GetActorEyesViewPoint(Eye, Aim);
    Primary->SetActorLocation(Eye + Aim.Vector() * 450);
    auto* Weapon = Player->GetWeapon(); Weapon->WeaponDefinition = DuplicateObject<UBreakerWeaponDefinition>(Weapon->GetActiveDefinition(), Weapon);
    Weapon->WeaponDefinition->HipSpreadDegrees = Weapon->WeaponDefinition->AimSpreadDegrees = 0; Weapon->WeaponDefinition->BleedChance = 0;
    Weapon->ResetAmmunition(); ASC->SetNumericAttributeBase(UBreakerAttributeSet::GetCriticalChanceAttribute(),0);
    Weapon->StartFire(); Weapon->StopFire();
    FBreakerDamageRequest Expected; Expected.BaseDamage = Weapon->GetItemLevelBaseDamage(); Expected.bCanCritical = false;
    UBreakerDamageLibrary::FillSourcePools(Attr, EBreakerDamageDelivery::Weapon, Expected);
    UBreakerDamageLibrary::AddSourceIncreased(Expected, 15);
    TestEqual(TEXT("Actual rifle adds Interrupt to its source bucket"), Weapon->GetLastShot().DamageResult.RawDamage,
        UBreakerDamageLibrary::ResolveDamage(Expected, {}).RawDamage, .05f);
    FBreakerDamageRequest Tick; Tick.BaseDamage = 100; Tick.bCanCritical = false; Tick.bIsDamageOverTime = true; Tick.SetInstigator(Player);
    UBreakerDamageLibrary::AddSourceIncreased(Tick, 100);
    TestEqual(TEXT("Periodic snapshot does not acquire Interrupt"), PrimaryCombat->ReceiveDamage(Tick).RawDamage, 200.0f, .01f);
    Tick.bIsDamageOverTime = false;
    TestEqual(TEXT("Direct hit adds fifteen Increased rather than fifteen More"), PrimaryCombat->ReceiveDamage(Tick).RawDamage, 215.0f, .01f);
    if (!TestTrue(TEXT("Real Core respec succeeds"), Progression->RespecCore(Reason))) return false;
    TestEqual(TEXT("Existing stagger does not retain removed attacker bonus"), PrimaryCombat->ReceiveDamage(Tick).RawDamage, 200.0f, .01f);
    PrimaryCombat->RestoreVitals();
    TestFalse(TEXT("Respec withdraws Lockstep on next application"), PrimaryCombat->ApplyStaggerFrom(Player,1));
    return true;
}
#endif
