#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Progression/BreakerClassDefinition.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionLibrary.h"
#include "Progression/BreakerExperience.h"
#include "Progression/BreakerProgressionNode.h"
#include "Progression/BreakerProgressionTree.h"
#include "Save/BreakerMissionContent.h"
#include "Save/BreakerQuestJournal.h"

// ---------------------------------------------------------------------------
// DOCTRINE TREES — the branch layer of every class.
// ---------------------------------------------------------------------------
// Fifteen doctrines, three per class. Their pair shape (six travel->impactful
// pairs, the keystone the impactful half of the last pair behind the
// six-invested gate and the commitment) is stated once in
// BreakerDoctrinePairTests.cpp. This file covers what that file does not:
// registration through the exact path the class screen walks, the
// keystone-tag guarantee the ultimates' variant rows key off, prerequisite
// integrity, and the shipped-budget keystone walk on Armory.
//
// Keystone tags are requested by STRING deliberately: the Keystone.* tags are
// file-static natives of the ability layer, and the string is what a granted
// GameplayEffect and the ultimate's ResolveVariant actually key off — the
// posture BreakerBuiltClassKitTests already takes.
//
// Helpers are prefixed because the module builds in unity mode.
namespace BreakerBuiltClassTreeTestHelpers
{
    struct FBuiltBranch
    {
        UBreakerProgressionTree* Tree;
        EBreakerClassId ClassId;
        const TCHAR* KeystoneTag;
    };

    TArray<FBuiltBranch> BuiltBranches()
    {
        return {
            { UBreakerProgressionLibrary::GetCasterVoidWhispererTree(), EBreakerClassId::Caster, TEXT("Keystone.Caster.LongDark") },
            { UBreakerProgressionLibrary::GetCasterSpellbladeTree(), EBreakerClassId::Caster, TEXT("Keystone.Caster.Edgework") },
            { UBreakerProgressionLibrary::GetCasterMultispellTree(), EBreakerClassId::Caster, TEXT("Keystone.Caster.Cascade") },
            { UBreakerProgressionLibrary::GetSwiftKineticTree(), EBreakerClassId::Swift, TEXT("Keystone.Swift.TerminalVelocity") },
            { UBreakerProgressionLibrary::GetSwiftMarksmanTree(), EBreakerClassId::Swift, TEXT("Keystone.Swift.StandingWave") },
            { UBreakerProgressionLibrary::GetSwiftFrenzyTree(), EBreakerClassId::Swift, TEXT("Keystone.Swift.Bloodrhythm") },
            { UBreakerProgressionLibrary::GetGunsmithArmoryTree(), EBreakerClassId::Gunsmith, TEXT("Keystone.Gunsmith.Machinist") },
            { UBreakerProgressionLibrary::GetGunsmithFieldTechTree(), EBreakerClassId::Gunsmith, TEXT("Keystone.Gunsmith.Foundry") },
            { UBreakerProgressionLibrary::GetGunsmithTinkererTree(), EBreakerClassId::Gunsmith, TEXT("Keystone.Gunsmith.Minefield") },
            { UBreakerProgressionLibrary::GetTankLeechTree(), EBreakerClassId::Tank, TEXT("Keystone.Tank.Vein") },
            { UBreakerProgressionLibrary::GetTankBastionTree(), EBreakerClassId::Tank, TEXT("Keystone.Tank.Wall") },
            { UBreakerProgressionLibrary::GetTankDemolitionistTree(), EBreakerClassId::Tank, TEXT("Keystone.Tank.Detonation") },
            { UBreakerProgressionLibrary::GetSupportMedicTree(), EBreakerClassId::Support, TEXT("Keystone.Support.Triage") },
            { UBreakerProgressionLibrary::GetSupportConductorTree(), EBreakerClassId::Support, TEXT("Keystone.Support.Downbeat") },
            { UBreakerProgressionLibrary::GetSupportWardenTree(), EBreakerClassId::Support, TEXT("Keystone.Support.Blackout") },
        };
    }
}

