#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Classes/BreakerChargeComponent.h"
#include "Classes/BreakerGritComponent.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerStatusComponent.h"
#include "GameFramework/Actor.h"
#include "Progression/BreakerProgressionComponent.h"

// ---------------------------------------------------------------------------
// TANK AND SUPPORT BRANCH NODES PAY (2026-08-16). Every test here follows one
// contract: buy the node -> an observable rule change on a rig; without it,
// bit-identical behaviour. The rigs are the house world-less pattern —
// NewObject actor (ROLE_Authority), components bound by hand, loops advanced
// through the same Advance* entry points the ticks call.
// ---------------------------------------------------------------------------

namespace BreakerBranchNodeTest
{
    // Prefixed, per the unity-build house rule.
    struct FBreakerBranchNodeRig
    {
        AActor* Owner = nullptr;
        UBreakerProgressionComponent* Progression = nullptr;
        UBreakerAttributeSet* Attributes = nullptr;
        UBreakerCombatComponent* Combat = nullptr;
    };

    static FBreakerBranchNodeRig BreakerMakeBranchRig(EBreakerClassId ClassId,
        std::initializer_list<TPair<const TCHAR*, int32>> NodeRanks)
    {
        FBreakerBranchNodeRig Rig;
        Rig.Owner = NewObject<AActor>();
        Rig.Attributes = NewObject<UBreakerAttributeSet>();
        Rig.Combat = NewObject<UBreakerCombatComponent>(Rig.Owner);
        Rig.Combat->BindAttributes(Rig.Attributes);
        Rig.Progression = NewObject<UBreakerProgressionComponent>(Rig.Owner);
        Rig.Progression->BindAttributes(Rig.Attributes);
        FBreakerProgressionState State;
        State.PermanentClass = ClassId;
        for (const TPair<const TCHAR*, int32>& Pair : NodeRanks)
        {
            State.CoreNodeRanks.Add({FName(Pair.Key), Pair.Value});
        }
        Rig.Progression->LoadProgressionState(State);
        return Rig;
    }

    static UBreakerGritComponent* BreakerAddGrit(FBreakerBranchNodeRig& Rig, float MaxHealth)
    {
        Rig.Attributes->ApplyMaxHealth(MaxHealth);
        UBreakerGritComponent* Grit = NewObject<UBreakerGritComponent>(Rig.Owner);
        Grit->BindAttributes(Rig.Attributes);
        return Grit;
    }

    static UBreakerChargeComponent* BreakerAddCharge(FBreakerBranchNodeRig& Rig, float MaxHealth)
    {
        Rig.Attributes->ApplyMaxHealth(MaxHealth);
        UBreakerChargeComponent* Charge = NewObject<UBreakerChargeComponent>(Rig.Owner);
        Charge->BindAttributes(Rig.Attributes);
        return Charge;
    }
}

// ---------------------------------------------------------------------------
// L4 Feed the Wound: shield absorption pays at two-thirds rate (R2: full),
// against the unowned half-rate baseline.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerNodeFeedTheWoundTest,
    "RiorsEdge.Progression.BranchNodes.Tank.FeedTheWound",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerNodeFeedTheWoundTest::RunTest(const FString& Parameters)
{
    using namespace BreakerBranchNodeTest;
    auto RunShieldAbsorb = [](int32 Rank) -> float
    {
        FBreakerBranchNodeRig Rig = BreakerMakeBranchRig(EBreakerClassId::Tank,
            {{TEXT("Tank.Leech.FeedTheWound"), Rank}});
        UBreakerGritComponent* Grit = BreakerAddGrit(Rig, 1000.0f);
        Grit->SetInCombat(true);
        const float AfterEntry = Grit->GetGrit();
        Grit->AdvanceLoop(1.0f);   // fill the per-source buckets
        Grit->NotifyDamageTaken(0.0f, 200.0f, /*bSelfInflicted=*/false, 1.0f);
        Grit->AdvanceLoop(1.0f);
        return Grit->GetGrit() - AfterEntry;
    };

    // 200 shield damage on 1000 health at 1-per-2%: 10 units before the rate.
    TestEqual(TEXT("Unowned, shield absorption pays at half rate"), RunShieldAbsorb(0), 5.0f, 0.01f);
    TestEqual(TEXT("R1 pays at two-thirds rate"), RunShieldAbsorb(1), 10.0f * (2.0f / 3.0f), 0.01f);
    TestEqual(TEXT("R2 pays at full rate"), RunShieldAbsorb(2), 10.0f, 0.01f);
    return true;
}

