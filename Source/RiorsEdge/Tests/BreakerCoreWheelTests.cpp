#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "GameFramework/Actor.h"
#include "Items/BreakerEquipmentComponent.h"
#include "Progression/BreakerCoreWheelMath.h"
#include "Progression/BreakerExperience.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionLibrary.h"
#include "Progression/BreakerProgressionNode.h"
#include "Progression/BreakerProgressionTree.h"

// ---------------------------------------------------------------------------
// The Core wheel under O211-O213: which wedges are drawn dark, and what a
// respec costs. The rules live in Progression/BreakerCoreWheelMath.h; these
// tests prove the rules and then the one caller that pays.
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerCoreWheelDarkConstellationsTest,
    "RiorsEdge.Progression.CoreWheel.DarkConstellations",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerCoreWheelDarkConstellationsTest::RunTest(const FString& Parameters)
{
    // O212: a constellation with a silent node is drawn dark. The derivation
    // is over the tree — an effect on a laneless target, or a node granting
    // nothing — and the shipped Core has neither: every Core node authors a
    // lane the aggregator pays or grants a tag or an ability.
    //
    // THE DOCUMENTED SET IS EMPTY, AND THE CENSUS DISAGREES BY ONE. The
    // census's second silence axis — a granted tag nothing reads — is a
    // source scan (Scripts/status.py's consumer index) with no runtime
    // equivalent, so BreakerCoreDarkConstellations cannot see it and this
    // test asserts what the runtime can derive: nothing dark. The constellation
    // the census reports silent is deliberately NOT hand-listed here to make
    // the two agree (BreakerCoreWheelMath.h records why at the site). When
    // tag consumption is reflected at runtime the predicate widens and this
    // expected set moves with it, in the same commit.
    const UBreakerProgressionTree* Core = UBreakerProgressionLibrary::GetCoreSliceTree();
    if (!TestNotNull(TEXT("Core tree exists"), Core)) return false;

    const TSet<FName> Dark = BreakerCoreDarkConstellations(Core);
    TArray<FString> DarkNames;
    for (const FName& Name : Dark) DarkNames.Add(Name.ToString());
    TestEqual(*FString::Printf(TEXT("No shipped Core constellation is dark by the runtime derivation (found: %s)"),
        DarkNames.Num() ? *FString::Join(DarkNames, TEXT(", ")) : TEXT("none")), Dark.Num(), 0);

    // Every wheel's entry is lit: an entry in a dark wedge would be a start
    // the wheel draws unselectable (O212's rule applied to O211's hub).
    for (const FName& Entry : Core->EntryNodeIds)
    {
        const UBreakerProgressionNode* Node = Core->FindNode(Entry);
        if (!TestNotNull(*(Entry.ToString() + TEXT(" (entry) resolves")), Node)) continue;
        TestFalse(*(Entry.ToString() + TEXT(" enters a lit wedge")), Dark.Contains(Node->Constellation));
    }

    // AND THE DERIVATION CAN SEE SILENCE. A throwaway tree — never the
    // process-lifetime singleton, which leaks between tests — carrying one
    // node on a target with no aggregation lane and one node granting
    // nothing. Both are the failures O212 draws dark, and each alone darkens
    // its wedge.
    UBreakerProgressionTree* Probe = NewObject<UBreakerProgressionTree>();
    Probe->TreeId = TEXT("Test.Probe");
    Probe->Currency = EBreakerPointCurrency::CorePoints;

    UBreakerProgressionNode* Laneless = NewObject<UBreakerProgressionNode>();
    Laneless->NodeId = TEXT("Core.ProbeA.Laneless");
    Laneless->Constellation = TEXT("ProbeA");
    FBreakerNodeEffect Effect;
    Effect.StatTarget = EBreakerNodeStatTarget::Lifesteal;   // the O30 target left unwired on purpose
    Effect.StatBucket = EBreakerNodeStatBucket::IncreasedPercent;
    Effect.ValuePerRank = 1.0f;
    Laneless->Effects.Add(Effect);
    TestFalse(TEXT("the probe's laneless target really has no lane"), BreakerStatTargetHasAggregationLane(Effect.StatTarget));
    Probe->Nodes.Add(Laneless);

    UBreakerProgressionNode* Empty = NewObject<UBreakerProgressionNode>();
    Empty->NodeId = TEXT("Core.ProbeB.Empty");
    Empty->Constellation = TEXT("ProbeB");
    Probe->Nodes.Add(Empty);

    UBreakerProgressionNode* Lit = NewObject<UBreakerProgressionNode>();
    Lit->NodeId = TEXT("Core.ProbeC.Lit");
    Lit->Constellation = TEXT("ProbeC");
    FBreakerNodeEffect LitEffect;
    LitEffect.StatTarget = EBreakerNodeStatTarget::Damage;
    LitEffect.StatBucket = EBreakerNodeStatBucket::IncreasedPercent;
    LitEffect.ValuePerRank = 1.0f;
    Lit->Effects.Add(LitEffect);
    Probe->Nodes.Add(Lit);

    const TSet<FName> ProbeDark = BreakerCoreDarkConstellations(Probe);
    TestEqual(TEXT("exactly the two silent wedges are dark"), ProbeDark.Num(), 2);
    TestTrue(TEXT("a laneless effect darkens its wedge"), ProbeDark.Contains(TEXT("ProbeA")));
    TestTrue(TEXT("a node granting nothing darkens its wedge"), ProbeDark.Contains(TEXT("ProbeB")));
    TestFalse(TEXT("a lit wedge stays lit"), ProbeDark.Contains(TEXT("ProbeC")));
    TestTrue(TEXT("a null tree is nothing dark, not a crash"), BreakerCoreDarkConstellations(nullptr).Num() == 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerCoreRespecFreeThenRiftglassTest,
    "RiorsEdge.Progression.CoreRespec.FreeThenRiftglass",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerCoreRespecFreeThenRiftglassTest::RunTest(const FString& Parameters)
{
    using namespace BreakerCoreWheel;

    // O213 free until level N, Riftglass after; O239 rules N as the level the
    // ability token schedule completes at. THE TRIPWIRE: the two constants
    // live in different headers because the wheel's maths stays world-free,
    // so nothing but this line stops them drifting apart the way the mantle
    // height once did (pawn 150 against grammar 145). Moving
    // AbilityCompletionLevel without moving N is a red, which is the point.
    TestEqual(TEXT("N is the ability schedule's completion level (O239)"),
        CoreRespecFreeUntilLevel, UBreakerProgressionLibrary::AbilityCompletionLevel);
    TestTrue(TEXT("N sits inside [1, CorePointCapLevel]"),
        CoreRespecFreeUntilLevel >= 1 && CoreRespecFreeUntilLevel <= UBreakerProgressionLibrary::CorePointCapLevel);
    TestTrue(TEXT("the Riftglass price is a price"), CoreRespecRiftglass > 0);

    // The pure rule.
    TestTrue(TEXT("level 1 is free"), BreakerCoreRespecCost(1).IsFree());
    TestTrue(TEXT("one level below N is free"), BreakerCoreRespecCost(CoreRespecFreeUntilLevel - 1).IsFree());
    TestFalse(TEXT("N itself is paid"), BreakerCoreRespecCost(CoreRespecFreeUntilLevel).IsFree());
    TestEqual(TEXT("at N the price is the one placeholder amount"), BreakerCoreRespecCost(CoreRespecFreeUntilLevel).Amount, CoreRespecRiftglass);
    TestEqual(TEXT("the cap pays the same one amount"), BreakerCoreRespecCost(UBreakerProgressionLibrary::CorePointCapLevel).Amount, CoreRespecRiftglass);

    const UBreakerProgressionTree* Core = UBreakerProgressionLibrary::GetCoreSliceTree();
    const FName Entry(TEXT("Core.Precision.Sightline"));
    FText Failure;

    // ---- Below N: the wallet is untouched and the points come back --------
    {
        AActor* Owner = NewObject<AActor>();
        UBreakerProgressionComponent* Progression = NewObject<UBreakerProgressionComponent>(Owner);
        UBreakerEquipmentComponent* Equipment = NewObject<UBreakerEquipmentComponent>(Owner);
        Progression->ApplySliceDefaultsIfFresh();
        TestTrue(TEXT("a fresh character sits below N"), Progression->GetCharacterLevel() < CoreRespecFreeUntilLevel);
        // Something in the wallet, so "untouched" is a real assertion rather
        // than zero staying zero. The same flat grant the rift payout uses.
        Equipment->GrantForgeCurrency(CoreRespecRiftglass);
        const int32 GlassBefore = Equipment->GetForgeWallet().Get();
        const int32 PointsBefore = Progression->GetUnspentPoints(EBreakerPointCurrency::CorePoints);

        TestTrue(TEXT("the entry purchases"), Progression->PurchaseNode(Core, Entry, Failure));
        TestTrue(TEXT("below N the Core respec succeeds"), Progression->RespecCore(Failure));
        TestEqual(TEXT("below N the wallet is untouched"), Equipment->GetForgeWallet().Get(), GlassBefore);
        TestEqual(TEXT("below N the rank is cleared"), Progression->GetNodeRank(Entry, EBreakerPointCurrency::CorePoints), 0);
        TestEqual(TEXT("below N the point comes back"), Progression->GetUnspentPoints(EBreakerPointCurrency::CorePoints), PointsBefore);
    }

    // ---- At N: refused on an empty wallet, debited exactly once paid ------
    {
        AActor* Owner = NewObject<AActor>();
        UBreakerProgressionComponent* Progression = NewObject<UBreakerProgressionComponent>(Owner);
        UBreakerEquipmentComponent* Equipment = NewObject<UBreakerEquipmentComponent>(Owner);
        Progression->ApplySliceDefaultsIfFresh();
        // Reach N the way the game does: XP, never a setter.
        Progression->AwardExperience(UBreakerExperienceLibrary::TotalXpToReachLevel(
            CoreRespecFreeUntilLevel, Progression->ExperienceCurve) - Progression->GetTotalExperience());
        TestEqual(TEXT("the character stands at N"), Progression->GetCharacterLevel(), CoreRespecFreeUntilLevel);
        TestEqual(TEXT("the wallet is empty"), Equipment->GetForgeWallet().Get(), 0);

        TestTrue(TEXT("the entry purchases"), Progression->PurchaseNode(Core, Entry, Failure));
        const int32 PointsAfterBuy = Progression->GetUnspentPoints(EBreakerPointCurrency::CorePoints);

        TestFalse(TEXT("at N with an empty wallet the respec refuses"), Progression->RespecCore(Failure));
        TestFalse(TEXT("the refusal carries a reason the UI can show"), Failure.IsEmpty());
        TestEqual(TEXT("a refused respec leaves the rank standing"), Progression->GetNodeRank(Entry, EBreakerPointCurrency::CorePoints), 1);
        TestEqual(TEXT("a refused respec refunds nothing"), Progression->GetUnspentPoints(EBreakerPointCurrency::CorePoints), PointsAfterBuy);
        TestEqual(TEXT("a refused respec takes nothing"), Equipment->GetForgeWallet().Get(), 0);
        // The Forge path is not a way around the price.
        TestFalse(TEXT("naming the Forge does not waive the Core price"),
            Progression->RespecAtForge(EBreakerPointCurrency::CorePoints, true, Failure));
        TestEqual(TEXT("the rank still stands"), Progression->GetNodeRank(Entry, EBreakerPointCurrency::CorePoints), 1);

        // Exactly the price, and one Riftglass over it, so "exactly the cost"
        // is distinguishable from "the whole wallet".
        const FBreakerForgeCost Cost = BreakerCoreRespecCost(Progression->GetCharacterLevel());
        Equipment->GrantForgeCurrency(Cost.Amount + 1);
        TestTrue(TEXT("at N with the price held the respec succeeds"), Progression->RespecCore(Failure));
        TestEqual(TEXT("the respec debits exactly its price"), Equipment->GetForgeWallet().Get(), 1);
        TestEqual(TEXT("a paid respec clears the rank"), Progression->GetNodeRank(Entry, EBreakerPointCurrency::CorePoints), 0);
        TestEqual(TEXT("a paid respec refunds the point"), Progression->GetUnspentPoints(EBreakerPointCurrency::CorePoints), PointsAfterBuy + 1);
    }

    // ---- At N with no wallet at all: refused, never waived ---------------
    {
        UBreakerProgressionComponent* Progression = NewObject<UBreakerProgressionComponent>(NewObject<AActor>());
        Progression->ApplySliceDefaultsIfFresh();
        Progression->AwardExperience(UBreakerExperienceLibrary::TotalXpToReachLevel(
            CoreRespecFreeUntilLevel, Progression->ExperienceCurve) - Progression->GetTotalExperience());
        TestTrue(TEXT("the entry purchases"), Progression->PurchaseNode(Core, Entry, Failure));
        TestFalse(TEXT("a pawn holding no wallet cannot pay, and is not waived"), Progression->RespecCore(Failure));
        TestEqual(TEXT("its rank stands"), Progression->GetNodeRank(Entry, EBreakerPointCurrency::CorePoints), 1);
    }
    return true;
}

#endif