// ---------------------------------------------------------------------------
// Registration: fifteen doctrine trees, found through the exact path
// GetAvailableTrees walks — GetAllFallbackTrees, GetTreesForClass, and the
// class definition's BranchTrees. A character SEES its doctrines or this
// fails by name. Every class definition lists three doctrines plus Core.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerBuiltClassTreesRegisteredTest,
    "RiorsEdge.Progression.BuiltClassTrees.Registered",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerBuiltClassTreesRegisteredTest::RunTest(const FString& Parameters)
{
    using namespace BreakerBuiltClassTreeTestHelpers;

    const TArray<UBreakerProgressionTree*>& AllTrees = UBreakerProgressionLibrary::GetAllFallbackTrees();
    for (const FBuiltBranch& Branch : BuiltBranches())
    {
        if (!TestNotNull(TEXT("Branch tree exists"), Branch.Tree)) continue;
        const FString TreeName = Branch.Tree->TreeId.ToString();
        TestTrue(*(TreeName + TEXT(" is in GetAllFallbackTrees()")), AllTrees.Contains(Branch.Tree));
        TestEqual(*(TreeName + TEXT(" is required by its class")), Branch.Tree->RequiredClass, Branch.ClassId);
        TestEqual(*(TreeName + TEXT(" spends doctrine points")), Branch.Tree->Currency, EBreakerPointCurrency::DoctrinePoints);

        const TArray<UBreakerProgressionTree*> ForClass = UBreakerProgressionLibrary::GetTreesForClass(Branch.ClassId);
        TestTrue(*(TreeName + TEXT(" is offered to its class")), ForClass.Contains(Branch.Tree));

        const UBreakerClassDefinition* Definition = UBreakerProgressionLibrary::GetFallbackClassDefinition(Branch.ClassId);
        if (TestNotNull(*(TreeName + TEXT("'s class has a fallback class definition")), Definition))
        {
            TestTrue(*(TreeName + TEXT(" is in its class definition's BranchTrees")), Definition->BranchTrees.Contains(Branch.Tree));
        }
    }

    // Every class definition carries three doctrines plus Core, so the branch
    // strip shows the three chips each class names.
    for (const EBreakerClassId ClassId : { EBreakerClassId::Caster, EBreakerClassId::Swift, EBreakerClassId::Gunsmith, EBreakerClassId::Tank, EBreakerClassId::Support })
    {
        const UBreakerClassDefinition* Definition = UBreakerProgressionLibrary::GetFallbackClassDefinition(ClassId);
        if (!TestNotNull(TEXT("Class has a definition"), Definition)) continue;
        TestEqual(TEXT("Class definition lists three branches plus the Core tree"), Definition->BranchTrees.Num(), 4);
        TestTrue(TEXT("Core tree is still listed"), Definition->BranchTrees.Contains(UBreakerProgressionLibrary::GetCoreSliceTree()));
    }
    return true;
}

// ---------------------------------------------------------------------------
// Keystones: each of the fifteen Keystone.* tags the shipped ultimate variant
// rows key off is granted by exactly one node across all fallback trees, and
// that node is its own doctrine's cornerstone. This is the branch-local half
// of the guarantee whose global half is BreakerKeystoneReachabilityTests.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerBuiltClassTreesKeystonesTest,
    "RiorsEdge.Progression.BuiltClassTrees.Keystones",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerBuiltClassTreesKeystonesTest::RunTest(const FString& Parameters)
{
    using namespace BreakerBuiltClassTreeTestHelpers;

    for (const FBuiltBranch& Branch : BuiltBranches())
    {
        if (!TestNotNull(TEXT("Branch tree exists"), Branch.Tree)) continue;
        const FGameplayTag KeystoneTag = FGameplayTag::RequestGameplayTag(Branch.KeystoneTag, false);
        if (!TestTrue(*FString::Printf(TEXT("%s is a registered tag (declared by the ability layer)"), Branch.KeystoneTag), KeystoneTag.IsValid()))
        {
            continue;
        }

        // Granted exactly once across ALL fallback trees, and by this
        // doctrine's cornerstone — no orphan, no double-authoring, no keystone
        // rewrite hanging off an ordinary node (O37).
        int32 GrantCount = 0;
        const UBreakerProgressionNode* GrantingNode = nullptr;
        for (const UBreakerProgressionTree* Tree : UBreakerProgressionLibrary::GetAllFallbackTrees())
        {
            if (!Tree) continue;
            for (const UBreakerProgressionNode* Node : Tree->Nodes)
            {
                if (Node && Node->GrantedTags.HasTag(KeystoneTag))
                {
                    ++GrantCount;
                    GrantingNode = Node;
                }
            }
        }
        TestEqual(*FString::Printf(TEXT("%s is granted by exactly one node"), Branch.KeystoneTag), GrantCount, 1);
        if (GrantingNode)
        {
            TestTrue(*FString::Printf(TEXT("%s is granted by a cornerstone (O37 commitment gate)"), Branch.KeystoneTag), GrantingNode->bCornerstone);
            TestNotNull(*FString::Printf(TEXT("%s's granting node lives in its own branch"), Branch.KeystoneTag),
                Branch.Tree->FindNode(GrantingNode->NodeId));
        }
    }
    return true;
}

