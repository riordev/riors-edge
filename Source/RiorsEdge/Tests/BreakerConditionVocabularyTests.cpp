#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Progression/BreakerBuildConditions.h"
#include "Progression/BreakerProgressionTypes.h"

// ---------------------------------------------------------------------------
// O30 hook-and-condition vocabulary — Docs/Design/Hook-And-Condition-Vocabulary.md
// ---------------------------------------------------------------------------
// The widening pass took EBreakerBuildCondition from 6 entries to 24 and
// EBreakerNodeStatTarget from 10 to 31, because 53 of the 97 authored tree nodes
// shipped with no stat effect and these two enums were named as the blocker.
//
// WHAT THIS FILE IS FOR, and why it is not a copy of
// RiorsEdge.Progression.BuildConditionMask: that test guards the STORAGE — that
// the widened uint32 mask sets and reads back every bit without aliasing — and
// it keeps doing so unchanged. This file guards the VOCABULARY: composition,
// the self/target partition, which entries can be evaluated today, and the
// register of which stat targets actually have somewhere to pay out. The mask
// test would still pass if every new condition were dead; these are the tests
// that notice.
//
// ALL WORLD-FREE. Everything below constructs plain structs. What that costs is
// recorded honestly at the foot of this file — there is a real class of thing
// these cannot cover, and pretending otherwise would be the same dishonesty the
// vocabulary document exists to end.

// ---------------------------------------------------------------------------
// Every condition, including the nineteen new ones, is independently storable
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerConditionVocabularyMaskTest,
    "RiorsEdge.Progression.ConditionVocabulary.Mask",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerConditionVocabularyMaskTest::RunTest(const FString& Parameters)
{
    // The same no-aliasing property the pre-O30 test pins, re-asserted over the
    // WIDENED set and reported by NAME rather than by index. The index form was
    // right when there were six entries; with twenty-four, "condition 17 aliases
    // condition 21" costs a trip to the header to read, and a failure message
    // that needs a lookup is a failure message people skim.
    for (int32 Index = 1; Index < FBreakerBuildConditionState::ConditionCount; ++Index)  // skip Always (0)
    {
        const EBreakerBuildCondition Condition = static_cast<EBreakerBuildCondition>(Index);
        const TCHAR* Name = FBreakerBuildConditionState::DescribeCondition(Condition);

        FBreakerBuildConditionState State;
        TestFalse(*FString::Printf(TEXT("%s starts inactive"), Name), State.IsActive(Condition));
        State.Set(Condition, true);
        TestTrue(*FString::Printf(TEXT("%s activates after Set(true)"), Name), State.IsActive(Condition));

        for (int32 Other = 1; Other < FBreakerBuildConditionState::ConditionCount; ++Other)
        {
            if (Other == Index) continue;
            TestFalse(
                *FString::Printf(TEXT("%s does not alias %s"), Name,
                    FBreakerBuildConditionState::DescribeCondition(static_cast<EBreakerBuildCondition>(Other))),
                State.IsActive(static_cast<EBreakerBuildCondition>(Other)));
        }

        State.Set(Condition, false);
        TestFalse(*FString::Printf(TEXT("%s deactivates after Set(false)"), Name), State.IsActive(Condition));
    }

    FBreakerBuildConditionState Empty;
    TestTrue(TEXT("Always reads active even on the empty state"), Empty.IsActive(EBreakerBuildCondition::Always));
    return true;
}

