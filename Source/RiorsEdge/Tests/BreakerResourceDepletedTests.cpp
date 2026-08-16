#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Classes/BreakerChargeComponent.h"
#include "Classes/BreakerGritComponent.h"
#include "Classes/BreakerManaComponent.h"
#include "Classes/BreakerMomentumComponent.h"
#include "Classes/BreakerScrapComponent.h"
#include "GameFramework/Actor.h"
#include "Progression/BreakerBuildConditions.h"
#include "Progression/BreakerProgressionComponent.h"

// ---------------------------------------------------------------------------
// ResourceDepleted semantics (build-math finding #3).
//
// The bug this file pins shut: "fraction <= 0" is TRUE AT REST for every
// bank-style loop — Grit, Scrap and Charge start and idle at zero, and
// Momentum decays back to zero the moment a Swift stands still — so
// Anomaly.EntropyDebt's "while resource is empty" line was an always-on
// unconditional for an idle Tank/Gunsmith/Support while the Caster it was
// written against had to EARN the state through Overcast. The fix: the
// condition means DRAINED PAST EMPTY, so it may only be true for a loop
// whose RESTING state is full/positive — each loop answers
// IsRestingStateFull() for itself, and EvaluateForActor gates the bit on it.
//
// The per-class truth table, pinned below:
//   Caster (Mana, rests full)      — CAN proc: empty or Overcast-negative bank
//   Swift (Momentum, rests empty)  — cannot
//   Tank (Grit, banks from empty)  — cannot
//   Gunsmith (Scrap, banks)        — cannot
//   Support (Charge, banks)        — cannot
// ---------------------------------------------------------------------------

namespace BreakerResourceDepletedTest
{
    // Prefixed, per the unity-build house rule.
    struct FBreakerDepletedRig
    {
        AActor* Owner = nullptr;
        UBreakerProgressionComponent* Progression = nullptr;
        UBreakerAttributeSet* Attributes = nullptr;
    };