// ---------------------------------------------------------------------------
// Integrity: prerequisites resolve inside the SAME tree and never point up
// the ladder (a cross-branch or above-tier prerequisite is silently
// unsatisfiable, since PurchaseNode reads the prerequisite from the tree it
// was given).
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerBuiltClassTreesPrerequisitesResolveTest,
    "RiorsEdge.Progression.BuiltClassTrees.PrerequisitesResolve",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerBuiltClassTreesPrerequisitesResolveTest::RunTest(const FString& Parameters)
{
    using namespace BreakerBuiltClassTreeTestHelpers;

    for (const FBuiltBranch& Branch : BuiltBranches())
    {
        if (!TestNotNull(TEXT("Branch tree exists"), Branch.Tree)) continue;
        for (const UBreakerProgressionNode* Node : Branch.Tree->Nodes)
        {
            const FString Context = Branch.Tree->TreeId.ToString() + TEXT(".") + Node->NodeId.ToString();
            for (const FBreakerNodePrerequisite& Prerequisite : Node->Prerequisites)
            {
                const UBreakerProgressionNode* Required = Branch.Tree->FindNode(Prerequisite.NodeId);
                TestNotNull(*(Context + TEXT(" prerequisite resolves in the same tree")), Required);
                if (Required)
                {
                    TestTrue(*(Context + TEXT(" prerequisite sits at or below its tier")), Required->Tier <= Node->Tier);
                    TestTrue(*(Context + TEXT(" prerequisite rank is reachable")), Prerequisite.RequiredRank <= Required->MaxRank);
                }
            }
        }
    }
    return true;
}

