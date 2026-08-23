#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Combat/BreakerBossEnemy.h"
#include "Combat/BreakerEnemy.h"
#include "Combat/BreakerMonsterChassis.h"
#include "Items/BreakerAffixLibrary.h"
#include "Items/BreakerItemTypes.h"
#include "Tests/BreakerStatusEmit.h"
#include "Weapons/BreakerWeaponComponent.h"
#include "Weapons/BreakerWeaponDefinition.h"
#include "Weapons/BreakerWeaponMath.h"

// ---------------------------------------------------------------------------
// THE PROMOTED FINDINGS
// ---------------------------------------------------------------------------
// Findings that were carried as prose until somebody promoted them to
// invariants, and then carried as invariant rows with no test — which is the
// worst of the three states a claim can be in, because a named test that was
// never written looks asserted and is not.
//
// All of it is pure maths against the shipped chassis: no world, no actor, no
// ability system, and nothing granted that the game does not grant. Where a
// figure is a target the game does not meet, the test is RED ON PURPOSE and
// enumerated in Scripts/status-pins.json with the condition that deletes it.
// ---------------------------------------------------------------------------

namespace BreakerPromotedFindingTest
{
    // Distinctively named for the unity build, per the twice-shipped rule about
    // anonymous-namespace collisions.

    // The weapon a baseline character actually carries, READ rather than
    // transcribed and rather than named.
    //
    // This was `constexpr float PromotedArchetypeBase = 13.0f` under a comment
    // calling it "the rifle, as authored". 13 is the SMG's damage. The rifle is
    // the prototype factory's `default:` case — it authors a DisplayName and
    // inherits every number from the definition CDO, where Damage is 24 — so
    // the transcription described no weapon in the table, and it had never been
    // right: Damage has been 24 since the commit that added the weapon. The
    // comment it carried, that the constant "cancels out of every ratio", is
    // true in BreakerCurveCompositionTests where it was written and FALSE here,
    // because this file returns seconds. Every trash, elite and boss figure was
    // inflated 24/13 = 1.85x and the ratio was ruled on twice as a shortfall in
    // the game.
    //
    // It is not enough to read the CDO. That gives the right number today only
    // because the Rifle has no factory case, so "the CDO" and "the rifle" are
    // the same object by accident; give the Rifle a case and a CDO read keeps
    // measuring the CDO. And it is not enough to name the Rifle either, because
    // that is a CHOICE of representative, which is the same defect wearing a
    // different hat. Ask the component which archetype slot one ships holding
    // and resolve THAT, so the baseline follows the loadout.
    const UBreakerWeaponDefinition* PromotedBaselineWeapon()
    {
        UBreakerWeaponComponent* Weapon = NewObject<UBreakerWeaponComponent>();
        Weapon->EquipArchetype(Weapon->GetSlotArchetype(1));
        return Weapon->GetActiveDefinition();
    }

    // THE REFERENCE ARCHETYPE. O18's seed targets describe the enemy archetype
    // multiplier of 1.0 and nothing else; every other archetype's band is that
    // target times its own multiplier, derived rather than authored. Named here
    // so a TTK figure below says what it is a target FOR — a Warden trash mob
    // at x3.2 kills in 2.9s and that is on target, not 3x adrift.
    constexpr float PromotedReferenceArchetype = 1.0f;

    float PromotedWeaponGrowth()
    {
        return GetDefault<UBreakerWeaponComponent>()->ItemLevelDamageGrowth;
    }

    // A full gear set's Health lines at the tier this item level can roll.
    // One line per slot the affix is ALLOWED on, so a character who wanted
    // nothing else could carry the lot, and the question this test asks is
    // whether the CEILING of gear defence keeps up. A smaller set only makes
    // the answer worse.
    //
    // The slot count is read off the affix rather than written as 8. It was 8,
    // and it agreed with the pool — but PromotedInvestedDamageReduction two
    // functions down was already reading AllowedSlots.Num() for the same pool,
    // so the file held both conventions and only one of them survives a slot
    // being added.
    float PromotedGearHealthAt(int32 ItemLevel)
    {
        const TArray<FBreakerAffixDefinition>& Pool = UBreakerAffixLibrary::GetSliceAffixPool();
        const FBreakerAffixDefinition* Line = UBreakerAffixLibrary::FindAffix(Pool, TEXT("Core.Health"));
        if (!Line) return 0.0f;
        return Line->AllowedSlots.Num() * UBreakerAffixLibrary::ValueForTier(
            *Line, UBreakerAffixLibrary::BestTierForItemLevel(ItemLevel));
    }

