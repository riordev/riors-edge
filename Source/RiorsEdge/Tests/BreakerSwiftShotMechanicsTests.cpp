#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include <initializer_list>
#include "Classes/BreakerMomentumComponent.h"
#include "GameFramework/Actor.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionLibrary.h"
#include "Progression/BreakerProgressionNode.h"
#include "Progression/BreakerProgressionTree.h"
#include "Progression/BreakerProgressionTypes.h"
#include "Weapons/BreakerWeaponComponent.h"
#include "Weapons/BreakerWeaponMath.h"

// ---------------------------------------------------------------------------
// Swift projectile channels — the owner's 2026-08-16 ruling, verbatim: "swifts
// identity should be based around multishot, pierce, chain, ricochet,
// movement, manipulation of projectiles with your momentum type of deal".
//
// The suite is world-free by construction (no test in this project spawns a
// world), so the mechanics are split exactly along that line: the DECISION
// math — multishot accumulation, the pierce damage ladder, nearest-target
// selection, the momentum coupling table, the salted seed streams — lives in
// FBreakerWeaponMath / static members and is pinned here; the trace loop that
// feeds it (UBreakerWeaponComponent::ResolvePelletImpacts) is the thin
// world-touching shell and is exercised by playtest, like every other trace
// in this project. What that costs is stated at the foot of this file.
// ---------------------------------------------------------------------------

namespace BreakerSwiftShotTest
{
    // Prefixed: identical anonymous-namespace helper names in two translation
    // units have collided under this project's unity build before.
    static UBreakerProgressionNode* BreakerMakeChannelNode(FName NodeId, std::initializer_list<TPair<EBreakerNodeStatTarget, float>> FlatEffects)
    {
        UBreakerProgressionNode* Node = NewObject<UBreakerProgressionNode>();
        Node->NodeId = NodeId;
        Node->MaxRank = 2;
        for (const TPair<EBreakerNodeStatTarget, float>& Pair : FlatEffects)
        {
            FBreakerNodeEffect Effect;
            Effect.StatTarget = Pair.Key;
            Effect.StatBucket = EBreakerNodeStatBucket::Flat;
            Effect.ValuePerRank = Pair.Value;
            Node->Effects.Add(Effect);
        }
        return Node;
    }
}

// ---------------------------------------------------------------------------
// Multishot accumulation: fractional projectiles bank across pulls
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerSwiftMultishotAccumulationTest,
    "RiorsEdge.Weapons.SwiftChannels.MultishotAccumulation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerSwiftMultishotAccumulationTest::RunTest(const FString& Parameters)
{
    // The zero channel is the whole non-Swift population: it must drain
    // nothing and touch nothing.
    float Accumulator = 0.25f;
    TestEqual(TEXT("a zero channel fires no extra pellet"), FBreakerWeaponMath::ConsumeMultishot(0.0f, Accumulator), 0);
    TestEqual(TEXT("a zero channel leaves the bank untouched"), Accumulator, 0.25f);
    TestEqual(TEXT("a negative channel fires nothing"), FBreakerWeaponMath::ConsumeMultishot(-1.0f, Accumulator), 0);

    // A whole +1 is a pellet every pull, forever, with nothing banked.
    Accumulator = 0.0f;
    for (int32 Pull = 0; Pull < 5; ++Pull)
    {
        TestEqual(TEXT("+1.0 fires exactly one extra pellet every pull"), FBreakerWeaponMath::ConsumeMultishot(1.0f, Accumulator), 1);
    }
    TestEqual(TEXT("+1.0 banks nothing"), Accumulator, 0.0f);

    // +0.5 is the perceptibility case: a second pellet every OTHER shot — a
    // rhythm the player can hear — never a lost rounding.
    Accumulator = 0.0f;
    int32 Fired = 0;
    const int32 ExpectedPattern[6] = { 0, 1, 0, 1, 0, 1 };
    for (int32 Pull = 0; Pull < 6; ++Pull)
    {
        const int32 Extra = FBreakerWeaponMath::ConsumeMultishot(0.5f, Accumulator);
        TestEqual(*FString::Printf(TEXT("+0.5 alternates (pull %d)"), Pull), Extra, ExpectedPattern[Pull]);
        Fired += Extra;
    }
    TestEqual(TEXT("+0.5 over six pulls pays exactly three pellets"), Fired, 3);

    // A mixed channel pays its whole part every pull and banks the fraction.
    Accumulator = 0.0f;
    TestEqual(TEXT("+2.5 pays two immediately"), FBreakerWeaponMath::ConsumeMultishot(2.5f, Accumulator), 2);
    TestEqual(TEXT("+2.5 pays three on the second pull"), FBreakerWeaponMath::ConsumeMultishot(2.5f, Accumulator), 3);
    TestEqual(TEXT("the bank never holds a whole pellet"), Accumulator, 0.0f);
    return true;
}