// ---------------------------------------------------------------------------
// The shipped budget walk: a Gunsmith reaches its branch keystone on the
// doctrine grant and NOTHING ELSE, through the real commitment gate.
//
// IT USED TO GRANT ITSELF ELEVEN. The comment said "run at exactly that grant"
// and the arithmetic was CornerstoneInvestmentGate 8 + keystone cost 3 = 11 —
// correct against the per-level class budget it was written for, and three more
// than O111's doctrine wallet ever pays. So the one test whose whole purpose
// was to prove affordability was proving it with points the game does not
// grant, which is the exact failure CLAUDE.md names, sitting inside the test
// written to prevent it.
//
// The grant is now read from the library rather than typed here, so a ruling
// that moves the wallet moves this walk in the same commit.
//
// Armory is a six-pair doctrine (O272): every node is one point and one
// rank, Machinist is the impactful half of Chambered's pair and keeps the
// six-invested gate plus the commitment (O86). The walk is the one
// BreakerDoctrinePairTests runs for every doctrine: the keystone's travel,
// two whole pairs, a third travel, commit, keystone. Eight points buy seven
// nodes; the eighth is the honest remainder and is asserted as one, not
// spent on a node the walk did not need.
//
// O37's check is still live on the tree it can fire for: commitment is
// per-doctrine, ordinary nodes need none (O15), so a Gunsmith committed to
// Armory can spend Armory's grant inside Tinkerer and still be refused
// Tinkerer's keystone. The commitment check runs before the investment
// gate, so it is the reason reported.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerBuiltClassTreesKeystoneBudgetTest,
    "RiorsEdge.Progression.BuiltClassTrees.KeystoneAtShippedBudget",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerBuiltClassTreesKeystoneBudgetTest::RunTest(const FString& Parameters)
{
    UBreakerProgressionTree* Armory = UBreakerProgressionLibrary::GetGunsmithArmoryTree();
    if (!TestNotNull(TEXT("Armory tree exists"), Armory)) return false;

    UBreakerProgressionTree* Tinkerer = UBreakerProgressionLibrary::GetGunsmithTinkererTree();
    if (!TestNotNull(TEXT("Tinkerer tree exists"), Tinkerer)) return false;

    UBreakerProgressionComponent* Progression = NewObject<UBreakerProgressionComponent>();
    TestTrue(TEXT("Choosing Gunsmith succeeds"), Progression->ChoosePermanentClassById(EBreakerClassId::Gunsmith));

    // Neither class selection nor commitment pays: a fresh Gunsmith holds no
    // Doctrine until completed mission benchmarks are settled.
    TestEqual(TEXT("A Gunsmith holds no doctrine points before committing"),
        Progression->GetUnspentPoints(EBreakerPointCurrency::DoctrinePoints), 0);

    // THE GRANT SITS ON FOUR BENCHMARKS, not on commitment or on levels, so
    // the walk levels to the cap and settles a completed journal to hold the
    // pool it is about to spend. Still exactly the grant and not one point
    // more -- that was the whole point of this fixture when it granted eleven
    // by hand, and it stays the point now that the number comes from the game.
    // Level fifty is the final device prerequisite; XP alone grants no Doctrine.
    const FBreakerExperienceCurve BenchmarkCurve;
    Progression->AwardExperience(UBreakerExperienceLibrary::TotalXpToReachLevel(
        UBreakerProgressionLibrary::CorePointCapLevel, BenchmarkCurve) - Progression->GetTotalExperience());
    // Completed-campaign journal fixture, not a playthrough claim. The native
    // finale probe earns the physical actions; this tests spending their pool.
    TestEqual(TEXT("Level cap alone pays no Doctrine"), Progression->GetUnspentPoints(EBreakerPointCurrency::DoctrinePoints), 0);
    TestEqual(TEXT("Fixture reaches actual level fifty"), Progression->GetCharacterLevel(), 50);
    UBreakerQuestJournal* Journal = NewObject<UBreakerQuestJournal>();
    FBreakerQuestFlagSet CompletedCampaign;
    for (const FBreakerMissionDefinition& Mission : UBreakerMissionLibrary::GetMissions())
        for (const FBreakerMissionBeat& Beat : Mission.Beats)
            for (FName Flag : UBreakerMissionLibrary::BeatCompletionFlags(Beat)) CompletedCampaign.Add(Flag);
    CompletedCampaign.Add(TEXT("Quest.Finale.Seal")); // One explicit ending in this completed-state fixture.
    Journal->RestoreFrom(CompletedCampaign);
    TestEqual(TEXT("Authored completed benchmarks owe exactly eight"), UBreakerMissionLibrary::DoctrinePointEntitlement(Journal->GetState()), 8);
    Progression->SettleDoctrineEntitlement(Journal->GetState());
    TestEqual(TEXT("Settlement records exactly eight paid"), Progression->GetProgressionState().LevelDoctrinePointsGranted, 8);
    Progression->SettleDoctrineEntitlement(Journal->GetState());
    TestEqual(TEXT("Replaying the same journal cannot pay sixteen"), Progression->GetUnspentPoints(EBreakerPointCurrency::DoctrinePoints), 8);
    TestEqual(TEXT("Every benchmark passed pays exactly the grant"),
        Progression->GetUnspentPoints(EBreakerPointCurrency::DoctrinePoints),
        UBreakerProgressionLibrary::DoctrinePointGrant);

    FText Failure;
    // Machinist hangs from Chambered (O272: the keystone is the impactful
    // half of its own pair). The travel buys first, and one invested is not
    // the gate.
    TestTrue(TEXT("Chambered purchases"), Progression->PurchaseNode(Armory, TEXT("Gunsmith.Armory.Chambered"), Failure));
    TestFalse(TEXT("Machinist refuses at one invested"), Progression->CanPurchaseNode(Armory, TEXT("Gunsmith.Armory.Machinist"), Failure));
    // Two whole pairs, one point each half, no gate below the keystone.
    TestTrue(TEXT("Field Stripping purchases"), Progression->PurchaseNode(Armory, TEXT("Gunsmith.Armory.FieldStripping"), Failure));
    TestTrue(TEXT("No Reserve purchases behind its travel"), Progression->PurchaseNode(Armory, TEXT("Gunsmith.Armory.NoReserve"), Failure));
    TestTrue(TEXT("Working Stock purchases"), Progression->PurchaseNode(Armory, TEXT("Gunsmith.Armory.WorkingStock"), Failure));
    TestTrue(TEXT("Overpressure purchases behind its travel"), Progression->PurchaseNode(Armory, TEXT("Gunsmith.Armory.Overpressure"), Failure));
    TestEqual(TEXT("Five invested"), Progression->GetTreeInvestment(Armory), 5);
    TestFalse(TEXT("Machinist refuses at five invested"), Progression->CanPurchaseNode(Armory, TEXT("Gunsmith.Armory.Machinist"), Failure));
    // A third travel opens the gate. Its impactful is deliberately NOT
    // bought: the point it would cost is the remainder asserted below.
    TestTrue(TEXT("Cold Barrel purchases"), Progression->PurchaseNode(Armory, TEXT("Gunsmith.Armory.ColdBarrel"), Failure));
    TestEqual(TEXT("Armory investment reaches the keystone's six-invested gate"), Progression->GetTreeInvestment(Armory), 6);
    TestEqual(TEXT("...with two points left"),
        Progression->GetUnspentPoints(EBreakerPointCurrency::DoctrinePoints), 2);
    // O86: the gate is open and the keystone still wants the commitment.
    TestFalse(TEXT("Machinist refuses uncommitted at six invested"),
        Progression->CanPurchaseNode(Armory, TEXT("Gunsmith.Armory.Machinist"), Failure));
    TestFalse(TEXT("The uncommitted refusal carries a reason"), Failure.IsEmpty());

    TestTrue(TEXT("Committing to Armory succeeds"), Progression->CommitToBranch(TEXT("Doctrine.Gunsmith.Armory"), Failure));
    TestEqual(TEXT("Committing pays no points by itself"),
        Progression->GetUnspentPoints(EBreakerPointCurrency::DoctrinePoints), 2);
    // O37, on the tree it can still fire for: committed to Armory, refused
    // Tinkerer's keystone. The points are in hand, so this is the commitment
    // check answering and not the wallet.
    Failure = FText::GetEmpty();
    TestFalse(TEXT("A keystone in an uncommitted doctrine refuses"),
        Progression->CanPurchaseNode(Tinkerer, TEXT("Gunsmith.Tinkerer.Minefield"), Failure));
    TestFalse(TEXT("The refusal carries a reason"), Failure.IsEmpty());

    TestTrue(TEXT("Machinist purchases for one point at six invested and committed"),
        Progression->PurchaseNode(Armory, TEXT("Gunsmith.Armory.Machinist"), Failure));
    // THE LIVE WALLET. This read ClassPoints_Retired, which has no storage and
    // answers zero to everything — so an assertion on the wallet was true
    // before the walk started and would have stayed true if every purchase
    // above had failed. Seven of the eight are spent; the eighth is the
    // remainder, and it is asserted as one rather than spent to make a zero.
    TestEqual(TEXT("Seven nodes leave one point of the eight"),
        Progression->GetUnspentPoints(EBreakerPointCurrency::DoctrinePoints), UBreakerProgressionLibrary::DoctrinePointGrant - 7);
    TestEqual(TEXT("...and every spent point went into this one tree"),
        Progression->GetTreeInvestment(Armory), 7);
    TestEqual(TEXT("Machinist is owned at rank one"),
        Progression->GetNodeRank(TEXT("Gunsmith.Armory.Machinist"), EBreakerPointCurrency::DoctrinePoints), 1);

    // The purchase is not decorative: the branch identity tag and the
    // ultimate's keystone tag both reach the aggregate, so Field Assembly's
    // Machinist row resolves for this character.
    TestTrue(TEXT("Machinist's node tag reaches the aggregate"),
        Progression->GetNodeStats().GrantedTags.HasTag(BreakerNodeTags::Node_AR_Machinist.GetTag()));
    TestTrue(TEXT("Machinist's keystone tag reaches the aggregate"),
        Progression->GetNodeStats().GrantedTags.HasTag(FGameplayTag::RequestGameplayTag(TEXT("Keystone.Gunsmith.Machinist"))));
    return true;
}

#endif  // WITH_DEV_AUTOMATION_TESTS