// ---------------------------------------------------------------------------
// The 32-bit ceiling, and the headroom left in it
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerConditionVocabularyCeilingTest,
    "RiorsEdge.Progression.ConditionVocabulary.Ceiling",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerConditionVocabularyCeilingTest::RunTest(const FString& Parameters)
{
    // Thirty-two is a HARD ceiling: the mask is a uint32 and 1u << 32 is
    // undefined behaviour, not a wrap. The static_assert in the header is the
    // real guard; this is the readable statement of the same fact plus the
    // thing the assert cannot express — how much room is left.
    TestTrue(TEXT("ConditionCount fits inside the 32-bit mask"),
        FBreakerBuildConditionState::ConditionCount <= 32);

    // Room to spare, deliberately. The widening pass spent eighteen bits at
    // once; a budget with two bits left would mean the NEXT axis (minions,
    // elements post-O38) forces a storage change rather than a design choice.
    // If this fails, the correct response is to read
    // Hook-And-Condition-Vocabulary.md §2.6 — which records what was already
    // left out to stay inside the budget — and cut before widening the type.
    const int32 Remaining = 32 - FBreakerBuildConditionState::ConditionCount;
    TestTrue(*FString::Printf(TEXT("at least 4 condition bits remain unspent (have %d)"), Remaining),
        Remaining >= 4);

    // The highest bit actually shifts. A ceiling that holds arithmetically but
    // breaks at the top bit would be the uint8 bug again one width up.
    FBreakerBuildConditionState Top;
    const EBreakerBuildCondition Highest =
        static_cast<EBreakerBuildCondition>(FBreakerBuildConditionState::ConditionCount - 1);
    Top.Set(Highest, true);
    TestTrue(TEXT("the highest condition sets a real bit"), Top.GetMask() != 0);
    TestTrue(TEXT("the highest condition reads back"), Top.IsActive(Highest));
    return true;
}

// ---------------------------------------------------------------------------
// Naming — every entry distinct, non-empty, and not the fallback
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerConditionVocabularyNameTest,
    "RiorsEdge.Progression.ConditionVocabulary.Names",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerConditionVocabularyNameTest::RunTest(const FString& Parameters)
{
    // DescribeCondition feeds the warn-once diagnostics, so a missing or
    // duplicated name turns the one signal this system has into "an effect
    // requires condition 'Unknown'". It also has no default case, which means a
    // new enum entry breaks the BUILD rather than silently reaching the
    // fallback — the mistake UI/BreakerMenu.cpp's two label switches made, where
    // `default: return TEXT("STAT")` meant every damage node on the board once
    // read "+4% STAT". This test covers the half a compiler cannot: that the
    // names are actually distinct.
    TSet<FString> Seen;
    for (int32 Index = 0; Index < FBreakerBuildConditionState::ConditionCount; ++Index)
    {
        const EBreakerBuildCondition Condition = static_cast<EBreakerBuildCondition>(Index);
        const FString Name = FBreakerBuildConditionState::DescribeCondition(Condition);

        TestFalse(*FString::Printf(TEXT("condition %d has a non-empty name"), Index), Name.IsEmpty());
        TestNotEqual(*FString::Printf(TEXT("condition %d is not the Unknown fallback"), Index), Name, FString(TEXT("Unknown")));
        TestFalse(*FString::Printf(TEXT("condition name '%s' is unique"), *Name), Seen.Contains(Name));
        Seen.Add(Name);
    }
    TestEqual(TEXT("every condition contributed a distinct name"), Seen.Num(), FBreakerBuildConditionState::ConditionCount);
    return true;
}

