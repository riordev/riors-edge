#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Game/BreakerCoverRegistry.h"

// THE ARENA LAYOUT — Game/BreakerCoverRegistry.h.
//
// The gym's cover field, proved as maths. The precedent is
// Game/BreakerWaveBudget.h and Combat/BreakerCoverBehavior.h and the reason is
// the same one both of those give: WHERE a thing stands is a decision, and a
// decision with a UWorld in it is a decision nobody can test.
//
// WHAT THESE TESTS CANNOT SEE, stated up front because a green suite that quietly
// covers a third of the work is worse than a red one:
//
//   * THE SPAWNING. ABreakerGameMode::SpawnCoverField turns each
//     FBreakerCoverPiece into an AStaticMeshActor at Frame.At(Forward, Right, z)
//     and registers it. That needs a UWorld and a field frame and is not
//     covered here. What IS covered is every number it consumes.
//   * WHETHER A TRACE IS ACTUALLY BLOCKED. "Full-height cover breaks a LATTICE
//     line" is an argument from the height of the piece against the height of a
//     character, not a measurement of the engine's collision. The heights are
//     asserted; the traces are not.
//   * WHETHER ANY OF IT IS FUN, or whether 400 cm is the right height, or
//     whether the field reads at speed. O2: every value is a PLACEHOLDER and
//     automation cannot see a layout. That is what the capture harness is for.
//   * THE POCKETS AT +/-6050 cm. The second and third combat pockets sit outside
//     the contested band and on top of the sniper and wall-ride lanes — a
//     pre-existing collision this lane did not create. They carry their authored
//     cluster; their coverage is not part of the band assertions.
//   * GROUND SNAPPING. Enemies snap to ground every tick. Cover pieces are
//     vertical-sided boxes with no walkable ledge between 120 and 400 cm, which
//     is an argument that nothing can be stranded on one, not a proof.

// The params the gym actually ships, mirroring ABreakerGameMode::
// MakeCoverFieldParams. Kept in one place so every test below argues about the
// same field rather than about four slightly different ones.
static FBreakerCoverFieldParams MakeGymParams()
{
    FBreakerCoverFieldParams Params;
    // Transported grammar (Level-Design §3), at the values the game mode holds.
    Params.CoverPitchMaxCm = 1700.0f;
    Params.DashCorridorWidthCm = 1600.0f;
    Params.CombatPocketRadiusCm = 2000.0f;
    Params.RangedSightlineDepthCm = 2800.0f;
    Params.SafeZoneRadiusCm = 1800.0f;
    Params.ArenaDistanceCm = 17000.0f;

    Params.PocketInnerRingRadiusCm = Params.CoverPitchMaxCm * 0.5f;
    Params.BandNearCm = 4000.0f;                 // RangeFiringLineDistance 4600 - 600
    Params.BandFarCm = 19000.0f;                 // ArenaDistance + CombatPocketRadius
    Params.CorridorNearCm = 4000.0f;
    Params.CorridorFarCm = 10500.0f;             // EncounterPocketDistance + CombatPocketRadius
    Params.CorridorShoulderPitchCm = Params.CoverPitchMaxCm;

    Params.CorridorPocketCentres.Add(FVector2D(8500.0f, 0.0f));
    Params.PocketCentres.Add(FVector2D(12900.0f, 6050.0f));
    Params.PocketCentres.Add(FVector2D(9000.0f, -6050.0f));

    Params.JumpRunNearCm = 10700.0f;
    // 16000, not the run's true 16400 far edge: the elite arena's marker ring
    // starts at 15000, so the jump-gap run and the arena PHYSICALLY OVERLAP in
    // the field. A box drawn to the run's true edge swallows the arena's own
    // §3.3 pillars. Reported as a station collision, not papered over.
    Params.JumpRunFarCm = 16000.0f;
    Params.JumpRunHalfWidthCm = 4160.0f;
    Params.SniperLaneRightCm = -6820.0f;
    Params.SniperLaneHalfWidthCm = 800.0f;
    Params.WallLaneRightCm = 6820.0f;
    Params.WallLaneHalfWidthCm = 400.0f;
    Params.LaneCoverForwardCm = 5900.0f;
    Params.LaneCoverRightCm = -6320.0f;
    return Params;
}

