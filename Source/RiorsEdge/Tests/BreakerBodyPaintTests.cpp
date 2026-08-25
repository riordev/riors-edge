#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Combat/BreakerBodyPaint.h"
#include "Combat/BreakerHitReactionComponent.h"
#include "Combat/BreakerAlteredEnemy.h"
#include "Combat/BreakerEnemy.h"
#include "Combat/BreakerRangedEnemy.h"
#include "Combat/BreakerSkirmisherEnemy.h"
#include "Combat/BreakerWardenEnemy.h"

// ---------------------------------------------------------------------------
// THE BODY'S COLOUR (O128, O129). Three tests for three different claims:
//
//  * COMPOSITION is the O128 regression pin. The defect it holds is a RACE
//    between two capture-and-restore caches over one material parameter, and
//    the fix is that the colour is a pure function of state with no captures
//    in it. So the assertion is not "the flash restores correctly" — it is
//    that a flash LEAVES NOTHING BEHIND, for every rank and every health
//    fraction, which is a property a capture cannot have and this can.
//  * RAMP is O129's authored table and the two rules that keep it honest:
//    nothing moves on an undamaged body, and the body never goes overbright.
//  * SHIPPED is the configuration half, per the standing rule that a pure
//    test proves the rule and never the wiring: every enemy class the game
//    fields DECLARES the paint its constructor PAINTS.
// ---------------------------------------------------------------------------

namespace
{
    // Prefixed for the unity build as always.
    const EBreakerMonsterRank BreakerBodyPaintAllRanks[] = {
        EBreakerMonsterRank::Trash, EBreakerMonsterRank::Elite,
        EBreakerMonsterRank::ModifierBearing, EBreakerMonsterRank::Boss };

