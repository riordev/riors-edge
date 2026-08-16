#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include <initializer_list>
#include "Attributes/BreakerAttributeSet.h"
#include "Classes/BreakerMomentumComponent.h"
#include "Combat/BreakerCombatTypes.h"
#include "GameFramework/Actor.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionTypes.h"
#include "Weapons/BreakerWeaponComponent.h"
#include "Weapons/BreakerWeaponMath.h"

// ---------------------------------------------------------------------------
// THE LAST SWIFT RULE-HALVES IN THE WEAPON LAYER (Class-Kits §1.3 / §1.5).
//
// Same split as BreakerSwiftShotMechanicsTests: the suite is world-free, so
// each rule's DECISION math is a pure static pinned here, and each rule that
// can pay through a component with no world is exercised as a
// buy-the-node-observable change on a real progression + momentum rig. The
// wiring that needs a world (FireOnce's trace, reload timers) has the same
// inspection-plus-playtest standing as every other trace in Weapons/; what
// that costs is stated at the foot of this file.
// ---------------------------------------------------------------------------

namespace BreakerSwiftRuleTest
{
    // Prefixed, per the unity-build house rule about anonymous-namespace
    // helper collisions.

    struct FBreakerSwiftRig
    {
        AActor* Owner = nullptr;
        UBreakerProgressionComponent* Progression = nullptr;
        UBreakerCharacterMovementComponent* Movement = nullptr;
        UBreakerMomentumComponent* Momentum = nullptr;
        UBreakerAttributeSet* Attributes = nullptr;
    };

    // A Swift-locked rig whose progression component genuinely OWNS the given
    // class nodes at the given ranks — loaded state against the real fallback
    // trees, so every rank read below goes through the same GetNodeRank the
    // live consumers use.
    static FBreakerSwiftRig BreakerMakeSwiftRig(std::initializer_list<TPair<FName, int32>> ClassRanks)
    {
        FBreakerSwiftRig Rig;
        Rig.Owner = NewObject<AActor>();
        Rig.Progression = NewObject<UBreakerProgressionComponent>(Rig.Owner);
        Rig.Movement = NewObject<UBreakerCharacterMovementComponent>(Rig.Owner);
        Rig.Momentum = NewObject<UBreakerMomentumComponent>(Rig.Owner);
        Rig.Attributes = NewObject<UBreakerAttributeSet>();

        FBreakerProgressionState State;
        State.PermanentClass = EBreakerClassId::Swift;
        for (const TPair<FName, int32>& Pair : ClassRanks)
        {
            State.ClassNodeRanks.Add({Pair.Key, Pair.Value});
        }
        Rig.Progression->LoadProgressionState(State);
        // BindAttributes runs HandleProgressionChanged, which finds the
        // sibling progression component already holding Swift.
        Rig.Momentum->BindAttributes(Rig.Attributes);
        return Rig;
    }

    // A hitscan shot record: one pellet, hit or miss. bHit is the legacy OR
    // across the spread, which is exactly what the momentum handlers read.
    static FBreakerShotResult BreakerMakeHitscanShot(bool bHit, bool bWeakPoint = false)
    {
        FBreakerShotResult Shot;
        Shot.bFired = true;
        Shot.bHit = bHit;
        Shot.bWeakPoint = bWeakPoint;
        Shot.Pellets.AddDefaulted();
        Shot.Pellets[0].bHit = bHit;
        return Shot;
    }
}