// ---------------------------------------------------------------------------
// The SELF / TARGET partition
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerConditionVocabularyPartitionTest,
    "RiorsEdge.Progression.ConditionVocabulary.Partition",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerConditionVocabularyPartitionTest::RunTest(const FString& Parameters)
{
    // IsTargetCondition is a >= comparison against the index of the first target
    // entry, which is what lets a newly appended target condition classify
    // itself with no second edit. The property that makes that safe is that the
    // target block is CONTIGUOUS AND LAST. If someone appends a self condition
    // after the target block, this fails — which is the intended alarm, because
    // the classification would otherwise silently mislabel it.
    bool bSeenTarget = false;
    for (int32 Index = 0; Index < FBreakerBuildConditionState::ConditionCount; ++Index)
    {
        const EBreakerBuildCondition Condition = static_cast<EBreakerBuildCondition>(Index);
        const bool bIsTarget = FBreakerBuildConditionState::IsTargetCondition(Condition);
        if (bIsTarget) bSeenTarget = true;
        else
        {
            TestFalse(
                *FString::Printf(TEXT("self condition '%s' does not appear after the target block -- append self conditions BEFORE TargetAiling"),
                    FBreakerBuildConditionState::DescribeCondition(Condition)),
                bSeenTarget);
        }
    }
    TestTrue(TEXT("at least one target condition exists"), bSeenTarget);

    // Always is a self condition and must never need target information, or
    // every unconditional line in the game would depend on the damage pipeline.
    TestFalse(TEXT("Always is not a target condition"),
        FBreakerBuildConditionState::IsTargetCondition(EBreakerBuildCondition::Always));

    // A target condition is never self-evaluable regardless of anything else:
    // EvaluateForActor takes one actor and this is a two-actor fact.
    for (int32 Index = 0; Index < FBreakerBuildConditionState::ConditionCount; ++Index)
    {
        const EBreakerBuildCondition Condition = static_cast<EBreakerBuildCondition>(Index);
        if (!FBreakerBuildConditionState::IsTargetCondition(Condition)) continue;
        TestFalse(
            *FString::Printf(TEXT("target condition '%s' is not self-evaluable"),
                FBreakerBuildConditionState::DescribeCondition(Condition)),
            FBreakerBuildConditionState::IsSelfEvaluable(Condition));
    }
    return true;
}

// ---------------------------------------------------------------------------
// Which conditions can be evaluated TODAY — the pin that makes progress visible
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerConditionVocabularyEvaluabilityTest,
    "RiorsEdge.Progression.ConditionVocabulary.Evaluability",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerConditionVocabularyEvaluabilityTest::RunTest(const FString& Parameters)
{
    // This test EXISTS TO BE UPDATED. It pins the exact set of conditions that
    // nothing can evaluate yet, so that the day a call site lands the pin fails
    // and someone has to move the entry deliberately rather than discovering
    // months later that half the vocabulary was never wired. A list that only
    // ever shrinks by hand is the point.
    const TArray<EBreakerBuildCondition> ExpectedUnevaluableSelf = {
        EBreakerBuildCondition::RecentlyKilled,
        EBreakerBuildCondition::RecentlyTookDamage,
        EBreakerBuildCondition::RecentlyCastAbility,
        EBreakerBuildCondition::RecentlyAppliedStatus,
    };

    for (int32 Index = 0; Index < FBreakerBuildConditionState::ConditionCount; ++Index)
    {
        const EBreakerBuildCondition Condition = static_cast<EBreakerBuildCondition>(Index);
        if (FBreakerBuildConditionState::IsTargetCondition(Condition)) continue;  // covered by the partition test

        const bool bExpectedEvaluable = !ExpectedUnevaluableSelf.Contains(Condition);
        TestEqual(
            *FString::Printf(TEXT("self condition '%s' evaluability matches the recorded state"),
                FBreakerBuildConditionState::DescribeCondition(Condition)),
            FBreakerBuildConditionState::IsSelfEvaluable(Condition), bExpectedEvaluable);
    }

    // The four unevaluable entries all belong to one family with one fix — a
    // recorder binding the delegates that already exist. Stated as a count so
    // that adding a fifth orphan of a DIFFERENT kind is visible.
    TestEqual(TEXT("exactly four self conditions await a recorder"), ExpectedUnevaluableSelf.Num(), 4);
    return true;
}

