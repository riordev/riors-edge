#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerDeployable.h"
#include "Combat/BreakerEnemy.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Progression/BreakerClassDefinition.h"
#include "Progression/BreakerExperience.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionNode.h"
#include "Progression/BreakerProgressionTree.h"
#include "Tests/BreakerThreatRuntimeObserver.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerIgnoreMeRuntimeTest, "RiorsEdge.Combat.Threat.IgnoreMeOwnedSources",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerIgnoreMeRuntimeTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Init; Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!World) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
    World->InitializeActorsForPlay(FURL());
    auto MakePlayer = [&](FVector Position)
    {
        auto* Player = World->SpawnActor<ABreakerCharacter>(Position, FRotator::ZeroRotator);
        if (!Player) return Player;
        Player->SetActorTickEnabled(false); Player->GetBreakerMovement()->SetComponentTickEnabled(false);
        Player->GetAbilitySystemComponent()->InitAbilityActorInfo(Player, Player);
        Player->GetAbilitySystemComponent()->AddAttributeSetSubobject(Player->GetAttributes());
        Player->GetCombat()->BindAttributes(Player->GetAttributes());
        Player->GetProgression()->BindAttributes(Player->GetAttributes());
        return Player;
    };
    auto* Owner = MakePlayer(FVector(500, 0, 100));
    auto* Other = MakePlayer(FVector(900, 0, 100));
    auto* Enemy = World->SpawnActor<ABreakerEnemy>(FVector(0, 0, 100), FRotator::ZeroRotator);
    if (!Owner || !Other || !Enemy) return false;
    Enemy->ConfigureCrowdProbe(); Enemy->DispatchBeginPlay(); Enemy->SetActorTickEnabled(false);
    auto* Victim = Enemy->FindComponentByClass<UBreakerCombatComponent>();
    auto* Progression = Owner->GetProgression();
    auto* Tree = NewObject<UBreakerProgressionTree>(); Tree->TreeId = TEXT("Test.Core.IgnoreMe"); Tree->Currency = EBreakerPointCurrency::CorePoints;
    auto* Rule = NewObject<UBreakerProgressionNode>(Tree); Rule->NodeId = TEXT("Test.Core.IgnoreMe.Rule"); Rule->Currency = Tree->Currency;
    Rule->GrantedTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Progression.Node.Core.Threat.IgnoreMe"))); Tree->Nodes.Add(Rule);
    auto* Numeric = NewObject<UBreakerProgressionNode>(Tree); Numeric->NodeId = TEXT("Test.Core.IgnoreMe.Numeric"); Numeric->Currency = Tree->Currency;
    FBreakerNodeEffect Effect; Effect.StatTarget = EBreakerNodeStatTarget::ThreatGenerated;
    Effect.StatBucket = EBreakerNodeStatBucket::IncreasedPercent; Effect.ValuePerRank = 50;
    Numeric->Effects.Add(Effect); Tree->Nodes.Add(Numeric);
    auto* Class = NewObject<UBreakerClassDefinition>(); Class->ClassId = EBreakerClassId::Gunsmith; Class->BranchTrees.Add(Tree);
    if (!Progression->ChoosePermanentClass(Class)) return false;
    Progression->AwardExperience(UBreakerExperienceLibrary::TotalXpToReachLevel(3, Progression->ExperienceCurve));
    auto Damage = [&](AActor* RewardOwner, AActor* Producer, float Amount)
    {
        FBreakerDamageRequest Hit; Hit.BaseDamage = Amount; Hit.DamageFamily = EBreakerDamageFamily::TrueDamage;
        Hit.bCanCritical = false; Hit.bCanBeAvoided = false; Hit.SetInstigator(RewardOwner); Hit.ThreatSource = Producer;
        return Victim->ReceiveDamage(Hit);
    };
    auto Select = [&]() { Enemy->Tick(0); return Enemy->GetThreatTarget(); };
    Damage(Owner, nullptr, 12); Damage(Other, nullptr, 10);
    TestTrue(TEXT("Ordinary accepted damage selects the larger owner ledger"), Select() == Owner);
    FText Reason;
    if (!TestTrue(TEXT("Earned point purchases Ignore Me"), Progression->PurchaseNode(Tree, Rule->NodeId, Reason))) return false;
    TestTrue(TEXT("Purchase suppresses existing personal credit immediately at selection"), Select() == Other);
    const auto Personal = Damage(Owner, nullptr, 5);
    TestTrue(TEXT("Ignored player still deals real damage"), Personal.HealthDamage + Personal.ShieldDamage > 0);
    TestTrue(TEXT("New personal damage cannot regain threat while owned"), Select() == Other);
    auto Deploy = [&](ABreakerCharacter* Creator, FVector Position)
    {
        auto* Actor = World->SpawnActor<ABreakerDeployable>(Position, FRotator::ZeroRotator);
        if (!Actor) return Actor;
        Actor->DispatchBeginPlay(); Actor->SetActorTickEnabled(false);
        Actor->InitializeDeployable(EBreakerDeployableType::Turret, Creator, 0);
        return Actor;
    };
    auto* Owned = Deploy(Owner, FVector(700, 100, 100));
    auto* Foreign = Deploy(Other, FVector(750, -100, 100));
    if (!Owned || !Foreign) return false;
    auto* Observer = NewObject<UBreakerThreatRuntimeObserver>();
    Owner->GetCombat()->OnHitDealt.AddDynamic(Observer, &UBreakerThreatRuntimeObserver::Observe);
    Damage(Owner, Owned, 6);
    TestTrue(TEXT("Owned producer earns twelve threat from six accepted damage"), Select() == Owned);
    TestTrue(TEXT("Reward attribution remains the player"), Observer->LastRewardOwner == Owner);
    TestTrue(TEXT("Threat attribution remains the direct producer"), Observer->LastThreatSource == Owned);
    Owned->Destroy();
    TestTrue(TEXT("Destroyed deployable is pruned without restoring suppressed personal credit"), Select() == Other);
    if (!TestTrue(TEXT("Native Core respec succeeds"), Progression->RespecCore(Reason))) return false;
    TestTrue(TEXT("Respec restores only the twelve previously earned personal credit"), Select() == Owner);
    Damage(Other, nullptr, 3);
    TestTrue(TEXT("Damage dealt while ignored never enters the retained personal ledger"), Select() == Other);

    if (!Progression->PurchaseNode(Tree, Rule->NodeId, Reason) || !Progression->PurchaseNode(Tree, Numeric->NodeId, Reason)) return false;
    Enemy->ClearThreat();
    Owned = Deploy(Owner, FVector(700, 100, 100));
    if (!Owned) return false;
    Damage(Other, nullptr, 15); Damage(Owner, Owned, 6);
    TestTrue(TEXT("Owner Increased and rule doubling compose once: eighteen beats fifteen"), Select() == Owned);
    Damage(Other, nullptr, 4);
    TestTrue(TEXT("No extra application of Increased or doubling: nineteen beats eighteen"), Select() == Other);
    Enemy->ClearThreat();
    Damage(Other, nullptr, 10); Damage(Other, Foreign, 6);
    TestTrue(TEXT("A foreign deployable does not receive owned-source doubling"), Select() == Other);
    Enemy->ClearThreat();
    Damage(Owner, nullptr, 10);
    TestTrue(TEXT("No positive threat uses existing nearest-player fallback, including Ignore Me owner"), Select() == Owner);
    Damage(Other, nullptr, 1);
    TestTrue(TEXT("Any actual positive threat outranks the fallback"), Select() == Other);
    return true;
}
#endif
