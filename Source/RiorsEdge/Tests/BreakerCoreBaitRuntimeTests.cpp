#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerDamageLibrary.h"
#include "Combat/BreakerDeployable.h"
#include "Combat/BreakerEnemy.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Progression/BreakerClassDefinition.h"
#include "Progression/BreakerExperience.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionNode.h"
#include "Progression/BreakerProgressionTree.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerCoreBaitRuntimeTest, "RiorsEdge.Combat.Threat.CoreBait",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerCoreBaitRuntimeTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Init; Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!World) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
    World->InitializeActorsForPlay(FURL());
    auto PlayerAt = [&](FVector Location)
    {
        auto* P = World->SpawnActor<ABreakerCharacter>(Location, FRotator::ZeroRotator);
        P->GetAbilitySystemComponent()->InitAbilityActorInfo(P, P);
        P->GetAbilitySystemComponent()->AddAttributeSetSubobject(P->GetAttributes());
        P->GetCombat()->BindAttributes(P->GetAttributes());
        P->GetProgression()->BindAttributes(P->GetAttributes());
        return P;
    };
    auto* Player = PlayerAt(FVector(500,0,100));
    auto* Other = PlayerAt(FVector(900,0,100));
    auto* Progression = Player->GetProgression();
    auto* Tree = NewObject<UBreakerProgressionTree>(); Tree->TreeId = TEXT("Test.Core.Bait"); Tree->Currency = EBreakerPointCurrency::CorePoints;
    auto* Node = NewObject<UBreakerProgressionNode>(Tree); Node->NodeId = TEXT("Test.Core.Bait.Rule"); Node->Currency = Tree->Currency;
    Node->GrantedTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Progression.Node.Core.Threat.Bait"))); Tree->Nodes.Add(Node);
    auto* Class = NewObject<UBreakerClassDefinition>(); Class->ClassId = EBreakerClassId::Gunsmith; Class->BranchTrees.Add(Tree);
    if (!Progression->ChoosePermanentClass(Class)) return false;
    Progression->AwardExperience(UBreakerExperienceLibrary::TotalXpToReachLevel(2, Progression->ExperienceCurve));
    auto* Enemy = World->SpawnActor<ABreakerEnemy>(FVector(0,0,100), FRotator::ZeroRotator);
    Enemy->ConfigureCrowdProbe(); Enemy->DispatchBeginPlay(); Enemy->SetActorTickEnabled(false);
    auto* Victim = Enemy->FindComponentByClass<UBreakerCombatComponent>();
    auto Deploy = [&](AActor* Owner)
    {
        auto* D = World->SpawnActor<ABreakerDeployable>(FVector(700,200,100), FRotator::ZeroRotator);
        D->DispatchBeginPlay(); D->SetActorTickEnabled(false);
        D->InitializeDeployable(EBreakerDeployableType::Turret, Owner, 0);
        return D;
    };
    auto* Lure = Deploy(Player);
    auto Target = [&](AActor* RewardOwner, ABreakerDeployable* Producer)
    {
        Enemy->ClearThreat(); Victim->RestoreVitals();
        FBreakerDamageRequest Pull; Pull.BaseDamage = 1; Pull.bCanCritical = false; Pull.bCanBeAvoided = false;
        Pull.SetInstigator(RewardOwner); Pull.ThreatSource = Producer;
        Victim->ReceiveDamage(Pull); Enemy->Tick(0);
        return Enemy->GetThreatTarget() == Producer;
    };
    FBreakerDamageRequest Hit; Hit.BaseDamage = 10; Hit.bCanCritical = false; Hit.bCanBeAvoided = false; Hit.SetInstigator(Player);
    UBreakerDamageLibrary::AddSourceIncreased(Hit, 100);
    if (!TestTrue(TEXT("Actual deployable damage acquires enemy target"), Target(Player, Lure))) return false;
    TestEqual(TEXT("Unowned Bait has no effect"), Victim->ReceiveDamage(Hit).RawDamage, 20.f, .001f);
    FText Reason;
    if (!TestTrue(TEXT("Earned Core point buys Bait"), Progression->PurchaseNode(Tree, Node->NodeId, Reason))) return false;
    Target(Player, Lure);
    TestEqual(TEXT("Bait adds twelve Increased to existing source bucket"), Victim->ReceiveDamage(Hit).RawDamage, 21.2f, .001f);
    Hit.bIsDamageOverTime = true;
    TestEqual(TEXT("Funded periodic damage does not gain Bait twice"), Victim->ReceiveDamage(Hit).RawDamage, 20.f, .001f);
    Hit.bIsDamageOverTime = false;
    auto* Foreign = Deploy(Other); Target(Other, Foreign);
    TestEqual(TEXT("Another owner's deployable cannot activate Bait"), Victim->ReceiveDamage(Hit).RawDamage, 20.f, .001f);
    Target(Player, Lure); Lure->Destroy();
    TestEqual(TEXT("Destroyed lure immediately withdraws bonus before target refresh"), Victim->ReceiveDamage(Hit).RawDamage, 20.f, .001f);
    auto* Replacement = Deploy(Player); Target(Player, Replacement);
    if (!TestTrue(TEXT("Core respec succeeds"), Progression->RespecCore(Reason))) return false;
    TestEqual(TEXT("Respec withdraws Bait while target remains"), Victim->ReceiveDamage(Hit).RawDamage, 20.f, .001f);
    return true;
}
#endif
