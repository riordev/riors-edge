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

    // VALIDATED AS A ZONE OF YARDS AND SEAMS, which is what Fernhall now is.
    // Every yard is measured in its OWN frame — anchored at its player start or
    // its yard marker, pointing at its own rift — and the seam between them
    // answers the CONNECTION rule rather than the field one, because a seam is
    // a different kind of space and the field rules would call it triply
    // illegal for being exactly what it is meant to be.
    const TArray<FBreakerZoneField> Zone = UBreakerZoneBuilder::BuildZoneFields(Pieces, Markers);
    const TArray<FBreakerZoneConnection> Seams = UBreakerZoneBuilder::FernhallConnections();

    if (!TestTrue(TEXT("the zone has more than one yard"), Zone.Num() >= 2))
    {
        return false;
    }

    // THE READOUT IS LOGGED PER YARD, whether or not it passes. A zone-level
    // verdict with no per-yard numbers sends the reader to search a world for a
    // figure that belongs to one room — and a figure nobody reads while it is
    // green is a figure free to drift to the edge of its band unremarked.
    for (const FBreakerZoneField& Yard : Zone)
    {
        AddInfo(FString::Printf(TEXT("yard '%s': %s"),
            Yard.Yard.IsNone() ? TEXT("<entry>") : *Yard.Yard.ToString(),
            *UBreakerCoverLayoutLibrary::DescribeCoverField(Yard.Pieces, Yard.Params)));
    }

    FString Reason;
    const bool bLegal = UBreakerCoverLayoutLibrary::IsZoneLegal(Zone, Seams, Reason);
    if (!bLegal)
    {
        AddError(FString::Printf(TEXT("the placed zone is grammar-illegal: %s"), *Reason));
    }

    // EVERY YARD CARRIES COVER OF ITS OWN. Pieces are assigned to yards by
    // GEOMETRY, so a yard whose lattice all landed in another's bucket would
    // pass the field rules by being empty of the things they measure — which
    // is the failure a geometric assignment invites and the reason this is
    // asserted rather than assumed.
    for (const FBreakerZoneField& Yard : Zone)
    {
        TestTrue(FString::Printf(TEXT("yard '%s' has cover of its own"),
            Yard.Yard.IsNone() ? TEXT("<entry>") : *Yard.Yard.ToString()), Yard.Pieces.Num() > 0);
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

    // The composer's roster: 113 meshes across TWO yards and the seam between
    // them, of which 32 are measured cover \u2014 16 per yard, the same lattice in
    // each yard's own frame. A drifted count means the composer and this file
    // disagree about what the zone IS, and re-authoring both is the deliberate
    // act rather than the accident.
    TestEqual(TEXT("imported piece count"), Pieces.Num(), 113);

    const TArray<FBreakerZoneField> Zone = UBreakerZoneBuilder::BuildZoneFields(Pieces, Markers);
    TestEqual(TEXT("the zone has two yards"), Zone.Num(), 2);

    int32 TotalCover = 0;
    for (const FBreakerZoneField& Yard : Zone)
    {
        TotalCover += Yard.Pieces.Num();
        const FString YardName = Yard.Yard.IsNone() ? TEXT("<entry>") : Yard.Yard.ToString();
        TestEqual(FString::Printf(TEXT("yard '%s' measured cover"), *YardName), Yard.Pieces.Num(), 16);
        TestEqual(FString::Printf(TEXT("yard '%s' chest-high pieces"), *YardName),
            UBreakerCoverLayoutLibrary::CountOfClass(Yard.Pieces, EBreakerCoverClass::ChestHigh), 10);
        TestEqual(FString::Printf(TEXT("yard '%s' full-height pieces"), *YardName),
            UBreakerCoverLayoutLibrary::CountOfClass(Yard.Pieces, EBreakerCoverClass::FullHeight), 6);
    }
    TestEqual(TEXT("measured cover across the zone"), TotalCover, 32);
    const TArray<FBreakerCoverPiece> Cover = Zone[0].Pieces;

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
    TestEqual(TEXT("the zone authors five markers"), Markers.All.Num(), 5);
    TestEqual(TEXT("two yards means two rift doors"),
        Markers.OfRole(EBreakerZoneMarkerRole::Rift).Num(), 2);
    TestEqual(TEXT("and one anchor for the yard that is not the entry"),
        Markers.OfRole(EBreakerZoneMarkerRole::Yard).Num(), 1);
    TestTrue(TEXT("the substation yard has both an anchor and a door"),
        Markers.Has(EBreakerZoneMarkerRole::Yard, FName(TEXT("substation")))
        && Markers.Has(EBreakerZoneMarkerRole::Rift, FName(TEXT("substation"))));
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

    // The yard anchor parses like any other role, suffix and all.
    TestTrue(TEXT("marker_yard_north parses"),
        UBreakerZoneBuilder::ParseMarkerName(TEXT("marker_yard_north"), Role, Yard));
    TestEqual(TEXT("...as a yard anchor"), static_cast<int32>(Role),
        static_cast<int32>(EBreakerZoneMarkerRole::Yard));
    TestEqual(TEXT("...for the north yard"), Yard, FName(TEXT("north")));

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
    //
    // THE ANCHOR COMES WITH THE YARD. This block used to declare a rift in
    // 'north' and assert the zone complete; the yard-anchor rule correctly made
    // it red, because a yard nothing anchors has no frame. Naming a yard and
    // anchoring it are one act, so the test does both — the point it was making
    // (a rift outside the entry yard is fine) survives intact.
    One.All.Add({ EBreakerZoneMarkerRole::Yard, FName(TEXT("north")), FVector(80.0f, 0.0f, 0.0f) });
    One.All.Add({ EBreakerZoneMarkerRole::Rift, FName(TEXT("north")), FVector(100.0f, 0.0f, 0.0f) });
    TestTrue(TEXT("a rift in another anchored yard is fine"), One.IsComplete(Reason));
    TestTrue(TEXT("and it is found by its yard"), One.Has(EBreakerZoneMarkerRole::Rift, FName(TEXT("north"))));
    TestFalse(TEXT("and is NOT found in the entry yard"), One.Has(EBreakerZoneMarkerRole::Rift));

    FBreakerZoneMarkers Two;
    Two.All.Add({ EBreakerZoneMarkerRole::PlayerStart, NAME_None, FVector::ZeroVector });
    Two.All.Add({ EBreakerZoneMarkerRole::PlayerStart, FName(TEXT("north")), FVector::ZeroVector });
    TestFalse(TEXT("two player starts is a broken export, even in different yards"),
        Two.IsComplete(Reason));

    // THE YARD ANCHOR (ruled, shape one). A yard that a door names but nothing
    // anchors has no frame to be measured in, so its grammar would be measured
    // in the ENTRY yard's and pass while meaning nothing — a missing anchor
    // arriving as a passing test is the failure this clause exists to stop.
    FBreakerZoneMarkers Unanchored;
    Unanchored.All.Add({ EBreakerZoneMarkerRole::PlayerStart, NAME_None, FVector::ZeroVector });
    Unanchored.All.Add({ EBreakerZoneMarkerRole::Rift, FName(TEXT("north")), FVector(100.0f, 0.0f, 0.0f) });
    TestFalse(TEXT("a named yard with no anchor is refused"), Unanchored.IsComplete(Reason));
    TestTrue(FString::Printf(TEXT("and the reason names the yard: %s"), *Reason),
        Reason.Contains(TEXT("north")));

    Unanchored.All.Add({ EBreakerZoneMarkerRole::Yard, FName(TEXT("north")), FVector(80.0f, 0.0f, 0.0f) });
    TestTrue(TEXT("anchored, the same zone is complete"), Unanchored.IsComplete(Reason));

    // THE ENTRY YARD IS EXEMPT, because the player start anchors it. Without
    // this the shipped one-yard export would stop importing.
    FBreakerZoneMarkers EntryOnly;
    EntryOnly.All.Add({ EBreakerZoneMarkerRole::PlayerStart, NAME_None, FVector::ZeroVector });
    EntryOnly.All.Add({ EBreakerZoneMarkerRole::Rift, NAME_None, FVector(100.0f, 0.0f, 0.0f) });
    TestTrue(TEXT("the entry yard needs no yard anchor"), EntryOnly.IsComplete(Reason));

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
    TestEqual(TEXT("every chest pair in this yard moved"), Moved, 10);

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

// SPAWN CONTAINMENT, against the yard's REAL dimensions (Part One-L/One-Q).
// The owner watched enemies spawn outside the tileset and walk in; once the
// yard IS the rift interior, every run hits it. These prove the placement is
// inside the field rather than merely offset from the player — which is the
// distinction the old spawner could not make, because it had no boundary.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerSpawnContainmentTest,
    "RiorsEdge.Zone.Fernhall.SpawnContainment",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerSpawnContainmentTest::RunTest(const FString& Parameters)
{
    const FBreakerCoverFieldParams Params = UBreakerZoneBuilder::FernhallFieldParams();
    const float BandMin = 1500.0f;
    const float BandMax = 4000.0f;
    const float PackRadius = 900.0f;

    // The pack must sit inside the band with its own radius to spare, from
    // anywhere in the yard and facing anywhere. THE SWEEP IS THE TEST: the
    // defect was intermittent precisely because it depended on position and
    // facing, so a single sample would have passed while the yard was broken.
    int32 Placements = 0;
    int32 Starved = 0;
    for (float F = Params.BandNearCm; F <= Params.BandFarCm; F += 500.0f)
    {
        for (float R = -Params.BandHalfWidthCm; R <= Params.BandHalfWidthCm; R += 500.0f)
        {
            for (int32 Degrees = 0; Degrees < 360; Degrees += 15)
            {
                const float Theta = FMath::DegreesToRadians(static_cast<float>(Degrees));
                float CentreF = 0.0f;
                float CentreR = 0.0f;
                float Afforded = 0.0f;
                const bool bFits = UBreakerCoverLayoutLibrary::SolveContainedSpawnCentre(
                    Params, F, R, FMath::Cos(Theta), FMath::Sin(Theta),
                    BandMin, BandMax, PackRadius, CentreF, CentreR, Afforded);
                ++Placements;
                if (!bFits) ++Starved;

                // THE INVARIANT THAT MATTERS, asserted for EVERY placement
                // including the starved ones: whatever the yard afforded, the
                // pack is inside the field. A fallback that placed the pack
                // outside would be the original defect wearing a log line.
                const bool bInside =
                    CentreF >= Params.BandNearCm + PackRadius - 1.0f
                    && CentreF <= Params.BandFarCm - PackRadius + 1.0f
                    && FMath::Abs(CentreR) <= Params.BandHalfWidthCm - PackRadius + 1.0f;
                if (!bInside)
                {
                    AddError(FString::Printf(
                        TEXT("pack centre (%.0f, %.0f) is outside the field from (%.0f, %.0f) facing %d deg"),
                        CentreF, CentreR, F, R, Degrees));
                    return false;
                }
            }
        }
    }

    TestTrue(TEXT("the sweep actually placed packs"), Placements > 1000);

    // I PREDICTED THIS WOULD STARVE AND IT DOES NOT. The lane's report told the
    // seat the 100 x 50 yard was already the case where a yard cannot hold the
    // authored band; this assertion was written expecting Starved > 0 and went
    // red. THE PREDICTION WAS WRONG FOR A REASON WORTH KEEPING: it was true
    // only while direction was FORCED to the player's facing, which is exactly
    // the defect containment removes. Once a heading may rotate, a 50 m width
    // affords a 15 m floor from everywhere in the yard — the yard was never too
    // small, the spawner was too rigid.
    //
    // So this is pinned the strong way round: the shipped yard NEVER starves.
    // It goes red if the band's floor rises past what the yard can hold or a
    // future yard is authored smaller, which is the build-time signal the
    // report asked for, arriving from a passing test rather than a design
    // argument.
    TestEqual(TEXT("the shipped yard affords the authored band from every position and facing"),
        Starved, 0);

    // The facing is HONOURED where the yard affords it: standing near the near
    // edge looking down the long axis is the case with the most room, and the
    // solved centre should sit ahead of the player rather than off to a side.
    float AheadF = 0.0f;
    float AheadR = 0.0f;
    float AheadAfforded = 0.0f;
    const bool bAheadFits = UBreakerCoverLayoutLibrary::SolveContainedSpawnCentre(
        Params, Params.BandNearCm + 500.0f, 0.0f, 1.0f, 0.0f,
        BandMin, BandMax, PackRadius, AheadF, AheadR, AheadAfforded);
    TestTrue(TEXT("the long axis affords the band"), bAheadFits);
    TestTrue(TEXT("and the pack lands ahead of the player, not beside them"),
        AheadF > Params.BandNearCm + 500.0f && FMath::Abs(AheadR) < 1.0f);
    return true;
}
