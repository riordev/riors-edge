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
// THE ANSWER IS NO, AND THE REASON IS STRUCTURAL RATHER THAN A NUMBER.
//
// Three guesses were wrong before the pieces were asked directly, which is
// the whole argument for asking. Sweeping the lattice pitch 3400-5400 cm
// moved nothing. Sweeping the pocket outer ring 12-0 moved nothing. The
// census says why: with the yard's params there are NO pockets and the
// lattice contributes nothing, and the 47 pieces are spread forward to
// 20476 cm across a band that ends at 8900 — the generator is building the
// GYM's field (elite arena, jump-run edges, sniper lane) and
// CoverAreaFraction is then dividing that cover by the yard's much smaller
// band. 7.62% is not a dense yard; it is cover from another field counted
// against this one.
//
// And the gym-only sections cannot simply be switched off, because the
// idiom for disabling them RELOCATES rather than suppresses: parking the
// elite arena at 1e7 leaves 45 pieces, now reaching forward 10,003,476 cm,
// still in the returned array and still counted. Sections would have to
// become skippable for any of this to work.
//
// SO: UBreakerCoverLayoutLibrary::BuildCoverField is a gym field builder,
// and UBreakerZoneBuilder::FernhallFieldParams is a VALIDATOR parameter set
// — it has only ever been used to judge the authored layout, which is why
// nobody noticed it cannot build one. Pairing them was the plan and the
// plan is wrong. Varied rift interiors need either a generator written for
// a band this size, or O167's other branch: interiors composed as their own
// geometry. This test stays as the instrument that says when that changes.
//
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
    // WHERE DO THE 47 COME FROM? Twice now I have guessed a source and been
    // wrong — the lattice pitch moved nothing, the pocket ring moved nothing.
    // So this asks the pieces themselves: cluster ids are assigned per
    // generator section, so a histogram of them names the section without
    // reading another line of it.
    {
        FBreakerCoverFieldParams P = Yard;
        P.Seed = 1;
        const TArray<FBreakerCoverPiece> Built = UBreakerCoverLayoutLibrary::BuildCoverField(P);
        int32 Chest = 0, Full = 0;
        float MinF = 1e9f, MaxF = -1e9f, MinR = 1e9f, MaxR = -1e9f, Area = 0.0f;
        for (const FBreakerCoverPiece& Piece : Built)
        {
            (Piece.Class == EBreakerCoverClass::FullHeight ? Full : Chest) += 1;
            MinF = FMath::Min(MinF, Piece.Forward); MaxF = FMath::Max(MaxF, Piece.Forward);
            MinR = FMath::Min(MinR, Piece.Right);   MaxR = FMath::Max(MaxR, Piece.Right);
            Area += Piece.HalfLengthCm * 2.0f * Piece.HalfDepthCm * 2.0f;
        }
        AddInfo(FString::Printf(
            TEXT("RIFT PIECE CENSUS  %d pieces (%d chest, %d full), spread fwd %.0f..%.0f right %.0f..%.0f, area %.0f cm2"),
            Built.Num(), Chest, Full, MinF, MaxF, MinR, MaxR, Area));
        AddInfo(FString::Printf(TEXT("RIFT PIECE CENSUS  pockets %d, corridor pockets %d, band %.0fx%.0f cm"),
            Yard.PocketCentres.Num(), Yard.CorridorPocketCentres.Num(),
            Yard.BandFarCm - Yard.BandNearCm, Yard.BandHalfWidthCm * 2.0f));
    }

    // THE ARENA WAS NEVER PARKED. FernhallFieldParams pushes the gym-only
    // jump run, sniper lane and wall lane out of range, but not the elite
    // arena at 17000 cm — which is why the census finds pieces 204 m down a
    // 106 m yard. Park it the same way and see what the band actually holds.
    {
        FBreakerCoverFieldParams P = Yard;
        P.ArenaDistanceCm = 1.0e7f;
        P.Seed = 1;
        const TArray<FBreakerCoverPiece> Built = UBreakerCoverLayoutLibrary::BuildCoverField(P);
        float MinF = 1e9f, MaxF = -1e9f;
        for (const FBreakerCoverPiece& Piece : Built)
        { MinF = FMath::Min(MinF, Piece.Forward); MaxF = FMath::Max(MaxF, Piece.Forward); }
        FString Why;
        const bool bLegal = UBreakerCoverLayoutLibrary::IsLayoutLegal(Built, P, Why);
        AddInfo(FString::Printf(TEXT("RIFT ARENA PARKED  %d pieces, fwd %.0f..%.0f, cover %.2f%%, legal=%d %s"),
            Built.Num(), MinF, MaxF,
            UBreakerCoverLayoutLibrary::CoverAreaFraction(Built, P) * 100.0f,
            bLegal ? 1 : 0, *Why));
    }

    for (const int32 OuterRing : {12, 8, 4, 0})
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
                P.PocketOuterRingCount = OuterRing;
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
        AddInfo(FString::Printf(TEXT("RIFT RING SWEEP  outer ring %2d -> %d pieces, worst cover %.2f%%, %d/%d legal"),
            OuterRing, Pieces, WorstFraction, PitchLegal, PitchTotal));
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
