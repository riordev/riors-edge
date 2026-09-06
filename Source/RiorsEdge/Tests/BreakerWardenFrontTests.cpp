#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Combat/BreakerBossEnemy.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerCombatTypes.h"
#include "Combat/BreakerMonsterChassis.h"
#include "Combat/BreakerShieldMath.h"
#include "Combat/BreakerWardenEnemy.h"
#include "Weapons/BreakerWeaponComponent.h"
#include "Weapons/BreakerWeaponDefinition.h"
#include "Weapons/BreakerWeaponMath.h"

// ---------------------------------------------------------------------------
// THE WARDEN'S FRONT BREAKS (O198)
// ---------------------------------------------------------------------------
// The front is a pool of 15% of the bearer's max health that absorbs frontal
// damage until it is spent, then is gone for the fight; the rear never meets
// it; the boss inherits the rule. Three layers, in the order the code
// discipline asks for them: the pure arithmetic, the shipped configuration
// read off the class default objects, and the routing on the world-free
// ReceiveDamage harness the defense-triad and healing tests already use.
//
// What no test can prove: whether the Gold impact at the slab reads as "the
// front just opened", or whether five rifle rounds FEEL like a shield.
// ---------------------------------------------------------------------------

namespace BreakerWardenFrontTest
{
    // Distinctively named for the unity build.

    // The weapon a baseline character ships holding, resolved the way the
    // promoted-finding tests resolve it: the slot-1 archetype, never a named
    // gun and never the CDO by accident.
    const UBreakerWeaponDefinition* BreakerWardenFrontBaselineWeapon()
    {
        UBreakerWeaponComponent* Weapon = NewObject<UBreakerWeaponComponent>();
        Weapon->EquipArchetype(Weapon->GetSlotArchetype(1));
        return Weapon->GetActiveDefinition();
    }

