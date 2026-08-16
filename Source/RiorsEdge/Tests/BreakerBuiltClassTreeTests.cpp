#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Progression/BreakerClassDefinition.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionLibrary.h"
#include "Progression/BreakerProgressionNode.h"
#include "Progression/BreakerProgressionTree.h"

// ---------------------------------------------------------------------------
// GUNSMITH / TANK / SUPPORT BRANCH TREES — authored 2026-08-16 (owner
// authorization: "feel free to do all 5 classes" + "keep building").
// ---------------------------------------------------------------------------
// The branch-layer coverage for the last three classes, in the mold of
// Tests/BreakerCasterTreeTests.cpp: registration through the same catalogue
// the built classes use, exact per-branch content pins (twelve named nodes,
// the Swift-shaped tier compression), the keystone-on-cornerstone guarantee's
// branch-local half, and one full purchase walk at the SHIPPED level-11
// budget — never an inflated test grant, which is exactly how the keystone
// budget contradiction stayed invisible the first time.
//
// Every non-keystone node in these nine trees ships as its treatment rule
// verbatim, as a tag with NO stat effect, and every keystone More is RESERVED
// rather than spent (the Edgework/Cascade posture) — see the block comment
// above GetGunsmithArmoryTree in BreakerProgressionLibrary.cpp. Both facts
// are pinned below as assertions, because a stat line or a More quietly
// appearing in this content would be a content decision nobody made.
//
// Keystone tags are requested by STRING deliberately: the nine
// Keystone.Gunsmith/Tank/Support.* tags are file-static natives of
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
            { UBreakerProgressionLibrary::GetGunsmithArmoryTree(), EBreakerClassId::Gunsmith, TEXT("Keystone.Gunsmith.Machinist"), {
                TEXT("Gunsmith.Armory.FieldStripping"), TEXT("Gunsmith.Armory.WorkingStock"), TEXT("Gunsmith.Armory.Chambered"),
                TEXT("Gunsmith.Armory.DeepPockets"), TEXT("Gunsmith.Armory.LastRound"), TEXT("Gunsmith.Armory.ColdBarrel"),
                TEXT("Gunsmith.Armory.BenchWork"), TEXT("Gunsmith.Armory.RigDiscipline"),
                TEXT("Gunsmith.Armory.Reciprocal"), TEXT("Gunsmith.Armory.Overpressure"), TEXT("Gunsmith.Armory.NoReserve"),
                TEXT("Gunsmith.Armory.Machinist") } },
            { UBreakerProgressionLibrary::GetGunsmithFieldTechTree(), EBreakerClassId::Gunsmith, TEXT("Keystone.Gunsmith.Foundry"), {
                TEXT("Gunsmith.FieldTech.Salvage"), TEXT("Gunsmith.FieldTech.Overwatch"), TEXT("Gunsmith.FieldTech.SecondShift"),
                TEXT("Gunsmith.FieldTech.Tithe"), TEXT("Gunsmith.FieldTech.Requisition"), TEXT("Gunsmith.FieldTech.Foreman"),
                TEXT("Gunsmith.FieldTech.Emplacement"), TEXT("Gunsmith.FieldTech.Logistics"),
                TEXT("Gunsmith.FieldTech.Redundancy"), TEXT("Gunsmith.FieldTech.Automation"), TEXT("Gunsmith.FieldTech.Deadman"),
                TEXT("Gunsmith.FieldTech.Foundry") } },
            { UBreakerProgressionLibrary::GetGunsmithTinkererTree(), EBreakerClassId::Gunsmith, TEXT("Keystone.Gunsmith.Minefield"), {
                TEXT("Gunsmith.Tinkerer.CheapWork"), TEXT("Gunsmith.Tinkerer.QuickSet"), TEXT("Gunsmith.Tinkerer.Tripwire"),
                TEXT("Gunsmith.Tinkerer.Rearm"), TEXT("Gunsmith.Tinkerer.AttritionField"), TEXT("Gunsmith.Tinkerer.Overlap"),
                TEXT("Gunsmith.Tinkerer.Ordnance"), TEXT("Gunsmith.Tinkerer.Interdiction"),
                TEXT("Gunsmith.Tinkerer.Patience"), TEXT("Gunsmith.Tinkerer.DeadGround"), TEXT("Gunsmith.Tinkerer.CommandDetonation"),
                TEXT("Gunsmith.Tinkerer.Minefield") } },
            { UBreakerProgressionLibrary::GetTankLeechTree(), EBreakerClassId::Tank, TEXT("Keystone.Tank.Vein"), {
                TEXT("Tank.Leech.Clot"), TEXT("Tank.Leech.SlowBleed"), TEXT("Tank.Leech.OpenWound"),
                TEXT("Tank.Leech.FeedTheWound"), TEXT("Tank.Leech.Bloodlet"), TEXT("Tank.Leech.Transfusion"),
                TEXT("Tank.Leech.RendMastery"), TEXT("Tank.Leech.SecondHeart"),
                TEXT("Tank.Leech.NothingWasted"), TEXT("Tank.Leech.Reciprocity"), TEXT("Tank.Leech.Exsanguinate"),
                TEXT("Tank.Leech.Vein") } },
            { UBreakerProgressionLibrary::GetTankBastionTree(), EBreakerClassId::Tank, TEXT("Keystone.Tank.Wall"), {
                TEXT("Tank.Bastion.LineOfSight"), TEXT("Tank.Bastion.Footing"), TEXT("Tank.Bastion.Loud"),
                TEXT("Tank.Bastion.HeldGround"), TEXT("Tank.Bastion.AnsweringFire"), TEXT("Tank.Bastion.Bulk"),
                TEXT("Tank.Bastion.Emplacement"), TEXT("Tank.Bastion.Interposition"),
                TEXT("Tank.Bastion.Conversion"), TEXT("Tank.Bastion.StandingOrder"), TEXT("Tank.Bastion.ImmovableObject"),
                TEXT("Tank.Bastion.Wall") } },
            { UBreakerProgressionLibrary::GetTankDemolitionistTree(), EBreakerClassId::Tank, TEXT("Keystone.Tank.Detonation"), {
                TEXT("Tank.Demolitionist.ShapedCharge"), TEXT("Tank.Demolitionist.Bootstraps"), TEXT("Tank.Demolitionist.BracedForImpact"),
                TEXT("Tank.Demolitionist.Fragmentation"), TEXT("Tank.Demolitionist.Concussion"), TEXT("Tank.Demolitionist.Overpressure"),
                TEXT("Tank.Demolitionist.Demolition"), TEXT("Tank.Demolitionist.TerminalDescent"),
                TEXT("Tank.Demolitionist.BlastRadius"), TEXT("Tank.Demolitionist.KineticRecovery"), TEXT("Tank.Demolitionist.ChainReaction"),
                TEXT("Tank.Demolitionist.Detonation") } },
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
// Registration: nine trees, three per class, found through the exact path
// GetAvailableTrees walks — GetAllFallbackTrees, GetTreesForClass, and each
// class definition's BranchTrees. A Gunsmith character SEES its trees or this
// fails by name.
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
        TestEqual(*(TreeName + TEXT(" spends class points")), Branch.Tree->Currency, EBreakerPointCurrency::ClassPoints);

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
// multiplier anywhere — all nine keystone Mores are reserved, the
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

            // The compressed grammar, one rule set for all nine branches:
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
                TestEqual(*(Context + TEXT(" cornerstone sits at the compressed keystone tier")), Node->Tier, 3);
                TestEqual(*(Context + TEXT(" cornerstone costs 3")), Node->CostPerRank, 3);
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
        TestEqual(*(TreeName + TEXT(" has three tier-3 nodes (two rewrites plus the keystone)")), TierCounts[3], 3);
        TestEqual(*(TreeName + TEXT(" has three tier-4 rewrite nodes")), TierCounts[4], 3);
        TestEqual(*(TreeName + TEXT(" has exactly one cornerstone")), CornerstoneCount, 1);
    }
    return true;
}

