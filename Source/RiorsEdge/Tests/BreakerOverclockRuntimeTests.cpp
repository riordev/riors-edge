#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Abilities/BreakerGameplayAbility.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Progression/BreakerClassDefinition.h"
#include "Progression/BreakerExperience.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionNode.h"
#include "Progression/BreakerProgressionTree.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerOverclockRuntimeTest, "RiorsEdge.Progression.OverclockRuntime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerOverclockRuntimeTest::RunTest(const FString& Parameters)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    if (!World) return false;
    ON_SCOPE_EXIT { World->DestroyWorld(false); };
    AActor* Owner = World->SpawnActor<AActor>();
    if (!Owner) return false;
    auto* Progression = NewObject<UBreakerProgressionComponent>(Owner);
    Owner->AddInstanceComponent(Progression); Progression->RegisterComponent();
    auto* Class = NewObject<UBreakerClassDefinition>(); Class->ClassId = EBreakerClassId::Caster;
    auto* Tree = NewObject<UBreakerProgressionTree>(Class);
    Tree->TreeId = TEXT("Test.Core.Overclock"); Tree->Currency = EBreakerPointCurrency::CorePoints;
    Class->BranchTrees.Add(Tree);
    auto MakeNode = [&](const TCHAR* Id)
    {
        auto* Node = NewObject<UBreakerProgressionNode>(Tree);
        Node->NodeId = Id; Tree->Nodes.Add(Node); return Node;
    };
    auto Add = [](UBreakerProgressionNode* Node, EBreakerNodeStatTarget Target, float Percent)
    {
        FBreakerNodeEffect Effect; Effect.StatTarget = Target;
        Effect.StatBucket = EBreakerNodeStatBucket::IncreasedPercent; Effect.ValuePerRank = Percent;
        Node->Effects.Add(Effect);
    };
    auto* NativeRate = MakeNode(TEXT("Test.Overclock.NativeRates"));
    Add(NativeRate, EBreakerNodeStatTarget::AbilityCastRate, 20);
    Add(NativeRate, EBreakerNodeStatTarget::AbilityChannelRate, 30);
    auto* Recovery = MakeNode(TEXT("Test.Overclock.Recovery")); Recovery->MaxRank = 3;
    Add(Recovery, EBreakerNodeStatTarget::AbilityCooldown, 20);
    auto* Rule = MakeNode(TEXT("Test.Overclock.Rule"));
    Rule->GrantedTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Progression.Node.Core.Overclock")));
    // Isolated schema asset, no changes to a shipped node or new Core activation.
    if (!Progression->ChoosePermanentClass(Class)) return false;
    Progression->AwardExperience(UBreakerExperienceLibrary::TotalXpToReachLevel(5, Progression->ExperienceCurve));
    FText Reason;
    auto Buy = [&](UBreakerProgressionNode* Node)
    { return TestTrue(TEXT("Actual level-earned rank purchase"), Progression->PurchaseNode(Tree, Node->NodeId, Reason)); };
    if (!Buy(NativeRate) || !Buy(Recovery)) return false;
    TestFalse(TEXT("Recovery alone cannot activate Overclock"), Progression->GetNodeStats().bCooldownRecoveryAffectsTempo);
    TestEqual(TEXT("Unowned rule leaves native cast rate"), UBreakerGameplayAbility::AbilityCastRateMultiplierFor(Owner), 1.2f, .0001f);
    TestEqual(TEXT("Unowned rule leaves native channel rate"), UBreakerGameplayAbility::AbilityChannelRateMultiplierFor(Owner), 1.3f, .0001f);
    if (!Buy(Rule)) return false;
    TestTrue(TEXT("Purchased tag reaches actual aggregate rule"), Progression->GetNodeStats().bCooldownRecoveryAffectsTempo);
    for (int32 Rank = 1; Rank <= 3; ++Rank)
    {
        if (Rank > 1 && !Buy(Recovery)) return false;
        TestEqual(TEXT("Half recovery adds once to cast"), UBreakerGameplayAbility::AbilityCastRateMultiplierFor(Owner), 1.2f + .1f * Rank, .0001f);
        TestEqual(TEXT("Half recovery adds once to channel"), UBreakerGameplayAbility::AbilityChannelRateMultiplierFor(Owner), 1.3f + .1f * Rank, .0001f);
        TestEqual(TEXT("Recovery retains full original cooldown effect"), UBreakerGameplayAbility::ScaledCooldownSeconds(12, UBreakerGameplayAbility::AbilityCooldownReductionFor(Owner)), 12.0f / (1 + .2f * Rank), .0001f);
    }
    TestEqual(TEXT("All five earned points actually spent"), Progression->GetUnspentPoints(Tree->Currency), 0);
    if (!TestTrue(TEXT("Real respec removes rule and contributors"), Progression->RespecCore(Reason))) return false;
    TestFalse(TEXT("Respec clears aggregate rule"), Progression->GetNodeStats().bCooldownRecoveryAffectsTempo);
    TestEqual(TEXT("Respec restores cast baseline"), UBreakerGameplayAbility::AbilityCastRateMultiplierFor(Owner), 1.0f);
    TestEqual(TEXT("Respec restores channel baseline"), UBreakerGameplayAbility::AbilityChannelRateMultiplierFor(Owner), 1.0f);
    return true;
}
#endif