// =========================================================================
//  1. THE FIELD EXISTS, AND IT EXISTS IN BOTH CLASSES
// =========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerArenaLayoutCoverExistsTest,
    "RiorsEdge.Game.ArenaLayout.CoverExists",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerArenaLayoutCoverExistsTest::RunTest(const FString& Parameters)
{
    using ELib = UBreakerCoverLayoutLibrary;
    const FBreakerCoverFieldParams Params = MakeGymParams();
    const TArray<FBreakerCoverPiece> Pieces = ELib::BuildCoverField(Params);

    // The shipped field is 106 pieces. The exact number is not the assertion —
    // it moves whenever a station moves — but "a field with cover in it" and
    // "a field with BOTH KINDS of cover in it" both are. A field of only
    // chest-high pieces satisfies the cover pitch and still has no Skirmisher
    // in it, because a 120 cm block does not break a line of sight.
    TestTrue(TEXT("the field builds hard cover at all"), Pieces.Num() > 20);
    TestTrue(TEXT("there is chest-high cover to crouch behind"),
        ELib::CountOfClass(Pieces, EBreakerCoverClass::ChestHigh) >= 20);
    TestTrue(TEXT("there are line breaks to hide behind"),
        ELib::CountOfClass(Pieces, EBreakerCoverClass::FullHeight) >= 8);

    // The heights are the design. 120 hides a crouched capsule and leaves a
    // standing one's sights clear, and is under MantleStepHeight 145 so it is
    // climbable. 400 clears standing height 176 plus jump apex 178 = 354, so it
    // cannot be seen over at the top of a jump.
    for (const FBreakerCoverPiece& Piece : Pieces)
    {
        if (Piece.Class == EBreakerCoverClass::ChestHigh)
        {
            TestEqual(TEXT("chest-high cover is 120 cm"), Piece.HeightCm, 120.0f);
        }
        else
        {
            TestTrue(TEXT("a line break clears standing height plus a jump apex (354 cm)"),
                Piece.HeightCm >= 354.0f);
        }
    }

    // Determinism. An F1 reset rebuilds the enemies and not the geometry, and a
    // screenshot taken two sessions ago has to describe the same ground.
    const TArray<FBreakerCoverPiece> Again = ELib::BuildCoverField(Params);
    TestEqual(TEXT("the same params build the same field"), Again.Num(), Pieces.Num());
    if (Again.Num() == Pieces.Num())
    {
        bool bIdentical = true;
        for (int32 Index = 0; Index < Pieces.Num(); ++Index)
        {
            bIdentical &= FMath::IsNearlyEqual(Again[Index].Forward, Pieces[Index].Forward)
                && FMath::IsNearlyEqual(Again[Index].Right, Pieces[Index].Right)
                && Again[Index].Class == Pieces[Index].Class;
        }
        TestTrue(TEXT("piece for piece, the field is deterministic"), bIdentical);
    }
    return true;
}

