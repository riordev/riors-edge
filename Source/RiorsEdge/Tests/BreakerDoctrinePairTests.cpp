#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"
#include "Abilities/BreakerAbilityDefinition.h"
#include "Abilities/BreakerAbility_Cleave.h"
#include "Abilities/BreakerAbility_Closequarter.h"
#include "Abilities/BreakerAbility_Resonance.h"
#include "Abilities/BreakerAbility_Rot.h"
#include "Abilities/BreakerAbility_Siphon.h"
#include "Classes/BreakerManaComponent.h"
#include "Classes/BreakerMomentumComponent.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionLibrary.h"
#include "Progression/BreakerProgressionNode.h"
#include "Progression/BreakerProgressionTree.h"
#include "Save/BreakerMissionContent.h"
#include "Save/BreakerQuestContent.h"

#if WITH_DEV_AUTOMATION_TESTS

// ---------------------------------------------------------------------------
// O272: a doctrine is six PAIRS. Each pair is one travel node (single rank,
// the scaling magnitude its ranks used to total) whose purchase unlocks one
// impactful node (single rank, a rule or behaviour tag). No pair is gated
// below the keystone, so a benchmark's two points always land an impactful
// node and any pair may be the first. The keystone is the impactful half of
// its own pair and keeps the six-invested gate, so it is the fourth pair by
// arithmetic.
//
// A doctrine may also carry UNPAIRED roots outside the six pairs: a travel
// node nothing follows (Marksman's Deadeye, Frenzy's Feed), or the class's
// granted node (Kinetic's Longstride — cost 0, seeded at rank 1 by the class
// choice, never purchasable). An unpaired root has no dependents and buys
// with one point, or is already owned.
//
// This test states that shape for all nine trees, buys every pair with
// exactly one benchmark's points settled the way the game settles them, and
// pins the data the travel magnitudes moved to.
// ---------------------------------------------------------------------------
namespace
{
    struct FBreakerDoctrinePairRow
    {
        const TCHAR* Travel;
        const TCHAR* Impactful;
    };

    struct FBreakerDoctrinePairTree
    {
        const TCHAR* Label;
        EBreakerClassId Class;
        UBreakerProgressionTree* Tree;
        // The last row is the keystone's pair.
        TArray<FBreakerDoctrinePairRow> Pairs;
        // Roots outside the pairs: no dependents, one point (or granted).
        TArray<const TCHAR*> Unpaired;
    };