// ---------------------------------------------------------------------------
// Composition — the owner's first ruling, "conditions do compose yes"
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerConditionVocabularyCompositionTest,
    "RiorsEdge.Progression.ConditionVocabulary.Composition",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerConditionVocabularyCompositionTest::RunTest(const FString& Parameters)
{
    // Two SELF conditions, both evaluable today, so the composition property is
    // tested without the warn-once path confusing the result.
    FBreakerNodeEffect Effect;
    Effect.StatTarget = EBreakerNodeStatTarget::Damage;
    Effect.StatBucket = EBreakerNodeStatBucket::IncreasedPercent;
    Effect.ValuePerRank = 1.0f;  // O2 PLACEHOLDER — structure only; magnitude is not what is under test
    Effect.Condition = EBreakerBuildCondition::Airborne;
    Effect.AlsoRequires.Add(EBreakerBuildCondition::Aiming);

    // Neither.
    FBreakerBuildConditionState Neither;
    TestFalse(TEXT("a two-condition effect does not pay when neither holds"),
        Neither.SatisfiesAll(Effect.Condition, Effect.AlsoRequires));

    // The primary only. This is the case a single-condition system would have
    // paid out, and it is exactly the over-payment composition exists to stop.
    FBreakerBuildConditionState PrimaryOnly;
    PrimaryOnly.Set(EBreakerBuildCondition::Airborne, true);
    TestFalse(TEXT("a two-condition effect does not pay on the primary alone"),
        PrimaryOnly.SatisfiesAll(Effect.Condition, Effect.AlsoRequires));

    // The secondary only. Tested separately from the primary because an
    // implementation that only ever consulted AlsoRequires would pass the case
    // above and fail here.
    FBreakerBuildConditionState SecondaryOnly;
    SecondaryOnly.Set(EBreakerBuildCondition::Aiming, true);
    TestFalse(TEXT("a two-condition effect does not pay on the secondary alone"),
        SecondaryOnly.SatisfiesAll(Effect.Condition, Effect.AlsoRequires));

    // Both.
    FBreakerBuildConditionState Both;
    Both.Set(EBreakerBuildCondition::Airborne, true);
    Both.Set(EBreakerBuildCondition::Aiming, true);
    TestTrue(TEXT("a two-condition effect pays when both hold"),
        Both.SatisfiesAll(Effect.Condition, Effect.AlsoRequires));

    // Three conditions, to prove the array is genuinely iterated rather than
    // its first element special-cased.
    Effect.AlsoRequires.Add(EBreakerBuildCondition::Grounded);
    TestFalse(TEXT("a three-condition effect does not pay with two of three"),
        Both.SatisfiesAll(Effect.Condition, Effect.AlsoRequires));
    Both.Set(EBreakerBuildCondition::Grounded, true);
    TestTrue(TEXT("a three-condition effect pays with all three"),
        Both.SatisfiesAll(Effect.Condition, Effect.AlsoRequires));

    // An unconditional effect is unchanged by any of this — the property every
    // pre-existing authored row depends on.
    FBreakerNodeEffect Unconditional;
    TestFalse(TEXT("a default effect reports itself unconditional"), Unconditional.IsConditional());
    TestTrue(TEXT("a default effect pays on the empty state"),
        Neither.SatisfiesAll(Unconditional.Condition, Unconditional.AlsoRequires));

    // Always plus a composed condition is still conditional. A cheap mistake to
    // make in IsConditional, and it would mis-report the whole conditional
    // damage display.
    FBreakerNodeEffect AlwaysPlus;
    AlwaysPlus.Condition = EBreakerBuildCondition::Always;
    AlwaysPlus.AlsoRequires.Add(EBreakerBuildCondition::Sliding);
    TestTrue(TEXT("Always plus a composed condition is still conditional"), AlwaysPlus.IsConditional());
    TestFalse(TEXT("Always plus an unmet composed condition does not pay"),
        Neither.SatisfiesAll(AlwaysPlus.Condition, AlwaysPlus.AlsoRequires));
    return true;
}

