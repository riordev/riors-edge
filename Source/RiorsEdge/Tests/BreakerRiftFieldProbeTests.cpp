#include "Misc/AutomationTest.h"
#include "Game/BreakerCoverRegistry.h"
#include "Game/BreakerRiftDefinition.h"
#include "Game/BreakerZoneBuilder.h"

#if WITH_DEV_AUTOMATION_TESTS

// ---------------------------------------------------------------------------
// CAN A RIFT'S INTERIOR BE GENERATED INSTEAD OF AUTHORED?
//
// The plan for varied rift interiors is to stop spawning the yard's authored
// cover lattice and run the existing generator over the yard's own band
// instead, seeded by the rift. That is only viable if the generator produces
// a LEGAL field in that band — the yard's params were written to VALIDATE an
// authored layout, and being able to judge a layout is not the same as being
// able to produce one.
//
// So this asks the question directly, for many rift seeds, before any of it
// is wired into the builder.
//
// THE ANSWER TODAY IS NO, AND THE REASON IS NOT THE ONE EXPECTED. Every seed
// refuses: the generator lays 47 pieces into the yard's band for 7.62% cover
// against a 0.50-5.00% ceiling. The obvious knob does nothing — the pitch
// sweep below runs 3400 cm to 5400 cm and the piece count and cover fraction
// do not move by a hundredth, because in a band this small the LATTICE
// contributes nothing and all 47 pieces come from the combat pockets and
// their outer rings. Widening the lattice cannot thin a field the lattice is
// not filling.
//
// What that means for the plan: making the generator fit a yard is not a
// number to tune, it is pocket composition authored for a new purpose — so
// the cheap route to varied rift interiors is closed until the generator
// grows a small-band profile, and O167's other branch (interiors get their
// own composed geometry) is the honest alternative. This test stays as the
// instrument that says when the route opens.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerRiftFieldProbeTest,
    "RiorsEdge.Zone.RiftGeneratedField",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerRiftFieldProbeTest::RunTest(const FString&)
{
    constexpr int32 Base = 20260814;
    const FBreakerCoverFieldParams Yard = UBreakerZoneBuilder::FernhallFieldParams();

    // WHAT PITCH FITS THIS YARD? The lattice spacing shipped for the gym's
    // 220 m apron; the yard is 106 x 56 m and the same pitch over-fills it.
    // Cover density falls with the square of the pitch, so the sweep is short.
    for (const float Pitch : {3400.f, 3800.f, 4200.f, 4600.f, 5000.f, 5400.f})
    {
        int32 PitchLegal = 0, PitchTotal = 0, Pieces = 0;
        float WorstFraction = 0.0f;
        for (int32 Level = 1; Level <= 60; Level += 7)
        {
            for (const TCHAR* Id : {TEXT("fernhall.approach"), TEXT("breach.marshalling"), TEXT("rift.deep")})
            {
                FBreakerRiftDefinition R;
                R.EncounterId = FName(Id);
                R.AreaLevel = Level;
                FBreakerCoverFieldParams P = Yard;
                P.ClusterPitchCm = Pitch;
                P.Seed = R.LayoutSeed(Base);
                const TArray<FBreakerCoverPiece> Built = UBreakerCoverLayoutLibrary::BuildCoverField(P);
                Pieces = Built.Num();
                WorstFraction = FMath::Max(WorstFraction,
                    UBreakerCoverLayoutLibrary::CoverAreaFraction(Built, P) * 100.0f);
                FString Why;
                ++PitchTotal;
                if (UBreakerCoverLayoutLibrary::IsLayoutLegal(Built, P, Why)) ++PitchLegal;
            }
        }
        AddInfo(FString::Printf(TEXT("RIFT PITCH SWEEP  pitch %.0f cm -> %d pieces, worst cover %.2f%%, %d/%d legal"),
            Pitch, Pieces, WorstFraction, PitchLegal, PitchTotal));
    }

    int32 Legal = 0;
    int32 Total = 0;
    int32 FewestPieces = TNumericLimits<int32>::Max();
    int32 MostPieces = 0;
    FString FirstFailure;

    for (int32 Level = 1; Level <= 60; Level += 7)
    {
        for (const TCHAR* Id : {TEXT("fernhall.approach"), TEXT("breach.marshalling"), TEXT("rift.deep")})
        {
            FBreakerRiftDefinition Rift;
            Rift.EncounterId = FName(Id);
            Rift.AreaLevel = Level;

            FBreakerCoverFieldParams Params = Yard;
            Params.Seed = Rift.LayoutSeed(Base);

            const TArray<FBreakerCoverPiece> Pieces = UBreakerCoverLayoutLibrary::BuildCoverField(Params);
            ++Total;
            FewestPieces = FMath::Min(FewestPieces, Pieces.Num());
            MostPieces = FMath::Max(MostPieces, Pieces.Num());

            FString Reason;
            if (UBreakerCoverLayoutLibrary::IsLayoutLegal(Pieces, Params, Reason)) ++Legal;
            else if (FirstFailure.IsEmpty())
            {
                FirstFailure = FString::Printf(TEXT("%s @%d (%d pieces): %s"), Id, Level, Pieces.Num(), *Reason);
            }
        }
    }

    AddInfo(FString::Printf(
        TEXT("RIFT FIELD PROBE  %d/%d seeds legal in the yard's own band, %d-%d pieces per field"),
        Legal, Total, FewestPieces, MostPieces));
    if (!FirstFailure.IsEmpty()) AddInfo(FString::Printf(TEXT("RIFT FIELD PROBE  first refusal: %s"), *FirstFailure));

    // The generator must at least PRODUCE something in this band; a field of
    // nothing would mean the yard's params only ever judged, never built.
    TestTrue(TEXT("the generator produces cover in the yard's band"), MostPieces > 0);

    // Deliberately not asserting that every seed is legal yet. This test is a
    // probe: its emitted numbers decide whether the cheap route is taken, and
    // the assertion that every shipped rift layout is legal belongs with the
    // commit that actually spawns one.
    return true;
}

#endif