    TArray<FBreakerDoctrinePairTree> BreakerDoctrinePairTrees()
    {
        return {
            { TEXT("Void Whisperer"), EBreakerClassId::Caster, UBreakerProgressionLibrary::GetCasterVoidWhispererTree(), {
                { TEXT("Caster.VoidWhisperer.StandingWater"), TEXT("Caster.VoidWhisperer.Wellspring") },
                { TEXT("Caster.VoidWhisperer.Lingering"), TEXT("Caster.VoidWhisperer.Zonework") },
                { TEXT("Caster.VoidWhisperer.Attrition"), TEXT("Caster.VoidWhisperer.Terminal") },
                { TEXT("Caster.VoidWhisperer.Seep"), TEXT("Caster.VoidWhisperer.SnapshotDiscipline") },
                { TEXT("Caster.VoidWhisperer.Drain"), TEXT("Caster.VoidWhisperer.LongDebt") },
                { TEXT("Caster.VoidWhisperer.Patience"), TEXT("Caster.VoidWhisperer.LongDark") } }, {} },
            { TEXT("Spellblade"), EBreakerClassId::Caster, UBreakerProgressionLibrary::GetCasterSpellbladeTree(), {
                { TEXT("Caster.Spellblade.Debt"), TEXT("Caster.Spellblade.Overreach") },
                { TEXT("Caster.Spellblade.Bloodprice"), TEXT("Caster.Spellblade.Reprisal") },
                { TEXT("Caster.Spellblade.MomentumTransfer"), TEXT("Caster.Spellblade.Blink") },
                { TEXT("Caster.Spellblade.FollowThrough"), TEXT("Caster.Spellblade.Edge") },
                { TEXT("Caster.Spellblade.Close"), TEXT("Caster.Spellblade.NoDistance") },
                { TEXT("Caster.Spellblade.ContactCharge"), TEXT("Caster.Spellblade.Edgework") } }, {} },
            { TEXT("Multispell"), EBreakerClassId::Caster, UBreakerProgressionLibrary::GetCasterMultispellTree(), {
                { TEXT("Caster.Multispell.Payment"), TEXT("Caster.Multispell.Resonance") },
                { TEXT("Caster.Multispell.Reservoir"), TEXT("Caster.Multispell.Prepared") },
                { TEXT("Caster.Multispell.Chain"), TEXT("Caster.Multispell.ConductorRule") },
                { TEXT("Caster.Multispell.Cycle"), TEXT("Caster.Multispell.Fracture") },
                { TEXT("Caster.Multispell.Variance"), TEXT("Caster.Multispell.Interference") },
                { TEXT("Caster.Multispell.Sequence"), TEXT("Caster.Multispell.Cascade") } }, {} },
            { TEXT("Kinetic"), EBreakerClassId::Swift, UBreakerProgressionLibrary::GetSwiftKineticTree(), {
                { TEXT("Swift.Kinetic.ReadTheRoom"), TEXT("Swift.Kinetic.NoGround") },
                { TEXT("Swift.Kinetic.Landing"), TEXT("Swift.Kinetic.AirWork") },
                { TEXT("Swift.Kinetic.Redirect"), TEXT("Swift.Kinetic.SpendToLive") },
                { TEXT("Swift.Kinetic.Downforce"), TEXT("Swift.Kinetic.MomentumShield") },
                { TEXT("Swift.Kinetic.Contact"), TEXT("Swift.Kinetic.EvadeConversion") },
                { TEXT("Swift.Kinetic.Carry"), TEXT("Swift.Kinetic.Overpressure") } },
                { TEXT("Swift.Kinetic.Longstride") } },
            { TEXT("Marksman"), EBreakerClassId::Swift, UBreakerProgressionLibrary::GetSwiftMarksmanTree(), {
                { TEXT("Swift.Marksman.Steady"), TEXT("Swift.Marksman.Reserve") },
                { TEXT("Swift.Marksman.PierceDiscipline"), TEXT("Swift.Marksman.Sightline") },
                { TEXT("Swift.Marksman.Angle"), TEXT("Swift.Marksman.Overpenetration") },
                { TEXT("Swift.Marksman.MarkEconomy"), TEXT("Swift.Marksman.Lead") },
                { TEXT("Swift.Marksman.Ledger"), TEXT("Swift.Marksman.CalledShot") },
                { TEXT("Swift.Marksman.LongLens"), TEXT("Swift.Marksman.Culling") } },
                { TEXT("Swift.Marksman.Deadeye") } },
            { TEXT("Frenzy"), EBreakerClassId::Swift, UBreakerProgressionLibrary::GetSwiftFrenzyTree(), {
                { TEXT("Swift.Frenzy.Rhythm"), TEXT("Swift.Frenzy.SlipcutMastery") },
                { TEXT("Swift.Frenzy.DryFire"), TEXT("Swift.Frenzy.AmmunitionEconomy") },
                { TEXT("Swift.Frenzy.ShortLeash"), TEXT("Swift.Frenzy.NoSafety") },
                { TEXT("Swift.Frenzy.Loaded"), TEXT("Swift.Frenzy.RedlineTrigger") },
                { TEXT("Swift.Frenzy.TriggerDiscipline"), TEXT("Swift.Frenzy.SecondWind") },
                { TEXT("Swift.Frenzy.Overrev"), TEXT("Swift.Frenzy.Bloodrhythm") } },
                { TEXT("Swift.Frenzy.Feed") } },
            { TEXT("Armory"), EBreakerClassId::Gunsmith, UBreakerProgressionLibrary::GetGunsmithArmoryTree(), {
                { TEXT("Gunsmith.Armory.FieldStripping"), TEXT("Gunsmith.Armory.NoReserve") },
                { TEXT("Gunsmith.Armory.WorkingStock"), TEXT("Gunsmith.Armory.Overpressure") },
                { TEXT("Gunsmith.Armory.DeepPockets"), TEXT("Gunsmith.Armory.BenchWork") },
                { TEXT("Gunsmith.Armory.LastRound"), TEXT("Gunsmith.Armory.Reciprocal") },
                { TEXT("Gunsmith.Armory.ColdBarrel"), TEXT("Gunsmith.Armory.RigDiscipline") },
                { TEXT("Gunsmith.Armory.Chambered"), TEXT("Gunsmith.Armory.Machinist") } }, {} },
            { TEXT("Field Tech"), EBreakerClassId::Gunsmith, UBreakerProgressionLibrary::GetGunsmithFieldTechTree(), {
                { TEXT("Gunsmith.FieldTech.Salvage"), TEXT("Gunsmith.FieldTech.Redundancy") },
                { TEXT("Gunsmith.FieldTech.Overwatch"), TEXT("Gunsmith.FieldTech.Automation") },
                { TEXT("Gunsmith.FieldTech.SecondShift"), TEXT("Gunsmith.FieldTech.Emplacement") },
                { TEXT("Gunsmith.FieldTech.Requisition"), TEXT("Gunsmith.FieldTech.Deadman") },
                { TEXT("Gunsmith.FieldTech.Foreman"), TEXT("Gunsmith.FieldTech.Logistics") },
                { TEXT("Gunsmith.FieldTech.Tithe"), TEXT("Gunsmith.FieldTech.Foundry") } }, {} },
            { TEXT("Tinkerer"), EBreakerClassId::Gunsmith, UBreakerProgressionLibrary::GetGunsmithTinkererTree(), {
                { TEXT("Gunsmith.Tinkerer.CheapWork"), TEXT("Gunsmith.Tinkerer.DeadGround") },
                { TEXT("Gunsmith.Tinkerer.QuickSet"), TEXT("Gunsmith.Tinkerer.Ordnance") },
                { TEXT("Gunsmith.Tinkerer.Tripwire"), TEXT("Gunsmith.Tinkerer.Patience") },
                { TEXT("Gunsmith.Tinkerer.Rearm"), TEXT("Gunsmith.Tinkerer.CommandDetonation") },
                { TEXT("Gunsmith.Tinkerer.Overlap"), TEXT("Gunsmith.Tinkerer.Interdiction") },
                { TEXT("Gunsmith.Tinkerer.AttritionField"), TEXT("Gunsmith.Tinkerer.Minefield") } }, {} },
        };
    }