// ---------------------------------------------------------------------------
// Target state — supplied, absent, and the difference between them
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerConditionVocabularyTargetStateTest,
    "RiorsEdge.Progression.ConditionVocabulary.TargetState",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerConditionVocabularyTargetStateTest::RunTest(const FString& Parameters)
{
    // "We asked and the answer was no" must not read the same as "we never
    // asked". The first is an ordinary miss against an enemy carrying no
    // statuses; the second is a wiring bug, and only the second should warn.
    FBreakerBuildConditionState NeverAsked;
    TestFalse(TEXT("a fresh state has no target information"), NeverAsked.HasTargetState());

    // SupplyTargetState with a null target still counts as having asked. A DoT
    // ticking after its victim was destroyed hits this path, and it must
    // produce an ordinary empty result rather than a warning storm.
    FBreakerBuildConditionState AskedAboutNothing;
    AskedAboutNothing.SupplyTargetState(nullptr, nullptr);
    TestTrue(TEXT("supplying a null target still records that target state was asked for"),
        AskedAboutNothing.HasTargetState());
    TestFalse(TEXT("a null target satisfies no target condition"),
        AskedAboutNothing.SatisfiesOne(EBreakerBuildCondition::TargetBleeding));

    // All() is the tooltip's hypothetical: every condition live, target half
    // included and marked as answered. Without the flag it would trip the
    // warn-once path on a caller that is not misconfigured at all.
    const FBreakerBuildConditionState Everything = FBreakerBuildConditionState::All();
    TestTrue(TEXT("All() counts as having target state"), Everything.HasTargetState());
    for (int32 Index = 0; Index < FBreakerBuildConditionState::ConditionCount; ++Index)
    {
        const EBreakerBuildCondition Condition = static_cast<EBreakerBuildCondition>(Index);
        TestTrue(
            *FString::Printf(TEXT("All() satisfies '%s'"), FBreakerBuildConditionState::DescribeCondition(Condition)),
            Everything.SatisfiesOne(Condition));
    }

    // The subtle one, and the reason SatisfiesOne consults the bit BEFORE it
    // consults evaluability: All() sets the Recently* family, which nothing can
    // evaluate yet. If the order were reversed they would read false here, and
    // every tooltip's "potential" figure would silently understate itself.
    TestTrue(TEXT("a set-but-not-yet-evaluable condition still satisfies"),
        Everything.SatisfiesOne(EBreakerBuildCondition::RecentlyKilled));

    // RequiresTargetState routes an effect to the cheap source-side path or the
    // expensive target-side one, so it has to see a target condition wherever it
    // sits in the requirement.
    FBreakerNodeEffect SelfOnly;
    SelfOnly.Condition = EBreakerBuildCondition::Airborne;
    SelfOnly.AlsoRequires.Add(EBreakerBuildCondition::Aiming);
    TestFalse(TEXT("an all-self effect needs no target state"), SelfOnly.RequiresTargetState());

    FBreakerNodeEffect TargetPrimary;
    TargetPrimary.Condition = EBreakerBuildCondition::TargetBleeding;
    TestTrue(TEXT("a target primary condition needs target state"), TargetPrimary.RequiresTargetState());

    FBreakerNodeEffect TargetComposed;
    TargetComposed.Condition = EBreakerBuildCondition::Airborne;
    TargetComposed.AlsoRequires.Add(EBreakerBuildCondition::TargetLowHealth);
    TestTrue(TEXT("a target condition buried in AlsoRequires still needs target state"),
        TargetComposed.RequiresTargetState());
    return true;
}