    FBreakerDamageRequest BreakerWardenFrontHit(float Damage, const FVector& From, bool bBypassShield = false)
    {
        FBreakerDamageRequest Request;
        Request.BaseDamage = Damage;
        Request.DamageFamily = EBreakerDamageFamily::Physical;
        Request.bCanCritical = false;
        Request.bBypassShield = bBypassShield;
        Request.SourceLocation = From;
        Request.bHasSourceLocation = true;
        return Request;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerWardenFrontBreaksTest,
    "RiorsEdge.Combat.Warden.FrontBreaks",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerWardenFrontBreaksTest::RunTest(const FString& Parameters)
{
    using namespace BreakerWardenFrontTest;

    // ---- Pure ----------------------------------------------------------
    TestEqual(TEXT("A 704-health bearer arms a pool of 105.6"),
        BreakerShield::FrontPool(704.0f, 0.15f), 105.6f, 0.001f);
    TestEqual(TEXT("A negative max health arms nothing"), BreakerShield::FrontPool(-5.0f, 0.15f), 0.0f);
    TestEqual(TEXT("The fraction is clamped to one"), BreakerShield::FrontPool(100.0f, 4.0f), 100.0f, 0.001f);

    float Pool = BreakerShield::FrontPool(704.0f, 0.15f);
    for (int32 Round = 1; Round <= 4; ++Round)
    {
        const BreakerShield::FFrontSpend Spend = BreakerShield::SpendFrontPool(Pool, 24.0f);
        TestFalse(*FString::Printf(TEXT("Round %d does not break the pool"), Round), Spend.bBroke);
        TestEqual(*FString::Printf(TEXT("Round %d spills nothing"), Round), Spend.Spill, 0.0f, 0.001f);
        Pool = Spend.Remaining;
    }
    TestEqual(TEXT("Four rounds of 24 leave 9.6 in the pool"), Pool, 9.6f, 0.001f);
    const BreakerShield::FFrontSpend Fifth = BreakerShield::SpendFrontPool(Pool, 24.0f);
    TestTrue(TEXT("The fifth round breaks the pool"), Fifth.bBroke);
    TestEqual(TEXT("...leaving nothing"), Fifth.Remaining, 0.0f, 0.001f);
    TestEqual(TEXT("...and spilling 14.4 on to the ward"), Fifth.Spill, 14.4f, 0.001f);
    // A pool already at zero cannot break twice: the tell fires once.
    TestFalse(TEXT("An empty pool does not break again"), BreakerShield::SpendFrontPool(0.0f, 24.0f).bBroke);
    TestEqual(TEXT("An empty pool spills the whole hit"), BreakerShield::SpendFrontPool(0.0f, 24.0f).Spill, 24.0f, 0.001f);

    TestEqual(TEXT("105.6 breaks in five rounds of 24"), BreakerShield::RoundsToBreak(105.6f, 24.0f), 5);
    TestEqual(TEXT("866.25 breaks in thirty-seven rounds of 24"), BreakerShield::RoundsToBreak(866.25f, 24.0f), 37);
    TestEqual(TEXT("An exact multiple needs exactly that many"), BreakerShield::RoundsToBreak(48.0f, 24.0f), 2);
    TestEqual(TEXT("No pool breaks in zero rounds"), BreakerShield::RoundsToBreak(0.0f, 24.0f), 0);
    TestEqual(TEXT("A round of nothing never breaks a pool"),
        BreakerShield::RoundsToBreak(10.0f, 0.0f), TNumericLimits<int32>::Max());

    // ---- Shipped configuration, off the class default objects ----------
    const ABreakerWardenEnemy* Warden = GetDefault<ABreakerWardenEnemy>();
    const ABreakerBossEnemy* Boss = GetDefault<ABreakerBossEnemy>();
    if (!TestNotNull(TEXT("The Warden has a default object"), Warden)) return false;
    if (!TestNotNull(TEXT("The boss has a default object"), Boss)) return false;
    TestEqual(TEXT("The Warden's front is 15% of its max health"), Warden->FrontShieldFractionOfMaxHealth, 0.15f, 0.0001f);
    TestEqual(TEXT("The boss inherits the same fraction"), Boss->FrontShieldFractionOfMaxHealth, 0.15f, 0.0001f);
    // The facing geometry is untouched by the rule change: the arc that
    // decides "rear" is the one the armour step always used.
    TestEqual(TEXT("The rear arc cosine is unchanged"), GetDefault<UBreakerCombatComponent>()->RearArcCosine, 0.15f, 0.0001f);

    const FBreakerMonsterChassisParams Chassis;
    const float WardenHealth = UBreakerMonsterChassisLibrary::GetMonsterHealth(
        1, EBreakerMonsterRank::Trash, Chassis, Warden->GetArchetypeHealthMultiplier());
    TestEqual(TEXT("An area-level-1 Warden has 704 health (220 x 3.2)"), WardenHealth, 704.0f, 0.01f);
    const float BossHealth = UBreakerMonsterChassisLibrary::GetMonsterHealth(
        1, EBreakerMonsterRank::Boss, Chassis, Boss->GetArchetypeHealthMultiplier());
    TestEqual(TEXT("An area-level-1 boss has 5,775 health (220 x 75 x 0.35)"), BossHealth, 5775.0f, 0.01f);

    const UBreakerWeaponDefinition* Rifle = BreakerWardenFrontBaselineWeapon();
    if (!TestNotNull(TEXT("The baseline slot resolves to a weapon definition"), Rifle)) return false;
    TestEqual(TEXT("The rifle's round is 24"), Rifle->Damage, 24.0f, 0.001f);
    TestEqual(TEXT("The rifle fires at 600 rounds per minute"), Rifle->RoundsPerMinute, 600.0f, 0.001f);

    // The pinned numbers, composed from the reads above rather than restated.
    const float WardenPool = BreakerShield::FrontPool(WardenHealth, Warden->FrontShieldFractionOfMaxHealth);
    const float BossPool = BreakerShield::FrontPool(BossHealth, Boss->FrontShieldFractionOfMaxHealth);
    const int32 WardenRounds = BreakerShield::RoundsToBreak(WardenPool, Rifle->Damage);
    const int32 BossRounds = BreakerShield::RoundsToBreak(BossPool, Rifle->Damage);
    const float SecondsPerRound = 60.0f / Rifle->RoundsPerMinute;
    AddInfo(FString::Printf(TEXT("FRONT  Warden AL1 pool %.1f: %d rounds, %.2fs after the first"),
        WardenPool, WardenRounds, (WardenRounds - 1) * SecondsPerRound));
    AddInfo(FString::Printf(TEXT("FRONT  boss AL1 pool %.2f: %d rounds, %.2fs after the first"),
        BossPool, BossRounds, (BossRounds - 1) * SecondsPerRound));
    TestEqual(TEXT("The level-1 rifle breaks a Warden's front in five rounds"), WardenRounds, 5);
    TestEqual(TEXT("The level-1 rifle breaks the boss's front in thirty-seven rounds"), BossRounds, 37);

    // O18: the boss's front is 15% of a baseline boss kill, so it must break
    // inside 15% of the 20-45s band — [3, 6.75] s — at the on-level area the
    // band is asserted at. The same derivation BossBand uses, one factor in.
    {
        constexpr int32 AreaLevel = 50;
        const int32 ItemLevel = UBreakerMonsterChassisLibrary::GetDropItemLevel(AreaLevel);
        const float PerShot = FBreakerWeaponMath::WeaponBaseDamage(
            Rifle->Damage, ItemLevel, GetDefault<UBreakerWeaponComponent>()->ItemLevelDamageGrowth)
            * FMath::Max(1, Rifle->PelletsPerShot);
        const float DamagePerSecond = PerShot * Rifle->RoundsPerMinute / 60.0f;
        const float OnLevelBossHealth = UBreakerMonsterChassisLibrary::GetMonsterHealth(
            AreaLevel, EBreakerMonsterRank::Boss, Chassis, Boss->GetArchetypeHealthMultiplier());
        const float FrontSeconds = BreakerShield::FrontPool(OnLevelBossHealth, Boss->FrontShieldFractionOfMaxHealth)
            / FMath::Max(DamagePerSecond, UE_SMALL_NUMBER);
        AddInfo(FString::Printf(TEXT("FRONT  boss AL%d front breaks in %.2fs (O18 x 0.15: 3.00-6.75s)"), AreaLevel, FrontSeconds));
        TestTrue(*FString::Printf(TEXT("The boss's front (%.2fs) breaks no sooner than 3s"), FrontSeconds), FrontSeconds >= 3.0f);
        TestTrue(*FString::Printf(TEXT("The boss's front (%.2fs) breaks no later than 6.75s"), FrontSeconds), FrontSeconds <= 6.75f);
    }

    // ---- Wiring, on the world-free ReceiveDamage harness ---------------
    // A bare actor's forward is +X and its location the origin, so a source
    // at +X is frontal and one at -X is in the rear arc.
    AActor* Body = NewObject<AActor>(GetTransientPackage());
    UBreakerAttributeSet* Attributes = NewObject<UBreakerAttributeSet>(Body);
    UBreakerCombatComponent* Combat = NewObject<UBreakerCombatComponent>(Body);
    Combat->BindAttributes(Attributes);
    Attributes->ApplyMaxHealth(704.0f);
    Attributes->ApplyMaxShield(0.0f);
    Combat->RestoreVitals();
    const FVector Front(1000.0f, 0.0f, 0.0f);
    const FVector Rear(-1000.0f, 0.0f, 0.0f);

    TestEqual(TEXT("A body that never armed a pool has none"), Combat->GetFrontShieldMax(), 0.0f);
    TestFalse(TEXT("...and is not broken"), Combat->IsFrontShieldBroken());

    Combat->ArmFrontShield(BreakerShield::FrontPool(704.0f, 0.15f));
    TestEqual(TEXT("Arming sets the standing pool"), Combat->GetFrontShield(), 105.6f, 0.001f);
    TestEqual(TEXT("...and the bar's shield reads the pool"), Combat->GetDisplayShield(), 105.6f, 0.001f);
    TestEqual(TEXT("...and the bar's maximum reads the pool"), Combat->GetDisplayMaxShield(), 105.6f, 0.001f);

    // A rear 24 at full pool lands on health and leaves the pool whole.
    {
        const FBreakerDamageResult Hit = Combat->ReceiveDamage(BreakerWardenFrontHit(24.0f, Rear));
        TestEqual(TEXT("A rear hit pays health"), Hit.HealthDamage, 24.0f, 0.001f);
        TestEqual(TEXT("A rear hit pays no shield"), Hit.ShieldDamage, 0.0f, 0.001f);
        TestEqual(TEXT("A rear hit leaves the pool whole"), Combat->GetFrontShield(), 105.6f, 0.001f);
        TestEqual(TEXT("Health took the rear hit"), Attributes->GetHealth(), 680.0f, 0.001f);
    }

    // A shield-bypassing frontal tick pays health and leaves the pool whole.
    {
        const FBreakerDamageResult Tick = Combat->ReceiveDamage(BreakerWardenFrontHit(10.0f, Front, true));
        TestEqual(TEXT("A bypassing frontal tick pays health"), Tick.HealthDamage, 10.0f, 0.001f);
        TestEqual(TEXT("...and leaves the pool whole"), Combat->GetFrontShield(), 105.6f, 0.001f);
    }

    // Four frontal rounds of 24 spend the pool without breaking it.
    for (int32 Round = 1; Round <= 4; ++Round)
    {
        const FBreakerDamageResult Hit = Combat->ReceiveDamage(BreakerWardenFrontHit(24.0f, Front));
        TestEqual(*FString::Printf(TEXT("Frontal round %d pays the pool"), Round), Hit.ShieldDamage, 24.0f, 0.001f);
        TestEqual(*FString::Printf(TEXT("Frontal round %d pays no health"), Round), Hit.HealthDamage, 0.0f, 0.001f);
        TestFalse(*FString::Printf(TEXT("Frontal round %d does not break the front"), Round), Combat->IsFrontShieldBroken());
    }
    TestEqual(TEXT("Four frontal rounds leave 9.6"), Combat->GetFrontShield(), 9.6f, 0.001f);
    TestEqual(TEXT("The ward is untouched by pool payments"), Attributes->GetShield(), 0.0f, 0.001f);

    // The fifth breaks it: 9.6 to the pool, 14.4 spills to health (no ward).
    {
        const FBreakerDamageResult Hit = Combat->ReceiveDamage(BreakerWardenFrontHit(24.0f, Front));
        TestEqual(TEXT("The breaking round pays what the pool had"), Hit.ShieldDamage, 9.6f, 0.001f);
        TestEqual(TEXT("...and the spill reaches health"), Hit.HealthDamage, 14.4f, 0.001f);
        TestTrue(TEXT("The front is latched broken"), Combat->IsFrontShieldBroken());
        TestEqual(TEXT("The pool is empty"), Combat->GetFrontShield(), 0.0f, 0.001f);
        TestEqual(TEXT("A broken pool leaves the bar's shield"), Combat->GetDisplayShield(), 0.0f, 0.001f);
        TestEqual(TEXT("...and the bar's maximum"), Combat->GetDisplayMaxShield(), 0.0f, 0.001f);
    }

    // A frontal 24 after the break lands 24 on health: armour is 0, the pool
    // is gone, nothing else stands in the way.
    {
        const float Before = Attributes->GetHealth();
        const FBreakerDamageResult Hit = Combat->ReceiveDamage(BreakerWardenFrontHit(24.0f, Front));
        TestEqual(TEXT("A frontal hit after the break pays health"), Hit.HealthDamage, 24.0f, 0.001f);
        TestEqual(TEXT("...the whole hit"), Before - Attributes->GetHealth(), 24.0f, 0.001f);
        TestTrue(TEXT("The latch holds"), Combat->IsFrontShieldBroken());
    }

    // The ward is a separate pool. What AddModifierShield writes — the
    // attribute shield — rises without touching the front, and a frontal hit
    // pays the front first, the ward second, health third.
    Attributes->ApplyMaxShield(100.0f);
    Attributes->ApplyShield(100.0f);
    TestTrue(TEXT("Raising the ward does not clear the latch"), Combat->IsFrontShieldBroken());
    TestEqual(TEXT("Raising the ward leaves the front at zero"), Combat->GetFrontShield(), 0.0f, 0.001f);
    Combat->ArmFrontShield(BreakerShield::FrontPool(704.0f, 0.15f));
    TestEqual(TEXT("A re-armed front beside a ward reads as the sum"), Combat->GetDisplayShield(), 205.6f, 0.001f);
    {
        const FBreakerDamageResult Hit = Combat->ReceiveDamage(BreakerWardenFrontHit(24.0f, Front));
        TestEqual(TEXT("The front pays first"), Combat->GetFrontShield(), 81.6f, 0.001f);
        TestEqual(TEXT("...and the ward is untouched"), Attributes->GetShield(), 100.0f, 0.001f);
        TestEqual(TEXT("...and health is untouched"), Hit.HealthDamage, 0.0f, 0.001f);
    }
    {
        const float Before = Attributes->GetHealth();
        const FBreakerDamageResult Hit = Combat->ReceiveDamage(BreakerWardenFrontHit(200.0f, Front));
        TestTrue(TEXT("A hit through both pools breaks the front"), Combat->IsFrontShieldBroken());
        TestEqual(TEXT("...empties the ward with the spill"), Attributes->GetShield(), 0.0f, 0.001f);
        TestEqual(TEXT("...and the remainder reaches health"), Hit.HealthDamage, 18.4f, 0.001f);
        TestEqual(TEXT("Health paid exactly the remainder"), Before - Attributes->GetHealth(), 18.4f, 0.001f);
    }

    // A vitals restore alone does not re-arm — the archetype that means a
    // front binds its own arming — and ArmFrontShield after it refills and
    // clears the latch.
    Combat->RestoreVitals();
    TestTrue(TEXT("RestoreVitals leaves a broken front broken"), Combat->IsFrontShieldBroken());
    TestEqual(TEXT("RestoreVitals refills the ward"), Attributes->GetShield(), 100.0f, 0.001f);
    Combat->ArmFrontShield(BreakerShield::FrontPool(Attributes->GetMaxHealth(), 0.15f));
    TestFalse(TEXT("Arming after a restore clears the latch"), Combat->IsFrontShieldBroken());
    TestEqual(TEXT("...and refills the pool"), Combat->GetFrontShield(), 105.6f, 0.001f);
    TestEqual(TEXT("...and the bar reads ward plus front"), Combat->GetDisplayMaxShield(), 205.6f, 0.001f);
    return true;
}

#endif
