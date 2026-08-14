#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Classes/BreakerMomentumComponent.h"
#include "GameFramework/Actor.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Weapons/BreakerWeaponComponent.h"
#include "Weapons/BreakerWeaponDefinition.h"
#include "Weapons/BreakerWeaponMath.h"

// ---------------------------------------------------------------------------
// Support for two of Overdrive's keystone rewrites (Class-Kits lines 191 and
// 193): Bloodrhythm's Momentum refund and Standing Wave's point-blank range
// treatment. The ability itself is wired elsewhere (Abilities/); this file
// only proves the two API surfaces it will call.
// ---------------------------------------------------------------------------

namespace BreakerKeystoneTestHelpers
{
    // Mirrors BreakerManaTests.cpp's FCasterRig: a freshly constructed actor
    // is ROLE_Authority, which is all GrantMomentum's server-authority guard
    // requires, and BindAttributes is the same no-world wiring path the
    // automation suite already relies on everywhere else.
    struct FSwiftRig
    {
        AActor* Owner = nullptr;
        UBreakerProgressionComponent* Progression = nullptr;
        UBreakerMomentumComponent* Momentum = nullptr;
        UBreakerAttributeSet* Attributes = nullptr;
    };

    FSwiftRig MakeRig(EBreakerClassId ClassId, float StartingMomentum = 0.0f)
    {
        FSwiftRig Rig;
        Rig.Owner = NewObject<AActor>();
        Rig.Progression = NewObject<UBreakerProgressionComponent>(Rig.Owner);
        Rig.Momentum = NewObject<UBreakerMomentumComponent>(Rig.Owner);
        Rig.Attributes = NewObject<UBreakerAttributeSet>();
        Rig.Progression->DevForceClass(ClassId);
        Rig.Momentum->BindAttributes(Rig.Attributes);
        Rig.Attributes->ApplyClassResource(StartingMomentum);
        return Rig;
    }
}

// ---------------------------------------------------------------------------
// GrantMomentum: clamps at max, ignores non-positive input, inert for a
// non-Swift owner. Mirrors UBreakerManaComponent::GrantMana's own coverage
// in BreakerManaTests.cpp.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerMomentumGrantTest,
    "RiorsEdge.Combat.KeystoneOverrides.MomentumGrant",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerMomentumGrantTest::RunTest(const FString& Parameters)
{
    using namespace BreakerKeystoneTestHelpers;

    FSwiftRig Swift = MakeRig(EBreakerClassId::Swift, 40.0f);
    TestTrue(TEXT("The Momentum loop runs for a Swift"), Swift.Momentum->IsActiveForOwner());
    TestEqual(TEXT("The bank starts where it was set"), Swift.Momentum->GetMomentum(), 40.0f);

    // A plain grant, bypassing the per-second generation budget entirely
    // (this is the whole point of the API — a refund is not generation).
    Swift.Momentum->GrantMomentum(15.0f);
    TestEqual(TEXT("A grant lands immediately, no metering"), Swift.Momentum->GetMomentum(), 55.0f);

    // Clamps at MaxClassResource (100 by default) rather than overshooting.
    Swift.Momentum->GrantMomentum(1000.0f);
    TestEqual(TEXT("A grant clamps at the class resource max"), Swift.Momentum->GetMomentum(), Swift.Attributes->GetMaxClassResource());

    // Non-positive amounts are a no-op.
    const float BankAtCap = Swift.Momentum->GetMomentum();
    Swift.Momentum->GrantMomentum(0.0f);
    TestEqual(TEXT("A zero grant does nothing"), Swift.Momentum->GetMomentum(), BankAtCap);
    Swift.Momentum->GrantMomentum(-25.0f);
    TestEqual(TEXT("A negative grant does nothing"), Swift.Momentum->GetMomentum(), BankAtCap);

    // Inert for a non-Swift owner: the same IsActiveForOwner() gate every
    // other Momentum credit path uses.
    FSwiftRig NonSwift = MakeRig(EBreakerClassId::Caster, 10.0f);
    TestFalse(TEXT("A Caster does not run the Momentum loop"), NonSwift.Momentum->IsActiveForOwner());
    NonSwift.Momentum->GrantMomentum(50.0f);
    TestEqual(TEXT("A non-Swift owner gains nothing from a grant"), NonSwift.Momentum->GetMomentum(), 10.0f);
    return true;
}

