#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Abilities/BreakerAbilityDefinition.h"
#include "Abilities/BreakerAbility_Rot.h"
#include "UI/BreakerEffectMath.h"
#include "UI/BreakerHUDMath.h"
#include "UI/BreakerUIStyle.h"

// ---------------------------------------------------------------------------
// UI.AbilityPresentation: what an ability looks like is what the ability IS.
//
// Owner, playtest 2026-09-11: "Rot is literally just an orange circle on the
// ground that doesn't even place correctly half the time. And same thing with
// Cleave. It's just a blue line that goes across your screen."
//
// Three separate defects behind that sentence, and all three are assertable
// without a viewport: a colour law nothing enforced, a floor rule that
// discarded the floor, and a sweep with no geometry but its own rim.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerAbilityPresentationColourTest,
    "RiorsEdge.Abilities.PresentationColourLaw",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerAbilityPresentationColourTest::RunTest(const FString& Parameters)
{
    using namespace BreakerHUDMath;

    // THE LAW ITSELF (O179). Cyan is the MOVEMENT verb and nothing else, which
    // is exactly what three ability draw sites got wrong: Cleave's swing,
    // Siphon's beam and Resonance's detonation were all painted it.
    TestTrue(TEXT("Movement is the cyan"),
        AbilityRailColor(EBreakerAbilityVerb::Movement, false).Equals(BreakerUI::VerbMove, 0.001f));
    TestFalse(TEXT("A weapon verb is not the movement cyan"),
        AbilityRailColor(EBreakerAbilityVerb::Weapon, false).Equals(BreakerUI::VerbMove, 0.001f));
    TestFalse(TEXT("A reward verb is not the movement cyan"),
        AbilityRailColor(EBreakerAbilityVerb::Reward, false).Equals(BreakerUI::VerbMove, 0.001f));
    TestTrue(TEXT("An ultimate is violet whatever its verb"),
        AbilityRailColor(EBreakerAbilityVerb::Weapon, true).Equals(BreakerUI::Violet, 0.001f));

    // AND THE SHIPPED ABILITIES EACH RESOLVE TO ONE. Walked over the two
    // classes the owner is building this slice around: every one of them names
    // a verb, so not one of them falls to the resting border — which is the
    // colour a draw site gets when a definition has said nothing, and would be
    // an ability drawing itself the colour of a disabled button.
    const EBreakerClassId Classes[] = { EBreakerClassId::Swift, EBreakerClassId::Caster };
    const EBreakerAbilitySlot Slots[] = { EBreakerAbilitySlot::ClassAbilityOne,
        EBreakerAbilitySlot::ClassAbilityTwo, EBreakerAbilitySlot::Ultimate };
    int32 Checked = 0;
    for (const EBreakerClassId ClassId : Classes)
    {
        for (const EBreakerAbilitySlot Slot : Slots)
        {
            for (const UBreakerAbilityDefinition* Definition
                : UBreakerAbilityDefinition::GetClassAbilities(ClassId, Slot))
            {
                if (!Definition) continue;
                ++Checked;
                const FLinearColor Paint = AbilityRailColor(Definition->Verb, Definition->IsUltimate());
                TestFalse(*FString::Printf(TEXT("%s draws a verb, not the resting border"),
                    *Definition->AbilityId.ToString()), Paint.Equals(BreakerUI::BorderRest, 0.001f));
                // The one that matters for the complaint: a Weapon ability may
                // never come out the movement colour.
                if (Definition->Verb == EBreakerAbilityVerb::Weapon && !Definition->IsUltimate())
                {
                    TestTrue(*FString::Printf(TEXT("%s is the weapon orange"), *Definition->AbilityId.ToString()),
                        Paint.Equals(BreakerUI::Orange, 0.001f));
                }
            }
        }
    }
    TestTrue(TEXT("Swift and Caster have abilities to check"), Checked > 0);
    return true;
}