// ---------------------------------------------------------------------------
// Marksman rule halves: the pure decision math (Class-Kits §1.5 M2/M3/M5/M11)
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerMarksmanRuleHalvesTest,
    "RiorsEdge.Weapons.MarksmanRules.PureHalves",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerMarksmanRuleHalvesTest::RunTest(const FString& Parameters)
{
    // Steady (§1.5 M2). Without the node — the whole non-owner population —
    // the composed movement penalty passes through untouched, to the bit.
    TestEqual(TEXT("no Steady, no relief"), FBreakerWeaponMath::SteadyMovementSpreadDegrees(2.0f, 1.0f, 0, false), 2.0f);
    TestEqual(TEXT("Steady from the hip changes nothing"), FBreakerWeaponMath::SteadyMovementSpreadDegrees(2.0f, 0.0f, 2, false), 2.0f);
    // R1: fully sighted, grounded movement stops widening spread entirely.
    TestEqual(TEXT("R1 at full ADS removes the grounded movement penalty"), FBreakerWeaponMath::SteadyMovementSpreadDegrees(2.0f, 1.0f, 1, false), 0.0f);
    // Partway into the sights is partway to the rule, the same ramp as every
    // other ADS benefit.
    TestEqual(TEXT("half ADS is half the relief"), FBreakerWeaponMath::SteadyMovementSpreadDegrees(2.0f, 0.5f, 1, false), 1.0f);
    // The doc's rank split: airborne movement keeps its penalty at R1 and
    // loses it at R2 ("R2: ADS while airborne likewise").
    TestEqual(TEXT("R1 airborne keeps the penalty"), FBreakerWeaponMath::SteadyMovementSpreadDegrees(2.0f, 1.0f, 1, true), 2.0f);
    TestEqual(TEXT("R2 airborne loses it"), FBreakerWeaponMath::SteadyMovementSpreadDegrees(2.0f, 1.0f, 2, true), 0.0f);

    // Called Shot (§1.5 M11): 25 m -> 10 m needs BOTH the node and Redline.
    TestEqual(TEXT("no node, no rewrite"), FBreakerWeaponMath::LeadRangeGateCm(2500.0f, false, true), 2500.0f);
    TestEqual(TEXT("node without Redline keeps Lead's gate"), FBreakerWeaponMath::LeadRangeGateCm(2500.0f, true, false), 2500.0f);
    TestEqual(TEXT("node at Redline drops the gate to 10 m"), FBreakerWeaponMath::LeadRangeGateCm(2500.0f, true, true), 1000.0f);

    // Ledger (§1.5 M3): 25% at R1, 50% at R2, nothing unowned.
    TestEqual(TEXT("Ledger unowned refunds nothing"), FBreakerWeaponMath::LedgerRefundFraction(0), 0.0f);
    TestEqual(TEXT("Ledger R1 refunds a quarter"), FBreakerWeaponMath::LedgerRefundFraction(1), 0.25f);
    TestEqual(TEXT("Ledger R2 refunds half"), FBreakerWeaponMath::LedgerRefundFraction(2), 0.50f);

    // Mark Economy (§1.5 M5): 15 m at R1, 25 m at R2, no jump unowned.
    TestEqual(TEXT("no node, no jump radius"), FBreakerWeaponMath::MarkJumpRadiusCm(0), 0.0f);
    TestEqual(TEXT("R1 seeks 15 m"), FBreakerWeaponMath::MarkJumpRadiusCm(1), 1500.0f);
    TestEqual(TEXT("R2 seeks 25 m"), FBreakerWeaponMath::MarkJumpRadiusCm(2), 2500.0f);

    // Loaded (§1.3 F2): R1 refunds half the window's shots (floored — half a
    // round is not a round), R2 refunds them all.
    TestEqual(TEXT("Loaded unowned refunds nothing"), FBreakerWeaponMath::LoadedRefundRounds(5, 0), 0);
    TestEqual(TEXT("R1 refunds half, rounded down"), FBreakerWeaponMath::LoadedRefundRounds(5, 1), 2);
    TestEqual(TEXT("R1 with one shot refunds nothing"), FBreakerWeaponMath::LoadedRefundRounds(1, 1), 0);
    TestEqual(TEXT("R2 refunds every shot in the window"), FBreakerWeaponMath::LoadedRefundRounds(5, 2), 5);
    TestEqual(TEXT("an empty window refunds nothing at any rank"), FBreakerWeaponMath::LoadedRefundRounds(0, 2), 0);
    return true;
}

// ---------------------------------------------------------------------------
// Frenzy rule halves: the pure decision math (Class-Kits §1.3 F1/F4/F6)
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerFrenzyRuleHalvesTest,
    "RiorsEdge.Classes.FrenzyRules.PureHalves",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerFrenzyRuleHalvesTest::RunTest(const FString& Parameters)
{
    // F1 posture: the authored requirement holds for non-owners, is satisfied
    // by the posture itself, and is waived at any rank of the node.
    TestFalse(TEXT("grounded without the node fails the posture gate"), UBreakerMomentumComponent::WeakPointPostureSatisfied(true, false, 0));
    TestTrue(TEXT("airborne or sliding always satisfies it"), UBreakerMomentumComponent::WeakPointPostureSatisfied(true, true, 0));
    TestTrue(TEXT("the node waives it on the ground"), UBreakerMomentumComponent::WeakPointPostureSatisfied(true, false, 1));
    TestTrue(TEXT("a component that never required posture never gates"), UBreakerMomentumComponent::WeakPointPostureSatisfied(false, false, 0));

    // F1 R2: internal cooldown 0.25 -> 0.15. Ranks 0-1 keep the authored knob.
    TestEqual(TEXT("no rank keeps the authored interval"), UBreakerMomentumComponent::WeakPointIntervalForRank(0.25f, 0), 0.25f);
    TestEqual(TEXT("R1 keeps the authored interval"), UBreakerMomentumComponent::WeakPointIntervalForRank(0.25f, 1), 0.25f);
    TestEqual(TEXT("R2 runs at 0.15s"), UBreakerMomentumComponent::WeakPointIntervalForRank(0.25f, 2), 0.15f);

    // F4: every 5th hit at R1, every 4th at R2, nothing unowned.
    TestEqual(TEXT("Rhythm unowned has no stride"), UBreakerMomentumComponent::RhythmStride(0), 0);
    TestEqual(TEXT("R1 pays every 5th"), UBreakerMomentumComponent::RhythmStride(1), 5);
    TestEqual(TEXT("R2 pays every 4th"), UBreakerMomentumComponent::RhythmStride(2), 4);

    // F6: 10% at R1, 20% at R2.
    TestEqual(TEXT("Feed unowned refunds nothing"), UBreakerMomentumComponent::FeedRefundFraction(0), 0.0f);
    TestEqual(TEXT("Feed R1 refunds a tenth"), UBreakerMomentumComponent::FeedRefundFraction(1), 0.10f);
    TestEqual(TEXT("Feed R2 refunds a fifth"), UBreakerMomentumComponent::FeedRefundFraction(2), 0.20f);
    return true;
}