    bool BreakerBodyPaintExactlyEquals(const FLinearColor& A, const FLinearColor& B)
    {
        return A.R == B.R && A.G == B.G && A.B == B.B;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerBodyPaintCompositionTest,
    "RiorsEdge.Combat.BodyPaint.Composition",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerBodyPaintCompositionTest::RunTest(const FString& Parameters)
{
    const FLinearColor Family = BreakerBodyPaint::AlteredFamilyPaint;
    const float Fractions[] = { 1.0f, 0.75f, 0.5f, 0.25f, 0.1f, 0.0f };

    for (EBreakerMonsterRank Rank : BreakerBodyPaintAllRanks)
    {
        for (float Fraction : Fractions)
        {
            BreakerBodyPaint::FState State;
            State.FamilyPaint = Family;
            State.Rank = Rank;
            State.HealthFraction = Fraction;
            State.bHealthRamp = true;

            const FLinearColor Rested = BreakerBodyPaint::Resolve(State);

            // A FLASH LEAVES NOTHING BEHIND. Under the two-cache design this
            // was false whenever the rank layer captured while a flash was in
            // flight: the flash colour became the family paint and every
            // later demotion restored the body to white. There is no path to
            // that here because the flash contributes a value and never takes
            // one — so the assertion is bit-identity, not tolerance.
            for (bool bWeakPoint : { false, true })
            {
                BreakerBodyPaint::FState Flashing = State;
                Flashing.Reaction = BreakerBodyPaint::EReaction::Flash;
                Flashing.bReactionWeakPoint = bWeakPoint;
                const FLinearColor Flash = BreakerBodyPaint::Resolve(Flashing);
                TestTrue(TEXT("A flash occludes the body outright"),
                    BreakerBodyPaintExactlyEquals(Flash, bWeakPoint
                        ? BreakerBodyPaint::FlashWeakPointColor : BreakerBodyPaint::FlashColor));

                BreakerBodyPaint::FState Settled = Flashing;
                Settled.Reaction = BreakerBodyPaint::EReaction::Rest;
                TestTrue(TEXT("The colour after a flash is bit-identical to the colour before it"),
                    BreakerBodyPaintExactlyEquals(BreakerBodyPaint::Resolve(Settled), Rested));

                // The whole death beat, same claim: pop, crumple, then rest.
                BreakerBodyPaint::FState Crumpling = Flashing;
                Crumpling.Reaction = BreakerBodyPaint::EReaction::DeathCrumple;
                Crumpling.ReactionAlpha = 1.0f;
                // Tolerance, not bit-identity, and the distinction is the
                // point: this end of the beat is an INTERPOLATION, and
                // Lerp(A, B, 1) is A + (B - A) which lands a float ulp off B.
                // Every other claim in this test is an identity and is
                // asserted as one.
                TestTrue(TEXT("A landed crumple is ash, whatever the body was"),
                    BreakerBodyPaint::Resolve(Crumpling).Equals(BreakerBodyPaint::DeathAshColor, 1.0e-5f));
                Crumpling.Reaction = BreakerBodyPaint::EReaction::Rest;
                Crumpling.ReactionAlpha = 0.0f;
                TestTrue(TEXT("A revive after the death beat recomputes, it does not restore"),
                    BreakerBodyPaintExactlyEquals(BreakerBodyPaint::Resolve(Crumpling), Rested));
            }
        }
    }

    // THE DEMOTION, on the shipped component rather than on the maths: a body
    // promoted to Elite and sent back to Trash is the family paint again,
    // exactly, with no restore step anywhere in the path. This is the pooled
    // reuse's contract and the reason ReviveFromPool no longer depends on its
    // caller running a chassis pass afterwards. No world is needed — the
    // paint state is a plain struct and an unregistered part list paints
    // nothing.
    UBreakerHitReactionComponent* Reaction = NewObject<UBreakerHitReactionComponent>();
    Reaction->SetFamilyPaint(Family);
    TestTrue(TEXT("A rested Trash body IS its family paint, bit for bit"),
        BreakerBodyPaintExactlyEquals(Reaction->GetResolvedBodyColor(), Family));

    Reaction->SetRank(EBreakerMonsterRank::Elite);
    const FLinearColor Gold = Reaction->GetResolvedBodyColor();
    TestFalse(TEXT("An Elite is not its family paint"),
        BreakerBodyPaintExactlyEquals(Gold, Family));

    // Promote, drain, promote again, drain again, then demote. Compounding
    // was the OTHER failure the capture existed to prevent, and forward
    // composition gets it for nothing.
    Reaction->SetHealthRampEnabled(true);
    Reaction->SetHealthFraction(0.2f);
    Reaction->SetRank(EBreakerMonsterRank::ModifierBearing);
    Reaction->SetRank(EBreakerMonsterRank::Elite);
    Reaction->SetHealthFraction(1.0f);
    TestTrue(TEXT("Repeated promotions never compound the tint"),
        BreakerBodyPaintExactlyEquals(Reaction->GetResolvedBodyColor(), Gold));

    Reaction->SetRank(EBreakerMonsterRank::Trash);
    TestTrue(TEXT("A demotion gives the gold back with no restore step"),
        BreakerBodyPaintExactlyEquals(Reaction->GetResolvedBodyColor(), Family));

    // Trash and Boss blend at zero, deliberately: Trash keeps its family
    // paint and the Boss subclass owns its whole identity.
    TestEqual(TEXT("Trash blends at zero"), BreakerBodyPaint::RankBlendFor(EBreakerMonsterRank::Trash), 0.0f);
    TestEqual(TEXT("Boss blends at zero"), BreakerBodyPaint::RankBlendFor(EBreakerMonsterRank::Boss), 0.0f);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerBodyPaintHealthRampTest,
    "RiorsEdge.Combat.BodyPaint.HealthRamp",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerBodyPaintHealthRampTest::RunTest(const FString& Parameters)
{
    const float* Stops = BreakerBodyPaint::HealthStops();

    for (EBreakerMonsterRank Rank : BreakerBodyPaintAllRanks)
    {
        const FLinearColor* Row = BreakerBodyPaint::HealthRampRow(Rank);

        // NOTHING MOVES ON AN UNDAMAGED BODY. The ramp ships as a delta from
        // its own full-health entry, so a full-health enemy is bit-identical
        // to what it was before O129 landed — the whole reason the delta form
        // was chosen over the pack's absolutes, which collapse the three
        // families to one colour.
        TestTrue(TEXT("Full health contributes exactly no offset"),
            BreakerBodyPaintExactlyEquals(
                BreakerBodyPaint::HealthRampOffset(Rank, 1.0f), FLinearColor(0, 0, 0)));

        // THE BODY REDDENS AS IT DIES, monotonically, in every rank row.
        // This is the readable claim the twenty colours are for, and it is
        // the one that survives a colour-blind screen as a value change.
        for (int32 i = 0; i < BreakerBodyPaint::HealthStopCount - 1; ++i)
        {
            TestTrue(TEXT("Red rises at every step toward death"), Row[i + 1].R > Row[i].R);
            TestTrue(TEXT("The authored stop is what the sampler returns"),
                BreakerBodyPaintExactlyEquals(BreakerBodyPaint::SampleHealthRamp(Rank, Stops[i]), Row[i]));
        }

        // Flat outside the authored range in both directions: overhealing and
        // the last sliver hold still rather than extrapolating off the table.
        TestTrue(TEXT("Above full health holds the full-health colour"),
            BreakerBodyPaintExactlyEquals(BreakerBodyPaint::SampleHealthRamp(Rank, 4.0f), Row[0]));
        TestTrue(TEXT("Below the last stop holds the last colour"),
            BreakerBodyPaintExactlyEquals(BreakerBodyPaint::SampleHealthRamp(Rank, 0.0f),
                Row[BreakerBodyPaint::HealthStopCount - 1]));

        // THE BODY NEVER GOES OVERBRIGHT; ONLY THE REACTION DOES. Unclamped,
        // an Elite at ten percent pushes red past 1.0 and blooms, spending
        // the hit flash's whole vocabulary on a health value.
        for (float Fraction = 0.0f; Fraction <= 1.0f; Fraction += 0.05f)
        {
            for (const FLinearColor& Family : { BreakerBodyPaint::VestigeFamilyPaint,
                BreakerBodyPaint::AlteredFamilyPaint, BreakerBodyPaint::LatticeFamilyPaint })
            {
                BreakerBodyPaint::FState State;
                State.FamilyPaint = Family;
                State.Rank = Rank;
                State.HealthFraction = Fraction;
                State.bHealthRamp = true;
                const FLinearColor Body = BreakerBodyPaint::Resolve(State);
                TestTrue(TEXT("A resting body stays inside 0-1 on every channel"),
                    Body.R >= 0.0f && Body.R <= 1.0f && Body.G >= 0.0f && Body.G <= 1.0f
                    && Body.B >= 0.0f && Body.B <= 1.0f);
            }
        }
    }

    // The ramp is OPT-IN, and the dummy is the opt-out: with the flag clear,
    // a body at one percent health is still exactly its family paint.
    BreakerBodyPaint::FState Dummy;
    Dummy.FamilyPaint = BreakerBodyPaint::DummyFamilyPaint;
    Dummy.HealthFraction = 0.01f;
    TestTrue(TEXT("With the ramp off, health moves nothing"),
        BreakerBodyPaintExactlyEquals(BreakerBodyPaint::Resolve(Dummy), BreakerBodyPaint::DummyFamilyPaint));

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerBodyPaintShippedFamilyTest,
    "RiorsEdge.Combat.BodyPaint.ShippedFamilies",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerBodyPaintShippedFamilyTest::RunTest(const FString& Parameters)
{
    // THE CONFIGURATION HALF. The maths above proves the rule against any
    // family paint; this proves the game hands it the right one. A class that
    // declares a colour it does not paint would compose a body nobody
    // authored, and only reading both ends can see it.
    auto Check = [this](const TCHAR* Name, ABreakerEnemy* Enemy, const FLinearColor& Expected)
    {
        if (!Enemy) { AddError(FString::Printf(TEXT("%s failed to construct"), Name)); return; }
        TestTrue(FString::Printf(TEXT("%s declares its authored family paint"), Name),
            BreakerBodyPaintExactlyEquals(Enemy->GetFamilyPaint(), Expected));
        TestTrue(FString::Printf(TEXT("%s ships at rank Trash"), Name),
            Enemy->GetMonsterRank() == EBreakerMonsterRank::Trash);
    };

    // The Vestige grey-violet is the default, and three of the five shipped
    // classes take it: the base humanoid, the Warden (its identity is the
    // shield, not the body) and the Skirmisher (its identity is the insignia
    // and the muzzle). Neither of the latter two paints a body part at all,
    // which is precisely why an unstated default would be invisible.
    Check(TEXT("Vestige"), NewObject<ABreakerEnemy>(), BreakerBodyPaint::VestigeFamilyPaint);
    Check(TEXT("Warden"), NewObject<ABreakerWardenEnemy>(), BreakerBodyPaint::VestigeFamilyPaint);
    Check(TEXT("Skirmisher"), NewObject<ABreakerSkirmisherEnemy>(), BreakerBodyPaint::VestigeFamilyPaint);
    // O24's two reserved reads: the Altered's olive-slate and the Lattice's
    // deeper green. The Altered's has to be a different HUE from the Vestige
    // grey-violet or the two families silhouette identically.
    Check(TEXT("Altered"), NewObject<ABreakerAlteredEnemy>(), BreakerBodyPaint::AlteredFamilyPaint);
    Check(TEXT("Lattice"), NewObject<ABreakerRangedEnemy>(), BreakerBodyPaint::LatticeFamilyPaint);

    // And the reserved hues stay apart from each other. Not a tolerance: the
    // families are authored to differ and a future edit that collapses two of
    // them should fail here rather than in a screenshot.
    TestFalse(TEXT("Altered is not the Vestige paint"), BreakerBodyPaintExactlyEquals(
        BreakerBodyPaint::AlteredFamilyPaint, BreakerBodyPaint::VestigeFamilyPaint));
    TestFalse(TEXT("Lattice is not the Vestige paint"), BreakerBodyPaintExactlyEquals(
        BreakerBodyPaint::LatticeFamilyPaint, BreakerBodyPaint::VestigeFamilyPaint));
    TestFalse(TEXT("Lattice is not the Altered paint"), BreakerBodyPaintExactlyEquals(
        BreakerBodyPaint::LatticeFamilyPaint, BreakerBodyPaint::AlteredFamilyPaint));

    return true;
}

#endif
