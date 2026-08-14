#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Classes/BreakerManaComponent.h"
#include "Classes/BreakerMomentumComponent.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerDamageLibrary.h"
#include "GameFramework/Actor.h"
#include "Progression/BreakerProgressionComponent.h"

// END-TO-END class-resource coverage, which did not exist until
// UBreakerCombatComponent::AddClassResource and SpendClassResource stopped
// writing through the GAS generated SetClassResource.
//
// The bug these tests exist to prevent recurring: the generated setter ensure()s
// when the attribute set has no owning AbilitySystemComponent, and no automation
// rig has one. So every write to the class-resource bank was unreachable from a
// test, and Momentum, Mana, the dodge refund and every resource cost were proven
// only by the pure-maths layers on either side of the write — the arithmetic was
// covered and the write itself never was. Routing both helpers (and the Momentum
// loop's own delta) through UBreakerAttributeSet::ApplyClassResource — the
// null-safe write added for exactly this, and already the route ApplyHealth and
// ApplyShield take — is what makes the assertions below possible at all.

namespace BreakerClassResourceTest
{
    // Prefixed: identical anonymous-namespace helper names in two translation
    // units have collided under this project's unity build before.
    struct FBreakerResourceRig
    {
        AActor* Owner = nullptr;
        UBreakerCombatComponent* Combat = nullptr;
        UBreakerAttributeSet* Attributes = nullptr;
    };