    // Everything a baseline character has to absorb a hit with, at a level.
    float PromotedEffectiveHealthAt(int32 AreaLevel)
    {
        const float Base = GetDefault<UBreakerAttributeSet>()->GetMaxHealth();
        return Base + PromotedGearHealthAt(UBreakerMonsterChassisLibrary::GetDropItemLevel(AreaLevel));
    }

    // Seconds a BASELINE build takes to kill one enemy of this rank, in on-level
    // content. Absolute rather than relative, so it needs a cadence and both
    // archetypes — the weapon's and the ENEMY's. A baseline carries no
    // multiplier band by construction, which is what makes it the baseline.
    //
    // EnemyArchetypeMultiplier is the caller's, because it is a property of the
    // actor rather than of the rank: GetMonsterHealth composes rank x archetype
    // and defaults the second to 1.0. This test called the defaulting overload
    // and so measured a boss rank on a trash archetype, which the game never
    // fields. That is the same mistake as the weapon constant above, in the
    // other direction — the enemy granted more than the game grants.
    float PromotedBaselineSecondsToKill(int32 AreaLevel, EBreakerMonsterRank Rank,
        const FBreakerMonsterChassisParams& Params,
        float EnemyArchetypeMultiplier = PromotedReferenceArchetype)
    {
        const UBreakerWeaponDefinition* Definition = PromotedBaselineWeapon();
        if (!Definition) return 0.0f;
        const int32 ItemLevel = UBreakerMonsterChassisLibrary::GetDropItemLevel(AreaLevel);
        const float PerShot = FBreakerWeaponMath::WeaponBaseDamage(
            Definition->Damage, ItemLevel, PromotedWeaponGrowth())
            * FMath::Max(1, Definition->PelletsPerShot);
        const float DamagePerSecond = PerShot * Definition->RoundsPerMinute / 60.0f;
        const float Health = UBreakerMonsterChassisLibrary::GetMonsterHealth(
            AreaLevel, Rank, Params, EnemyArchetypeMultiplier);
        return Health / FMath::Max(DamagePerSecond, UE_SMALL_NUMBER);
    }

    // Hits a trash enemy needs to kill a baseline character at this area level.
    float PromotedHitsToDieAt(int32 AreaLevel, const FBreakerMonsterChassisParams& Params)
    {
        const float Incoming = UBreakerMonsterChassisLibrary::GetChassisDamage(AreaLevel, Params);
        if (Incoming <= 0.0f) return 0.0f;
        return PromotedEffectiveHealthAt(AreaLevel) / Incoming;
    }

    // The same figure in the unit the target is authored in. One attacker at
    // the shipped cadence — a pack kills faster, and a target written for a
    // pack would have to say so.
    //
    // The cadence was transcribed here as 1.15f because AttackCooldown is
    // protected on ABreakerEnemy. It agreed with the shipped default, so it
    // never produced a wrong figure — but the constant it sat beside did, and
    // the accessor it said it was waiting for cost one line. ABreakerEnemy is
    // the MELEE base and that is deliberate: ABreakerRangedEnemy sets the
    // cooldown to 0 and fires on its own timer, so "the enemy" has no single
    // cadence and this test means the melee one.
    float PromotedEnemyAttackCooldown()
    {
        return GetDefault<ABreakerEnemy>()->GetAttackCooldown();
    }

    float PromotedTimeToDieAt(int32 AreaLevel, const FBreakerMonsterChassisParams& Params)
    {
        return PromotedHitsToDieAt(AreaLevel, Params) * PromotedEnemyAttackCooldown();
    }