// =========================================================================
//  2. NOTHING STANDS WHERE THE INSTRUMENT NEEDS THE GROUND
// =========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerArenaLayoutExclusionsTest,
    "RiorsEdge.Game.ArenaLayout.Exclusions",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerArenaLayoutExclusionsTest::RunTest(const FString& Parameters)
{
    using ELib = UBreakerCoverLayoutLibrary;
    const FBreakerCoverFieldParams Params = MakeGymParams();
    const TArray<FBreakerCoverPiece> Pieces = ELib::BuildCoverField(Params);

    int32 InSafeRing = 0;
    int32 OnMarkerRing = 0;
    int32 InJumpRun = 0;
    int32 InFiringLine = 0;
    int32 FullHeightInCorridor = 0;
    for (const FBreakerCoverPiece& Piece : Pieces)
    {
        const float FromSpawn = FMath::Sqrt(FMath::Square(Piece.Forward) + FMath::Square(Piece.Right));
        if (FromSpawn < Params.SafeZoneRadiusCm + Piece.ExtentCm()) ++InSafeRing;

        const float FromArena = FMath::Sqrt(
            FMath::Square(Piece.Forward - Params.ArenaDistanceCm) + FMath::Square(Piece.Right));
        if (FMath::Abs(FromArena - Params.CombatPocketRadiusCm) < Piece.ExtentCm() + 100.0f) ++OnMarkerRing;

        if (Piece.Forward > Params.JumpRunNearCm - Piece.ExtentCm()
            && Piece.Forward < Params.JumpRunFarCm + Piece.ExtentCm()
            && FMath::Abs(Piece.Right) < Params.JumpRunHalfWidthCm + Piece.ExtentCm()) ++InJumpRun;

        const bool bCorridorSpan = Piece.Forward > Params.CorridorNearCm && Piece.Forward < Params.CorridorFarCm;
        if (bCorridorSpan && FMath::Abs(Piece.Right) < Params.CorridorHalfWidthCm) ++InFiringLine;
        if (bCorridorSpan && Piece.Class == EBreakerCoverClass::FullHeight
            && FMath::Abs(Piece.Right) < Params.CorridorShoulderOffsetCm + Params.ChestHalfLengthCm)
        {
            ++FullHeightInCorridor;
        }
    }

    // The brief's two hard prohibitions, and the instrument's own.
    TestEqual(TEXT("nothing stands inside the safe ring"), InSafeRing, 0);
    TestEqual(TEXT("nothing stands on the arena marker ring"), OnMarkerRing, 0);
    TestEqual(TEXT("nothing stands in the jump-gap run"), InJumpRun, 0);
    // The TTK sampler shoots down this corridor at four dummies between 1200 and
    // 4500 cm from the firing line. A block in it is a falloff reading taken
    // against a wall.
    TestEqual(TEXT("nothing stands in the firing line's corridor"), InFiringLine, 0);
    // And nothing full-height anywhere in it, which is also the player's line to
    // the standing encounter on arrival.
    TestEqual(TEXT("no line break blocks the corridor or the arrival read"), FullHeightInCorridor, 0);

    // The movement lanes are instruments too, and both are measured in a
    // straight line. The sniper lane's own authored hard-cover piece is the one
    // deliberate exception and is placed by the layout, not despite it.
    int32 OnMovementLane = 0;
    for (const FBreakerCoverPiece& Piece : Pieces)
    {
        const bool bOnWallLane = FMath::Abs(Piece.Right - Params.WallLaneRightCm)
            < Params.WallLaneHalfWidthCm + Piece.ExtentCm();
        if (bOnWallLane) ++OnMovementLane;
    }
    TestEqual(TEXT("nothing stands in the wall-ride corridor"), OnMovementLane, 0);
    return true;
}

