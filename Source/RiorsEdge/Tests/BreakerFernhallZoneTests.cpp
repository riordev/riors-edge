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
    const FBreakerZoneMarker* Start = Markers.Find(EBreakerZoneMarkerRole::PlayerStart);
    const FBreakerZoneMarker* Rift = Markers.Find(EBreakerZoneMarkerRole::Rift);
    if (TestTrue(TEXT("the entry yard has a player start and a rift"), Start != nullptr && Rift != nullptr))
    {
        TestTrue(TEXT("the rift is over 80 m downrange of the player start"),
            FVector::Dist2D(Rift->Location, Start->Location) > 8000.0f);
    }

    // THE SHIPPED YARD IS A ONE-YARD ZONE, and it says so rather than leaving
    // it implied. Every marker belongs to the entry yard (no name suffix),
    // which is what keeps the pre-yards export valid unchanged — and this
    // assertion is what will move, deliberately, on the day a second yard is
    // authored.
    TestEqual(TEXT("the yard authors three markers"), Markers.All.Num(), 3);
    for (const FBreakerZoneMarker& Marker : Markers.All)
    {
        TestTrue(FString::Printf(TEXT("marker '%s' belongs to the entry yard"),
            UBreakerZoneBuilder::MarkerRoleName(Marker.Role)), Marker.Yard.IsNone());
    }
    TestEqual(TEXT("exactly one rift door is authored today"),
        Markers.OfRole(EBreakerZoneMarkerRole::Rift).Num(), 1);
    return true;
}

// THE NAME CONTRACT ITSELF, world-free. The parser is one of TWO halves of a
// contract — the other is breaker_import_fernhall.py's parse_marker — and the
// two are hand-kept in step, so the cases that would diverge are pinned here
// rather than discovered at an import.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerZoneMarkerNameTest,
    "RiorsEdge.Zone.Markers.NameContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerZoneMarkerNameTest::RunTest(const FString& Parameters)
{
    EBreakerZoneMarkerRole Role;
    FName Yard;

    // Every name authored before yards existed still parses, to the entry
    // yard. If this breaks, the shipped export stops importing.
    TestTrue(TEXT("marker_playerstart parses"),
        UBreakerZoneBuilder::ParseMarkerName(TEXT("marker_playerstart"), Role, Yard));
    TestEqual(TEXT("...as a player start"), static_cast<int32>(Role),
        static_cast<int32>(EBreakerZoneMarkerRole::PlayerStart));
    TestTrue(TEXT("...in the entry yard"), Yard.IsNone());

    // THE UNDERSCORE ROLE, which is the case a shortest-match parse gets
    // wrong: marker_npc_contract read as role 'npc' in yard 'contract' would
    // import the existing yard as one with no contract giver, silently.
    TestTrue(TEXT("marker_npc_contract parses"),
        UBreakerZoneBuilder::ParseMarkerName(TEXT("marker_npc_contract"), Role, Yard));
    TestEqual(TEXT("...as a contract giver, not as 'npc' in a yard called 'contract'"),
        static_cast<int32>(Role), static_cast<int32>(EBreakerZoneMarkerRole::NPCContract));
    TestTrue(TEXT("...in the entry yard"), Yard.IsNone());

    // A yard suffix on the same role.
    TestTrue(TEXT("marker_npc_contract_north parses"),
        UBreakerZoneBuilder::ParseMarkerName(TEXT("marker_npc_contract_north"), Role, Yard));
    TestEqual(TEXT("...as a contract giver"), static_cast<int32>(Role),
        static_cast<int32>(EBreakerZoneMarkerRole::NPCContract));
    TestEqual(TEXT("...in the north yard"), Yard, FName(TEXT("north")));

    TestTrue(TEXT("marker_rift_north parses"),
        UBreakerZoneBuilder::ParseMarkerName(TEXT("marker_rift_north"), Role, Yard));
    TestEqual(TEXT("...in the north yard"), Yard, FName(TEXT("north")));

    // THE BOUNDARY. Without requiring the role to end at '_' or at the end of
    // the name, 'rift' matches 'riftpad' and a floor piece becomes a marker.
    TestFalse(TEXT("marker_riftpad is not a rift marker"),
        UBreakerZoneBuilder::ParseMarkerName(TEXT("marker_riftpad"), Role, Yard));
    TestFalse(TEXT("a non-marker name is not a marker"),
        UBreakerZoneBuilder::ParseMarkerName(TEXT("flr_riftpad"), Role, Yard));
    TestFalse(TEXT("an unknown role is refused rather than guessed"),
        UBreakerZoneBuilder::ParseMarkerName(TEXT("marker_bogus"), Role, Yard));

    // --- The completeness rule ------------------------------------------
    FString Reason;
    FBreakerZoneMarkers Empty;
    TestFalse(TEXT("a zone with no player start is refused"), Empty.IsComplete(Reason));

    FBreakerZoneMarkers One;
    One.All.Add({ EBreakerZoneMarkerRole::PlayerStart, NAME_None, FVector::ZeroVector });
    TestTrue(TEXT("a player start alone is a complete zone: doors and givers are optional"),
        One.IsComplete(Reason));

    // A yard with no rift is LEGAL now. This is the assertion that says the
    // all-or-nothing rule is really gone, rather than moved.
    One.All.Add({ EBreakerZoneMarkerRole::Rift, FName(TEXT("north")), FVector(100.0f, 0.0f, 0.0f) });
    TestTrue(TEXT("a rift in another yard is fine"), One.IsComplete(Reason));
    TestTrue(TEXT("and it is found by its yard"), One.Has(EBreakerZoneMarkerRole::Rift, FName(TEXT("north"))));
    TestFalse(TEXT("and is NOT found in the entry yard"), One.Has(EBreakerZoneMarkerRole::Rift));

    FBreakerZoneMarkers Two;
    Two.All.Add({ EBreakerZoneMarkerRole::PlayerStart, NAME_None, FVector::ZeroVector });
    Two.All.Add({ EBreakerZoneMarkerRole::PlayerStart, FName(TEXT("north")), FVector::ZeroVector });
    TestFalse(TEXT("two player starts is a broken export, even in different yards"),
        Two.IsComplete(Reason));

    FBreakerZoneMarkers Dup;
    Dup.All.Add({ EBreakerZoneMarkerRole::PlayerStart, NAME_None, FVector::ZeroVector });
    Dup.All.Add({ EBreakerZoneMarkerRole::Rift, NAME_None, FVector::ZeroVector });
    Dup.All.Add({ EBreakerZoneMarkerRole::Rift, NAME_None, FVector(50.0f, 0.0f, 0.0f) });
    TestFalse(TEXT("two rift doors in ONE yard is a naming mistake, not two doors"),
        Dup.IsComplete(Reason));
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