// ---------------------------------------------------------------------------
// Buying the node changes what the loop does: the Frenzy rules on a real rig
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerFrenzyRulesObservableTest,
    "RiorsEdge.Classes.FrenzyRules.BuyNodeObservable",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerFrenzyRulesObservableTest::RunTest(const FString& Parameters)
{
    using namespace BreakerSwiftRuleTest;

    // ---- F1 Trigger Discipline: the grounded weak point pays -------------
    // No movement component posture (a worldless rig is grounded), the
    // component's default bWeakPointRequiresAirborneOrSlide=true: without the
    // node the weak-point grant is refused, with it the grant queues and the
    // next loop tick pays it.
    {
        FBreakerSwiftRig Without = BreakerMakeSwiftRig({});
        Without.Attributes->ApplyClassResource(10.0f);
        Without.Momentum->AdvanceLoop(0.01f);
        Without.Momentum->HandleShot(BreakerMakeHitscanShot(true, /*bWeakPoint=*/true));
        Without.Momentum->AdvanceLoop(0.5f);
        TestEqual(TEXT("a grounded weak point pays nothing without the node"), Without.Momentum->GetMomentum(), 10.0f);

        FBreakerSwiftRig With = BreakerMakeSwiftRig({ TPair<FName, int32>(TEXT("Swift.Frenzy.TriggerDiscipline"), 1) });
        With.Attributes->ApplyClassResource(10.0f);
        With.Momentum->AdvanceLoop(0.01f);
        With.Momentum->HandleShot(BreakerMakeHitscanShot(true, /*bWeakPoint=*/true));
        With.Momentum->AdvanceLoop(0.5f);
        TestEqual(TEXT("the node makes the same grounded weak point pay the grant"), With.Momentum->GetMomentum(), 10.0f + With.Momentum->WeakPointGrant);
    }

    // ---- F4 Rhythm: every 5th consecutive hit, outside the cap -----------
    {
        FBreakerSwiftRig Rig = BreakerMakeSwiftRig({ TPair<FName, int32>(TEXT("Swift.Frenzy.Rhythm"), 1) });
        Rig.Attributes->ApplyClassResource(20.0f);
        for (int32 Hit = 0; Hit < 4; ++Hit)
        {
            Rig.Momentum->HandleShot(BreakerMakeHitscanShot(true));
        }
        TestEqual(TEXT("four hits pay nothing"), Rig.Momentum->GetMomentum(), 20.0f);
        Rig.Momentum->HandleShot(BreakerMakeHitscanShot(true));
        TestEqual(TEXT("the fifth consecutive hit pays +8, no loop tick needed (outside the cap)"), Rig.Momentum->GetMomentum(), 28.0f);

        // A miss resets the counter — §1.3 F4's own clause.
        Rig.Momentum->HandleShot(BreakerMakeHitscanShot(false));
        for (int32 Hit = 0; Hit < 4; ++Hit)
        {
            Rig.Momentum->HandleShot(BreakerMakeHitscanShot(true));
        }
        TestEqual(TEXT("a miss resets: four hits after it pay nothing"), Rig.Momentum->GetMomentum(), 28.0f);
        Rig.Momentum->HandleShot(BreakerMakeHitscanShot(true));
        TestEqual(TEXT("the rebuilt streak pays on its own fifth hit"), Rig.Momentum->GetMomentum(), 36.0f);

        // A rocket's shot record has no pellets: it is not a miss, and must
        // not break the rhythm.
        FBreakerShotResult Rocket;
        Rocket.bFired = true;
        for (int32 Hit = 0; Hit < 4; ++Hit)
        {
            Rig.Momentum->HandleShot(BreakerMakeHitscanShot(true));
        }
        Rig.Momentum->HandleShot(Rocket);
        Rig.Momentum->HandleShot(BreakerMakeHitscanShot(true));
        TestEqual(TEXT("a pellet-less rocket record does not reset the streak"), Rig.Momentum->GetMomentum(), 44.0f);
    }

    // ---- F5 Dry Fire: the last round pays --------------------------------
    {
        FBreakerSwiftRig Without = BreakerMakeSwiftRig({});
        Without.Attributes->ApplyClassResource(20.0f);
        Without.Momentum->HandleMagazineEmptied(true);
        TestEqual(TEXT("emptying a magazine pays nothing without the node"), Without.Momentum->GetMomentum(), 20.0f);

        FBreakerSwiftRig With = BreakerMakeSwiftRig({ TPair<FName, int32>(TEXT("Swift.Frenzy.DryFire"), 1) });
        With.Attributes->ApplyClassResource(20.0f);
        With.Momentum->HandleMagazineEmptied(true);
        TestEqual(TEXT("the node pays +12 on the last round"), With.Momentum->GetMomentum(), 32.0f);
        // F5 has no started-full clause — that parameter belongs to Scrap's
        // dump bonus. Emptying any magazine rewards emptying it.
        With.Momentum->HandleMagazineEmptied(false);
        TestEqual(TEXT("a topped-off magazine's last round still pays"), With.Momentum->GetMomentum(), 44.0f);
    }

    // ---- F6 Feed: kills refund a fraction of the last observed spend ------
    {
        FBreakerSwiftRig Rig = BreakerMakeSwiftRig({ TPair<FName, int32>(TEXT("Swift.Frenzy.Feed"), 2) });
        Rig.Attributes->ApplyClassResource(80.0f);
        // One loop tick baselines the observer at 80 (an increase is never a
        // spend).
        Rig.Momentum->AdvanceLoop(0.01f);
        TestEqual(TEXT("filling the bar is not a spend"), Rig.Momentum->GetLastObservedSpend(), 0.0f);

        // An external drop of 40 — the shape of Lead's cost leaving through
        // the ability cost effect, the one external writer that decreases the
        // class resource.
        Rig.Attributes->ApplyClassResource(40.0f);
        Rig.Momentum->HandleKillDealt(FBreakerHitContext());
        TestEqual(TEXT("the kill observes the 40-point spend"), Rig.Momentum->GetLastObservedSpend(), 40.0f);
        TestEqual(TEXT("R2 refunds 20% of it"), Rig.Momentum->GetMomentum(), 48.0f);
        // Every kill pays against the same most-recent cost until a new spend
        // replaces it — F6 is per kill, not per cast.
        Rig.Momentum->HandleKillDealt(FBreakerHitContext());
        TestEqual(TEXT("a second kill refunds against the same cost"), Rig.Momentum->GetMomentum(), 56.0f);

        // With no spend ever observed, a kill pays nothing: the refund is OF
        // a cost, not a flat Resource-on-Kill duplicate (the doc's own words).
        FBreakerSwiftRig Fresh = BreakerMakeSwiftRig({ TPair<FName, int32>(TEXT("Swift.Frenzy.Feed"), 1) });
        Fresh.Attributes->ApplyClassResource(50.0f);
        Fresh.Momentum->AdvanceLoop(0.01f);
        Fresh.Momentum->HandleKillDealt(FBreakerHitContext());
        TestEqual(TEXT("a Swift who has cast nothing gets nothing from Feed"), Fresh.Momentum->GetMomentum(), 50.0f);
    }
    return true;
}

// ---------------------------------------------------------------------------
// WHAT THESE TESTS DO NOT COVER, stated plainly
// ---------------------------------------------------------------------------
// The suite is world-free, so nothing here fires a real trigger pull or runs
// a reload timer. Not covered, and not coverable without a world:
//  1. Steady / Called Shot / Ledger / Mark Economy THROUGH FireOnce — the
//     rank reads, the mark bookkeeping and the jump's FindNearestChainTarget
//     call. Their decision math is pinned above; the wiring has the same
//     inspection-plus-playtest standing as every other trace in Weapons/.
//  2. Loaded's captured refund through StartReload/FinishReload (both are
//     timer-driven). The refund rule itself is LoadedRefundRounds, pinned.
//  3. The delegate BINDINGS in UBreakerMomentumComponent::BeginPlay
//     (OnShot / OnMagazineEmptied / OnKillDealt) — BeginPlay needs a world;
//     the handlers are called directly here, the same convention
//     HandleProgressionChanged already established.
//  4. Dry Fire R2's cooldown-refund half: not built at all (no cooldown seam
//     exists outside the ability system); recorded as WAITING ON at the node.

#endif  // WITH_DEV_AUTOMATION_TESTS