    // What a full defensive commitment buys, from the shipped pool: the
    // physical reduction line on every slot that carries it, at the tier the
    // character cap can roll, clamped by the pool's own cap.
    float PromotedInvestedDamageReduction()
    {
        const TArray<FBreakerAffixDefinition>& Pool = UBreakerAffixLibrary::GetSliceAffixPool();
        const FBreakerAffixDefinition* Line = UBreakerAffixLibrary::FindAffix(Pool, TEXT("Core.PhysicalDR"));
        if (!Line) return 0.0f;
        const float Per = UBreakerAffixLibrary::ValueForTier(
            *Line, UBreakerAffixLibrary::BestTierForItemLevel(50));
        const int32 Slots = Line->AllowedSlots.Num();
        // The cap is the equipment layer's own, read rather than transcribed:
        // it was 0.60f here with a comment calling it "the pool's authored
        // cap", and the authored cap lives on FBreakerEquipmentStats. It is
        // also not fixed — BreakerItemRules raises it to 80 for a rewrite — so
        // a copy here would report a defensive ceiling the game does not have.
        const float Cap = FBreakerEquipmentStats::DefaultPhysicalDamageReductionCap / 100.0f;
        return FMath::Min(Slots * Per / 100.0f, Cap);
    }
}

// ---------------------------------------------------------------------------
// HITS-TO-DIE DOES NOT FALL ACROSS THE LEVEL RANGE
// ---------------------------------------------------------------------------
// The property `d` exists to produce, stated as the thing a player feels rather
// than as an exponent. Combat.Chassis.DamageBelowHealth already measures the
// two GROWTH rates against each other; this measures the quantity itself, at
// points across the range, because a ratio can hold on average while the curve
// dips in the middle — and a dip is a difficulty cliff nobody authored.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerDefenseCurveHitsToDieTest,
    "RiorsEdge.Combat.DefenseCurve.HitsToDie",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerDefenseCurveHitsToDieTest::RunTest(const FString& Parameters)
{
    using namespace BreakerPromotedFindingTest;
    const FBreakerMonsterChassisParams Params;

    const int32 Points[] = {1, 10, 20, 30, 40, 50};
    float First = 0.0f, Worst = 0.0f;
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(Points); ++Index)
    {
        const float Hits = PromotedHitsToDieAt(Points[Index], Params);
        if (Index == 0) { First = Hits; Worst = Hits; }
        Worst = FMath::Min(Worst, Hits);
        AddInfo(FString::Printf(TEXT("HITS-TO-DIE  area level %2d: %.2f hits"), Points[Index], Hits));
    }

    const float AtCap = PromotedHitsToDieAt(50, Params);
    AddInfo(FString::Printf(
        TEXT("HITS-TO-DIE  level 1 %.2f -> level 50 %.2f (%.0f%% of where it started); worst point %.2f"),
        First, AtCap, 100.0f * AtCap / FMath::Max(First, UE_SMALL_NUMBER), Worst));

    // THE PROPERTY, and it is a floor rather than a band: a character may get
    // tougher relative to content, and O27 rather wants that at the top end.
    // What it may not do is get FLIMSIER, which is the inversion O91 rules out
    // — a character at cap squishier than one at level 10, in content they
    // out-gear.
    TestTrue(*FString::Printf(
        TEXT("Hits-to-die at the cap (%.2f) is at least what it was at level 1 (%.2f)"), AtCap, First),
        AtCap >= First);
    TestTrue(*FString::Printf(
        TEXT("Hits-to-die never dips below its level-1 value anywhere in the range (worst %.2f)"), Worst),
        Worst >= First - UE_KINDA_SMALL_NUMBER);
    return true;
}

