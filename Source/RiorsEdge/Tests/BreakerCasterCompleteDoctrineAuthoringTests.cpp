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
    for (const auto* Tree : {UBreakerProgressionLibrary::GetCasterSpellbladeTree(),UBreakerProgressionLibrary::GetCasterVoidWhispererTree(),UBreakerProgressionLibrary::GetCasterMultispellTree()})
    {
        TestEqual(TEXT("Each fully authored Caster doctrine has twelve nodes"),Tree->Nodes.Num(),12);
        int32 Offered=0,FinalPicks=0;
        for (const UBreakerProgressionNode* Node:Tree->Nodes)
        {
            Offered+=Node->MaxRank*Node->CostPerRank;
            if (Node->Tier==4) ++FinalPicks;
        }
        TestEqual(TEXT("Each Caster doctrine offers twenty-four points"),Offered,24);
        TestEqual(TEXT("Each Caster doctrine offers four final-tier choices"),FinalPicks,4);
    }
    const TCHAR* Ids[]={TEXT("Caster.VoidWhisperer.SnapshotDiscipline"),TEXT("Caster.VoidWhisperer.Terminal"),TEXT("Caster.VoidWhisperer.LongDebt"),TEXT("Caster.Multispell.ConductorRule")};
    const TCHAR* Names[]={TEXT("Snapshot Discipline"),TEXT("Terminal"),TEXT("Long Debt"),TEXT("Conductor's Rule")};
    for (int32 Index=0;Index<4;++Index)
    {
        const bool bMultispell=Index==3;
        const auto* Tree=bMultispell?UBreakerProgressionLibrary::GetCasterMultispellTree():UBreakerProgressionLibrary::GetCasterVoidWhispererTree();
        const auto* Node=Tree->FindNode(FName(Ids[Index]));
        if (!TestNotNull(TEXT("Historical final-tier declaration exists"),Node)) return false;
        TestEqual(TEXT("Literal historical name"),Node->DisplayName.ToString(),FString(Names[Index]));
        TestEqual(TEXT("Final authored tier"),Node->Tier,4); TestEqual(TEXT("Single rank"),Node->MaxRank,1);
        TestEqual(TEXT("Two-point price"),Node->CostPerRank,2); TestEqual(TEXT("Six-point branch gate"),Node->RequiredTreeInvestment,6);
        TestTrue(TEXT("Caster class lock"),Node->RequiredClass==EBreakerClassId::Caster);
        TestTrue(TEXT("Doctrine wallet only"),Node->Currency==EBreakerPointCurrency::DoctrinePoints);
        TestTrue(TEXT("No invented numeric substitute"),Node->Effects.IsEmpty());
        TestFalse(TEXT("Rewrite is not another keystone"),Node->bCornerstone);
        const TCHAR* Prerequisites[]={TEXT("Caster.VoidWhisperer.Zonework"),TEXT("Caster.VoidWhisperer.Attrition"),TEXT("Caster.VoidWhisperer.Drain"),TEXT("Caster.Multispell.Chain")};
        if (!TestEqual(TEXT("Historical named prerequisite is retained"),Node->Prerequisites.Num(),1)) return false;
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
        TestFalse(TEXT("Full wallet alone cannot bypass six-point gate"),Progression->CanPurchaseNode(Tree,Node->NodeId,Reason));
        const TArray<FName> Path=bMultispell?
            TArray<FName>{TEXT("Caster.Multispell.Variance"),TEXT("Caster.Multispell.Cycle"),TEXT("Caster.Multispell.Reservoir"),TEXT("Caster.Multispell.Chain"),TEXT("Caster.Multispell.Payment"),TEXT("Caster.Multispell.Sequence")}:
            TArray<FName>{TEXT("Caster.VoidWhisperer.Seep"),TEXT("Caster.VoidWhisperer.StandingWater"),TEXT("Caster.VoidWhisperer.Patience"),TEXT("Caster.VoidWhisperer.Lingering"),TEXT("Caster.VoidWhisperer.Attrition"),TEXT("Caster.VoidWhisperer.Drain")};
        const TArray<FName> SnapshotPath={TEXT("Caster.VoidWhisperer.Seep"),TEXT("Caster.VoidWhisperer.StandingWater"),TEXT("Caster.VoidWhisperer.Patience"),TEXT("Caster.VoidWhisperer.Attrition"),TEXT("Caster.VoidWhisperer.Zonework")};
        for (FName Id:(Index==0?SnapshotPath:Path)) if (!TestTrue(TEXT("Actual priced branch prerequisite route"),Progression->PurchaseNode(Tree,Id,Reason))) return false;
        TestEqual(TEXT("Six prior points leave exactly final choice price"),Progression->GetUnspentPoints(Tree->Currency),2);
        if (!TestTrue(TEXT("Historical rewrite purchased through real gate"),Progression->PurchaseNode(Tree,Node->NodeId,Reason))) return false;
        TestEqual(TEXT("Purchase pays exactly two points"),Progression->GetUnspentPoints(Tree->Currency),0);
        TestTrue(TEXT("Purchased permission is published without faking behavior"),Progression->HasNodeTag(Tag));
        if (!TestTrue(TEXT("Actual doctrine Forge respec"),Progression->RespecAtForge(Tree->Currency,true,Reason))) return false;
        TestEqual(TEXT("Doctrine wallet refunded exactly"),Progression->GetUnspentPoints(Tree->Currency),8);
        TestFalse(TEXT("Respec removes the declared permission"),Progression->HasNodeTag(Tag));
    }
    return true;
}
#endif