// ---------------------------------------------------------------------------
// The pierce damage ladder, and Overpenetration's kill exception
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerSwiftPierceLadderTest,
    "RiorsEdge.Weapons.SwiftChannels.PierceLadder",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerSwiftPierceLadderTest::RunTest(const FString& Parameters)
{
    // A shot piercing a line of enemies pays the authored falloff per
    // penetration: 1.0, 0.7, 0.49 at the component's O2 placeholder of 0.70.
    float Multiplier = 1.0f;
    Multiplier = FBreakerWeaponMath::NextPierceMultiplier(Multiplier, 0.70f, false, false);
    TestEqual(TEXT("the second target pays the falloff once"), Multiplier, 0.70f, 0.0001f);
    Multiplier = FBreakerWeaponMath::NextPierceMultiplier(Multiplier, 0.70f, false, false);
    TestEqual(TEXT("the third target pays it twice"), Multiplier, 0.49f, 0.0001f);

    // Overpenetration (Class-Kits §1.5 M10): a KILLING hit skips the step —
    // the shot carries on at full remaining damage.
    Multiplier = 1.0f;
    Multiplier = FBreakerWeaponMath::NextPierceMultiplier(Multiplier, 0.70f, /*killed*/ true, /*overpen*/ true);
    TestEqual(TEXT("Overpenetration carries a killing hit through at full damage"), Multiplier, 1.0f, 0.0001f);
    // ...but only a killing hit. A survivor still costs the step.
    Multiplier = FBreakerWeaponMath::NextPierceMultiplier(Multiplier, 0.70f, /*killed*/ false, /*overpen*/ true);
    TestEqual(TEXT("Overpenetration does not waive the falloff for survivors"), Multiplier, 0.70f, 0.0001f);
    // And without the node, a kill is just a hit.
    Multiplier = FBreakerWeaponMath::NextPierceMultiplier(1.0f, 0.70f, /*killed*/ true, /*overpen*/ false);
    TestEqual(TEXT("without the node a kill still pays the falloff"), Multiplier, 0.70f, 0.0001f);

    // Degenerate falloffs clamp rather than invert.
    TestEqual(TEXT("a falloff above 1 clamps to 1"), FBreakerWeaponMath::NextPierceMultiplier(1.0f, 1.5f, false, false), 1.0f);
    TestEqual(TEXT("a negative falloff clamps to 0"), FBreakerWeaponMath::NextPierceMultiplier(1.0f, -0.5f, false, false), 0.0f);
    return true;
}

// ---------------------------------------------------------------------------
// Nearest-target selection: the pure half of chain arcs and ricochet seeks
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerSwiftNearestTargetTest,
    "RiorsEdge.Weapons.SwiftChannels.NearestTarget",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerSwiftNearestTargetTest::RunTest(const FString& Parameters)
{
    const FVector Origin = FVector::ZeroVector;
    TArray<FVector> Candidates;
    TestEqual(TEXT("no candidates selects nobody"), FBreakerWeaponMath::SelectNearestTarget(Origin, Candidates, 1200.0f), (int32)INDEX_NONE);

    // Three enemies at 10 m, 4 m and 8 m: the arc picks the 4 m one.
    Candidates = { FVector(1000, 0, 0), FVector(0, 400, 0), FVector(0, 0, 800) };
    TestEqual(TEXT("the nearest legal target wins"), FBreakerWeaponMath::SelectNearestTarget(Origin, Candidates, 1200.0f), 1);

    // The radius is a hard edge: with everything beyond it, nobody is picked
    // — a chain that reached across the arena would be a different mechanic.
    TestEqual(TEXT("targets beyond the radius are invisible"), FBreakerWeaponMath::SelectNearestTarget(Origin, Candidates, 300.0f), (int32)INDEX_NONE);

    // Determinism on the exact tie: the lower index wins, always, so the same
    // world state always chains the same way on the server.
    Candidates = { FVector(500, 0, 0), FVector(0, 500, 0) };
    TestEqual(TEXT("a distance tie breaks to the lower index"), FBreakerWeaponMath::SelectNearestTarget(Origin, Candidates, 1200.0f), 0);
    return true;
}