// ---------------------------------------------------------------------------
// The stat-target register — which of the widened entries can actually pay
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerConditionVocabularyStatTargetTest,
    "RiorsEdge.Progression.ConditionVocabulary.StatTargets",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerConditionVocabularyStatTargetTest::RunTest(const FString& Parameters)
{
    // Adding an enum entry does not make a node pay. AggregateStats needs a lane
    // per stat target, and twenty-one entries arrived at once without one. This
    // pins how many are wired so the number can only move deliberately, and so
    // the roadmap in Hook-And-Condition-Vocabulary.md §6 has a progress bar
    // rather than a promise.
    constexpr int32 TargetCount = static_cast<int32>(EBreakerNodeStatTarget::Count);
    int32 Wired = 0;
    for (int32 Index = 0; Index < TargetCount; ++Index)
    {
        if (BreakerStatTargetHasAggregationLane(static_cast<EBreakerNodeStatTarget>(Index))) ++Wired;
    }

    // 19: the ten that predate the widening pass, plus the five wired on
    // 2026-08-15 — AbilityCost, MaxClassResource, ClassResourceRegen, FireRate
    // and Armor. Each of those five already had an aggregated attribute and
    // gear already bid into it; only the node-side line was missing, so each
    // cost exactly one line in AggregateStats.
    //
    // Plus the four wired on 2026-08-16 under THE OWNER'S SWIFT RULING of the
    // same date ("swifts identity should be based around multishot, pierce,
    // chain, ricochet ... manipulation of projectiles with your momentum"):
    // ProjectileCount, Pierce, and the two appended in that pass, ChainCount
    // and RicochetCount. These four are Flat mechanic-count lanes landing on
    // FBreakerNodeStats and read by UBreakerWeaponComponent::GetShotChannels;
    // they do not route through the aggregated attribute set because no
    // aggregated attribute exists for them and gear does not bid yet — see
    // the register comment in BreakerProgressionTypes.h.
    //
    // DashCooldown was on the roadmap as a sixth and is NOT wired, because the
    // roadmap was wrong about it: EBreakerAggregatedAttribute has no dash entry
    // at all. Dash cooldown lives on FBreakerEquipmentStats and the movement
    // component reads it directly, so a node has nowhere to bid. Giving it one
    // means moving that read onto the aggregator first, or the two layers
    // multiply instead of sharing a bucket.
    //
    // Plus the four wired on 2026-08-16 in the loop-valve / ability-geometry
    // pass: ClassResourceDecay (composed to a decay-rate multiplier and
    // delivered to the Momentum/Grit components through their PushLoopOverride
    // seam by UBreakerProgressionComponent::PushLoopValveOverrides), and
    // AbilityArea / AbilityDuration / AbilityCooldown (composed onto
    // FBreakerNodeStats and read through the UBreakerGameplayAbility accessor
    // seam — Cleave's arc/range, Rot's radius/duration, and ApplyCooldown's
    // divisor). All four are single-bidder lanes like the projectile
    // channels: no aggregated attribute exists and gear does not bid.
    //
    // This number goes up in the same commit as the lane, never before it.
    TestEqual(TEXT("stat targets with an aggregation lane today"), Wired, 23);
    TestTrue(*FString::Printf(TEXT("%d stat targets still await a lane"), TargetCount - Wired), TargetCount > Wired);

    // The pre-existing ten specifically, so that a future reshuffle cannot quiet
    // this test by wiring a new entry while breaking an old one.
    const TArray<EBreakerNodeStatTarget> MustBeWired = {
        EBreakerNodeStatTarget::CriticalChance, EBreakerNodeStatTarget::CriticalDamage,
        EBreakerNodeStatTarget::MoveSpeed,      EBreakerNodeStatTarget::SlideSpeed,
        EBreakerNodeStatTarget::AirControl,     EBreakerNodeStatTarget::DodgeChance,
        EBreakerNodeStatTarget::BlockChance,    EBreakerNodeStatTarget::Health,
        EBreakerNodeStatTarget::DamageOverTime, EBreakerNodeStatTarget::Damage,
    };
    for (const EBreakerNodeStatTarget Target : MustBeWired)
    {
        TestTrue(*FString::Printf(TEXT("pre-existing stat target %d keeps its lane"), static_cast<int32>(Target)),
            BreakerStatTargetHasAggregationLane(Target));
    }

    // Damage's partitions must not silently claim Damage's lane. A node
    // authoring AbilityDamage today has to be visibly unpaid, not quietly
    // folded into the general damage bucket, or the pilot-one-axis-end-to-end
    // plan cannot tell whether the axis works.
    TestFalse(TEXT("AbilityDamage does not borrow Damage's lane"),
        BreakerStatTargetHasAggregationLane(EBreakerNodeStatTarget::AbilityDamage));
    TestFalse(TEXT("WeaponDamage does not borrow Damage's lane"),
        BreakerStatTargetHasAggregationLane(EBreakerNodeStatTarget::WeaponDamage));
    TestFalse(TEXT("MeleeDamage does not borrow Damage's lane"),
        BreakerStatTargetHasAggregationLane(EBreakerNodeStatTarget::MeleeDamage));

    // The enum is serialized by value into Data Assets, so the ten original
    // entries must keep their numbers. Pinning the first and last of the
    // original block catches an insertion anywhere inside it.
    TestEqual(TEXT("CriticalChance is still value 0"), static_cast<int32>(EBreakerNodeStatTarget::CriticalChance), 0);
    TestEqual(TEXT("Damage is still value 9"), static_cast<int32>(EBreakerNodeStatTarget::Damage), 9);

    // Same rule, same reason, for the condition enum — it is serialized into
    // affix rows in Items/BreakerAffixLibrary.cpp as well as node effects.
    TestEqual(TEXT("Always is still condition 0"), static_cast<int32>(EBreakerBuildCondition::Always), 0);
    TestEqual(TEXT("RecentlyDashed is still condition 5"), static_cast<int32>(EBreakerBuildCondition::RecentlyDashed), 5);
    return true;
}