// ---------------------------------------------------------------------------
// Keystones: each of the nine Keystone.* tags the shipped ultimate variant
// rows key off is granted by exactly one node across all fallback trees, and
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
// The shipped budget walk: a Gunsmith at the level-11 economy (the 10-point
// slice advance plus level 11's point — RiorsEdge.Progression.
// LevelPointEntitlement) reaches its branch keystone with NOTHING to spare:
// CornerstoneInvestmentGate 8 + keystone cost 3 = 11. Run at exactly that
// grant, and through the real commitment gate, so the affordability math the
// entitlement test states now holds for a class other than Swift by
// construction rather than by symmetry argument.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerBuiltClassTreesKeystoneBudgetTest,
    "RiorsEdge.Progression.BuiltClassTrees.KeystoneAtShippedBudget",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerBuiltClassTreesKeystoneBudgetTest::RunTest(const FString& Parameters)
{
    UBreakerProgressionTree* Armory = UBreakerProgressionLibrary::GetGunsmithArmoryTree();
    if (!TestNotNull(TEXT("Armory tree exists"), Armory)) return false;

    UBreakerProgressionComponent* Progression = NewObject<UBreakerProgressionComponent>();
    TestTrue(TEXT("Choosing Gunsmith succeeds"), Progression->ChoosePermanentClassById(EBreakerClassId::Gunsmith));
    // Exactly the level-11 Class Point budget. Not one more.
    Progression->GrantPlaytestPoints(11, 0);

    FText Failure;
    // Machinist's prerequisite chain: Chambered (T1) -> Cold Barrel (T2).
    // ORDER MATTERS: each purchase clears its own tier's investment gate at
    // the moment it is bought (tier 2 needs 2 points already in the tree).
    TestTrue(TEXT("Chambered rank 1 purchases"), Progression->PurchaseNode(Armory, TEXT("Gunsmith.Armory.Chambered"), Failure));
    TestTrue(TEXT("Chambered rank 2 purchases"), Progression->PurchaseNode(Armory, TEXT("Gunsmith.Armory.Chambered"), Failure));
    TestTrue(TEXT("Cold Barrel rank 1 purchases"), Progression->PurchaseNode(Armory, TEXT("Gunsmith.Armory.ColdBarrel"), Failure));
    TestTrue(TEXT("Cold Barrel rank 2 purchases"), Progression->PurchaseNode(Armory, TEXT("Gunsmith.Armory.ColdBarrel"), Failure));
    TestTrue(TEXT("Field Stripping rank 1 purchases"), Progression->PurchaseNode(Armory, TEXT("Gunsmith.Armory.FieldStripping"), Failure));
    TestTrue(TEXT("Field Stripping rank 2 purchases"), Progression->PurchaseNode(Armory, TEXT("Gunsmith.Armory.FieldStripping"), Failure));
    TestTrue(TEXT("Working Stock rank 1 purchases"), Progression->PurchaseNode(Armory, TEXT("Gunsmith.Armory.WorkingStock"), Failure));
    TestTrue(TEXT("Working Stock rank 2 purchases"), Progression->PurchaseNode(Armory, TEXT("Gunsmith.Armory.WorkingStock"), Failure));
    TestEqual(TEXT("Armory investment reaches the cornerstone gate"), Progression->GetTreeInvestment(Armory), 8);

    // O37 holds for the new classes too: gate open, prerequisite met, and the
    // keystone still refuses without a branch commitment.
    TestFalse(TEXT("Machinist refuses with the gate open but no commitment"),
        Progression->CanPurchaseNode(Armory, TEXT("Gunsmith.Armory.Machinist"), Failure));
    TestFalse(TEXT("The refusal carries a reason"), Failure.IsEmpty());

    FText CommitFailure;
    TestTrue(TEXT("Committing to Armory succeeds"), Progression->CommitToBranch(TEXT("Gunsmith.Armory"), CommitFailure));
    TestTrue(TEXT("Machinist purchases on the last three points of the level-11 budget"),
        Progression->PurchaseNode(Armory, TEXT("Gunsmith.Armory.Machinist"), Failure));
    TestEqual(TEXT("The level-11 budget is spent to exactly zero"),
        Progression->GetUnspentPoints(EBreakerPointCurrency::ClassPoints), 0);

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