// ---------------------------------------------------------------------------
// The momentum coupling table — Momentum STATE manipulates projectiles
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerSwiftMomentumCouplingTest,
    "RiorsEdge.Weapons.SwiftChannels.MomentumCoupling",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerSwiftMomentumCouplingTest::RunTest(const FString& Parameters)
{
    // Settled grants nothing, in any posture: the coupling is Momentum
    // manipulating projectiles, and an empty bar has nothing to manipulate.
    for (const bool bAirborne : { false, true })
    {
        for (const bool bSliding : { false, true })
        {
            const FBreakerShotChannels Settled = UBreakerWeaponComponent::MomentumChannelBonus(EBreakerMomentumState::Settled, bAirborne, bSliding);
            TestTrue(TEXT("Settled grants no channel in any posture"), Settled.IsIdentity());
        }
    }

    // Running: rounds punch through.
    const FBreakerShotChannels Running = UBreakerWeaponComponent::MomentumChannelBonus(EBreakerMomentumState::Running, false, false);
    TestEqual(TEXT("Running grants +1 pierce"), Running.PierceCount, 1);
    TestEqual(TEXT("Running grants no chain"), Running.ChainCount, 0);
    TestEqual(TEXT("Running on the ground grants no extra pellet"), Running.AdditionalProjectiles, 0.0f);

    // Redline: everything Running has, and hits arc onward.
    const FBreakerShotChannels Redline = UBreakerWeaponComponent::MomentumChannelBonus(EBreakerMomentumState::Redline, false, false);
    TestEqual(TEXT("Redline keeps Running's pierce"), Redline.PierceCount, 1);
    TestEqual(TEXT("Redline adds +1 chain"), Redline.ChainCount, 1);

    // Airborne and sliding modulate multishot — but only with a bar to spend.
    // Airborne halved 1.0 -> 0.5 (owner ruling 2026-08-16): base coupling is a
    // second pellet every other shot; the full airborne double is now bought
    // back through Swift.Kinetic.AirWork's +0.5 airborne ProjectileCount line.
    const FBreakerShotChannels AirborneRunning = UBreakerWeaponComponent::MomentumChannelBonus(EBreakerMomentumState::Running, true, false);
    TestEqual(TEXT("airborne at Running is half a pellet (owner ruling 2026-08-16; Air Work restores the double)"), AirborneRunning.AdditionalProjectiles, 0.5f);
    const FBreakerShotChannels SlidingRunning = UBreakerWeaponComponent::MomentumChannelBonus(EBreakerMomentumState::Running, false, true);
    TestEqual(TEXT("sliding at Running is half a pellet (every other shot)"), SlidingRunning.AdditionalProjectiles, 0.5f);
    // Airborne beats sliding when both are somehow true — one bonus, not two.
    const FBreakerShotChannels Both = UBreakerWeaponComponent::MomentumChannelBonus(EBreakerMomentumState::Redline, true, true);
    TestEqual(TEXT("airborne and sliding do not stack"), Both.AdditionalProjectiles, 0.5f);

    // The buy-up itself: Air Work authors the other +0.5 while airborne, so
    // coupling + node restore the full doubled shot. Aggregated with the
    // Airborne condition live it pays; with the default (grounded) state it
    // does not — the restoration is airtime-gated exactly like the coupling.
    {
        TArray<const UBreakerProgressionNode*> KineticNodes;
        for (const UBreakerProgressionNode* Node : UBreakerProgressionLibrary::GetSwiftKineticTree()->Nodes) KineticNodes.Add(Node);
        TArray<FBreakerNodeRank> Ranks;
        Ranks.Add({ TEXT("Swift.Kinetic.AirWork"), 1 });

        FBreakerBuildConditionState AirborneState;
        AirborneState.Set(EBreakerBuildCondition::Airborne, true);
        const FBreakerNodeStats AirborneStats = UBreakerProgressionComponent::AggregateStats(KineticNodes, Ranks, nullptr, AirborneState);
        TestEqual(TEXT("Air Work restores +0.5 projectile while airborne (owner ruling 2026-08-16)"),
            AirborneStats.BonusProjectileCount, 0.5f, 0.0001f);
        TestEqual(TEXT("coupling plus Air Work is the full doubled airborne shot"),
            AirborneStats.BonusProjectileCount + AirborneRunning.AdditionalProjectiles, 1.0f, 0.0001f);

        const FBreakerNodeStats GroundedStats = UBreakerProgressionComponent::AggregateStats(KineticNodes, Ranks);
        TestEqual(TEXT("Air Work's restoration pays nothing on the ground"), GroundedStats.BonusProjectileCount, 0.0f, 0.0001f);
    }

    // The state bands themselves are the momentum component's pinned rule;
    // re-asserted at the boundary here so the coupling and the loop can never
    // silently disagree about where Running begins.
    TestTrue(TEXT("a third of the bar is Running"), UBreakerMomentumComponent::StateForFraction(0.34f) == EBreakerMomentumState::Running);
    TestTrue(TEXT("two thirds of the bar is Redline"), UBreakerMomentumComponent::StateForFraction(0.67f) == EBreakerMomentumState::Redline);
    return true;
}