    // The first benchmark, settled the way the game settles it: the campaign
    // walked in mission order, every beat completed up to and including the
    // first Unlock beat(s) worth one benchmark's doctrine points. This is the
    // same journal shape SettleDoctrineEntitlement reads in play; no
    // playtest grant is involved.
    bool BreakerDoctrinePairFirstBenchmarkFlags(FBreakerQuestFlagSet& Out)
    {
        int32 Entitled = 0;
        for (const FBreakerMissionDefinition& Mission : UBreakerMissionLibrary::GetMissions())
        {
            for (const FBreakerMissionBeat& Beat : Mission.Beats)
            {
                for (const FName& Flag : UBreakerMissionLibrary::BeatCompletionFlags(Beat)) Out.Add(Flag);
                if (Beat.Kind == EBreakerMissionBeatKind::Unlock) Entitled += Beat.DoctrinePoints;
                if (Entitled >= UBreakerProgressionLibrary::DoctrinePointsPerBenchmark) return true;
            }
        }
        return false;
    }

    void BreakerDoctrinePairWholeCampaignFlags(FBreakerQuestFlagSet& Out)
    {
        for (const FBreakerMissionDefinition& Mission : UBreakerMissionLibrary::GetMissions())
            for (const FBreakerMissionBeat& Beat : Mission.Beats)
                for (const FName& Flag : UBreakerMissionLibrary::BeatCompletionFlags(Beat)) Out.Add(Flag);
    }

    // A fresh character of the row's class, committed the way the class
    // screen commits (so a Swift arrives with Longstride seeded), holding
    // exactly the doctrine points its journal settles.
    UBreakerProgressionComponent* BreakerDoctrinePairFreshCharacter(FAutomationTestBase& Test, EBreakerClassId Class, const FBreakerQuestFlagSet& Flags, int32 ExpectedPoints)
    {
        UBreakerProgressionComponent* Progression = NewObject<UBreakerProgressionComponent>(NewObject<AActor>());
        if (!Test.TestTrue(TEXT("A fresh component commits to the row's class"), Progression->ChoosePermanentClassById(Class))) return nullptr;
        Progression->SettleDoctrineEntitlement(Flags);
        if (!Test.TestEqual(TEXT("The journal settles exactly the expected doctrine points"),
            Progression->GetUnspentPoints(EBreakerPointCurrency::DoctrinePoints), ExpectedPoints)) return nullptr;
        return Progression;
    }

    // FindFallback patches the class default object from the file (O246), so
    // the CDO is read through the callback only after the definition exists.
    template <typename AbilityType>
    void BreakerDoctrinePairAuthoredNumber(FAutomationTestBase& Test, const TCHAR* AbilityId, const TCHAR* Name, float Expected, float (*Read)(const AbilityType&))
    {
        const UBreakerAbilityDefinition* Definition = UBreakerAbilityDefinition::FindFallback(AbilityId);
        const FString Context = FString::Printf(TEXT("%s %s"), AbilityId, Name);
        if (!Test.TestNotNull(*(Context + TEXT(" definition")), Definition)) return;
        const float* Authored = Definition->Numbers.Find(FName(Name));
        if (!Test.TestNotNull(*(Context + TEXT(" is authored in Data/abilities.json")), Authored)) return;
        Test.TestEqual(*(Context + TEXT(" authored value")), *Authored, Expected, 0.0001f);
        Test.TestEqual(*(Context + TEXT(" is what the ability reads")), Read(*GetDefault<AbilityType>()), Expected, 0.0001f);
    }

    void BreakerDoctrinePairSingleEffect(FAutomationTestBase& Test, const UBreakerProgressionTree* Tree, const TCHAR* NodeId, EBreakerNodeStatTarget Target, float Value)
    {
        const UBreakerProgressionNode* Node = Tree->FindNode(NodeId);
        const FString Context(NodeId);
        if (!Test.TestNotNull(*(Context + TEXT(" is authored")), Node)) return;
        if (!Test.TestEqual(*(Context + TEXT(" authors one scaling line")), Node->Effects.Num(), 1)) return;
        const FBreakerNodeEffect& Effect = Node->Effects[0];
        Test.TestEqual(*(Context + TEXT(" line target")), static_cast<int32>(Effect.StatTarget), static_cast<int32>(Target));
        Test.TestEqual(*(Context + TEXT(" line bucket is IncreasedPercent")), static_cast<int32>(Effect.StatBucket), static_cast<int32>(EBreakerNodeStatBucket::IncreasedPercent));
        Test.TestEqual(*(Context + TEXT(" single rank carries the old two-rank total")), Effect.ValuePerRank, Value, 0.0001f);
        Test.TestEqual(*(Context + TEXT(" line is unconditional")), static_cast<int32>(Effect.Condition), static_cast<int32>(EBreakerBuildCondition::Always));
    }