// =========================================================================
//  3. THE INTERLEAVE — cover close enough, lanes wide enough
// =========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerArenaLayoutInterleaveTest,
    "RiorsEdge.Game.ArenaLayout.Interleave",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerArenaLayoutInterleaveTest::RunTest(const FString& Parameters)
{
    using ELib = UBreakerCoverLayoutLibrary;
    const FBreakerCoverFieldParams Params = MakeGymParams();
    const TArray<FBreakerCoverPiece> Pieces = ELib::BuildCoverField(Params);

    // G23, THE EXPOSED CROSSING. A player's whole warning against a LATTICE is
    // WindupSeconds 0.85 plus 900/1100 of flight = 1.67 s, and a sprint covers
    // 1837 cm in that. Ground further than that from cover is ground where a
    // telegraph cannot be answered by moving, and under O1 movement is the only
    // active defence — so it is unanswerable damage, not difficulty. 1700 is the
    // authored margin under the 1837 limit, and the field is built to the margin.
    const float Gap = ELib::LargestUncoveredGap(Pieces, Params);
    TestTrue(FString::Printf(TEXT("largest uncovered gap %.0f cm is inside the cover pitch of %.0f"),
        Gap, Params.CoverPitchMaxCm), Gap <= Params.CoverPitchMaxCm);

    // G3, THE MAZE. Only full-height pieces close a lane: chest-high cover is
    // 120 cm, under MantleStepHeight 145, so a player goes over it rather than
    // around it. Two line breaks closer together than a dash corridor is a gate
    // the movement kit cannot take at speed.
    const float Lane = ELib::MinimumOpenLaneWidth(Pieces, Params);
    TestTrue(FString::Printf(TEXT("narrowest lane between line breaks %.0f cm clears the dash corridor width %.0f"),
        Lane, Params.DashCorridorWidthCm), Lane >= Params.DashCorridorWidthCm);

    // Somewhere to break the line, everywhere a fight can happen. Deliberately
    // looser than the cover pitch: chest-high cover is answered by crouching and
    // a line break is answered by choosing a route, and a route decision every
    // 17 m is a maze. Encounter-Design §3.3 gives a whole 4000 x 4000 boss arena
    // exactly two.
    const float LineBreak = ELib::LargestGapToLineBreak(Pieces, Params);
    TestTrue(FString::Printf(TEXT("furthest ground from a line break %.0f cm is inside the limit %.0f"),
        LineBreak, Params.MaximumLineBreakGapCm), LineBreak <= Params.MaximumLineBreakGapCm);

    // THE RATIO. A coarse rail rather than the design: the two measurements
    // above are what actually say "pockets with lanes between them". This
    // catches the two ways a parameter edit destroys that without moving either
    // of them — someone doubles the piece size, or someone deletes a pass.
    const float Fraction = ELib::CoverAreaFraction(Pieces, Params);
    TestTrue(FString::Printf(TEXT("cover occupies %.2f%% of the band, inside %.2f%%-%.2f%%"),
        Fraction * 100.0f, Params.MinimumCoverFraction * 100.0f, Params.MaximumCoverFraction * 100.0f),
        Fraction >= Params.MinimumCoverFraction && Fraction <= Params.MaximumCoverFraction);

    // And the whole rulebook in one call, which is what the game mode logs at
    // spawn time — so a failure here and a warning in the log say the same thing.
    FString Reason;
    const bool bLegal = ELib::IsLayoutLegal(Pieces, Params, Reason);
    TestTrue(FString::Printf(TEXT("the layout is legal: %s"), *Reason), bLegal);
    return true;
}