// ---------------------------------------------------------------------------
// Determinism: the channels ride salted sub-streams, never the primary one
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerSwiftChannelDeterminismTest,
    "RiorsEdge.Weapons.SwiftChannels.Determinism",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerSwiftChannelDeterminismTest::RunTest(const FString& Parameters)
{
    // Same owner, same shot, same channel, same index: the same draw, every
    // time — the server can replay a shot and land every pellet where it did.
    const int32 SeedA = FBreakerWeaponMath::SecondaryShotSeed(0xABCD1234u, 17, 0x3B0057A0u, 0);
    const int32 SeedB = FBreakerWeaponMath::SecondaryShotSeed(0xABCD1234u, 17, 0x3B0057A0u, 0);
    TestEqual(TEXT("a secondary seed is reproducible"), SeedA, SeedB);

    // Different indices and different salts are different streams.
    TestNotEqual(TEXT("sibling pellets draw differently"), SeedA, FBreakerWeaponMath::SecondaryShotSeed(0xABCD1234u, 17, 0x3B0057A0u, 1));
    TestNotEqual(TEXT("channels draw from distinct streams"), SeedA, FBreakerWeaponMath::SecondaryShotSeed(0xABCD1234u, 17, 0xC4A15000u, 0));

    // The structural half of "existing sequences don't move": the primary
    // spread draw is seeded by the raw sequence value, and extra pellets never
    // advance that counter (pinned by inspection in FireOnce — the ternary on
    // PelletSeed). What CAN be pinned here is that the same seed reproduces
    // the same cone draw, so a replayed sequence is bit-identical.
    const FVector Forward = FVector::ForwardVector;
    const FVector DrawA = FBreakerWeaponMath::ApplyConeSpread(Forward, 3.0f, 42);
    const FVector DrawB = FBreakerWeaponMath::ApplyConeSpread(Forward, 3.0f, 42);
    TestTrue(TEXT("the same seed draws the same pellet direction"), DrawA.Equals(DrawB));
    TestFalse(TEXT("a different seed draws a different direction"), DrawA.Equals(FBreakerWeaponMath::ApplyConeSpread(Forward, 3.0f, 43)));
    return true;
}

