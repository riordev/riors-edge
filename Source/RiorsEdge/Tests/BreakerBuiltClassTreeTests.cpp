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
// SUPPORT BRANCH TREES — authored 2026-08-16 (owner authorization:
// "feel free to do all 5 classes" + "keep building").
// ---------------------------------------------------------------------------
// The branch-layer coverage for the one class still on the compressed tier
// grammar, in the mold of Tests/BreakerCasterTreeTests.cpp: registration
// through the same catalogue the built classes use, exact per-branch content
// pins (twelve named nodes, the Swift-shaped tier compression), and the
// keystone-on-cornerstone guarantee's branch-local half. Gunsmith's and
// Tank's six trees are six-pair doctrines (O272) and their shape is stated
// in BreakerDoctrinePairTests.cpp with Caster's and Swift's; the
// shipped-budget walk at the bottom of this file still runs on Armory.
//
// Every non-keystone node in these three trees ships as its treatment rule
// verbatim, as a tag with NO stat effect, and every keystone More is RESERVED
// rather than spent (the Edgework/Cascade posture) — see the block comments
// above the Support tree getters in BreakerProgressionLibrary.cpp. Both
// facts are pinned below as assertions, because a stat line or a More
// quietly appearing in this content would be a content decision nobody made.
//
// Keystone tags are requested by STRING deliberately: the three
// Keystone.Support.* tags are file-static natives of
// Abilities/BreakerAbilityDefinition.cpp, and the string is what a granted
// GameplayEffect and the ultimate's ResolveVariant actually key off —
// the posture BreakerBuiltClassKitTests already takes.
//
// Helpers are prefixed because the module builds in unity mode.
namespace BreakerBuiltClassTreeTestHelpers
{
    struct FBuiltBranch
    {
        UBreakerProgressionTree* Tree;
        EBreakerClassId ClassId;
        const TCHAR* KeystoneTag;
        // The twelve node ids the treatment authors, in authored order.
        TArray<FName> NodeIds;
    };

    TArray<FBuiltBranch> BuiltBranches()
    {
        return {
            { UBreakerProgressionLibrary::GetSupportMedicTree(), EBreakerClassId::Support, TEXT("Keystone.Support.Triage"), {
                TEXT("Support.Medic.FieldDressing"), TEXT("Support.Medic.TriagePriority"), TEXT("Support.Medic.CleanHands"),
                TEXT("Support.Medic.SteadyHands"), TEXT("Support.Medic.SecondOpinion"), TEXT("Support.Medic.Attending"),
                TEXT("Support.Medic.FieldKit"), TEXT("Support.Medic.SustainedCare"),
                TEXT("Support.Medic.Overflow"), TEXT("Support.Medic.BloodDebt"), TEXT("Support.Medic.NoTriage"),
                TEXT("Support.Medic.Triage") } },
            { UBreakerProgressionLibrary::GetSupportConductorTree(), EBreakerClassId::Support, TEXT("Keystone.Support.Downbeat"), {
                TEXT("Support.Conductor.DownbeatDiscipline"), TEXT("Support.Conductor.Section"), TEXT("Support.Conductor.Sustain"),
                TEXT("Support.Conductor.Rehearsal"), TEXT("Support.Conductor.Tempo"), TEXT("Support.Conductor.Attunement"),
                TEXT("Support.Conductor.Conducting"), TEXT("Support.Conductor.Counterpoint"),
                TEXT("Support.Conductor.StandingOvation"), TEXT("Support.Conductor.SympatheticResonance"), TEXT("Support.Conductor.DetachedBaton"),
                TEXT("Support.Conductor.Downbeat") } },
            { UBreakerProgressionLibrary::GetSupportWardenTree(), EBreakerClassId::Support, TEXT("Keystone.Support.Blackout"), {
                TEXT("Support.Warden.Painted"), TEXT("Support.Warden.LongWatch"), TEXT("Support.Warden.FieldOfView"),
                TEXT("Support.Warden.Handoff"), TEXT("Support.Warden.Pressure"), TEXT("Support.Warden.Tell"),
                TEXT("Support.Warden.Suppression"), TEXT("Support.Warden.DeepMark"),
                TEXT("Support.Warden.ExecutionersLedger"), TEXT("Support.Warden.BlackoutProtocol"), TEXT("Support.Warden.HuntersEconomy"),
                TEXT("Support.Warden.Blackout") } }
        };
    }
}