// =========================================================================
//  4. WAVE MODE CAN FIND THE COVER FROM WHERE IT SPAWNS
// =========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerArenaLayoutWaveReachabilityTest,
    "RiorsEdge.Game.ArenaLayout.WaveReachability",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerArenaLayoutWaveReachabilityTest::RunTest(const FString& Parameters)
{
    using ELib = UBreakerCoverLayoutLibrary;
    const FBreakerCoverFieldParams Params = MakeGymParams();
    const TArray<FBreakerCoverPiece> Pieces = ELib::BuildCoverField(Params);

    // THIS IS THE TEST THE WHOLE PASS EXISTS FOR.
    //
    // StartNextWave does not use the authored arena: it builds its ring at
    // player + forward * DashRefreshDistance, wherever the playtest happens to
    // be standing. Skirmishers go on that ring at 0.8 x CombatPocketRadius and
    // then ask FindCoverAnchorNear for something within CoverPitchMax. Before
    // this pass all the field's cover lived inside four discs of radius 2000 in
    // 250 m of ground, so that question answered "no" almost everywhere and the
    // archetype quietly stopped existing — which is precisely what its own class
    // note warns about and what the spawner logs.
    //
    // So: sweep the band as if a wave were started from every point in it, and
    // check that every Skirmisher ring position lands within reach of an anchor.
    const int32 Samples = 40;
    const float FwdStep = (Params.BandFarCm - Params.BandNearCm) / Samples;
    const float RgtStep = (2.0f * Params.BandHalfWidthCm) / Samples;
    const float RingRadius = Params.CombatPocketRadiusCm * 0.8f;

    float Worst = 0.0f;
    float WorstFwd = 0.0f;
    float WorstRgt = 0.0f;
    int32 Checked = 0;
    for (int32 FwdIndex = 0; FwdIndex <= Samples; ++FwdIndex)
    {
        const float ArenaFwd = Params.BandNearCm + FwdIndex * FwdStep;
        for (int32 RgtIndex = 0; RgtIndex <= Samples; ++RgtIndex)
        {
            const float ArenaRgt = -Params.BandHalfWidthCm + RgtIndex * RgtStep;
            // A wave started from cleared ground — the jump-gap trench, a
            // movement lane, the safe ring — is a playtester standing on a
            // movement instrument, and that ground is open on purpose.
            if (ELib::IsPointInClearedGround(Params, ArenaFwd, ArenaRgt)) continue;
            for (int32 Bearing = 0; Bearing < 8; ++Bearing)
            {
                const float Radians = FMath::DegreesToRadians(45.0f * Bearing + 200.0f);
                const float Fwd = ArenaFwd + FMath::Cos(Radians) * RingRadius;
                const float Rgt = ArenaRgt + FMath::Sin(Radians) * RingRadius;
                if (Fwd < Params.BandNearCm || Fwd > Params.BandFarCm) continue;
                if (FMath::Abs(Rgt) > Params.BandHalfWidthCm) continue;
                if (ELib::IsPointInClearedGround(Params, Fwd, Rgt)) continue;
                ++Checked;
                const float Distance = ELib::DistanceToNearestCover(Pieces, Fwd, Rgt);
                if (Distance > Worst) { Worst = Distance; WorstFwd = Fwd; WorstRgt = Rgt; }
            }
        }
    }
    TestTrue(TEXT("the sweep actually checked something"), Checked > 1000);
    TestTrue(FString::Printf(
        TEXT("worst Skirmisher anchor search over the band is %.0f cm at (%.0f, %.0f), inside the %.0f cm search"),
        Worst, WorstFwd, WorstRgt, Params.CoverPitchMaxCm), Worst <= Params.CoverPitchMaxCm);

    // The standing encounter's own Skirmisher, which SpawnCombatEncounter places
    // on a 55-degree bearing at half a cover pitch from the pocket centre. The
    // encounter sits ON the instrument corridor, so what it finds is one of the
    // corridor's chest-high shoulders or one of the two flank line breaks — but
    // it must find SOMETHING, and before this pass the corridor had nothing.
    {
        const FVector2D Pocket = Params.CorridorPocketCentres[0];
        const float Radians = FMath::DegreesToRadians(55.0f);
        const float Fwd = Pocket.X + FMath::Cos(Radians) * (Params.CoverPitchMaxCm * 0.5f);
        const float Rgt = Pocket.Y + FMath::Sin(Radians) * (Params.CoverPitchMaxCm * 0.5f);
        const float Distance = ELib::DistanceToNearestCover(Pieces, Fwd, Rgt);
        TestTrue(FString::Printf(TEXT("the standing encounter's Skirmisher finds cover at %.0f cm"), Distance),
            Distance <= Params.CoverPitchMaxCm);
    }
    return true;
}