// ---------------------------------------------------------------------------
// The lanes: register honesty and the aggregation that feeds the weapon
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerSwiftChannelLanesTest,
    "RiorsEdge.Weapons.SwiftChannels.Lanes",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerSwiftChannelLanesTest::RunTest(const FString& Parameters)
{
    using namespace BreakerSwiftShotTest;

    // The register flips in the same commit as the lane — this is that
    // commit, and these four are the lanes. (The overall count pin lives in
    // BreakerConditionVocabularyTests and moved 15 -> 19 with this pass.)
    TestTrue(TEXT("ProjectileCount has its lane"), BreakerStatTargetHasAggregationLane(EBreakerNodeStatTarget::ProjectileCount));
    TestTrue(TEXT("Pierce has its lane"), BreakerStatTargetHasAggregationLane(EBreakerNodeStatTarget::Pierce));
    TestTrue(TEXT("ChainCount has its lane"), BreakerStatTargetHasAggregationLane(EBreakerNodeStatTarget::ChainCount));
    TestTrue(TEXT("RicochetCount has its lane"), BreakerStatTargetHasAggregationLane(EBreakerNodeStatTarget::RicochetCount));
    // Appending two entries must not have moved the serialized ordinals ahead
    // of them; the vocabulary test pins the originals, this pins the tail.
    TestEqual(TEXT("ChainCount appended after StatusChance"),
        static_cast<int32>(EBreakerNodeStatTarget::ChainCount), static_cast<int32>(EBreakerNodeStatTarget::StatusChance) + 1);
    // RicochetCount's ordinal is pinned ABSOLUTELY now rather than as
    // last-before-Count: DashDistance appended behind it (O139), which is
    // exactly the legal move this pin exists to distinguish from an
    // insertion — the originals must not shift.
    TestEqual(TEXT("RicochetCount appended after ChainCount"),
        static_cast<int32>(EBreakerNodeStatTarget::RicochetCount), static_cast<int32>(EBreakerNodeStatTarget::ChainCount) + 1);
    TestEqual(TEXT("DashDistance is the last entry before Count (O139)"),
        static_cast<int32>(EBreakerNodeStatTarget::DashDistance), static_cast<int32>(EBreakerNodeStatTarget::Count) - 1);

    // And the lanes actually pay: a rank-2 node authoring all four Flat lines
    // lands on the FBreakerNodeStats fields the weapon reads.
    TArray<const UBreakerProgressionNode*> Nodes;
    Nodes.Add(BreakerMakeChannelNode(TEXT("Test.Channels"), {
        TPair<EBreakerNodeStatTarget, float>(EBreakerNodeStatTarget::ProjectileCount, 0.5f),
        TPair<EBreakerNodeStatTarget, float>(EBreakerNodeStatTarget::Pierce, 1.0f),
        TPair<EBreakerNodeStatTarget, float>(EBreakerNodeStatTarget::ChainCount, 1.0f),
        TPair<EBreakerNodeStatTarget, float>(EBreakerNodeStatTarget::RicochetCount, 1.0f) }));
    TArray<FBreakerNodeRank> Ranks;
    FBreakerNodeRank Rank;
    Rank.NodeId = TEXT("Test.Channels");
    Rank.Rank = 2;
    Ranks.Add(Rank);

    const FBreakerNodeStats Stats = UBreakerProgressionComponent::AggregateStats(Nodes, Ranks);
    TestEqual(TEXT("the projectile lane sums per rank, fraction intact"), Stats.BonusProjectileCount, 1.0f);
    TestEqual(TEXT("the pierce lane sums per rank"), Stats.BonusPierceCount, 2.0f);
    TestEqual(TEXT("the chain lane sums per rank"), Stats.BonusChainCount, 2.0f);
    TestEqual(TEXT("the ricochet lane sums per rank"), Stats.BonusRicochetCount, 2.0f);

    // A build with no channel nodes reports zeros — the whole non-Swift
    // population, and every Swift from before this pass, is unchanged.
    const FBreakerNodeStats Empty = UBreakerProgressionComponent::AggregateStats({}, {});
    TestEqual(TEXT("no nodes, no projectiles"), Empty.BonusProjectileCount, 0.0f);
    TestEqual(TEXT("no nodes, no pierce"), Empty.BonusPierceCount, 0.0f);
    TestEqual(TEXT("no nodes, no chain"), Empty.BonusChainCount, 0.0f);
    TestEqual(TEXT("no nodes, no ricochet"), Empty.BonusRicochetCount, 0.0f);
    return true;
}

