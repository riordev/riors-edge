#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Game/BreakerZonePalette.h"

// ---------------------------------------------------------------------------
// A RIFT LOOKS LIKE THE PLACE IT COPIES, RUINED.
//
// Under O268 the rift and the ordinary world run different rules — one is
// finite and concludes, the other is patrolled and continuous — while sharing
// the same geometry. The palette is what tells a player which of the two they
// are standing in before they act on it, so what is asserted here is not a set
// of colours but the three properties that make the read work: it is DIMMER and
// DRAINED, it is far enough from the living colour to be seen, and it is still
// recognisably the same place rather than a different one.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerZonePaletteTest,
    "RiorsEdge.Zone.Palette.Dilapidation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerZonePaletteTest::RunTest(const FString& Parameters)
{
    using namespace BreakerZonePalette;

    // The shipped living palette, by the values the zone builder paints. Held
    // here as the surfaces a player actually looks at rather than as an
    // abstract sweep — a property that holds for every colour in the cube but
    // fails on the yard floor has proved nothing.
    struct FSurface { const TCHAR* What; FLinearColor Living; };
    const FSurface Surfaces[] = {
        { TEXT("concrete boundary"), FLinearColor(0.33f, 0.35f, 0.30f) },
        { TEXT("stone line break"),  FLinearColor(0.24f, 0.26f, 0.23f) },
        { TEXT("rusted chest cover"),FLinearColor(0.34f, 0.20f, 0.09f) },
        { TEXT("earth ground"),      FLinearColor(0.20f, 0.16f, 0.11f) },
        { TEXT("rift pad off-white"),FLinearColor(0.58f, 0.57f, 0.51f) },
        { TEXT("moss dressing"),     FLinearColor(0.14f, 0.26f, 0.11f) },
        { TEXT("yard floor"),        FLinearColor(0.36f, 0.36f, 0.31f) },
        { TEXT("substation floor"),  FLinearColor(0.17f, 0.20f, 0.19f) },
        { TEXT("seam"),              FLinearColor(0.43f, 0.42f, 0.35f) },
    };

    for (const FSurface& Surface : Surfaces)
    {
        const FLinearColor Ruined = Dilapidate(Surface.Living);
        const FString What(Surface.What);

        // IT READS. The whole feature is a player telling the two apart within
        // a few seconds of arriving; a ruin that lands on top of its living
        // colour has done nothing at all.
        TestTrue(*FString::Printf(TEXT("%s reads as ruined rather than as itself"), *What),
            Separation(Surface.Living, Ruined) >= ReadableSeparation);

        // IT IS DIMMER. Nobody maintained the lights either, and a ruin that
        // came back BRIGHTER would read as refurbishment.
        const float LivingLuma = 0.30f * Surface.Living.R + 0.59f * Surface.Living.G + 0.11f * Surface.Living.B;
        const float RuinedLuma = 0.30f * Ruined.R + 0.59f * Ruined.G + 0.11f * Ruined.B;
        TestTrue(*FString::Printf(TEXT("%s is dimmer ruined"), *What), RuinedLuma < LivingLuma);

        // IT IS DRAINED. Saturation as max-minus-min, which is what the eye
        // reads as "colour left in it".
        auto Chroma = [](const FLinearColor& C)
        {
            return FMath::Max3(C.R, C.G, C.B) - FMath::Min3(C.R, C.G, C.B);
        };
        TestTrue(*FString::Printf(TEXT("%s loses colour rather than gaining it"), *What),
            Chroma(Ruined) <= Chroma(Surface.Living) + KINDA_SMALL_NUMBER);

        // IT OXIDISES WARM, WHICH IS ALSO THE CANON GUARD. Teal is a noun in
        // this project — rift objects, suppression hardware, top-rarity frames,
        // beams and name text — and a rift's GROUND is not a rift object.
        // A ruin that drifted cool would spend the one reserved colour on the
        // surface the player looks at least.
        TestTrue(*FString::Printf(TEXT("%s never turns cool, so teal stays a noun"), *What),
            Ruined.R >= Ruined.B - KINDA_SMALL_NUMBER);

        // IT IS STILL THE SAME PLACE. A rift is a dilapidated version of the
        // area, not a different area: a ruin that landed on the ruin anchor
        // regardless of what it started as would make every surface identical
        // and throw away the layout's own reading.
        TestTrue(*FString::Printf(TEXT("%s keeps its own identity under the ruin"), *What),
            Separation(Ruined, RuinAnchor()) > 0.0f || Separation(Surface.Living, RuinAnchor()) == 0.0f);

        // Nothing leaves the representable range, and opacity is not the ruin's
        // business.
        TestTrue(*FString::Printf(TEXT("%s stays a paintable colour"), *What),
            Ruined.R >= 0.0f && Ruined.G >= 0.0f && Ruined.B >= 0.0f
            && Ruined.R <= 1.0f && Ruined.G <= 1.0f && Ruined.B <= 1.0f);
        TestEqual(*FString::Printf(TEXT("%s keeps its alpha"), *What), Ruined.A, Surface.Living.A, 0.0001f);
    }

    // THE LAYOUT SURVIVES THE RUIN. Two surfaces a player can tell apart in the
    // world must still be tellable apart inside a rift, or the ruin has flattened
    // the ground into one texture and the cover reads as scenery.
    for (int32 A = 0; A < UE_ARRAY_COUNT(Surfaces); ++A)
    {
        for (int32 B = A + 1; B < UE_ARRAY_COUNT(Surfaces); ++B)
        {
            if (Separation(Surfaces[A].Living, Surfaces[B].Living) < ReadableSeparation) continue;
            TestTrue(*FString::Printf(TEXT("%s and %s stay distinguishable once ruined"),
                Surfaces[A].What, Surfaces[B].What),
                Separation(Dilapidate(Surfaces[A].Living), Dilapidate(Surfaces[B].Living)) > 0.0f);
        }
    }

    // THE BRIGHT SURFACES MOVE FURTHEST, because the dim is multiplicative and
    // therefore takes most from whatever had most. That reads correctly: a
    // place goes dark from the lit surfaces down, and the rift pad — the
    // brightest thing in the yard — is the piece that stops looking maintained
    // first.
    //
    // This replaces a claim that was made here and turned out false in both
    // directions: that the MOSS would move furthest, being the most saturated.
    // The drain does move it most, but the dim dominates the drain and stone is
    // brighter. It was also wrong about ruins — a dilapidated industrial yard
    // grows MORE overgrown, not less, so a palette that killed the green would
    // have been reaching for the wrong picture as well as the wrong arithmetic.
    const FLinearColor Pad(0.58f, 0.57f, 0.51f);
    const FLinearColor Substation(0.17f, 0.20f, 0.19f);
    TestTrue(TEXT("the brightest surface loses the most"),
        Separation(Pad, Dilapidate(Pad)) > Separation(Substation, Dilapidate(Substation)));

    // THE SHIPPED DIALS. A drained fraction of zero would leave the rift
    // identical to the world and no assertion above would say which dial did it.
    TestTrue(TEXT("the shipped ruin actually drains"), DrainedFraction > 0.0f);
    TestTrue(TEXT("the shipped ruin actually stains"), StainedFraction > 0.0f);
    TestTrue(TEXT("the shipped ruin actually dims"), DimFraction < 1.0f && DimFraction > 0.0f);
    TestTrue(TEXT("and the ruin anchor is warm"), RuinAnchor().R > RuinAnchor().B);
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