// =========================================================================
//  5. THE ARCHETYPES — a band to hold, a flank to take, a body to fit
// =========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerArenaLayoutArchetypesTest,
    "RiorsEdge.Game.ArenaLayout.Archetypes",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerArenaLayoutArchetypesTest::RunTest(const FString& Parameters)
{
    using ELib = UBreakerCoverLayoutLibrary;
    const FBreakerCoverFieldParams Params = MakeGymParams();
    const TArray<FBreakerCoverPiece> Pieces = ELib::BuildCoverField(Params);

    // THE LATTICE (§2.2) holds 900-1900 cm and needs RangedSightlineDepth (2800)
    // of clear ground to work the whole band instead of backing into geometry.
    // The instrument corridor IS that ground: 6500 cm long, no full-height cover
    // in it at all, with chest-high shoulders on both sides to hold from.
    TestTrue(TEXT("the instrument corridor is longer than a ranged band's depth"),
        Params.CorridorFarCm - Params.CorridorNearCm >= Params.RangedSightlineDepthCm);
    int32 Shoulders = 0;
    for (const FBreakerCoverPiece& Piece : Pieces)
    {
        if (Piece.Class != EBreakerCoverClass::ChestHigh) continue;
        if (Piece.Forward < Params.CorridorNearCm || Piece.Forward > Params.CorridorFarCm) continue;
        if (FMath::Abs(FMath::Abs(Piece.Right) - Params.CorridorShoulderOffsetCm) > 1.0f) continue;
        ++Shoulders;
    }
    TestTrue(TEXT("the corridor has cover on both shoulders to hold the band from"), Shoulders >= 8);

    // THE SEVERED WARDEN (§2.4) punishes approaching FROM THE FRONT, so the
    // whole archetype is the decision to go around it. It stands between the
    // player and the pack on the corridor axis; going around means a lane wide
    // enough to dash down on each side, and the corridor is
    // 2 x CorridorShoulderOffset wide with nothing in it.
    TestTrue(TEXT("the Warden can be flanked at dash speed on the corridor axis"),
        2.0f * Params.CorridorShoulderOffsetCm >= Params.DashCorridorWidthCm);

    // THE SKIRMISHER (§C3) requires a LINE BREAK, not a lump of geometry: a
    // 120 cm block leaves it visible and it becomes a slower Lattice. The
    // standing encounter is on the corridor, which carries no full-height cover,
    // so it is given two flank pieces — and they have to be inside its reach.
    {
        const FVector2D Pocket = Params.CorridorPocketCentres[0];
        const float ToLineBreak = ELib::DistanceToNearestCoverOfClass(
            Pieces, Pocket.X, Pocket.Y, EBreakerCoverClass::FullHeight);
        TestTrue(FString::Printf(TEXT("the encounter pocket has a line break %.0f cm away"), ToLineBreak),
            ToLineBreak <= Params.CoverPitchMaxCm);
    }

    // THE SEVERED DRUDGE. Its capsule is overridden to 120 cm across against the
    // standard 90 — wider than anything else in the project — so a gap narrower
    // than that is a gap it cannot path through, and this is a layout bug that
    // would only ever show up in play. Negative would be worse still: that is
    // two pieces interpenetrating.
    const float Clearance = ELib::MinimumPieceClearance(Pieces);
    TestTrue(FString::Printf(TEXT("no two pieces interpenetrate (tightest gap %.0f cm)"), Clearance),
        Clearance > 0.0f);
    TestTrue(FString::Printf(
        TEXT("the tightest gap in the field is %.0f cm, wider than the SEVERED DRUDGE's %.0f cm body"),
        Clearance, Params.WidestEnemyBodyCm), Clearance >= Params.WidestEnemyBodyCm);

    // THE ELITE ARENA (§3.3). "Two pillars: full-height ... and are the only
    // hard cover. Placed off-centre so a single pillar never covers both
    // galleries", over an open centre floor. The boss orders adds into galleries
    // at +/-1900 and alcoves at +/-1700, so nothing may stand out there.
    int32 ArenaPillars = 0;
    int32 InsideOpenCentre = 0;
    int32 OnGalleryRadius = 0;
    for (const FBreakerCoverPiece& Piece : Pieces)
    {
        const float FromArena = FMath::Sqrt(
            FMath::Square(Piece.Forward - Params.ArenaDistanceCm) + FMath::Square(Piece.Right));
        if (FromArena > Params.CombatPocketRadiusCm) continue;
        if (Piece.Class == EBreakerCoverClass::FullHeight) ++ArenaPillars;
        if (FromArena < 800.0f) ++InsideOpenCentre;
        if (FromArena > 1650.0f) ++OnGalleryRadius;
    }
    TestEqual(TEXT("the arena has exactly two full-height pillars"), ArenaPillars, 2);
    TestEqual(TEXT("the arena's centre floor is open, which is where the boss wants you"), InsideOpenCentre, 0);
    TestEqual(TEXT("nothing stands where the boss orders its adds (alcoves 1700, galleries 1900)"),
        OnGalleryRadius, 0);
    return true;
}

