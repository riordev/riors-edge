#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionLibrary.h"
#include "Progression/BreakerProgressionNode.h"
#include "Progression/BreakerProgressionTree.h"
#include "Save/BreakerMissionContent.h"
#include "Save/BreakerQuestContent.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerCasterCompleteDoctrineAuthoringTest,"RiorsEdge.Progression.Caster.CompleteDoctrineAuthoring",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerCasterCompleteDoctrineAuthoringTest::RunTest(const FString&)
{
    // Declaration and paid reachability only. Dedicated runtime tests prove
    // the implemented consumers; this shape test makes no combat claim.
    // O272: a doctrine is six PAIRS of single-rank nodes. Twelve nodes offer
    // twelve points; the keystone is the only Tier-4 node and the only gate.
    for (const auto* Tree : {UBreakerProgressionLibrary::GetCasterSpellbladeTree(),UBreakerProgressionLibrary::GetCasterVoidWhispererTree(),UBreakerProgressionLibrary::GetCasterMultispellTree()})
    {
        TestEqual(TEXT("Each fully authored Caster doctrine has twelve nodes"),Tree->Nodes.Num(),12);
        int32 Offered=0,FinalPicks=0;
        for (const UBreakerProgressionNode* Node:Tree->Nodes)
        {
            Offered+=Node->MaxRank*Node->CostPerRank;
            if (Node->Tier==4) ++FinalPicks;
        }
        TestEqual(TEXT("Each Caster doctrine offers twelve points (O272: six single-rank pairs)"),Offered,12);
        TestEqual(TEXT("Each Caster doctrine has exactly one final-tier node, the keystone"),FinalPicks,1);
    }
    const TCHAR* Ids[]={TEXT("Caster.VoidWhisperer.SnapshotDiscipline"),TEXT("Caster.VoidWhisperer.Terminal"),TEXT("Caster.VoidWhisperer.LongDebt"),TEXT("Caster.Multispell.ConductorRule")};
    const TCHAR* Names[]={TEXT("Snapshot Discipline"),TEXT("Terminal"),TEXT("Long Debt"),TEXT("Conductor's Rule")};
    // O272 pairs: Seep -> Snapshot Discipline, Attrition -> Terminal,
    // Drain -> Long Debt, Chain -> Conductor's Rule.
    const TCHAR* Prerequisites[]={TEXT("Caster.VoidWhisperer.Seep"),TEXT("Caster.VoidWhisperer.Attrition"),TEXT("Caster.VoidWhisperer.Drain"),TEXT("Caster.Multispell.Chain")};
    for (int32 Index=0;Index<4;++Index)
    {
        const bool bMultispell=Index==3;
        const auto* Tree=bMultispell?UBreakerProgressionLibrary::GetCasterMultispellTree():UBreakerProgressionLibrary::GetCasterVoidWhispererTree();
        const auto* Node=Tree->FindNode(FName(Ids[Index]));
        if (!TestNotNull(TEXT("Historical rewrite declaration exists"),Node)) return false;
        TestEqual(TEXT("Literal historical name"),Node->DisplayName.ToString(),FString(Names[Index]));
        TestEqual(TEXT("Impactful half sits at tier one"),Node->Tier,1); TestEqual(TEXT("Single rank"),Node->MaxRank,1);
        TestEqual(TEXT("One-point price"),Node->CostPerRank,1); TestEqual(TEXT("No gate below the keystone"),Node->RequiredTreeInvestment,0);
        TestTrue(TEXT("Caster class lock"),Node->RequiredClass==EBreakerClassId::Caster);
        TestTrue(TEXT("Doctrine wallet only"),Node->Currency==EBreakerPointCurrency::DoctrinePoints);
        TestTrue(TEXT("No invented numeric substitute"),Node->Effects.IsEmpty());
        TestFalse(TEXT("Rewrite is not another keystone"),Node->bCornerstone);
        if (!TestEqual(TEXT("Exactly one prerequisite: the pair's travel node"),Node->Prerequisites.Num(),1)) return false;
        TestEqual(TEXT("Exact prerequisite identity"),Node->Prerequisites[0].NodeId,FName(Prerequisites[Index]));
        TestEqual(TEXT("Named prerequisite requires rank one"),Node->Prerequisites[0].RequiredRank,1);
        const FGameplayTag Tag=FGameplayTag::RequestGameplayTag(FName(*(FString(TEXT("Progression.Node."))+Ids[Index])));
        TestTrue(TEXT("Distinct declared runtime permission"),Node->GrantedTags.HasTagExact(Tag));
        auto* Progression=NewObject<UBreakerProgressionComponent>(NewObject<AActor>());
        if (!Progression->ChoosePermanentClassById(EBreakerClassId::Caster)) return false;
        FBreakerQuestFlagSet Flags;
        for (const auto& Mission:UBreakerMissionLibrary::GetMissions()) for (const auto& Beat:Mission.Beats)
            for (const auto& Flag:UBreakerMissionLibrary::BeatCompletionFlags(Beat)) Flags.Add(Flag);
        Progression->SettleDoctrineEntitlement(Flags);
        FText Reason;
        TestEqual(TEXT("Authored campaign flags settle eight doctrine points"),Progression->GetUnspentPoints(Tree->Currency),8);
        TestFalse(TEXT("Full wallet alone cannot bypass the travel prerequisite"),Progression->CanPurchaseNode(Tree,Node->NodeId,Reason));
        // The priced route to an impactful node is its travel node alone.
        if (!TestTrue(TEXT("Actual priced pair prerequisite route"),Progression->PurchaseNode(Tree,FName(Prerequisites[Index]),Reason))) return false;
        TestEqual(TEXT("One travel point leaves seven"),Progression->GetUnspentPoints(Tree->Currency),7);
        if (!TestTrue(TEXT("Historical rewrite purchased through its pair"),Progression->PurchaseNode(Tree,Node->NodeId,Reason))) return false;
        TestEqual(TEXT("Purchase pays exactly one point"),Progression->GetUnspentPoints(Tree->Currency),6);
        TestTrue(TEXT("Purchased permission is published without faking behavior"),Progression->HasNodeTag(Tag));
        if (!TestTrue(TEXT("Actual doctrine Forge respec"),Progression->RespecAtForge(Tree->Currency,true,Reason))) return false;
        TestEqual(TEXT("Doctrine wallet refunded exactly"),Progression->GetUnspentPoints(Tree->Currency),8);
        TestFalse(TEXT("Respec removes the declared permission"),Progression->HasNodeTag(Tag));
    }
    return true;
}
#endif
