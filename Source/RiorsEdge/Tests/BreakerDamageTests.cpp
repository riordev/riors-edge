#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Combat/BreakerDamageLibrary.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerShieldRoutingTest,
    "RiorsEdge.Combat.Damage.ShieldRouting",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerShieldRoutingTest::RunTest(const FString& Parameters)
{
    FBreakerDamageRequest Request;
    Request.BaseDamage = 75.0f;
    Request.bCanCritical = false;
    Request.DamageFamily = EBreakerDamageFamily::TrueDamage;
    FBreakerDefenseState Defense;
    Defense.Health = 100.0f;
    Defense.Shield = 50.0f;

    const FBreakerDamageResult Result = UBreakerDamageLibrary::ResolveDamage(Request, Defense);
    TestEqual(TEXT("Shield absorbs its available amount"), Result.ShieldDamage, 50.0f);
    TestEqual(TEXT("Overflow reaches health"), Result.HealthDamage, 25.0f);
    TestTrue(TEXT("Shield break is reported"), Result.bShieldBroken);
    TestFalse(TEXT("Target survives"), Result.bKilled);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerArmorTest,
    "RiorsEdge.Combat.Damage.ArmorAndPenetration",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerArmorTest::RunTest(const FString& Parameters)
{
    TestEqual(TEXT("100 armour mitigates 50 percent"), UBreakerDamageLibrary::CalculateArmorMitigation(100.0f, 0.0f), 0.5f);
    TestEqual(TEXT("Penetration reduces effective armour"), UBreakerDamageLibrary::CalculateArmorMitigation(100.0f, 50.0f), 1.0f / 3.0f);
    TestEqual(TEXT("Mitigation caps at 80 percent"), UBreakerDamageLibrary::CalculateArmorMitigation(10000.0f, 0.0f), 0.8f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerSnapshotDotTest,
    "RiorsEdge.Combat.Damage.SnapshotCriticalDot",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerSnapshotDotTest::RunTest(const FString& Parameters)
{
    FBreakerStatusApplicationSpec Status;
    Status.BaseDamagePerTick = 10.0f;
    Status.InitialStacks = 2;
    Status.ProcCoefficient = 0.25f;
    Status.Snapshot.SourcePower = 1.2f;
    Status.Snapshot.DamageOverTimeMultiplier = 1.5f;
    Status.Snapshot.CriticalMultiplier = 2.0f;
    Status.Snapshot.bRolledCritical = true;

    FBreakerDamageRequest Tick = UBreakerDamageLibrary::MakeSnapshotDotTick(Status, EBreakerDamageFamily::Physical, 3);
    Tick.bBypassShield = true;
    FBreakerDefenseState Defense;
    Defense.Health = 100.0f;
    Defense.Shield = 100.0f;
    Defense.Armor = 100.0f;

    const FBreakerDamageResult Result = UBreakerDamageLibrary::ResolveDamage(Tick, Defense);
    // Raw: 10 * 2 stacks * 1.2 power * 1.5 DoT * 2 crit = 72.
    // Physical bypass DoT receives half of normal 50% armour mitigation: 25%.
    TestEqual(TEXT("Snapshot critical fixes raw tick damage"), Result.RawDamage, 72.0f);
    TestEqual(TEXT("Physical bypass DoT uses half armour mitigation"), Result.HealthDamage, 54.0f);
    TestEqual(TEXT("Bypass leaves shield untouched"), Result.RemainingShield, 100.0f);
    TestTrue(TEXT("Snapshot critical result persists"), Result.bCritical);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerLethalDamageTest,
    "RiorsEdge.Combat.Damage.LethalResult",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerLethalDamageTest::RunTest(const FString& Parameters)
{
    FBreakerDamageRequest Request;
    Request.BaseDamage = 150.0f;
    Request.DamageFamily = EBreakerDamageFamily::TrueDamage;
    Request.bCanCritical = false;
    Request.bBypassShield = true;
    FBreakerDefenseState Defense;
    Defense.Health = 80.0f;
    Defense.Shield = 50.0f;

    const FBreakerDamageResult Result = UBreakerDamageLibrary::ResolveDamage(Request, Defense);
    TestEqual(TEXT("Health damage reports actual health removed"), Result.HealthDamage, 80.0f);
    TestEqual(TEXT("Health clamps at zero"), Result.RemainingHealth, 0.0f);
    TestTrue(TEXT("Lethal result is reported"), Result.bKilled);
    return true;
}

#endif