// =========================================================================
//  6. THE REGISTRY — the runtime half
// =========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerArenaLayoutRegistryTest,
    "RiorsEdge.Game.ArenaLayout.Registry",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerArenaLayoutRegistryTest::RunTest(const FString& Parameters)
{
    FBreakerCoverRegistry Registry;
    FBreakerCoverAnchor Found;

    // "There is no cover here" is a real answer, and it is why the Skirmisher
    // spawner falls back to a plain position rather than skipping the spawn.
    TestFalse(TEXT("an empty registry finds nothing"),
        Registry.FindNearest(FVector::ZeroVector, 10000.0f, Found));

    Registry.Add(FVector(1000.0f, 0.0f, 60.0f), EBreakerCoverClass::ChestHigh, 120.0f);
    Registry.Add(FVector(2000.0f, 0.0f, 200.0f), EBreakerCoverClass::FullHeight, 400.0f);

    TestEqual(TEXT("two anchors recorded"), Registry.Num(), 2);
    TestEqual(TEXT("one of each class"), Registry.CountOfClass(EBreakerCoverClass::ChestHigh), 1);
    TestEqual(TEXT("one of each class"), Registry.CountOfClass(EBreakerCoverClass::FullHeight), 1);

    // The nearest anchor is the chest-high one; the nearest LINE BREAK is not,
    // and the Skirmisher asks for the second. Getting this wrong is the whole
    // difference between the archetype existing and not.
    TestTrue(TEXT("the nearest anchor is found"), Registry.FindNearest(FVector::ZeroVector, 5000.0f, Found));
    TestEqual(TEXT("and it is the chest-high one"), Found.HeightCm, 120.0f);
    TestTrue(TEXT("the nearest LINE BREAK is found"),
        Registry.FindNearestOfClass(FVector::ZeroVector, 5000.0f, EBreakerCoverClass::FullHeight, Found));
    TestEqual(TEXT("and it is the 400 cm piece, not the 120"), Found.HeightCm, 400.0f);

    // 2D, deliberately. A full-height piece is registered at its mid-height and
    // a chest block at 60; comparing in 3D would cost the better piece 140 cm of
    // spurious distance and make the field prefer the worse cover.
    TestTrue(TEXT("the search is on the ground plane, not in 3D"),
        Registry.FindNearest(FVector(1000.0f, 0.0f, 9000.0f), 100.0f, Found));

    // Out of range means out of range.
    TestFalse(TEXT("an anchor past MaxDistance is not returned"),
        Registry.FindNearest(FVector(9000.0f, 0.0f, 0.0f), 500.0f, Found));

    Registry.Reset();
    TestEqual(TEXT("a reset registry is empty"), Registry.Num(), 0);
    return true;
}

#endif   // WITH_DEV_AUTOMATION_TESTS