    static FBreakerDepletedRig BreakerMakeDepletedRig(EBreakerClassId ClassId)
    {
        FBreakerDepletedRig Rig;
        Rig.Owner = NewObject<AActor>();
        Rig.Progression = NewObject<UBreakerProgressionComponent>(Rig.Owner);
        Rig.Attributes = NewObject<UBreakerAttributeSet>();
        Rig.Progression->DevForceClass(ClassId);
        return Rig;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerResourceDepletedCasterTest,
    "RiorsEdge.Progression.ConditionVocabulary.ResourceDepletedCaster",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerResourceDepletedCasterTest::RunTest(const FString& Parameters)
{
    using namespace BreakerResourceDepletedTest;
    FBreakerDepletedRig Rig = BreakerMakeDepletedRig(EBreakerClassId::Caster);
    UBreakerManaComponent* Mana = NewObject<UBreakerManaComponent>(Rig.Owner);
    // BindAttributes refreshes class ownership; the transition INTO Caster
    // fills the bank (owner ruling 2026-08-14: a Caster starts full).
    Mana->BindAttributes(Rig.Attributes);
    TestTrue(TEXT("the Mana loop runs for a Caster"), Mana->IsActiveForOwner());
    TestTrue(TEXT("Mana is the loop that rests full"), Mana->IsRestingStateFull());
    TestTrue(TEXT("a fresh Caster starts with a full bank"), Mana->GetManaFraction() > 0.9f);

    // Full bank: neither resource condition holds.
    FBreakerBuildConditionState Full = FBreakerBuildConditionState::EvaluateForActor(Rig.Owner);
    TestFalse(TEXT("a full bank is not depleted"), Full.IsActive(EBreakerBuildCondition::ResourceDepleted));
    TestFalse(TEXT("a full bank is not low"), Full.IsActive(EBreakerBuildCondition::ResourceLow));

    // Exactly empty: depleted (<= on purpose — an exactly-empty bar counts).
    Rig.Attributes->ApplyClassResource(0.0f);
    FBreakerBuildConditionState Empty = FBreakerBuildConditionState::EvaluateForActor(Rig.Owner);
    TestTrue(TEXT("an exactly-empty Caster bank is depleted"), Empty.IsActive(EBreakerBuildCondition::ResourceDepleted));
    TestTrue(TEXT("an empty bank is also low"), Empty.IsActive(EBreakerBuildCondition::ResourceLow));

    // Overcast: the debt state the condition was authored for. The attribute
    // clamp honours the published Overcast floor, so the write lands negative.
    Rig.Attributes->ApplyClassResource(-10.0f);
    TestTrue(TEXT("the fixture actually reached debt"), Rig.Attributes->GetClassResource() < 0.0f);
    FBreakerBuildConditionState Overcast = FBreakerBuildConditionState::EvaluateForActor(Rig.Owner);
    TestTrue(TEXT("an Overcast (negative) bank is depleted"), Overcast.IsActive(EBreakerBuildCondition::ResourceDepleted));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerResourceDepletedBankLoopsTest,
    "RiorsEdge.Progression.ConditionVocabulary.ResourceDepletedBankLoops",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerResourceDepletedBankLoopsTest::RunTest(const FString& Parameters)
{
    using namespace BreakerResourceDepletedTest;

    // Tank / Grit: an empty bank is the resting state, not a drained one.
    {
        FBreakerDepletedRig Rig = BreakerMakeDepletedRig(EBreakerClassId::Tank);
        UBreakerGritComponent* Grit = NewObject<UBreakerGritComponent>(Rig.Owner);
        Grit->BindAttributes(Rig.Attributes);
        TestTrue(TEXT("the Grit loop runs for a Tank"), Grit->IsActiveForOwner());
        TestFalse(TEXT("Grit does not rest full"), Grit->IsRestingStateFull());
        TestEqual(TEXT("the fixture is at its resting zero"), Grit->GetGrit(), 0.0f);

        const FBreakerBuildConditionState State = FBreakerBuildConditionState::EvaluateForActor(Rig.Owner);
        TestFalse(TEXT("an idle Tank at resting zero is NOT depleted"),
            State.IsActive(EBreakerBuildCondition::ResourceDepleted));
        // ResourceLow is deliberately unchanged by the fix: "nearly out" is a
        // fraction question, not a drained-state question, and its authored
        // consumers price it accordingly.
        TestTrue(TEXT("an empty bank still reads as low (unchanged)"),
            State.IsActive(EBreakerBuildCondition::ResourceLow));
    }

    // Gunsmith / Scrap.
    {
        FBreakerDepletedRig Rig = BreakerMakeDepletedRig(EBreakerClassId::Gunsmith);
        UBreakerScrapComponent* Scrap = NewObject<UBreakerScrapComponent>(Rig.Owner);
        Scrap->BindAttributes(Rig.Attributes);
        TestTrue(TEXT("the Scrap loop runs for a Gunsmith"), Scrap->IsActiveForOwner());
        TestFalse(TEXT("Scrap does not rest full"), Scrap->IsRestingStateFull());
        TestFalse(TEXT("an idle Gunsmith at resting zero is NOT depleted"),
            FBreakerBuildConditionState::EvaluateForActor(Rig.Owner).IsActive(EBreakerBuildCondition::ResourceDepleted));
    }

    // Support / Charge.
    {
        FBreakerDepletedRig Rig = BreakerMakeDepletedRig(EBreakerClassId::Support);
        UBreakerChargeComponent* Charge = NewObject<UBreakerChargeComponent>(Rig.Owner);
        Charge->BindAttributes(Rig.Attributes);
        TestTrue(TEXT("the Charge loop runs for a Support"), Charge->IsActiveForOwner());
        TestFalse(TEXT("Charge does not rest full"), Charge->IsRestingStateFull());
        TestFalse(TEXT("an idle Support at resting zero is NOT depleted"),
            FBreakerBuildConditionState::EvaluateForActor(Rig.Owner).IsActive(EBreakerBuildCondition::ResourceDepleted));
    }

    // Swift / Momentum: earned by moving, decays to zero at rest — same rule.
    {
        FBreakerDepletedRig Rig = BreakerMakeDepletedRig(EBreakerClassId::Swift);
        UBreakerMomentumComponent* Momentum = NewObject<UBreakerMomentumComponent>(Rig.Owner);
        Momentum->BindAttributes(Rig.Attributes);
        TestTrue(TEXT("the Momentum loop runs for a Swift"), Momentum->IsActiveForOwner());
        TestFalse(TEXT("Momentum does not rest full"), Momentum->IsRestingStateFull());
        TestFalse(TEXT("a standing Swift at zero Momentum is NOT depleted"),
            FBreakerBuildConditionState::EvaluateForActor(Rig.Owner).IsActive(EBreakerBuildCondition::ResourceDepleted));
    }
    return true;
}

#endif  // WITH_DEV_AUTOMATION_TESTS