// ---------------------------------------------------------------------------
// L6 Transfusion: while shielded, block procs pay more (bounded by the block
// source's per-second budget, exactly as the node text promises).
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerNodeTransfusionTest,
    "RiorsEdge.Progression.BranchNodes.Tank.Transfusion",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerNodeTransfusionTest::RunTest(const FString& Parameters)
{
    using namespace BreakerBranchNodeTest;
    auto RunBlockProc = [](int32 Rank, bool bShielded) -> float
    {
        FBreakerBranchNodeRig Rig = BreakerMakeBranchRig(EBreakerClassId::Tank,
            {{TEXT("Tank.Leech.Transfusion"), Rank}});
        UBreakerGritComponent* Grit = BreakerAddGrit(Rig, 1000.0f);
        Grit->SetInCombat(true);
        Grit->AdvanceLoop(1.0f);   // fill buckets; establishes the shield ceiling
        if (bShielded) Rig.Attributes->ApplyShield(50.0f);
        const float Before = Grit->GetGrit();
        Grit->NotifyBlockProc();
        Grit->AdvanceLoop(1.0f);
        return Grit->GetGrit() - Before;
    };

    TestEqual(TEXT("Unowned, a block proc pays the base grant"), RunBlockProc(0, true), 6.0f, 0.01f);
    // +9 requested against the 8/s block budget: the per-source cap binds.
    TestEqual(TEXT("Owned and shielded, the proc pays more (budget-bounded)"), RunBlockProc(2, true), 8.0f, 0.01f);
    TestEqual(TEXT("Owned but UNSHIELDED, the proc pays the base grant"), RunBlockProc(2, false), 6.0f, 0.01f);
    return true;
}

// ---------------------------------------------------------------------------
// The Leech shield clock: base 3s hold then bleed; L2 Slow Bleed lengthens the
// hold; L8 Second Heart suspends decay outright at IRONCLAD.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerNodeLeechShieldClockTest,
    "RiorsEdge.Progression.BranchNodes.Tank.LeechShieldClock",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerNodeLeechShieldClockTest::RunTest(const FString& Parameters)
{
    using namespace BreakerBranchNodeTest;

    auto ShieldAfter = [](std::initializer_list<TPair<const TCHAR*, int32>> Nodes, float IroncladFraction) -> float
    {
        FBreakerBranchNodeRig Rig = BreakerMakeBranchRig(EBreakerClassId::Tank, Nodes);
        UBreakerGritComponent* Grit = BreakerAddGrit(Rig, 1000.0f);
        Grit->SetInCombat(true);
        if (IroncladFraction > 0.0f)
        {
            Rig.Attributes->ApplyClassResource(Rig.Attributes->GetMaxClassResource() * IroncladFraction);
        }
        Grit->AdvanceLoop(0.1f);   // establishes the §T1 shield ceiling and the band
        Rig.Attributes->ApplyShield(100.0f);
        // Gain registers on the first advance; then hold four seconds.
        Grit->AdvanceLoop(0.1f);
        Grit->AdvanceLoop(2.0f);
        Grit->AdvanceLoop(2.0f);
        return Rig.Attributes->GetShield();
    };

    // Base: the second 2s step crosses the 3s hold and bleeds ~4%/s of current.
    const float BaseShield = ShieldAfter({}, 0.0f);
    TestTrue(TEXT("Unowned, the shield bleeds after the 3s hold"), BaseShield < 100.0f - KINDA_SMALL_NUMBER);

    // L2 R1: the hold is 5s — at 4.1s of clock, part of the last step decays;
    // strictly MORE shield must remain than the base run kept.
    const float SlowBleedShield = ShieldAfter({{TEXT("Tank.Leech.SlowBleed"), 1}}, 0.0f);
    TestTrue(TEXT("Slow Bleed holds longer than base"), SlowBleedShield > BaseShield);
    // L2 R2: 8s hold — the whole run sits inside it, no decay at all.
    TestEqual(TEXT("Slow Bleed R2 holds the whole window"),
        ShieldAfter({{TEXT("Tank.Leech.SlowBleed"), 2}}, 0.0f), 100.0f, 0.01f);

    // L8: at IRONCLAD (>= 2/3 of the bar), no decay at all.
    TestEqual(TEXT("Second Heart at IRONCLAD never decays"),
        ShieldAfter({{TEXT("Tank.Leech.SecondHeart"), 1}}, 0.9f), 100.0f, 0.01f);
    // L8 below IRONCLAD decays like base.
    TestTrue(TEXT("Second Heart below IRONCLAD still decays"),
        ShieldAfter({{TEXT("Tank.Leech.SecondHeart"), 1}}, 0.0f) < 100.0f - KINDA_SMALL_NUMBER);
    return true;
}

