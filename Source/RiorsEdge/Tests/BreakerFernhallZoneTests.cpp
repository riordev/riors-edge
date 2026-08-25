// THE FERNHALL YARD, MEASURED. This is the first time the suite validates
// PLACED geometry rather than generated geometry: the gym's cover field is
// born from BuildCoverField and its tests prove the generator, but the yard
// is authored in Scripts/compose_fernhall.py and imported as meshes — so
// these tests read the imported assets' bounds through the same
// UBreakerCoverLayoutLibrary validators the gym answers to. The numbers tell
// you where the walls go: a re-authored yard that narrows the dash lane or
// opens an exposed crossing goes red here before anyone stands in it.
//
// The pieces come from UBreakerZoneBuilder::CollectZonePieces — the SAME
// collection the runtime spawner uses — so what the suite measures and what
// the game assembles cannot drift apart. A missing asset folder is a FAILURE,
// not a skip: reachability is definition-of-done, the meshes are committed
// (LFS), and a suite that politely skips an absent zone is a suite that
// cannot see the import step being forgotten.

#include "Misc/AutomationTest.h"
#include "Misc/PackageName.h"

#include "Game/BreakerCoverRegistry.h"
#include "Game/BreakerGameInstance.h"
#include "Game/BreakerZoneBuilder.h"
#include "Interaction/BreakerTravelPoint.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerFernhallGrammarTest,
    "RiorsEdge.Zone.Fernhall.GrammarLegal",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerFernhallGrammarTest::RunTest(const FString& Parameters)
{
    TArray<FBreakerZonePiece> Pieces;
    if (!TestTrue(TEXT("the yard's mesh folder collects"),
        UBreakerZoneBuilder::CollectZonePieces(UBreakerZoneBuilder::FernhallMeshFolder(), Pieces)))
    {
        return false;
    }

    FBreakerZoneMarkers Markers;
    if (!TestTrue(TEXT("the marker set is complete"),
        UBreakerZoneBuilder::ExtractMarkers(Pieces, Markers)))
    {
        return false;
    }

    const TArray<FBreakerCoverPiece> Cover = UBreakerZoneBuilder::BuildCoverPieces(Pieces, Markers);
    const FBreakerCoverFieldParams Params = UBreakerZoneBuilder::FernhallFieldParams();

    // THE READOUT IS LOGGED WHETHER OR NOT THE YARD PASSES. It used to print
    // only on failure, which meant the one place the yard's measurements were
    // visible was the one run where they were already wrong — and a figure
    // nobody reads while it is green is a figure free to drift up to the edge
    // of its band unremarked. Every band in this line names its direction.
    AddInfo(FString::Printf(TEXT("Fernhall yard: %s"),
        *UBreakerCoverLayoutLibrary::DescribeCoverField(Cover, Params)));

    FString Reason;
    const bool bLegal = UBreakerCoverLayoutLibrary::IsLayoutLegal(Cover, Params, Reason);
    if (!bLegal)
    {
        AddError(FString::Printf(TEXT("the placed yard is grammar-illegal: %s | %s"),
            *Reason, *UBreakerCoverLayoutLibrary::DescribeCoverField(Cover, Params)));
    }
    return bLegal;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerFernhallPieceContractTest,
    "RiorsEdge.Zone.Fernhall.PieceContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerFernhallPieceContractTest::RunTest(const FString& Parameters)
{
    TArray<FBreakerZonePiece> Pieces;
    if (!TestTrue(TEXT("the yard's mesh folder collects"),
        UBreakerZoneBuilder::CollectZonePieces(UBreakerZoneBuilder::FernhallMeshFolder(), Pieces)))
    {
        return false;
    }
    FBreakerZoneMarkers Markers;
    if (!TestTrue(TEXT("the marker set is complete"),
        UBreakerZoneBuilder::ExtractMarkers(Pieces, Markers)))
    {
        return false;
    }

    // The composer's roster: 58 meshes, of which 16 are measured cover. A
    // drifted count means the composer and this file disagree about what the
    // yard IS — re-author both or neither.
    TestEqual(TEXT("imported piece count"), Pieces.Num(), 58);
    const TArray<FBreakerCoverPiece> Cover = UBreakerZoneBuilder::BuildCoverPieces(Pieces, Markers);
    TestEqual(TEXT("measured cover pieces"), Cover.Num(), 16);
    TestEqual(TEXT("chest-high pieces"),
        UBreakerCoverLayoutLibrary::CountOfClass(Cover, EBreakerCoverClass::ChestHigh), 10);
    TestEqual(TEXT("full-height pieces"),
        UBreakerCoverLayoutLibrary::CountOfClass(Cover, EBreakerCoverClass::FullHeight), 6);

    // The name prefix claims a class; the imported geometry must actually BE
    // that class. The composer scales kit walls to the grammar's authored
    // heights, and this is where a silently rescaled kit piece — chest cover
    // you cannot shoot over, a line break you can — gets caught.
    const FBreakerCoverFieldParams Params = UBreakerZoneBuilder::FernhallFieldParams();
    for (const FBreakerCoverPiece& Piece : Cover)
    {
        const float Authored = Piece.Class == EBreakerCoverClass::FullHeight
            ? Params.FullHeightCm : Params.ChestHeightCm;
        TestTrue(FString::Printf(TEXT("cover height %.0f within 5 cm of authored %.0f"),
            Piece.HeightCm, Authored), FMath::Abs(Piece.HeightCm - Authored) <= 5.0f);
    }

    // The yard points somewhere: the rift marker stands at the far end of the
    // lane, not next to the door.
    TestTrue(TEXT("the rift is over 80 m downrange of the player start"),
        FVector::Dist2D(Markers.Rift, Markers.PlayerStart) > 8000.0f);
    return true;
}

// WHICH RULE ACTUALLY HOLDS THE DASH LANE OPEN — established by PERTURBING
// the yard rather than by reading the validators and believing the answer.
//
// The composer's prose leads with a 19.8 m lane between the chest pairs, and
// the obvious reading is that MinimumOpenLaneWidth guards it. IT DOES NOT: that
// function is full-height only, on purpose, because chest cover at 120 cm is
// under MantleStepHeight 145 and is crossed by going over rather than around.
// From that alone it looks as though the yard's main lane is guarded by
// nothing and could be narrowed to any width with a green suite.
//
// It cannot. The rule that holds it is the CORRIDOR REJECTION — no piece of
// any class inside CorridorHalfWidth (900 cm) of the centreline over the
// corridor span — and this test proves it by walking the chest pairs inward
// until the grammar objects, then naming which rule objected. A perturbation
// is worth more than a comment here because the two rules are easy to confuse
// and the confusing pair is exactly what produced the misreading.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerFernhallLaneGuardTest,
    "RiorsEdge.Zone.Fernhall.LaneGuard",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerFernhallLaneGuardTest::RunTest(const FString& Parameters)
{
    TArray<FBreakerZonePiece> Pieces;
    if (!TestTrue(TEXT("the yard's mesh folder collects"),
        UBreakerZoneBuilder::CollectZonePieces(UBreakerZoneBuilder::FernhallMeshFolder(), Pieces)))
    {
        return false;
    }
    FBreakerZoneMarkers Markers;
    if (!TestTrue(TEXT("the marker set is complete"),
        UBreakerZoneBuilder::ExtractMarkers(Pieces, Markers)))
    {
        return false;
    }

    const TArray<FBreakerCoverPiece> Cover = UBreakerZoneBuilder::BuildCoverPieces(Pieces, Markers);
    const FBreakerCoverFieldParams Params = UBreakerZoneBuilder::FernhallFieldParams();

    // The shipped yard's margin: the closest piece to the centreline is the
    // chest shoulder line, and it clears the corridor floor.
    const float Margin = UBreakerCoverLayoutLibrary::NearestPieceToCorridorCentre(Cover, Params);
    TestTrue(FString::Printf(TEXT("the shipped shoulders clear the corridor floor: %.0f >= %.0f"),
        Margin, Params.CorridorHalfWidthCm), Margin >= Params.CorridorHalfWidthCm);
    TestTrue(TEXT("and they are the chest shoulders, not something further out"),
        Margin <= Params.CorridorShoulderOffsetCm + 1.0f);

    // THE PERTURBATION. Pull every chest piece in to +-500 cm — the width a
    // reader would reach for to show the lane is unguarded — and confirm the
    // grammar refuses it. The full-height pieces are left exactly where they
    // are, so whatever objects is objecting to the CHEST move alone.
    TArray<FBreakerCoverPiece> Narrowed = Cover;
    int32 Moved = 0;
    for (FBreakerCoverPiece& Piece : Narrowed)
    {
        if (Piece.Class != EBreakerCoverClass::ChestHigh) continue;
        Piece.Right = FMath::Sign(Piece.Right) * 500.0f;
        ++Moved;
    }
    TestEqual(TEXT("every chest pair moved"), Moved, 10);

    FString Reason;
    const bool bNarrowedLegal = UBreakerCoverLayoutLibrary::IsLayoutLegal(Narrowed, Params, Reason);
    TestFalse(FString::Printf(TEXT("a yard with its chest pairs at +-5 m is ILLEGAL (got: %s)"),
        bNarrowedLegal ? TEXT("legal") : *Reason), bNarrowedLegal);

    // AND THE RULE THAT OBJECTS IS THE CORRIDOR ONE. Asserting only that it
    // went red would pass if some unrelated rule caught it by accident, which
    // would leave the lane's actual guard still unidentified.
    TestTrue(FString::Printf(TEXT("the corridor rule is what refuses it, not the lane rule: %s"), *Reason),
        Reason.Contains(TEXT("corridor")));

    // The full-height lane measurement is INSENSITIVE to that move, which is
    // the other half of the finding: it is not the guard here and never was.
    TestEqual(TEXT("the full-height lane figure does not move when chest cover does"),
        UBreakerCoverLayoutLibrary::MinimumOpenLaneWidth(Narrowed, Params),
        UBreakerCoverLayoutLibrary::MinimumOpenLaneWidth(Cover, Params));

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerFernhallRegistrationTest,
    "RiorsEdge.Zone.Fernhall.MapRegistered",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerFernhallRegistrationTest::RunTest(const FString& Parameters)
{
    // THE FALL-THROUGH RULE, held. IsGymMap treats every unnamed map as the
    // gym — load-bearing for the harness, and exactly why a new named map
    // that is not excluded silently fills with targets and a boss key. This
    // is the assertion that Fernhall was excluded, plus both sides of the
    // rule so a future edit that breaks the fallback shows up too.
    TestFalse(TEXT("Fernhall is not the gym"),
        UBreakerGameInstance::IsGymMapName(UBreakerGameInstance::FernhallMapName()));
    TestFalse(TEXT("the front end is not the gym"),
        UBreakerGameInstance::IsGymMapName(UBreakerGameInstance::FrontEndMapName()));
    TestFalse(TEXT("the anchor is not the gym"),
        UBreakerGameInstance::IsGymMapName(UBreakerGameInstance::AnchorMapName()));
    TestTrue(TEXT("the gym is the gym"),
        UBreakerGameInstance::IsGymMapName(UBreakerGameInstance::GymMapName()));
    TestTrue(TEXT("an unnamed map falls through to the gym"),
        UBreakerGameInstance::IsGymMapName(TEXT("Lvl_SomethingUnregistered")));

    // Reachability: the destination is in the shipped travel registry and
    // enabled, so the Anchor's gate actually offers the yard.
    FBreakerTravelDestination Destination;
    TestTrue(TEXT("Fernhall is a registered travel destination"),
        ABreakerTravelPoint::FindDestination(ABreakerTravelPoint::FernhallDestinationId, Destination));
    TestTrue(TEXT("the Fernhall destination is enabled"), Destination.bEnabled);
    return true;
}