// ---------------------------------------------------------------------------
// Registration: three Support trees, found through the exact path
// GetAvailableTrees walks — GetAllFallbackTrees, GetTreesForClass, and the
// class definition's BranchTrees. A Support character SEES its trees or this
// fails by name. The class-definition loop below still covers Gunsmith and
// Tank: each definition lists three doctrines plus Core like every other
// class.
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
        TestEqual(*(TreeName + TEXT(" spends class points")), Branch.Tree->Currency, EBreakerPointCurrency::DoctrinePoints);

        const TArray<UBreakerProgressionTree*> ForClass = UBreakerProgressionLibrary::GetTreesForClass(Branch.ClassId);
        TestTrue(*(TreeName + TEXT(" is offered to its class")), ForClass.Contains(Branch.Tree));

        const UBreakerClassDefinition* Definition = UBreakerProgressionLibrary::GetFallbackClassDefinition(Branch.ClassId);
        if (TestNotNull(*(TreeName + TEXT("'s class has a fallback class definition")), Definition))
        {
            TestTrue(*(TreeName + TEXT(" is in its class definition's BranchTrees")), Definition->BranchTrees.Contains(Branch.Tree));
        }
    }

    // The class definitions carry three branches plus Core, the Swift/Caster
    // shape, so the branch strip shows the three chips each treatment names.
    for (const EBreakerClassId ClassId : { EBreakerClassId::Gunsmith, EBreakerClassId::Tank, EBreakerClassId::Support })
    {
        const UBreakerClassDefinition* Definition = UBreakerProgressionLibrary::GetFallbackClassDefinition(ClassId);
        if (!TestNotNull(TEXT("Class has a definition"), Definition)) continue;
        TestEqual(TEXT("Class definition lists three branches plus the Core tree"), Definition->BranchTrees.Num(), 4);
        TestTrue(TEXT("Core tree is still listed"), Definition->BranchTrees.Contains(UBreakerProgressionLibrary::GetCoreSliceTree()));
    }
    return true;
}