// ---------------------------------------------------------------------------
// L9 Nothing Wasted: every heal's unrouted overheal becomes Leech shield.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerNodeNothingWastedTest,
    "RiorsEdge.Progression.BranchNodes.Tank.NothingWasted",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerNodeNothingWastedTest::RunTest(const FString& Parameters)
{
    using namespace BreakerBranchNodeTest;
    auto ShieldFromOverheal = [](int32 Rank) -> float
    {
        FBreakerBranchNodeRig Rig = BreakerMakeBranchRig(EBreakerClassId::Tank,
            {{TEXT("Tank.Leech.NothingWasted"), Rank}});
        UBreakerGritComponent* Grit = BreakerAddGrit(Rig, 1000.0f);
        Grit->SetInCombat(true);
        Grit->AdvanceLoop(0.1f);   // establishes the shield ceiling
        FBreakerHealResult Result;
        Result.Overheal = 40.0f;
        Result.ShieldGranted = 0.0f;
        Grit->HandleOwnerHealed(Result);
        return Rig.Attributes->GetShield();
    };

    TestEqual(TEXT("Unowned, overheal is discarded"), ShieldFromOverheal(0), 0.0f, 0.01f);
    TestEqual(TEXT("Owned, unrouted overheal becomes shield"), ShieldFromOverheal(1), 40.0f, 0.01f);

    // A result whose overheal was ALREADY routed converts only the remainder.
    FBreakerBranchNodeRig Rig = BreakerMakeBranchRig(EBreakerClassId::Tank,
        {{TEXT("Tank.Leech.NothingWasted"), 1}});
    UBreakerGritComponent* Grit = BreakerAddGrit(Rig, 1000.0f);
    Grit->SetInCombat(true);
    Grit->AdvanceLoop(0.1f);
    FBreakerHealResult Routed;
    Routed.Overheal = 40.0f;
    Routed.ShieldGranted = 30.0f;
    Grit->HandleOwnerHealed(Routed);
    TestEqual(TEXT("Already-routed overheal converts only its remainder"), Rig.Attributes->GetShield(), 10.0f, 0.01f);
    return true;
}

// ---------------------------------------------------------------------------
// L10 Reciprocity: a broken shield returns 20% of what it absorbed as healing
// over 2s — after the break, never during.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerNodeReciprocityTest,
    "RiorsEdge.Progression.BranchNodes.Tank.Reciprocity",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerNodeReciprocityTest::RunTest(const FString& Parameters)
{
    using namespace BreakerBranchNodeTest;
    auto HealthAfterBreak = [](int32 Rank) -> float
    {
        FBreakerBranchNodeRig Rig = BreakerMakeBranchRig(EBreakerClassId::Tank,
            {{TEXT("Tank.Leech.Reciprocity"), Rank}});
        UBreakerGritComponent* Grit = BreakerAddGrit(Rig, 1000.0f);
        Rig.Attributes->ApplyHealth(500.0f);
        Grit->SetInCombat(true);
        Grit->AdvanceLoop(0.1f);
        Rig.Attributes->ApplyShield(100.0f);
        Grit->AdvanceLoop(0.1f);   // register the gain
        // The shield absorbs 100 post-mitigation, then breaks.
        Grit->NotifyDamageTaken(0.0f, 100.0f, false, 1.0f);
        Rig.Attributes->ApplyShield(0.0f);
        Grit->AdvanceLoop(1.0f);
        Grit->AdvanceLoop(1.0f);
        Grit->AdvanceLoop(1.0f);
        return Rig.Attributes->GetHealth();
    };

    const float Unowned = HealthAfterBreak(0);
    const float Owned = HealthAfterBreak(1);
    TestEqual(TEXT("Unowned, a break heals nothing"), Unowned, 500.0f, 0.5f);
    TestEqual(TEXT("Owned, 20% of the absorbed damage returns as healing"), Owned, 520.0f, 0.5f);
    return true;
}