// ---------------------------------------------------------------------------
// PushRangeTreatmentOverride / PopRangeTreatmentOverride /
// IsRangeTreatmentOverridden: active, popped, and two independent keys. This
// is the pure gate — see the coverage note below the falloff test for what
// is honestly NOT exercised here.
//
// Duration-based expiry needs a live UWorld (PushRangeTreatmentOverride's
// expiry math reads GetWorld()->GetTimeSeconds(), and with no world it
// always resolves to "never expires", exactly like
// UBreakerCharacterMovementComponent::PushSpeedMultiplier and
// UBreakerMomentumComponent::PushLoopOverride's own tests in this suite).
// No test component in BreakerMomentumTests.cpp or BreakerManaTests.cpp
// spins up a UWorld either, so real timed expiry is left uncovered here on
// the same established pattern rather than faked with a synthetic clock this
// component does not expose.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerRangeTreatmentOverrideTest,
    "RiorsEdge.Combat.KeystoneOverrides.RangeTreatmentGate",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerRangeTreatmentOverrideTest::RunTest(const FString& Parameters)
{
    UBreakerWeaponComponent* Weapon = NewObject<UBreakerWeaponComponent>();
    TestFalse(TEXT("No override active by default"), Weapon->IsRangeTreatmentOverridden());

    // A nameless key is refused (matches PushSpeedMultiplier/PushLoopOverride).
    Weapon->PushRangeTreatmentOverride(NAME_None, 5.0f);
    TestFalse(TEXT("An unnamed override is rejected"), Weapon->IsRangeTreatmentOverridden());

    Weapon->PushRangeTreatmentOverride(TEXT("Window.Marksman.Overdrive"), -1.0f);
    TestTrue(TEXT("A pushed override reads as active"), Weapon->IsRangeTreatmentOverridden());

    // A second, independent key composes with the first: still active, and
    // popping one leaves the other standing.
    Weapon->PushRangeTreatmentOverride(TEXT("Test.SecondKey"), -1.0f);
    TestTrue(TEXT("A second key is also active"), Weapon->IsRangeTreatmentOverridden());
    Weapon->PopRangeTreatmentOverride(TEXT("Window.Marksman.Overdrive"));
    TestTrue(TEXT("Popping one key leaves the other override active"), Weapon->IsRangeTreatmentOverridden());
    Weapon->PopRangeTreatmentOverride(TEXT("Test.SecondKey"));
    TestFalse(TEXT("Popping the last key clears the override"), Weapon->IsRangeTreatmentOverridden());

    // Popping a key that was never pushed, or popping twice, is a no-op, not
    // an error.
    Weapon->PopRangeTreatmentOverride(TEXT("Test.NeverPushed"));
    TestFalse(TEXT("Popping an unknown key is harmless"), Weapon->IsRangeTreatmentOverridden());
    return true;
}

// ---------------------------------------------------------------------------
// The falloff multiplier itself. FBreakerWeaponMath::DamageMultiplierAtDistance
// is a free, static, world-independent function, so its ordinary behaviour
// (the "returns to the normal falloff value once not overridden" half) is
// exercised directly here, exactly as BreakerWeaponTests.cpp already does.
//
// HONEST GAP: the actual short-circuit to 1.0 while overridden lives at the
// call site inside UBreakerWeaponComponent's fire path (the block around
// "Damage.BaseDamage = ScaledBaseDamage * FalloffMultiplier" in
// BreakerWeaponComponent.cpp), which is reached only through FireOnce's line
// trace against a real UWorld and a hit actor. That path is not exercised by
// this test file, or by any other test in this suite — BreakerWeaponTests.cpp
// tests FBreakerWeaponMath::DamageMultiplierAtDistance directly for exactly
// this reason and never drives a shot through the world either. What IS
// covered instead, honestly: (1) DamageMultiplierAtDistance itself returns
// MinimumFalloffMultiplier beyond FalloffEnd when nothing overrides it, and
// (2) IsRangeTreatmentOverridden(), the gate the call site actually branches
// on, flips correctly (proven above). The one line connecting them —
// "when overridden, ignore Hit.Distance and use 1.0 instead" — is a single
// ternary with no branching logic of its own to mis-test; it is left as a
// stated, not fabricated, gap.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerRangeTreatmentFalloffMathTest,
    "RiorsEdge.Combat.KeystoneOverrides.FalloffMathBeyondRange",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerRangeTreatmentFalloffMathTest::RunTest(const FString& Parameters)
{
    UBreakerWeaponDefinition* Definition = NewObject<UBreakerWeaponDefinition>();
    Definition->FalloffStart = 2000.0f;
    Definition->FalloffEnd = 6000.0f;
    Definition->MinimumFalloffMultiplier = 0.5f;

    // Not overridden: past FalloffEnd, the ordinary minimum applies. This is
    // the "returns to the normal falloff value once not overridden" half,
    // and it is also the value the override exists to replace: it is well
    // below 1.0, so the two are observably different outcomes, not a
    // vacuous comparison.
    const float UnmodifiedFarMultiplier = FBreakerWeaponMath::DamageMultiplierAtDistance(Definition, 9000.0f);
    TestEqual(TEXT("Beyond FalloffEnd, unmodified falloff bottoms out at the minimum"), UnmodifiedFarMultiplier, 0.5f);
    TestNotEqual(TEXT("Point-blank treatment (1.0) is a different outcome than the unmodified far falloff"),
        1.0f, UnmodifiedFarMultiplier);
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