// ---------------------------------------------------------------------------
// Content shape: exactly the treatment's twelve nodes per branch, on the
// Swift-shaped compression (doc tiers 1-4 keep their numbers; the doc's
// tier-5 keystone sits at tier 3, cost 3, as a cornerstone), every node a
// tag-carrying rule with NO stat effect and NO ability grant, and NO More
// multiplier anywhere — all three keystone Mores are reserved, the
// Edgework/Cascade posture. Pinned by name so content cannot drift without a
// diff saying so.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerBuiltClassTreesShapeTest,
    "RiorsEdge.Progression.BuiltClassTrees.Shape",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerBuiltClassTreesShapeTest::RunTest(const FString& Parameters)
{
    using namespace BreakerBuiltClassTreeTestHelpers;

    for (const FBuiltBranch& Branch : BuiltBranches())
    {
        if (!TestNotNull(TEXT("Branch tree exists"), Branch.Tree)) continue;
        const FString TreeName = Branch.Tree->TreeId.ToString();

        TestEqual(*(TreeName + TEXT(" ships the treatment's twelve nodes")), Branch.Tree->Nodes.Num(), 12);
        for (const FName NodeId : Branch.NodeIds)
        {
            TestNotNull(*(NodeId.ToString() + TEXT(" exists in its tree")), Branch.Tree->FindNode(NodeId));
        }

        int32 TierCounts[5] = {};
        int32 CornerstoneCount = 0;
        for (const UBreakerProgressionNode* Node : Branch.Tree->Nodes)
        {
            const FString Context = Node->NodeId.ToString();
            if (!TestTrue(*(Context + TEXT(" tier is 1-4 (keystone compressed to 3)")), Node->Tier >= 1 && Node->Tier <= 4)) continue;
            ++TierCounts[Node->Tier];

            // The compressed grammar, one rule set for all three branches:
            // entry/loop nodes are two ranks at 1; tier 3 is single rank at 2
            // (rewrites) or 3 (the cornerstone keystone); tier 4 rewrites are
            // single rank at 2.
            if (Node->Tier <= 2)
            {
                TestEqual(*(Context + TEXT(" entry/loop node costs 1")), Node->CostPerRank, 1);
                TestEqual(*(Context + TEXT(" entry/loop node has two ranks")), Node->MaxRank, 2);
            }
            else
            {
                TestEqual(*(Context + TEXT(" tier-3/4 node is single rank")), Node->MaxRank, 1);
                TestTrue(*(Context + TEXT(" tier-3/4 node costs 2 or 3")), Node->CostPerRank == 2 || Node->CostPerRank == 3);
            }
            if (Node->Tier == 4)
            {
                TestEqual(*(Context + TEXT(" tier-4 rewrite costs 2")), Node->CostPerRank, 2);
                TestTrue(*(Context + TEXT(" tier-4 rewrite builds on an earlier node")), Node->Prerequisites.Num() > 0);
            }

            if (Node->bCornerstone)
            {
                ++CornerstoneCount;
                // TIER 4, COST 2 -- and it used to be tier 3, cost 3, behind
                // an 8-point CornerstoneInvestmentGate. That arithmetic was
                // written against a per-level class budget where 8 + 3 = 11 was
                // affordable; against O111's 8-point doctrine wallet it was
                // not, and every keystone in the game was unbuyable. The gate
                // is gone and the cost is 2, which makes EVERY doctrine node
                // cost two points to max -- so the wallet divides into exactly
                // four picks with nothing stranded, where 3 left a point that
                // could buy nothing.
                TestEqual(*(Context + TEXT(" cornerstone sits at the rewrite tier")), Node->Tier, 4);
                TestEqual(*(Context + TEXT(" cornerstone costs 2, like every other doctrine pick")), Node->CostPerRank, 2);
            }

            // THE WHOLE LAYER IS RULES-AS-TAGS. A stat effect appearing here
            // would be an invented magnitude under the O2 freeze (the
            // treatments author no percentages), and an ability grant would
            // re-grant an id the class definition already catalogues as a
            // starter. Both are content decisions, not refactors.
            TestTrue(*(Context + TEXT(" carries its rule as a granted tag")), Node->GrantedTags.Num() > 0);
            TestEqual(*(Context + TEXT(" authors no stat effect (rules-as-tags layer)")), Node->Effects.Num(), 0);
            TestEqual(*(Context + TEXT(" re-grants no ability (starters are catalogued on the class definition)")), Node->GrantedAbilityIds.Num(), 0);
        }

        TestEqual(*(TreeName + TEXT(" has three tier-1 entry nodes")), TierCounts[1], 3);
        TestEqual(*(TreeName + TEXT(" has three tier-2 loop nodes")), TierCounts[2], 3);
        // The keystone moved from tier 3 to tier 4, so one node crossed
        // between these two counts. Their SUM is unchanged at six, which is the
        // part that matters: no node was added or lost, one was repriced.
        TestEqual(*(TreeName + TEXT(" has two tier-3 ability nodes")), TierCounts[3], 2);
        TestEqual(*(TreeName + TEXT(" has four tier-4 nodes: three rewrites and the keystone")), TierCounts[4], 4);
        TestEqual(*(TreeName + TEXT(" has exactly one cornerstone")), CornerstoneCount, 1);
    }
    return true;
}

// ---------------------------------------------------------------------------
// Keystones: each of the three Keystone.Support.* tags the shipped ultimate
// variant rows key off is granted by exactly one node across all fallback
// trees, and
// that node is its branch's cornerstone. This is the branch-local half of the
// guarantee whose global half is BreakerKeystoneReachabilityTests — whose
// honest-emptiness arm stopped applying to these classes the moment these
// trees registered, exactly as that file promises.
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
        // branch's cornerstone — no orphan, no double-authoring, no keystone
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