// ---------------------------------------------------------------------------
// D11 Chain Reaction's per-target blast ledger: first blast pays nothing,
// blasts inside the window stack to the cap.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerNodeChainReactionLedgerTest,
    "RiorsEdge.Progression.BranchNodes.Tank.ChainReactionLedger",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerNodeChainReactionLedgerTest::RunTest(const FString& Parameters)
{
    using namespace BreakerBranchNodeTest;
    FBreakerBranchNodeRig Rig = BreakerMakeBranchRig(EBreakerClassId::Tank, {});
    UBreakerGritComponent* Grit = BreakerAddGrit(Rig, 1000.0f);
    AActor* Target = NewObject<AActor>();

    TestEqual(TEXT("The first blast pays no bonus"), Grit->RegisterExplosiveBlast(Target), 0);
    TestEqual(TEXT("The second blast inside the window pays one stack"), Grit->RegisterExplosiveBlast(Target), 1);
    TestEqual(TEXT("The third pays two"), Grit->RegisterExplosiveBlast(Target), 2);
    TestEqual(TEXT("The fourth pays three"), Grit->RegisterExplosiveBlast(Target), 3);
    TestEqual(TEXT("The cap holds at three"), Grit->RegisterExplosiveBlast(Target), 3);

    AActor* Other = NewObject<AActor>();
    TestEqual(TEXT("A different target starts its own ledger"), Grit->RegisterExplosiveBlast(Other), 0);
    return true;
}

// ---------------------------------------------------------------------------
// MD1 Field Dressing: non-Support self-heals credit Charge only with the node.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerNodeFieldDressingTest,
    "RiorsEdge.Progression.BranchNodes.Support.FieldDressing",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerNodeFieldDressingTest::RunTest(const FString& Parameters)
{
    using namespace BreakerBranchNodeTest;
    auto ChargeFromOutsideHeal = [](int32 Rank) -> float
    {
        FBreakerBranchNodeRig Rig = BreakerMakeBranchRig(EBreakerClassId::Support,
            {{TEXT("Support.Medic.FieldDressing"), Rank}});
        UBreakerChargeComponent* Charge = BreakerAddCharge(Rig, 1000.0f);
        Charge->SetInCombat(true);
        Charge->AdvanceLoop(1.0f);   // fill the self-heal sub-cap bucket
        FBreakerHealResult Result;
        Result.HealthHealed = 300.0f;
        Charge->HandleOwnerHealed(Result);
        Charge->AdvanceLoop(1.0f);
        return Charge->GetCharge();
    };

    TestEqual(TEXT("Unowned, an outside self-heal credits nothing"), ChargeFromOutsideHeal(0), 0.0f, 0.01f);
    // 30% of max health at 1-per-3% is 10, metered to the 6/s self-heal cap.
    TestEqual(TEXT("Owned, it credits through the metered self lane"), ChargeFromOutsideHeal(1), 6.0f, 0.01f);

    // The Support-ability crediting scope stands the listener down.
    FBreakerBranchNodeRig Rig = BreakerMakeBranchRig(EBreakerClassId::Support,
        {{TEXT("Support.Medic.FieldDressing"), 1}});
    UBreakerChargeComponent* Charge = BreakerAddCharge(Rig, 1000.0f);
    Charge->SetInCombat(true);
    Charge->AdvanceLoop(1.0f);
    Charge->BeginSupportHealScope();
    FBreakerHealResult Result;
    Result.HealthHealed = 300.0f;
    Charge->HandleOwnerHealed(Result);
    Charge->EndSupportHealScope();
    Charge->AdvanceLoop(1.0f);
    TestEqual(TEXT("Inside a crediting scope the listener stands down"), Charge->GetCharge(), 0.0f, 0.01f);
    return true;
}