// ---------------------------------------------------------------------------
// TIME-TO-DIE, AS A MAGNITUDE — THE SECOND CONSTRAINT ON THE RETUNE
// ---------------------------------------------------------------------------
// HitsToDie above asserts that the curve does not FALL and never asserts what
// it should sit at. That is the identical shape that hid the throughput gap:
// PowerCurve.Composition asserted flatness for as long as anyone can remember
// and nobody asked what it was flat at, until a boss test tripped over it.
//
// Left alone it would repeat exactly. Solving for `d` with one constraint
// anchors the range at the level-1 value and pulls the cap up to meet it, which
// makes the report green while nobody has asked whether twenty-one hits is
// right. These two tests are the second constraint, and they are written BEFORE
// the retune rather than after it for that reason.
//
// Seconds, not hits, because the target is authored in seconds: hits-to-die
// times the shipped enemy attack cooldown.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerTimeToDieBareTest,
    "RiorsEdge.Combat.DefenseCurve.TimeToDieBare",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerTimeToDieBareTest::RunTest(const FString& Parameters)
{
    using namespace BreakerPromotedFindingTest;
    const FBreakerMonsterChassisParams Params;

    // O18: four to five seconds with no resources or sustain.
    constexpr float TtdFloor = 4.0f;
    constexpr float TtdCeiling = 5.0f;

    for (const int32 AreaLevel : {1, 25, 50})
    {
        AddInfo(FString::Printf(TEXT("TTD BARE  area level %2d: %.2fs (O18 target %.0f-%.0fs)"),
            AreaLevel, PromotedTimeToDieAt(AreaLevel, Params), TtdFloor, TtdCeiling));
    }

    // BOTH ENDS ARE ASSERTED, and that is the entire point of this test. A
    // single-point assertion would let the retune anchor wherever it liked.
    const float AtOne = PromotedTimeToDieAt(1, Params);
    const float AtCap = PromotedTimeToDieAt(50, Params);
    TestTrue(*FString::Printf(TEXT("TTD at level 1 (%.2fs) is at most %.0fs"), AtOne, TtdCeiling),
        AtOne <= TtdCeiling);
    TestTrue(*FString::Printf(TEXT("TTD at level 1 (%.2fs) is at least %.0fs"), AtOne, TtdFloor),
        AtOne >= TtdFloor);
    TestTrue(*FString::Printf(TEXT("TTD at the cap (%.2fs) is at most %.0fs"), AtCap, TtdCeiling),
        AtCap <= TtdCeiling);
    TestTrue(*FString::Printf(TEXT("TTD at the cap (%.2fs) is at least %.0fs"), AtCap, TtdFloor),
        AtCap >= TtdFloor);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerTimeToDieInvestedTest,
    "RiorsEdge.Combat.DefenseCurve.TimeToDieInvested",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerTimeToDieInvestedTest::RunTest(const FString& Parameters)
{
    using namespace BreakerPromotedFindingTest;
    const FBreakerMonsterChassisParams Params;

    // O18: "substantially higher invested". The number was never authored, so
    // this asserts the RATIO the word has to mean at minimum — a defensive
    // commitment that buys less than half again is not a commitment, it is a
    // rounding error the player paid slots for.
    constexpr float InvestedMustBeatBareBy = 1.5f;

    const float Bare = PromotedTimeToDieAt(50, Params);
    const float Invested = Bare / FMath::Max(1.0f - PromotedInvestedDamageReduction(), UE_SMALL_NUMBER);
    AddInfo(FString::Printf(
        TEXT("TTD INVESTED  area level 50: bare %.2fs, invested %.2fs at %.0f%% physical reduction (x%.2f)"),
        Bare, Invested, 100.0f * PromotedInvestedDamageReduction(), Invested / FMath::Max(Bare, UE_SMALL_NUMBER)));

    TestTrue(*FString::Printf(TEXT("A defensive commitment buys at least %.1fx the bare figure (x%.2f)"),
        InvestedMustBeatBareBy, Invested / FMath::Max(Bare, UE_SMALL_NUMBER)),
        Invested >= Bare * InvestedMustBeatBareBy);
    return true;
}

// ---------------------------------------------------------------------------
// TRASH AND ELITE TIME-TO-KILL, AS MAGNITUDES
// ---------------------------------------------------------------------------
// Combat.PowerCurve.Composition asserts the baseline TTK curve is FLAT across
// area level and never asserts what it is flat AT. A curve can be perfectly
// level and land at twice its target everywhere, which is what happened here: a
// throughput shortfall on the two commonest enemies in the game survived every
// green run until a boss test tripped over it, and the boss looked like the
// fault.
//
// So these assert the magnitude the flatness test takes for granted, on the same
// chassis and weapon maths. They are cheaper than any other finding outstanding,
// and they are the figure every other TTK number in the project rests on.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerTrashTtkTest,
    "RiorsEdge.Combat.PowerCurve.TrashTtk",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerTrashTtkTest::RunTest(const FString& Parameters)
{
    using namespace BreakerPromotedFindingTest;
    const FBreakerMonsterChassisParams Params;

    // O18: trash a little under a second. A CEILING rather than a band — trash
    // exists to be trivialized by an optimized build, so a baseline killing it
    // faster than the seed is not a defect.
    constexpr float TrashSecondsCeiling = 1.0f;

    // O18 names the reference archetype, so this figure does too. A fielded
    // trash mob multiplies it: the Warden's x3.2 is a 2.9s kill on target.
    for (const int32 AreaLevel : {1, 25, 50})
    {
        AddInfo(FString::Printf(
            TEXT("TRASH TTK  area level %2d: %.2fs at archetype x%.2f (O18 target under %.1fs)"),
            AreaLevel, PromotedBaselineSecondsToKill(AreaLevel, EBreakerMonsterRank::Trash, Params),
            PromotedReferenceArchetype, TrashSecondsCeiling));
    }

    const float AtCap = PromotedBaselineSecondsToKill(50, EBreakerMonsterRank::Trash, Params);
    TestTrue(*FString::Printf(TEXT("A baseline kills on-level trash in under %.1fs (measured %.2fs)"),
        TrashSecondsCeiling, AtCap), AtCap <= TrashSecondsCeiling);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerEliteTtkTest,
    "RiorsEdge.Combat.PowerCurve.EliteTtk",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerEliteTtkTest::RunTest(const FString& Parameters)
{
    using namespace BreakerPromotedFindingTest;
    const FBreakerMonsterChassisParams Params;

    // O18: elite around three. A band, because an elite dying too fast is a rank
    // that stopped meaning anything — the opposite failure from trash, which is
    // why the two are asserted differently.
    constexpr float EliteSecondsFloor = 2.0f;
    constexpr float EliteSecondsCeiling = 4.0f;

    const float AtCap = PromotedBaselineSecondsToKill(50, EBreakerMonsterRank::Elite, Params);
    AddInfo(FString::Printf(
        TEXT("ELITE TTK  area level 50: %.2fs at archetype x%.2f (O18 target ~3s, band %.0f-%.0fs)"),
        AtCap, PromotedReferenceArchetype, EliteSecondsFloor, EliteSecondsCeiling));

    TestTrue(*FString::Printf(TEXT("A baseline kills an on-level elite in at least %.0fs (measured %.2fs)"),
        EliteSecondsFloor, AtCap), AtCap >= EliteSecondsFloor);
    TestTrue(*FString::Printf(TEXT("A baseline kills an on-level elite in at most %.0fs (measured %.2fs)"),
        EliteSecondsCeiling, AtCap), AtCap <= EliteSecondsCeiling);
    return true;
}

// ---------------------------------------------------------------------------
// BOSS TIME-TO-KILL, FOR A BASELINE BUILD
// ---------------------------------------------------------------------------
// O94: the 20-45s figure describes a BASELINE build in on-level content, and
// its optimized twin is asserted separately (Combat.PowerCurve.BossOptimized)
// because a single target measured against an optimized character is measuring
// the wrong character.
//
// This is the one figure here that cannot be a ratio: seconds are absolute, so
// it needs a cadence and two archetypes — the weapon's and the boss's. All
// three are read from the shipped definitions rather than assumed, and the
// number they produce is a SEED — the chassis solves backwards from it, per the
// spec's own rule that TTK is an output and never an input.
//
// That sentence used to say "both are read" while the weapon archetype was
// transcribed as the wrong gun and the enemy archetype was not read at all, and
// the resulting 126.9s was ruled on as two chassis errors. THE RANK ROW WAS
// NEVER WRONG. The spec derives a rank multiplier as the ratio of its TTK
// target to trash's, and that ratio lands on rank TIMES archetype, not on rank:
// the fielded Field Marshal is x75 rank on a x0.35 archetype, a net x26.25 over
// trash, and 26.25 x the 0.917s trash kill is 24.06s inside O18's 20-45s. Rank
// alone matches the ratio only for an archetype of 1.0, which no boss is. O59
// stands, and the identity is asserted below so nobody re-derives the table a
// third time.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerBossBandTest,
    "RiorsEdge.Combat.PowerCurve.BossBand",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerBossBandTest::RunTest(const FString& Parameters)
{
    using namespace BreakerPromotedFindingTest;
    const FBreakerMonsterChassisParams Params;

    // O18's seed band for a baseline build.
    constexpr float BossSecondsFloor = 20.0f;
    constexpr float BossSecondsCeiling = 45.0f;

    constexpr int32 AreaLevel = 50;

    // The boss the game FIELDS, not a boss-rank trash archetype. Read off the
    // shipped actor's class default object, which is world-free.
    const float BossArchetype = GetDefault<ABreakerBossEnemy>()->GetArchetypeHealthMultiplier();

    const float Seconds = PromotedBaselineSecondsToKill(
        AreaLevel, EBreakerMonsterRank::Boss, Params, BossArchetype);
    const float TrashSeconds = PromotedBaselineSecondsToKill(AreaLevel, EBreakerMonsterRank::Trash, Params);
    const float RankTimesArchetype =
        UBreakerMonsterChassisLibrary::GetRankHealthMultiplier(EBreakerMonsterRank::Boss, Params) * BossArchetype;

    AddInfo(FString::Printf(TEXT("BOSS BAND  area level %d: %.1fs (O18 target %.0f-%.0fs)"),
        AreaLevel, Seconds, BossSecondsFloor, BossSecondsCeiling));
    AddInfo(FString::Printf(
        TEXT("BOSS BAND  the fielded boss is rank x%.0f on archetype x%.2f = x%.2f over trash"),
        UBreakerMonsterChassisLibrary::GetRankHealthMultiplier(EBreakerMonsterRank::Boss, Params),
        BossArchetype, RankTimesArchetype));
    AddInfo(FString::Printf(
        TEXT("BOSS BAND  trash kills in %.2fs, so the derivation predicts %.2fs and measures %.2fs"),
        TrashSeconds, TrashSeconds * RankTimesArchetype, Seconds));

    // THE DERIVATION IDENTITY, asserted rather than restated in prose. The
    // spec's rule is that a rank multiplier IS the ratio of its TTK target to
    // trash's. Composed with the actor's archetype it reproduces the measured
    // kill exactly, which is what proves the rank table was never the error.
    TestTrue(*FString::Printf(
        TEXT("Boss TTK (%.4fs) is trash TTK (%.4fs) times rank x archetype (x%.4f)"),
        Seconds, TrashSeconds, RankTimesArchetype),
        FMath::IsNearlyEqual(Seconds, TrashSeconds * RankTimesArchetype, 0.01f));

    // A BASELINE build carries no multiplier band by construction — that is
    // what makes it the baseline — so this is weapon base against boss health
    // and nothing else. An optimized build's faster kill is correct behaviour
    // and is the other test's subject, not a failure of this one.
    TestTrue(*FString::Printf(TEXT("A baseline boss kill (%.1fs) is at least %.0fs"), Seconds, BossSecondsFloor),
        Seconds >= BossSecondsFloor);
    TestTrue(*FString::Printf(TEXT("A baseline boss kill (%.1fs) is at most %.0fs"), Seconds, BossSecondsCeiling),
        Seconds <= BossSecondsCeiling);
    return true;
}

#endif