// ---------------------------------------------------------------------------
// WHAT THESE TESTS DO NOT COVER, stated plainly
// ---------------------------------------------------------------------------
// Everything above is world-free, which buys speed and determinism and costs
// exactly one thing: none of it proves that a condition reads the RIGHT LIVE
// STATE. Specifically, not covered here and not coverable without a world:
//
//  1. EvaluateForActor's reads. That Airborne tracks IsFalling(), that
//     Stationary tracks GetHorizontalSpeed() against its threshold, that
//     Grounded is the same frame's negation of Airborne, that Aiming tracks the
//     weapon component, and that the health/resource fractions come from the
//     right attribute — all need an actor with components on it. The nearest
//     existing coverage is RiorsEdge.Items.Rules, which drives the ORIGINAL
//     movement conditions through a fixture; the five new self conditions have
//     no equivalent yet and should get one in whichever lane owns a character
//     fixture.
//  2. SupplyTargetState's status and rank reads. PARTLY ANSWERED since Stage 6:
//     the half now HAS a production caller (UBreakerCombatComponent::
//     ReceiveDamage) and BreakerTargetRiderTests.cpp drives the two-actor path
//     end to end through TargetAtCloseRange and the "asked, answer no" case.
//     Still uncovered live: a status-component read with real applied statuses
//     and an ABreakerEnemy rank, which need world time.
//  3. That the aggregator honours composition — ANSWERED: AggregateStats now
//     calls SatisfiesAll(Effect.Condition, Effect.AlsoRequires) (Stage 1), and
//     since Stage 6 it routes target-requiring effects to the rider table
//     instead of warning about them.
//  4. That the warn-once path actually fires. The TSet is function-local and
//     process-wide by design, so a test asserting "this warned" would pass or
//     fail depending on which test ran first. Verified by inspection instead,
//     and the same choice BreakerProgressionAuditTests.cpp item 2 made for the
//     structurally identical dropped-More warning.
//  5. Anything about magnitudes. O2 freezes values; nothing here asserts that a
//     conditional line is worth any particular amount, and the one number in
//     this file is marked O2 PLACEHOLDER and is not what is under test.

#endif  // WITH_DEV_AUTOMATION_TESTS