// ---------------------------------------------------------------------------
// MD10 Blood Debt: healing banks into the pool, capped; consuming empties it.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerNodeBloodDebtTest,
    "RiorsEdge.Progression.BranchNodes.Support.BloodDebt",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerNodeBloodDebtTest::RunTest(const FString& Parameters)
{
    using namespace BreakerBranchNodeTest;

    FBreakerBranchNodeRig Base = BreakerMakeBranchRig(EBreakerClassId::Support, {});
    UBreakerChargeComponent* BaseCharge = BreakerAddCharge(Base, 1000.0f);
    BaseCharge->NotifyHealingDone(200.0f, 0.0f, 1000.0f, true, 1.0f);
    TestEqual(TEXT("Unowned, nothing banks"), BaseCharge->GetBloodDebtPool(), 0.0f, 0.01f);

    FBreakerBranchNodeRig Rig = BreakerMakeBranchRig(EBreakerClassId::Support,
        {{TEXT("Support.Medic.BloodDebt"), 1}});
    UBreakerChargeComponent* Charge = BreakerAddCharge(Rig, 1000.0f);
    Charge->NotifyHealingDone(100.0f, 0.0f, 1000.0f, true, 1.0f);
    TestEqual(TEXT("Healing banks at full rate"), Charge->GetBloodDebtPool(), 100.0f, 0.01f);
    Charge->NotifyHealingDone(100.0f, 0.0f, 1000.0f, true, 1.0f);
    TestEqual(TEXT("The pool caps"), Charge->GetBloodDebtPool(), Charge->BloodDebtPoolCap, 0.01f);
    TestEqual(TEXT("Consuming returns the pool"), Charge->ConsumeBloodDebt(), Charge->BloodDebtPoolCap, 0.01f);
    TestEqual(TEXT("Consuming empties it"), Charge->GetBloodDebtPool(), 0.0f, 0.01f);
    return true;
}

// ---------------------------------------------------------------------------
// CO3 Sustain: buff-uptime Charge keeps paying for a grace after the last
// buff expires.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerNodeSustainTest,
    "RiorsEdge.Progression.BranchNodes.Support.Sustain",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerNodeSustainTest::RunTest(const FString& Parameters)
{
    using namespace BreakerBranchNodeTest;
    auto ChargeAfterExpiry = [](int32 Rank) -> float
    {
        FBreakerBranchNodeRig Rig = BreakerMakeBranchRig(EBreakerClassId::Support,
            {{TEXT("Support.Conductor.Sustain"), Rank}});
        UBreakerChargeComponent* Charge = BreakerAddCharge(Rig, 1000.0f);
        Charge->SetInCombat(true);
        Charge->SetAnyBuffActive(true);
        Charge->AdvanceLoop(1.0f);
        const float WhileLive = Charge->GetCharge();
        Charge->SetAnyBuffActive(false);
        Charge->AdvanceLoop(1.0f);
        return Charge->GetCharge() - WhileLive;
    };

    TestEqual(TEXT("Unowned, the source stops with the buff"), ChargeAfterExpiry(0), 0.0f, 0.01f);
    FBreakerBranchNodeRig ProbeRig = BreakerMakeBranchRig(EBreakerClassId::Support, {});
    UBreakerChargeComponent* Probe = BreakerAddCharge(ProbeRig, 1000.0f);
    TestEqual(TEXT("Owned, the grace second still pays the uptime rate"), ChargeAfterExpiry(1), Probe->BuffUptimeRate, 0.01f);
    return true;
}

// ---------------------------------------------------------------------------
// MD7 Field Kit's primitive: the status-immunity window refuses new
// applications and expires on its own clock.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerNodeStatusImmunityTest,
    "RiorsEdge.Progression.BranchNodes.Support.StatusImmunity",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerNodeStatusImmunityTest::RunTest(const FString& Parameters)
{
    AActor* Owner = NewObject<AActor>();
    UBreakerStatusComponent* Status = NewObject<UBreakerStatusComponent>(Owner);

    FBreakerStatusApplicationSpec Spec;
    Spec.StatusTag = FGameplayTag::RequestGameplayTag(TEXT("Status.Bleed"), false);
    Spec.Duration = 5.0f;
    Spec.TickInterval = 1.0f;
    Spec.BaseDamagePerTick = 1.0f;

    Status->GrantStatusImmunity(3.0f);
    TestTrue(TEXT("The immunity window opens"), Status->IsStatusImmune());
    Status->ApplyStatus(Spec, EBreakerDamageFamily::Physical, nullptr);
    TestEqual(TEXT("An application inside the window is refused"), Status->GetActiveStatuses().Num(), 0);

    Status->AdvanceStatuses(3.5f);
    TestFalse(TEXT("The window expires on its own clock"), Status->IsStatusImmune());
    Status->ApplyStatus(Spec, EBreakerDamageFamily::Physical, nullptr);
    TestEqual(TEXT("Afterwards, applications land again"), Status->GetActiveStatuses().Num(), 1);
    return true;
}

#endif   // WITH_DEV_AUTOMATION_TESTS