// ---------------------------------------------------------------------------
// UI.AbilityPresentation.RotFloor: which hit under the aim point is the floor.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerRotFloorTest,
    "RiorsEdge.Abilities.Rot.FloorCorrection",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerRotFloorTest::RunTest(const FString& Parameters)
{
    using namespace BreakerRotFloor;

    auto Hit = [](float PointZ, float NormalZ, bool bPenetrating = false)
    {
        FProbeHit Probe;
        Probe.PointZ = PointZ;
        Probe.NormalZ = NormalZ;
        Probe.bStartPenetrating = bPenetrating;
        return Probe;
    };

    // A WALL FIRST, THEN THE FLOOR — the case that put the zone on a wall's
    // face for the whole life of the old single-hit trace.
    {
        const FProbeHit Hits[] = { Hit(310.0f, 0.02f), Hit(0.0f, 1.0f) };
        float Z = -1.0f;
        TestTrue(TEXT("A wall in front of the floor does not hide it"), PickFloorZ(Hits, Z));
        TestEqual(TEXT("and the floor is the floor"), Z, 0.0f);
    }
    // NOTHING BUT WALLS — the aim point is left exactly where it was, because
    // this correction fires on evidence of a floor and never on its absence.
    {
        const FProbeHit Hits[] = { Hit(310.0f, 0.02f), Hit(120.0f, -0.3f) };
        float Z = -1.0f;
        TestFalse(TEXT("A column of walls is not a floor"), PickFloorZ(Hits, Z));
        TestEqual(TEXT("and nothing is written"), Z, -1.0f);
    }
    // AN EMPTY WORLD. Four ability fixtures cast Rot into one and assert where
    // the zone lands; this is the assertion that protects them.
    {
        float Z = -1.0f;
        TestFalse(TEXT("No hits is no floor"), PickFloorZ(TArrayView<const FProbeHit>(), Z));
    }
    // THE PENETRATING START, which is the other half of "half the time": a
    // probe beginning inside geometry reports the trace's own start point with
    // an invented up normal, and accepting it lifted the zone to the top of the
    // probe — four metres into the air.
    {
        const FProbeHit Hits[] = { Hit(400.0f, 1.0f, /*bPenetrating=*/true), Hit(0.0f, 1.0f) };
        float Z = -1.0f;
        TestTrue(TEXT("A penetrating start is stepped over"), PickFloorZ(Hits, Z));
        TestEqual(TEXT("and the real floor is taken"), Z, 0.0f);
    }
    // A RAMP IS A FLOOR. Forty-five degrees is 0.707 of up, so the gate admits
    // every walkable slope and no wall.
    {
        const FProbeHit Hits[] = { Hit(85.0f, 0.71f) };
        float Z = -1.0f;
        TestTrue(TEXT("A walkable ramp counts"), PickFloorZ(Hits, Z));
        TestEqual(TEXT("at the ramp's own height"), Z, 85.0f);
    }
    TestTrue(TEXT("The gate admits a forty-five degree ramp"), MinimumFloorNormalZ <= 0.7072f);
    return true;
}

// ---------------------------------------------------------------------------
// UI.AbilityPresentation.SweptBlade: Cleave has an inside.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerSweptBladeTest,
    "RiorsEdge.UI.EffectMath.SweptBlade",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerSweptBladeTest::RunTest(const FString& Parameters)
{
    // THE RINGS ARE ORDERED AND THE OUTER ONE IS THE EDGE. The sweep used to be
    // the rim alone, which is why it read as a line: one radius has no inside.
    float Previous = 0.0f;
    for (int32 Ring = 0; Ring < BreakerFX::SweptArcRings; ++Ring)
    {
        const float Fraction = BreakerFX::SweptArcRingFraction(Ring);
        TestTrue(TEXT("Each ring is further out than the last"), Fraction > Previous);
        TestTrue(TEXT("and inside the authored range"), Fraction <= 1.0f);
        Previous = Fraction;
    }
    TestEqual(TEXT("The outer ring IS the authored range"),
        BreakerFX::SweptArcRingFraction(BreakerFX::SweptArcRings - 1), 1.0f);

    // AND THINNER TOWARD THE CENTRE, so what the eye lands on is the edge.
    float PreviousThickness = 0.0f;
    for (int32 Ring = 0; Ring < BreakerFX::SweptArcRings; ++Ring)
    {
        const float Thickness = BreakerFX::SweptArcRingThickness(Ring, 7.0f);
        TestTrue(TEXT("Each ring is thicker than the one inside it"), Thickness > PreviousThickness);
        PreviousThickness = Thickness;
    }
    TestEqual(TEXT("The edge keeps the authored thickness"),
        BreakerFX::SweptArcRingThickness(BreakerFX::SweptArcRings - 1, 7.0f), 7.0f);

    // THE STROKE BUDGET IS REAL. The pool holds 48 and a live zone rim is 16 of
    // them, so a sweep may not exceed the 24 that leaves a zone alone.
    TestTrue(TEXT("A whole blade fits beside a live zone rim"),
        BreakerFX::SweptArcRings * BreakerFX::SweptArcStrokes <= 24);

    // AND THE BLADE IS STILL AN ARC: every vertex of every ring sits at its own
    // radius from the origin, or the fan would not be a fan.
    {
        const FVector Origin(100.0f, -50.0f, 20.0f);
        const FVector Forward = FVector(1.0f, 0.3f, 0.0f).GetSafeNormal();
        for (int32 Ring = 0; Ring < BreakerFX::SweptArcRings; ++Ring)
        {
            const float Radius = 650.0f * BreakerFX::SweptArcRingFraction(Ring);
            for (int32 Index = 0; Index <= BreakerFX::SweptArcStrokes; ++Index)
            {
                const FVector Vertex = BreakerFX::ArcVertex(Origin, Forward, 120.0f, Radius,
                    Index, BreakerFX::SweptArcStrokes);
                TestTrue(TEXT("Every vertex sits on its ring"),
                    FMath::IsNearlyEqual(FVector::Dist(Vertex, Origin), Radius, 0.5f));
            }
        }
    }
    return true;
}

#endif   // WITH_DEV_AUTOMATION_TESTS