    // A Swift travel node's lines: any bucket, any condition, matched by
    // target so authoring order is not what the pin is about. The node
    // authors exactly these lines and no other.
    struct FBreakerDoctrinePairLine
    {
        EBreakerNodeStatTarget Target;
        EBreakerNodeStatBucket Bucket;
        float Value;
        EBreakerBuildCondition Condition;
    };

    void BreakerDoctrinePairTravelLines(FAutomationTestBase& Test, const UBreakerProgressionTree* Tree, const TCHAR* NodeId, const TArray<FBreakerDoctrinePairLine>& Lines)
    {
        const UBreakerProgressionNode* Node = Tree->FindNode(NodeId);
        const FString Context(NodeId);
        if (!Test.TestNotNull(*(Context + TEXT(" is authored")), Node)) return;
        Test.TestEqual(*(Context + TEXT(" authors exactly the expected lines")), Node->Effects.Num(), Lines.Num());
        for (const FBreakerDoctrinePairLine& Line : Lines)
        {
            const FBreakerNodeEffect* Effect = Node->Effects.FindByPredicate(
                [&Line](const FBreakerNodeEffect& Candidate) { return Candidate.StatTarget == Line.Target; });
            const FString LineContext = FString::Printf(TEXT("%s line on target %d"), NodeId, static_cast<int32>(Line.Target));
            if (!Test.TestNotNull(*(LineContext + TEXT(" is authored")), Effect)) continue;
            Test.TestEqual(*(LineContext + TEXT(" bucket")), static_cast<int32>(Effect->StatBucket), static_cast<int32>(Line.Bucket));
            Test.TestEqual(*(LineContext + TEXT(" single rank carries the old two-rank total")), Effect->ValuePerRank, Line.Value, 0.0001f);
            Test.TestEqual(*(LineContext + TEXT(" condition")), static_cast<int32>(Effect->Condition), static_cast<int32>(Line.Condition));
        }
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerDoctrinePairFirstBenchmarkTest, "RiorsEdge.Progression.Doctrine.FirstBenchmarkReachesAnImpactful",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerDoctrinePairFirstBenchmarkTest::RunTest(const FString&)
{
    const EBreakerPointCurrency Currency = EBreakerPointCurrency::DoctrinePoints;
    const int32 Benchmark = UBreakerProgressionLibrary::DoctrinePointsPerBenchmark;
    TestEqual(TEXT("A benchmark is two points: one travel, one impactful"), Benchmark, 2);

    FBreakerQuestFlagSet FirstBenchmark;
    if (!TestTrue(TEXT("The campaign authors a first doctrine benchmark"), BreakerDoctrinePairFirstBenchmarkFlags(FirstBenchmark))) return false;
    TestEqual(TEXT("The first benchmark entitles exactly one benchmark's points"), UBreakerMissionLibrary::DoctrinePointEntitlement(FirstBenchmark), Benchmark);
    FBreakerQuestFlagSet WholeCampaign;
    BreakerDoctrinePairWholeCampaignFlags(WholeCampaign);

    const UBreakerProgressionNode* DefaultNode = GetDefault<UBreakerProgressionNode>();
    const FName GrantedId = UBreakerProgressionComponent::SwiftGrantedDashNodeId;
    FText Reason;
    for (const FBreakerDoctrinePairTree& Row : BreakerDoctrinePairTrees())
    {
        const UBreakerProgressionTree* Tree = Row.Tree;
        const FString Label(Row.Label);
        if (!TestNotNull(*(Label + TEXT(" tree")), Tree)) return false;
        const int32 UnpairedCount = Row.Unpaired.Num();
        auto IsUnpaired = [&Row](const FName& NodeId)
        {
            return Row.Unpaired.ContainsByPredicate([&NodeId](const TCHAR* Id) { return NodeId == FName(Id); });
        };

        // --- Shape: six pair roots plus the unpaired roots -------------------
        TestEqual(*(Label + TEXT(" has twelve pair nodes plus its unpaired roots")), Tree->Nodes.Num(), 12 + UnpairedCount);
        TestEqual(*(Label + TEXT(" pair table names six pairs")), Row.Pairs.Num(), 6);
        TArray<FName> Roots;
        TMap<FName, int32> Dependents;
        int32 Keystones = 0;
        for (const UBreakerProgressionNode* Node : Tree->Nodes)
        {
            const FString Context = Node->NodeId.ToString();
            // Cost 0 belongs to the granted node alone (O139): it is seeded,
            // never bought, and its refund is rank x 0 by construction.
            const bool bGranted = Node->NodeId == GrantedId;
            TestEqual(*(Context + TEXT(" is single rank")), Node->MaxRank, 1);
            TestEqual(*(Context + TEXT(" costs one, or zero only as the granted node")), Node->CostPerRank, bGranted ? 0 : 1);
            TestTrue(*(Context + TEXT(" spends the doctrine wallet")), Node->Currency == Currency);
            TestTrue(*(Context + TEXT(" is locked to the row's class")), Node->RequiredClass == Row.Class);
            TestTrue(*(Context + TEXT(" uses no prerequisite groups")), Node->PrerequisiteGroups.IsEmpty());
            if (Node->bCornerstone)
            {
                ++Keystones;
                TestEqual(*(Context + TEXT(" keystone sits at tier four")), Node->Tier, 4);
                TestEqual(*(Context + TEXT(" keystone keeps the six-invested gate")), Node->RequiredTreeInvestment, 6);
            }
            else
            {
                // The pair shape IS the node's default-constructed rule set:
                // the authoring adds nothing below the keystone.
                TestEqual(*(Context + TEXT(" sits at tier one")), Node->Tier, DefaultNode->Tier);
                TestEqual(*(Context + TEXT(" has no investment gate")), Node->RequiredTreeInvestment, DefaultNode->RequiredTreeInvestment);
                TestEqual(*(Context + TEXT(" default rank")), Node->MaxRank, DefaultNode->MaxRank);
                if (!bGranted) TestEqual(*(Context + TEXT(" default price")), Node->CostPerRank, DefaultNode->CostPerRank);
            }
            if (Node->Prerequisites.IsEmpty())
            {
                Roots.Add(Node->NodeId);
            }
            else
            {
                TestEqual(*(Context + TEXT(" has exactly one prerequisite")), Node->Prerequisites.Num(), 1);
                TestEqual(*(Context + TEXT(" prerequisite requires rank one")), Node->Prerequisites[0].RequiredRank, 1);
                TestTrue(*(Context + TEXT(" impactful half carries a rule tag")), Node->GrantedTags.Num() > 0);
                ++Dependents.FindOrAdd(Node->Prerequisites[0].NodeId);
            }
        }
        TestEqual(*(Label + TEXT(" has one keystone")), Keystones, 1);
        TestEqual(*(Label + TEXT(" has six travel roots plus its unpaired roots")), Roots.Num(), 6 + UnpairedCount);
        for (const FName& Root : Roots)
        {
            TestEqual(*(Root.ToString() + TEXT(" is the prerequisite of exactly one node, or of none when unpaired")),
                Dependents.FindRef(Root), IsUnpaired(Root) ? 0 : 1);
        }
        for (const auto& Dependent : Dependents)
        {
            TestTrue(*(Dependent.Key.ToString() + TEXT(" is a travel root")), Roots.Contains(Dependent.Key));
        }
        for (const TCHAR* Unpaired : Row.Unpaired)
        {
            const UBreakerProgressionNode* Node = Tree->FindNode(Unpaired);
            if (!TestNotNull(*(FString(Unpaired) + TEXT(" is authored")), Node)) return false;
            TestTrue(*(FString(Unpaired) + TEXT(" is a root")), Node->Prerequisites.IsEmpty());
            TestFalse(*(FString(Unpaired) + TEXT(" is not the keystone")), Node->bCornerstone);
        }

        // --- The pair table ---------------------------------------------------
        for (int32 Index = 0; Index < Row.Pairs.Num(); ++Index)
        {
            const FBreakerDoctrinePairRow& Pair = Row.Pairs[Index];
            const UBreakerProgressionNode* Travel = Tree->FindNode(Pair.Travel);
            const UBreakerProgressionNode* Impactful = Tree->FindNode(Pair.Impactful);
            if (!TestNotNull(*(FString(Pair.Travel) + TEXT(" is authored")), Travel)) return false;
            if (!TestNotNull(*(FString(Pair.Impactful) + TEXT(" is authored")), Impactful)) return false;
            TestTrue(*(FString(Pair.Travel) + TEXT(" is a travel root")), Travel->Prerequisites.IsEmpty());
            TestFalse(*(FString(Pair.Travel) + TEXT(" is not a keystone")), Travel->bCornerstone);
            TestFalse(*(FString(Pair.Travel) + TEXT(" is not also listed unpaired")), IsUnpaired(Travel->NodeId));
            if (!TestEqual(*(FString(Pair.Impactful) + TEXT(" follows one node")), Impactful->Prerequisites.Num(), 1)) return false;
            TestEqual(*(FString(Pair.Impactful) + TEXT(" follows its travel")), Impactful->Prerequisites[0].NodeId, FName(Pair.Travel));
            TestTrue(*(FString(Pair.Impactful) + TEXT(" is the keystone only in the last pair")), Impactful->bCornerstone == (Index == Row.Pairs.Num() - 1));
        }

        // --- One benchmark buys any pair's travel and then its impactful ------
        for (int32 Index = 0; Index < Row.Pairs.Num(); ++Index)
        {
            const FBreakerDoctrinePairRow& Pair = Row.Pairs[Index];
            const bool bKeystonePair = Index == Row.Pairs.Num() - 1;
            UBreakerProgressionComponent* Progression = BreakerDoctrinePairFreshCharacter(*this, Row.Class, FirstBenchmark, Benchmark);
            if (!Progression) return false;
            TestFalse(*(FString(Pair.Impactful) + TEXT(" refuses before its travel")), Progression->CanPurchaseNode(Tree, Pair.Impactful, Reason));
            if (!TestTrue(*(FString(Pair.Travel) + TEXT(" buys with the first point")), Progression->PurchaseNode(Tree, Pair.Travel, Reason))) return false;
            TestEqual(*(FString(Pair.Travel) + TEXT(" leaves one point")), Progression->GetUnspentPoints(Currency), Benchmark - 1);
            if (bKeystonePair)
            {
                TestFalse(*(FString(Pair.Impactful) + TEXT(" keystone refuses on one invested")), Progression->CanPurchaseNode(Tree, Pair.Impactful, Reason));
                // The granted node costs nothing, so it never counts as
                // investment: one travel bought is one point invested.
                TestEqual(*(Label + TEXT(" one point invested")), Progression->GetTreeInvestment(Tree), 1);
            }
            else
            {
                if (!TestTrue(*(FString(Pair.Impactful) + TEXT(" buys with the second point")), Progression->PurchaseNode(Tree, Pair.Impactful, Reason))) return false;
                TestEqual(*(FString(Pair.Impactful) + TEXT(" spends the whole benchmark")), Progression->GetUnspentPoints(Currency), 0);
                TestEqual(*(FString(Pair.Impactful) + TEXT(" is owned")), Progression->GetNodeRank(Pair.Impactful, Currency), 1);
            }
        }

        // --- An unpaired root buys with one point, or arrived with the class ---
        for (const TCHAR* Unpaired : Row.Unpaired)
        {
            const FString Context(Unpaired);
            UBreakerProgressionComponent* Progression = BreakerDoctrinePairFreshCharacter(*this, Row.Class, FirstBenchmark, Benchmark);
            if (!Progression) return false;
            if (FName(Unpaired) == GrantedId)
            {
                // Longstride: rank 1 the moment the class is chosen, cost 0,
                // and the wallet is untouched. Purchase is refused because
                // there is no rank left to buy, not because of a gate.
                TestEqual(*(Context + TEXT(" is already rank one after the class choice")), Progression->GetNodeRank(Unpaired, Currency), 1);
                TestFalse(*(Context + TEXT(" refuses purchase: granted, never bought")), Progression->CanPurchaseNode(Tree, Unpaired, Reason));
                TestEqual(*(Context + TEXT(" costs the wallet nothing")), Progression->GetUnspentPoints(Currency), Benchmark);
                TestEqual(*(Context + TEXT(" counts as no investment")), Progression->GetTreeInvestment(Tree), 0);
            }
            else
            {
                TestEqual(*(Context + TEXT(" is unowned on a fresh character")), Progression->GetNodeRank(Unpaired, Currency), 0);
                if (!TestTrue(*(Context + TEXT(" buys with one point")), Progression->PurchaseNode(Tree, Unpaired, Reason))) return false;
                TestEqual(*(Context + TEXT(" leaves one point")), Progression->GetUnspentPoints(Currency), Benchmark - 1);
                TestEqual(*(Context + TEXT(" is owned")), Progression->GetNodeRank(Unpaired, Currency), 1);
                TestFalse(*(Context + TEXT(" refuses a second rank")), Progression->CanPurchaseNode(Tree, Unpaired, Reason));
            }
        }

        // --- The keystone is the fourth pair by arithmetic ---------------------
        {
            const FBreakerDoctrinePairRow& Keystone = Row.Pairs.Last();
            UBreakerProgressionComponent* Progression = BreakerDoctrinePairFreshCharacter(*this, Row.Class, WholeCampaign, UBreakerProgressionLibrary::DoctrinePointGrant);
            if (!Progression) return false;
            if (!TestTrue(TEXT("Keystone travel buys first"), Progression->PurchaseNode(Tree, Keystone.Travel, Reason))) return false;
            TestFalse(*(Label + TEXT(" keystone refuses at one invested")), Progression->CanPurchaseNode(Tree, Keystone.Impactful, Reason));
            for (int32 Index = 0; Index < 2; ++Index)
            {
                if (!TestTrue(TEXT("A whole pair buys"), Progression->PurchaseNode(Tree, Row.Pairs[Index].Travel, Reason)
                    && Progression->PurchaseNode(Tree, Row.Pairs[Index].Impactful, Reason))) return false;
            }
            TestEqual(*(Label + TEXT(" five invested")), Progression->GetTreeInvestment(Tree), 5);
            TestFalse(*(Label + TEXT(" keystone refuses at five invested")), Progression->CanPurchaseNode(Tree, Keystone.Impactful, Reason));
            if (!TestTrue(TEXT("A third travel buys"), Progression->PurchaseNode(Tree, Row.Pairs[2].Travel, Reason))) return false;
            TestEqual(*(Label + TEXT(" six invested")), Progression->GetTreeInvestment(Tree), 6);
            // O86: the keystone also wants the commitment, which pays nothing.
            TestFalse(*(Label + TEXT(" keystone refuses uncommitted at six")), Progression->CanPurchaseNode(Tree, Keystone.Impactful, Reason));
            if (!TestTrue(*(Label + TEXT(" commits to the doctrine")), Progression->CommitToBranch(Tree->TreeId, Reason))) return false;
            if (!TestTrue(*(Label + TEXT(" keystone buys at six invested")), Progression->PurchaseNode(Tree, Keystone.Impactful, Reason))) return false;
            TestEqual(*(Label + TEXT(" keystone costs one")), Progression->GetUnspentPoints(Currency), 1);
            if (!TestTrue(TEXT("The third impactful closes the eight"), Progression->PurchaseNode(Tree, Row.Pairs[2].Impactful, Reason))) return false;
            TestEqual(*(Label + TEXT(" eight points buy four pairs")), Progression->GetUnspentPoints(Currency), 0);
        }
    }

    // --- Travel magnitudes that live on nodes ------------------------------
    BreakerDoctrinePairSingleEffect(*this, UBreakerProgressionLibrary::GetCasterVoidWhispererTree(), TEXT("Caster.VoidWhisperer.Lingering"), EBreakerNodeStatTarget::AbilityDuration, 30.0f);
    BreakerDoctrinePairSingleEffect(*this, UBreakerProgressionLibrary::GetCasterMultispellTree(), TEXT("Caster.Multispell.Reservoir"), EBreakerNodeStatTarget::MaxClassResource, 24.0f);

    // Swift's travel lines: single rank at the old two-rank totals. O2
    // PLACEHOLDER values, pinned as shipped.
    {
        using ET = EBreakerNodeStatTarget;
        using EB = EBreakerNodeStatBucket;
        using EC = EBreakerBuildCondition;
        const UBreakerProgressionTree* Kinetic = UBreakerProgressionLibrary::GetSwiftKineticTree();
        const UBreakerProgressionTree* Marksman = UBreakerProgressionLibrary::GetSwiftMarksmanTree();
        const UBreakerProgressionTree* Frenzy = UBreakerProgressionLibrary::GetSwiftFrenzyTree();
        BreakerDoctrinePairTravelLines(*this, Kinetic, TEXT("Swift.Kinetic.Carry"), {{ET::SlideSpeed, EB::IncreasedPercent, 24.0f, EC::Always}});
        BreakerDoctrinePairTravelLines(*this, Kinetic, TEXT("Swift.Kinetic.Redirect"), {{ET::AbilityCooldown, EB::IncreasedPercent, 40.0f, EC::Airborne}});
        BreakerDoctrinePairTravelLines(*this, Kinetic, TEXT("Swift.Kinetic.EvadeConversion"), {{ET::DodgeChance, EB::Flat, 8.0f, EC::Always}});
        BreakerDoctrinePairTravelLines(*this, Kinetic, TEXT("Swift.Kinetic.Downforce"), {{ET::Damage, EB::IncreasedPercent, 22.0f, EC::Airborne}});
        BreakerDoctrinePairTravelLines(*this, Marksman, TEXT("Swift.Marksman.LongLens"), {
            {ET::CriticalDamage, EB::Flat, 36.0f, EC::Always},
            {ET::Damage, EB::IncreasedPercent, 6.0f, EC::Aiming}});
        BreakerDoctrinePairTravelLines(*this, Marksman, TEXT("Swift.Marksman.Angle"), {{ET::RicochetCount, EB::Flat, 2.0f, EC::Always}});
        BreakerDoctrinePairTravelLines(*this, Marksman, TEXT("Swift.Marksman.PierceDiscipline"), {
            {ET::Pierce, EB::Flat, 2.0f, EC::Always},
            {ET::CriticalChance, EB::Flat, 12.0f, EC::Always},
            {ET::Damage, EB::IncreasedPercent, 6.0f, EC::Aiming}});
        BreakerDoctrinePairTravelLines(*this, Marksman, TEXT("Swift.Marksman.Deadeye"), {{ET::CriticalChance, EB::Flat, 8.0f, EC::Always}});
        BreakerDoctrinePairTravelLines(*this, Frenzy, TEXT("Swift.Frenzy.TriggerDiscipline"), {{ET::CriticalChance, EB::Flat, 6.0f, EC::Always}});
        BreakerDoctrinePairTravelLines(*this, Frenzy, TEXT("Swift.Frenzy.Loaded"), {{ET::Damage, EB::IncreasedPercent, 12.0f, EC::Redline}});
        BreakerDoctrinePairTravelLines(*this, Frenzy, TEXT("Swift.Frenzy.ShortLeash"), {{ET::MoveSpeed, EB::IncreasedPercent, 10.0f, EC::Always}});
        BreakerDoctrinePairTravelLines(*this, Frenzy, TEXT("Swift.Frenzy.Rhythm"), {{ET::CriticalChance, EB::Flat, 6.0f, EC::Always}});
        BreakerDoctrinePairTravelLines(*this, Frenzy, TEXT("Swift.Frenzy.DryFire"), {{ET::Damage, EB::IncreasedPercent, 10.0f, EC::Redline}});
        BreakerDoctrinePairTravelLines(*this, Frenzy, TEXT("Swift.Frenzy.Feed"), {{ET::Health, EB::Flat, 90.0f, EC::Always}});
        BreakerDoctrinePairTravelLines(*this, Frenzy, TEXT("Swift.Frenzy.Overrev"), {{ET::Damage, EB::IncreasedPercent, 24.0f, EC::Redline}});
    }

    // --- Travel magnitudes that live in Data/caster-resource.json ----------
    // The rank-one key is what a single-rank travel node reads; it carries
    // what rank two used to. O2 PLACEHOLDER values, pinned as shipped.
    const FBreakerCasterResourceTuning& Tuning = UBreakerManaComponent::GetResourceTuning();
    TestEqual(TEXT("Seep rank one"), Tuning.SeepRankOneMultiplier, 2.0f, 0.0001f);
    TestEqual(TEXT("Attrition rank one"), Tuning.AttritionRankOneRefund, 8.0f, 0.0001f);
    TestEqual(TEXT("Close rank one"), Tuning.CloseRankOneRangeCm, 900.0f, 0.0001f);
    TestEqual(TEXT("Debt rank one"), Tuning.DebtRankOneExtension, 20.0f, 0.0001f);
    TestEqual(TEXT("Bloodprice rank one"), Tuning.BloodpriceRankOneFraction, 0.2f, 0.0001f);
    TestEqual(TEXT("Patience rank one"), Tuning.PatienceRankOneDelay, 2.0f, 0.0001f);
    TestEqual(TEXT("Variance rank one"), Tuning.VarianceRankOneMultiplier, 3.0f, 0.0001f);
    TestEqual(TEXT("Sequence rank one"), Tuning.SequenceRankOneMana, 15.0f, 0.0001f);

    // --- Travel magnitudes that live on the Momentum component -------------
    // Swift has no Data keys: the rank-one member is what a single-rank
    // travel node reads, and it carries what rank two used to. O2
    // PLACEHOLDER values, pinned against the default-constructed component.
    const UBreakerMomentumComponent* Momentum = GetDefault<UBreakerMomentumComponent>();
    TestEqual(TEXT("Read the Room rank one"), Momentum->ReadTheRoomRankOneSeconds, 6.0f, 0.0001f);
    TestEqual(TEXT("Contact rank one"), Momentum->ContactRankOneSeconds, 0.70f, 0.0001f);
    TestEqual(TEXT("Landing rank one per metre"), Momentum->LandingRankOnePerMeter, 3.0f, 0.0001f);
    TestEqual(TEXT("Landing rank one cap"), Momentum->LandingRankOneCap, 30.0f, 0.0001f);

    // --- Travel magnitudes that live in Data/abilities.json ----------------
    // The definition's number and the CDO's member are held equal: the file
    // is the only magnitude authority and the ability reads its own member.
    BreakerDoctrinePairAuthoredNumber<UBreakerAbility_Cleave>(*this, TEXT("Caster.Cleave"), TEXT("FollowThroughRankOneKillRefund"), 6.0f,
        [](const UBreakerAbility_Cleave& Ability) { return Ability.FollowThroughRankOneKillRefund; });
    BreakerDoctrinePairAuthoredNumber<UBreakerAbility_Closequarter>(*this, TEXT("Caster.Closequarter"), TEXT("MomentumTransferRankOneSeconds"), 3.0f,
        [](const UBreakerAbility_Closequarter& Ability) { return Ability.MomentumTransferRankOneSeconds; });
    BreakerDoctrinePairAuthoredNumber<UBreakerAbility_Rot>(*this, TEXT("Caster.Rot"), TEXT("StandingWaterRankOneManaPerSecond"), 4.0f,
        [](const UBreakerAbility_Rot& Ability) { return Ability.StandingWaterRankOneManaPerSecond; });
    BreakerDoctrinePairAuthoredNumber<UBreakerAbility_Siphon>(*this, TEXT("Caster.Siphon"), TEXT("DrainRankOneThreshold"), 0.15f,
        [](const UBreakerAbility_Siphon& Ability) { return Ability.DrainRankOneThreshold; });
    BreakerDoctrinePairAuthoredNumber<UBreakerAbility_Resonance>(*this, TEXT("Caster.Resonance"), TEXT("PaymentRankOneManaPerStatus"), 4.0f,
        [](const UBreakerAbility_Resonance& Ability) { return Ability.PaymentRankOneManaPerStatus; });
    return true;
}

#endif  // WITH_DEV_AUTOMATION_TESTS