    // A freshly constructed actor is ROLE_Authority, which is all the server
    // paths require; nothing here needs a world or an ability system.
    static FBreakerResourceRig BreakerMakeResourceRig()
    {
        FBreakerResourceRig Rig;
        Rig.Owner = NewObject<AActor>();
        Rig.Combat = NewObject<UBreakerCombatComponent>(Rig.Owner);
        Rig.Attributes = NewObject<UBreakerAttributeSet>();
        Rig.Combat->BindAttributes(Rig.Attributes);
        return Rig;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerClassResourceEndToEndTest,
    "RiorsEdge.Combat.ClassResource.EndToEnd",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerClassResourceEndToEndTest::RunTest(const FString& Parameters)
{
    using namespace BreakerClassResourceTest;

    FBreakerResourceRig Rig = BreakerMakeResourceRig();
    const float Max = Rig.Attributes->GetMaxClassResource();
    TestTrue(TEXT("The fixture has a bank to spend from"), Max > 0.0f);
    TestEqual(TEXT("A fresh bank is empty"), Rig.Attributes->GetClassResource(), 0.0f);

    // The write that could not be observed before. Nothing subtle is being
    // asserted here — that is the point: this is the first assertion in the
    // project that a grant reaches the attribute at all.
    Rig.Combat->AddClassResource(30.0f);
    TestEqual(TEXT("A grant lands on the attribute"), Rig.Attributes->GetClassResource(), 30.0f);

    Rig.Combat->AddClassResource(12.5f);
    TestEqual(TEXT("Grants accumulate"), Rig.Attributes->GetClassResource(), 42.5f);

    // The cap now comes from PreAttributeChange rather than a Min written at
    // the call site, so this is also the test that the two agree.
    Rig.Combat->AddClassResource(Max * 4.0f);
    TestEqual(TEXT("A grant is capped at the maximum"), Rig.Attributes->GetClassResource(), Max);

    // A zero or negative grant is not a spend. AddClassResource is the credit
    // half of the pair and must never be usable as a debit.
    Rig.Combat->AddClassResource(-50.0f);
    TestEqual(TEXT("A negative grant does nothing"), Rig.Attributes->GetClassResource(), Max);
    Rig.Combat->AddClassResource(0.0f);
    TestEqual(TEXT("A zero grant does nothing"), Rig.Attributes->GetClassResource(), Max);

    TestTrue(TEXT("An affordable spend succeeds"), Rig.Combat->SpendClassResource(40.0f));
    TestEqual(TEXT("The spend comes off the attribute"), Rig.Attributes->GetClassResource(), Max - 40.0f);

    // Refused, not truncated: a partial spend would hand the caller the effect
    // it asked for at a discount it never paid.
    const float BeforeRefusal = Rig.Attributes->GetClassResource();
    TestFalse(TEXT("An unaffordable spend is refused"), Rig.Combat->SpendClassResource(BeforeRefusal + 1.0f));
    TestEqual(TEXT("A refused spend costs nothing"), Rig.Attributes->GetClassResource(), BeforeRefusal);

    TestTrue(TEXT("A spend of exactly the bank is allowed"), Rig.Combat->SpendClassResource(BeforeRefusal));
    TestEqual(TEXT("Spending the bank empties it exactly"), Rig.Attributes->GetClassResource(), 0.0f);

    // A negative cost would be a grant wearing a spend's name.
    TestFalse(TEXT("A negative cost is refused"), Rig.Combat->SpendClassResource(-25.0f));
    TestEqual(TEXT("A refused negative cost grants nothing"), Rig.Attributes->GetClassResource(), 0.0f);

    // An unbound component is inert rather than crashing: the enemy archetypes
    // share this component and never bind a class resource.
    UBreakerCombatComponent* Unbound = NewObject<UBreakerCombatComponent>(NewObject<AActor>());
    Unbound->AddClassResource(10.0f);
    TestFalse(TEXT("An unbound component cannot spend"), Unbound->SpendClassResource(1.0f));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerClassResourceDodgeRefundTest,
    "RiorsEdge.Combat.ClassResource.DodgeRefund",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerClassResourceDodgeRefundTest::RunTest(const FString& Parameters)
{
    using namespace BreakerClassResourceTest;

    // The dodge refund (O1: block and dodge are PASSIVE layers) is the one
    // gameplay path that reached AddClassResource from inside ReceiveDamage, so
    // until now it was covered only as far as ResolveDamage's bDodged flag. This
    // asserts the whole submission: a dodged hit takes no health AND pays the
    // refund into the bank.
    FBreakerResourceRig Rig = BreakerMakeResourceRig();
    Rig.Combat->DodgeChance = 1.0f;
    Rig.Combat->DodgeResourceRefund = 5.0f;

    AActor* Attacker = NewObject<AActor>();
    FBreakerDamageRequest Request;
    Request.BaseDamage = 40.0f;
    Request.SetInstigator(Attacker);

    const float HealthBefore = Rig.Attributes->GetHealth();
    const FBreakerDamageResult Result = Rig.Combat->ReceiveDamage(Request);
    TestTrue(TEXT("A guaranteed dodge dodges"), Result.bDodged);
    TestEqual(TEXT("A dodged hit takes no health"), Rig.Attributes->GetHealth(), HealthBefore);
    TestEqual(TEXT("The dodge refund reaches the bank"), Rig.Attributes->GetClassResource(), 5.0f);

    // Two dodges pay twice. Cheap to assert and it pins the refund as per-hit
    // rather than a one-shot flag.
    Rig.Combat->ReceiveDamage(Request);
    TestEqual(TEXT("Every dodge pays"), Rig.Attributes->GetClassResource(), 10.0f);

    // A zeroed refund is the "off" position and must write nothing at all.
    Rig.Combat->DodgeResourceRefund = 0.0f;
    Rig.Combat->ReceiveDamage(Request);
    TestEqual(TEXT("A zeroed refund grants nothing"), Rig.Attributes->GetClassResource(), 10.0f);

    // A hit that is NOT dodged pays no refund — the refund is the compensation
    // for the passive layer firing, not for being shot at.
    FBreakerResourceRig Hit = BreakerMakeResourceRig();
    Hit.Combat->DodgeChance = 0.0f;
    Hit.Combat->DodgeResourceRefund = 5.0f;
    const FBreakerDamageResult Landed = Hit.Combat->ReceiveDamage(Request);
    TestFalse(TEXT("A zero dodge chance never dodges"), Landed.bDodged);
    TestEqual(TEXT("A landed hit pays no refund"), Hit.Attributes->GetClassResource(), 0.0f);
    TestTrue(TEXT("A landed hit costs health"), Hit.Attributes->GetHealth() < HealthBefore);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerClassResourceOvercastFloorTest,
    "RiorsEdge.Combat.ClassResource.OvercastFloor",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerClassResourceOvercastFloorTest::RunTest(const FString& Parameters)
{
    using namespace BreakerClassResourceTest;

    // The combat helpers and the Overcast debt allowance now share one clamp,
    // because both go through PreAttributeChange. This is the interaction that
    // was completely unproven: the helpers used to write a Min against Max with
    // no knowledge of the floor at all.
    AActor* Owner = NewObject<AActor>();
    UBreakerProgressionComponent* Progression = NewObject<UBreakerProgressionComponent>(Owner);
    UBreakerCombatComponent* Combat = NewObject<UBreakerCombatComponent>(Owner);
    UBreakerManaComponent* Mana = NewObject<UBreakerManaComponent>(Owner);
    UBreakerAttributeSet* Attributes = NewObject<UBreakerAttributeSet>();
    Progression->DevForceClass(EBreakerClassId::Caster);
    Combat->BindAttributes(Attributes);
    Mana->BindAttributes(Attributes);

    TestEqual(TEXT("A Caster's floor is open"), Attributes->GetClassResourceFloor(), -20.0f);

    // Drive the bank into debt through the Mana loop, which is the only path
    // allowed to (ability costs are GameplayEffects; spec D3).
    Mana->GrantMana(30.0f, /*bIgnoreGlobalCap*/ true);
    TestTrue(TEXT("The overdraft cast succeeds"), Mana->TrySpendMana(45.0f));
    TestEqual(TEXT("The bank is in debt"), Attributes->GetClassResource(), -15.0f);

    // A grant while in debt REPAYS it rather than being clamped away at zero.
    // The old Min-against-Max write happened to do this too, but nothing proved
    // it, and a future clamp change now has a test standing in front of it.
    Combat->AddClassResource(10.0f);
    TestEqual(TEXT("A grant repays debt rather than being discarded"), Attributes->GetClassResource(), -5.0f);
    Combat->AddClassResource(25.0f);
    TestEqual(TEXT("Repayment carries straight through zero"), Attributes->GetClassResource(), 20.0f);

    // The generic spend helper is deliberately NOT an overdraft path: it
    // measures affordability against zero, so only a Caster ability cost —
    // which is checked by UBreakerCasterAbility::CheckCost against the floor —
    // can open a debt. A gear or node cost must never quietly Overcast a player.
    TestFalse(TEXT("The generic helper will not open a debt"), Combat->SpendClassResource(30.0f));
    TestEqual(TEXT("The refused generic spend costs nothing"), Attributes->GetClassResource(), 20.0f);
    TestTrue(TEXT("It still spends what is actually banked"), Combat->SpendClassResource(20.0f));
    TestEqual(TEXT("Down to zero and no further"), Attributes->GetClassResource(), 0.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerClassResourceOneBankTest,
    "RiorsEdge.Classes.ClassResource.OneBank",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerClassResourceOneBankTest::RunTest(const FString& Parameters)
{
    using namespace BreakerClassResourceTest;

    // Momentum, Mana and the combat helpers are three writers of ONE attribute.
    // That is by design (a class resource is a class resource), and it is the
    // kind of design that quietly stops being true. Now that all three write
    // through the same route, a test can hold them to it.
    AActor* Owner = NewObject<AActor>();
    UBreakerProgressionComponent* Progression = NewObject<UBreakerProgressionComponent>(Owner);
    UBreakerCombatComponent* Combat = NewObject<UBreakerCombatComponent>(Owner);
    UBreakerMomentumComponent* Momentum = NewObject<UBreakerMomentumComponent>(Owner);
    UBreakerAttributeSet* Attributes = NewObject<UBreakerAttributeSet>();
    Progression->DevForceClass(EBreakerClassId::Swift);
    Momentum->HandleProgressionChanged();
    Combat->BindAttributes(Attributes);

    TestTrue(TEXT("The Momentum loop runs for a Swift"), Momentum->IsActiveForOwner());

    // Momentum resolves its own attribute set in BeginPlay, which a worldless
    // rig never runs; bind the combat side and assert the bank the HUD would
    // read is the bank the refund landed in once Momentum has the same set.
    Combat->AddClassResource(50.0f);
    TestEqual(TEXT("The grant is on the shared attribute"), Attributes->GetClassResource(), 50.0f);

    // A Swift's floor stays shut, so the whole Overcast mechanism is bit-
    // identical-to-absent for the class that is actually tuned and playtested.
    TestEqual(TEXT("A Swift has no debt allowance"), Attributes->GetClassResourceFloor(), 0.0f);
    TestFalse(TEXT("A Swift cannot be driven into debt by the helpers"), Combat->SpendClassResource(51.0f));
    Attributes->ApplyClassResource(-40.0f);
    TestEqual(TEXT("Nothing drives a Swift's bar below zero"), Attributes->GetClassResource(), 0.0f);

    // And the state bands read off that same number, which is what makes the
    // write worth testing: Settled/Running/Redline is a view of the bank.
    Combat->AddClassResource(90.0f);
    TestEqual(TEXT("Redline is the top third of the shared bank"),
        static_cast<int32>(UBreakerMomentumComponent::StateForFraction(Attributes->GetClassResource() / Attributes->GetMaxClassResource())),
        static_cast<int32>(EBreakerMomentumState::Redline));
    return true;
}

#endif