// ---------------------------------------------------------------------------
// The composed channels on the component: identity, pushes, and the gate
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerSwiftChannelCompositionTest,
    "RiorsEdge.Weapons.SwiftChannels.Composition",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerSwiftChannelCompositionTest::RunTest(const FString& Parameters)
{
    // OFF BY DEFAULT is the load-bearing property: a bare weapon — no nodes,
    // no windows, no momentum component — composes the identity, so nothing
    // changes for any build until something explicitly grants a count.
    AActor* Owner = NewObject<AActor>();
    UBreakerWeaponComponent* Weapon = NewObject<UBreakerWeaponComponent>(Owner);
    TestTrue(TEXT("a bare weapon fires the authored shot exactly"), Weapon->GetShotChannels().IsIdentity());

    // A keyed push (Sidearm Rig's +1 Pierce is the first real caller) is live
    // while held and gone on the pop, with nothing left behind.
    Weapon->PushShotChannelBonus(TEXT("TestRig"), 0.0f, 1, 0, 0);
    TestEqual(TEXT("a pushed pierce bonus is live"), Weapon->GetShotChannels().PierceCount, 1);
    TestTrue(TEXT("the push leaves multishot alone"), Weapon->GetShotChannels().AdditionalProjectiles == 0.0f);

    // Re-pushing the same key replaces rather than stacks — a re-cast window
    // must refresh, not compound.
    Weapon->PushShotChannelBonus(TEXT("TestRig"), 0.0f, 2, 1, 0);
    TestEqual(TEXT("re-pushing a key replaces its pierce"), Weapon->GetShotChannels().PierceCount, 2);
    TestEqual(TEXT("re-pushing a key replaces its chain"), Weapon->GetShotChannels().ChainCount, 1);

    // Two keys compose additively.
    Weapon->PushShotChannelBonus(TEXT("TestOther"), 1.0f, 1, 0, 1);
    TestEqual(TEXT("two windows sum their pierce"), Weapon->GetShotChannels().PierceCount, 3);
    TestEqual(TEXT("two windows sum their ricochet"), Weapon->GetShotChannels().RicochetCount, 1);

    Weapon->PopShotChannelBonus(TEXT("TestRig"));
    Weapon->PopShotChannelBonus(TEXT("TestOther"));
    TestTrue(TEXT("popping every key restores the identity exactly"), Weapon->GetShotChannels().IsIdentity());

    // SWIFT-ONLY GATING, the half a world-free rig can prove: a momentum
    // component that is INERT for its owner (no progression, so bIsSwift is
    // false — exactly a Caster's situation after a dev class swap) adds
    // nothing, whatever its state. The state table itself is pinned in the
    // MomentumCoupling test; IsActiveForOwner's Swift lock is the momentum
    // component's own tested contract.
    AActor* CasterOwner = NewObject<AActor>();
    UBreakerWeaponComponent* CasterWeapon = NewObject<UBreakerWeaponComponent>(CasterOwner);
    UBreakerMomentumComponent* InertMomentum = NewObject<UBreakerMomentumComponent>(CasterOwner);
    CasterOwner->AddOwnedComponent(InertMomentum);
    TestFalse(TEXT("the fixture's momentum component is inert for a classless owner"), InertMomentum->IsActiveForOwner());
    TestTrue(TEXT("an inert momentum component grants no channel — a Caster's shots are unchanged"), CasterWeapon->GetShotChannels().IsIdentity());
    return true;
}

// ---------------------------------------------------------------------------
// WHAT THESE TESTS DO NOT COVER, stated plainly
// ---------------------------------------------------------------------------
// The suite is world-free, so nothing here traces. Not covered, and not
// coverable without a world:
//  1. That ResolvePelletImpacts' pierce loop actually resolves N enemies in a
//     line — the trace, the ignored-actor accumulation, and the per-leg
//     SecondaryImpacts records. The ladder it pays (NextPierceMultiplier) and
//     the budget rule (first hit free) are pinned above; the loop is
//     inspection plus playtest, the same standing every other LineTrace in
//     Weapons/ has.
//  2. FindNearestChainTarget's line-of-sight filter. Its selection math is
//     SelectNearestTarget, pinned above; the LOS trace is world-only.
//  3. Momentum GENERATION from Pierce Discipline (GrantMomentum per pierced
//     target) — the grant rides the pierce count the loop produces. The
//     transcribed magnitudes (+4/+7, cap 3) are cited at the call site in
//     FireOnce.
//  4. The live coupling read (GetMomentumState on an ACTIVE Swift component
//     mid-fight). Driving CachedState requires the movement loop; the table
//     it feeds and the inert-component gate are both pinned above, so the
//     only unpinned link is the one delegate read.

#endif  // WITH_DEV_AUTOMATION_TESTS
