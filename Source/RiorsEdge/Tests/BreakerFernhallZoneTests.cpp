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
#include "Misc/ScopeExit.h"

#include "AbilitySystemComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerCombatComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Game/BreakerCoverRegistry.h"
#include "Game/BreakerGameInstance.h"
#include "Game/BreakerZoneBuilder.h"
#include "Game/BreakerFernhallSurfaceRepair.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "Interaction/BreakerRiftDoor.h"
#include "Interaction/BreakerTravelPoint.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Save/BreakerMissionContent.h"
#include "Save/BreakerQuestJournal.h"

// THE COMPOSER'S ROSTER, in one place. The count is set from the export
// (breaker_import_fernhall.py's EXPECTED_TOTAL is the same figure, kept by
// hand) and moves only when the zone is deliberately re-authored; a drifted
// count means the composer and this file disagree about what the zone IS.
// One line to change, so the pin cannot be half-moved.
//
// 631 is the four-yard figure before O274's openings; 645 carries the
// fourteen `marker_spawn_*` cubes the composer authors as patrol returns.
// THIS FIGURE MOVES WITH THE COMPOSER'S ROSTER — the count printed at the
// foot of Scripts/compose_fernhall.py is the same number, and a pass that
// adds pieces (O278's stair treads, for one) moves both in the same commit.
// 721 -> 793 with the APRONS (O285): two raised aprons across the lane in
// each of the four yards, each a top and four two-tread runs — nine pieces
// an apron, seventy-two in all. Not one cover box moved.
static constexpr int32 BreakerFernhallExpectedPieceCount = 793;
// THE OPENINGS (O274), by the same discipline: one figure, set from the
// export, moved only when the composer deliberately authors another mouth.
// Fourteen across four yards — entry 4, substation 4, depot 3, siding 3.
static constexpr int32 BreakerFernhallExpectedSpawnSiteCount = 14;
// And no yard has fewer than a bay mouth and a dock: the two openings every
// yard's work pass built, so every yard has at least two places a patrol can
// come back from before any tear is placed.
static constexpr int32 BreakerFernhallMinSpawnSitesPerYard = 2;

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
    // THREE SEAMS FOR FOUR YARDS (O276): plaza-substation, substation-depot,
    // and plaza-siding off the entry plaza's west flank. Pinned so a fourth
    // yard cannot arrive without its seam, or a seam without its yard — the
    // zone rule below refuses a seam to a yard nobody authored.
    TestEqual(TEXT("three seams join the four yards"), Seams.Num(), 3);

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

    // The composer's roster: BreakerFernhallExpectedPieceCount meshes across
    // FOUR yards and the three seams between them, of which 64 are measured
    // cover — 16 per yard, the same lattice in each yard's own frame. A
    // drifted count means the composer and this file disagree about what the
    // zone IS, and re-authoring both is the deliberate act rather than the
    // accident.
    //
    // 113 -> 167 with the DEPOT, 167 -> 275 with the shape pass (two raised
    // decks, a catwalk, their stairs and their dressing, in each of the three). The per-yard figures did not move by
    // one, which is the point: a third yard authored from the validated frame
    // rather than by eye reproduces the lattice exactly.
    // 275 -> 323 with the gantries and the building masses; 323 -> 453 with the
    // WORK PASS — a perimeter that varies in height, depth and roofline instead
    // of ten copies of one box, plus one enterable bay and one loading dock in
    // each of the three yards. Owner: "it just has random assets that are
    // broken laying in random places ... the goal is to have a decent starting
    // area". Every piece added is wall_, flr_ or dress_; not one measured cover
    // box moved, which is why the grammar tests beside this one did not.
    // 453 -> 472 with the vegetation pass: the six random clumps a yard became
    // trees against the flanks and in the corners with grass at their feet,
    // and the lane stays open because that is where the traffic was.
    // 472 -> the SIDING (O276): a fourth yard off the entry plaza's west
    // flank, authored from the validated frame like the depot was, so the
    // per-yard figures below hold for it unchanged.
    // 631 -> 645 with the OPENINGS (O274): fourteen marker cubes, consumed as
    // transforms and never spawned, at the bay mouths, dock faces and seam
    // mouths a patrol returns through. Not one cover box moved.
    TestEqual(TEXT("imported piece count"), Pieces.Num(), BreakerFernhallExpectedPieceCount);

    const TArray<FBreakerZoneField> Zone = UBreakerZoneBuilder::BuildZoneFields(Pieces, Markers);
    TestEqual(TEXT("the zone has four yards"), Zone.Num(), 4);

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
    TestEqual(TEXT("measured cover across the zone"), TotalCover, 64);
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

    // THE SHIPPED ZONE SAYS HOW MANY YARDS IT IS rather than leaving it
    // implied. The entry yard's markers carry no suffix, which is what keeps
    // the pre-yards export valid unchanged — and these assertions are what
    // move, deliberately, on the day a yard is authored. Eight FIXTURES: the
    // entry yard's player start, rift and contract giver; an anchor and a door
    // for the substation; an anchor alone for the depot; an anchor and a door
    // for the siding. Twenty-two markers overall, and the other fourteen are
    // the OPENINGS (O274), which the extractor keeps apart from All because
    // they are many per yard by design — so this pin stays a pin on the
    // one-per-yard roles and the openings are pinned on their own list.
    TestEqual(TEXT("the zone authors eight fixture markers"), Markers.All.Num(), 8);
    TestEqual(TEXT("and fourteen openings a patrol can return through"),
        Markers.SpawnSites.Num(), BreakerFernhallExpectedSpawnSiteCount);
    for (const FName& Yard : Markers.Yards())
    {
        const TArray<FBreakerZoneMarker> Sites = UBreakerZoneBuilder::SpawnSitesForYard(Markers, Yard);
        TestTrue(FString::Printf(TEXT("yard '%s' authors at least a bay mouth and a dock (%d sites)"),
            Yard.IsNone() ? TEXT("<entry>") : *Yard.ToString(), Sites.Num()),
            Sites.Num() >= BreakerFernhallMinSpawnSitesPerYard);
        // NOT asserted: that each site's location falls in its yard's band.
        // A seam mouth is authored AT the band's edge and YardForPoint's
        // out-of-band fallback is nearest-centre, which can hand a mouth to
        // the neighbour it opens onto; the yard tag in the name is the
        // composer's statement of which pocket uses it and is read as such.
    }
    TestEqual(TEXT("four yards, three rift doors"),
        Markers.OfRole(EBreakerZoneMarkerRole::Rift).Num(), 3);
    TestEqual(TEXT("and an anchor for each yard that is not the entry"),
        Markers.OfRole(EBreakerZoneMarkerRole::Yard).Num(), 3);
    TestTrue(TEXT("the substation yard has both an anchor and a door"),
        Markers.Has(EBreakerZoneMarkerRole::Yard, FName(TEXT("substation")))
        && Markers.Has(EBreakerZoneMarkerRole::Rift, FName(TEXT("substation"))));
    // FOUR YARDS, AND ONLY THREE DOORS. A yard with no rift is a legal yard and
    // the depot is the one: a second door into the substation undercroft
    // would be two ways into one place, and a rift definition for the depot
    // would be an encounter nobody can reach. Its anchor alone gives it a
    // frame, and YardFrame keeps +X when a yard points at no rift — the
    // direction the player is already walking when they leave the second seam.
    TestTrue(TEXT("the depot has an anchor"),
        Markers.Has(EBreakerZoneMarkerRole::Yard, FName(TEXT("depot"))));
    TestFalse(TEXT("and deliberately no door"),
        Markers.Has(EBreakerZoneMarkerRole::Rift, FName(TEXT("depot"))));
    // THE SIDING (O276) is the other kind: a yard WITH a door, off the entry
    // plaza's west flank, so the third door in the world is one seam from the
    // player start rather than three.
    TestTrue(TEXT("the siding has both an anchor and a door"),
        Markers.Has(EBreakerZoneMarkerRole::Yard, FName(TEXT("siding")))
        && Markers.Has(EBreakerZoneMarkerRole::Rift, FName(TEXT("siding"))));
    // Each yard along the road is DEEPER than the one before it, and the
    // siding sits BESIDE the entry yard's rung rather than on the road's: one
    // step over the entry, under the substation. The ORDERING is asserted,
    // not the four magnitudes: the gap is what is authored and every one is O2.
    TestTrue(TEXT("each yard is deeper than the one before it"),
        UBreakerZoneBuilder::FernhallYardAreaLevel(NAME_None)
            < UBreakerZoneBuilder::FernhallYardAreaLevel(FName(TEXT("substation")))
        && UBreakerZoneBuilder::FernhallYardAreaLevel(FName(TEXT("substation")))
            < UBreakerZoneBuilder::FernhallYardAreaLevel(FName(TEXT("depot"))));
    TestTrue(TEXT("the siding is beside the entry yard's level, not above the substation's"),
        UBreakerZoneBuilder::FernhallYardAreaLevel(NAME_None)
            < UBreakerZoneBuilder::FernhallYardAreaLevel(FName(TEXT("siding")))
        && UBreakerZoneBuilder::FernhallYardAreaLevel(FName(TEXT("siding")))
            < UBreakerZoneBuilder::FernhallYardAreaLevel(FName(TEXT("substation"))));
    return true;
}

// ---------------------------------------------------------------------------
// THE SIDE RIFT (O276), as a definition and as a door. The siding is the first
// rift in the world that no mission beat stands in front of: it is authored
// like the others, it is campaign like the others, and it completes on its
// generic terminator because no boss is authored for it. Every one of those
// is asserted, because every one is the kind of thing that is easy to get by
// copying the substation's branch and forgetting to change one line — the
// SAME EncounterId would make the side door a second way into the
// undercroft, and a boss resolving for it would make its terminator wait on a
// fight that never spawns.
//
// The door's gate is exercised against a LIVING CHARACTER WITH AN EMPTY
// JOURNAL, the way the eligibility suite exercises the entry door, because
// the gate is a runtime rule and a world-free read of CanEnterRift's source
// would prove only that nobody typed "siding" into it.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerFernhallSideRiftDefinitionTest,
    "RiorsEdge.World.Fernhall.SideRiftDefinition",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerFernhallSideRiftDefinitionTest::RunTest(const FString& Parameters)
{
    const FBreakerRiftDefinition Siding = UBreakerZoneBuilder::FernhallRiftFor(FName(TEXT("siding")));
    const FBreakerRiftDefinition Entry = UBreakerZoneBuilder::FernhallRiftFor(NAME_None);
    const FBreakerRiftDefinition Substation = UBreakerZoneBuilder::FernhallRiftFor(FName(TEXT("substation")));

    TestTrue(TEXT("the siding's rift is set"), Siding.IsSet());
    TestEqual(TEXT("and campaign, like every rift in the world today"),
        static_cast<int32>(Siding.Tier), static_cast<int32>(EBreakerRiftTier::Campaign));
    TestEqual(TEXT("under its own stable encounter id"), Siding.EncounterId, FName(TEXT("fernhall.siding")));
    TestNotEqual(TEXT("which is not the entry's"), Siding.EncounterId, Entry.EncounterId);
    TestNotEqual(TEXT("nor the substation's"), Siding.EncounterId, Substation.EncounterId);
    TestEqual(TEXT("at the siding's own level, not a copied yard's"),
        Siding.AreaLevel, UBreakerZoneBuilder::FernhallYardAreaLevel(FName(TEXT("siding"))));
    TestFalse(TEXT("it has a name"), Siding.AreaName.IsEmpty());
    TestFalse(TEXT("and an area line"), Siding.AreaLine.IsEmpty());

    // NO MISSION BEAT, NO BOSS. The shipped Data/quests.json is what answers
    // here, so a beat authored against the siding later would turn this red
    // and say so — at which point the rift stops completing on its terminator
    // and this pin moves with the ruling that moved it.
    TestTrue(TEXT("no authored boss resolves for the side rift"),
        UBreakerMissionLibrary::BossForRift(Siding).IsNone());

    // THE DOOR, with nothing in the journal.
    UWorld::InitializationValues Init;
    Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!TestNotNull(TEXT("a bare world"), World)) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
    ABreakerCharacter* Player = World->SpawnActor<ABreakerCharacter>();
    if (!TestNotNull(TEXT("a living character"), Player)) return false;
    UAbilitySystemComponent* ASC = Player->GetAbilitySystemComponent();
    ASC->InitAbilityActorInfo(Player, Player);
    ASC->AddAttributeSetSubobject(Player->GetAttributes());
    Player->GetCombat()->BindAttributes(Player->GetAttributes());
    if (!TestNotNull(TEXT("with a journal"), Player->GetQuestJournal())) return false;
    TestEqual(TEXT("that holds no flags"), Player->GetQuestJournal()->GetFlags().Num(), 0);

    FText Reason = FText::FromString(TEXT("stale"));
    TestTrue(TEXT("the side door opens to a character who has done nothing yet"),
        ABreakerRiftDoor::CanEnterRift(Siding, Player, Reason));
    TestTrue(TEXT("and gives no reason, because there is none"), Reason.IsEmpty());
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

    // --- THE SPAWN ROLE (O274): yard, then index, many per yard -----------
    int32 Index = INDEX_NONE;
    TestTrue(TEXT("marker_spawn_sub_0 parses"),
        UBreakerZoneBuilder::ParseMarkerName(TEXT("marker_spawn_sub_0"), Role, Yard, Index));
    TestEqual(TEXT("...as a spawn site"), static_cast<int32>(Role),
        static_cast<int32>(EBreakerZoneMarkerRole::SpawnSite));
    TestEqual(TEXT("...in the sub yard"), Yard, FName(TEXT("sub")));
    TestEqual(TEXT("...at index 0"), Index, 0);
    TestTrue(TEXT("marker_spawn_sub_1 parses"),
        UBreakerZoneBuilder::ParseMarkerName(TEXT("marker_spawn_sub_1"), Role, Yard, Index));
    TestEqual(TEXT("...in the same yard"), Yard, FName(TEXT("sub")));
    TestEqual(TEXT("...at index 1"), Index, 1);
    // THE ENTRY YARD HAS NO TAG, so its sites are `marker_spawn_<n>` — and
    // the index has to come off BEFORE the yard is read, or this is a yard
    // called "0" that no anchor will ever frame.
    TestTrue(TEXT("marker_spawn_0 parses"),
        UBreakerZoneBuilder::ParseMarkerName(TEXT("marker_spawn_0"), Role, Yard, Index));
    TestEqual(TEXT("...as a spawn site"), static_cast<int32>(Role),
        static_cast<int32>(EBreakerZoneMarkerRole::SpawnSite));
    TestTrue(TEXT("...in the ENTRY yard, not a yard called '0'"), Yard.IsNone());
    TestEqual(TEXT("...at index 0"), Index, 0);
    TestTrue(TEXT("marker_spawn_substation_12 parses with a two-digit index"),
        UBreakerZoneBuilder::ParseMarkerName(TEXT("marker_spawn_substation_12"), Role, Yard, Index));
    TestEqual(TEXT("...in the substation"), Yard, FName(TEXT("substation")));
    TestEqual(TEXT("...at index 12"), Index, 12);
    // No index is refused, loudly: without one, two openings in a yard would
    // be the (role, yard) collision the index exists to avoid.
    TestFalse(TEXT("marker_spawn_sub without an index is refused"),
        UBreakerZoneBuilder::ParseMarkerName(TEXT("marker_spawn_sub"), Role, Yard, Index));
    TestFalse(TEXT("marker_spawn alone is refused"),
        UBreakerZoneBuilder::ParseMarkerName(TEXT("marker_spawn"), Role, Yard, Index));
    TestFalse(TEXT("marker_spawnpad is not a spawn marker"),
        UBreakerZoneBuilder::ParseMarkerName(TEXT("marker_spawnpad"), Role, Yard, Index));
    // The other roles never carry an index, and say so.
    TestTrue(TEXT("marker_rift still parses through the indexed overload"),
        UBreakerZoneBuilder::ParseMarkerName(TEXT("marker_rift"), Role, Yard, Index));
    TestEqual(TEXT("...with no index"), Index, static_cast<int32>(INDEX_NONE));

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

    // MANY OPENINGS IN ONE YARD ARE MANY OPENINGS (O274). The (role, yard)
    // rule is unchanged and never sees them; the (yard, index) rule is what
    // holds them distinct, and a repeated index is the same naming mistake.
    FBreakerZoneMarkers Open;
    Open.All.Add({ EBreakerZoneMarkerRole::PlayerStart, NAME_None, FVector::ZeroVector });
    Open.All.Add({ EBreakerZoneMarkerRole::Yard, FName(TEXT("sub")), FVector(80.0f, 0.0f, 0.0f) });
    Open.SpawnSites.Add({ EBreakerZoneMarkerRole::SpawnSite, FName(TEXT("sub")), FVector(90.0f, 0.0f, 0.0f), 0 });
    Open.SpawnSites.Add({ EBreakerZoneMarkerRole::SpawnSite, FName(TEXT("sub")), FVector(90.0f, 40.0f, 0.0f), 1 });
    Open.SpawnSites.Add({ EBreakerZoneMarkerRole::SpawnSite, NAME_None, FVector(10.0f, 0.0f, 0.0f), 0 });
    TestTrue(FString::Printf(TEXT("two spawn sites in one yard are both accepted: %s"), *Reason),
        Open.IsComplete(Reason));
    TestEqual(TEXT("and both are found by their yard"),
        UBreakerZoneBuilder::SpawnSitesForYard(Open, FName(TEXT("sub"))).Num(), 2);
    TestEqual(TEXT("while the entry yard's is found alone"),
        UBreakerZoneBuilder::SpawnSitesForYard(Open, NAME_None).Num(), 1);
    TestEqual(TEXT("and none are fixtures"), Open.All.Num(), 2);
    // Nearest within reach, on the ground plane, and nothing when out of it.
    const FBreakerZoneMarker* Near = UBreakerZoneBuilder::NearestSpawnSite(
        Open, FName(TEXT("sub")), FVector(95.0f, 30.0f, 500.0f), 100.0f);
    TestTrue(TEXT("the nearest site of the yard is found, ignoring height"), Near && Near->Index == 1);
    TestNull(TEXT("and none is found beyond reach"),
        UBreakerZoneBuilder::NearestSpawnSite(Open, FName(TEXT("sub")), FVector(500.0f, 0.0f, 0.0f), 100.0f));
    TestNull(TEXT("nor in a yard that authored none"),
        UBreakerZoneBuilder::NearestSpawnSite(Open, FName(TEXT("north")), FVector(90.0f, 0.0f, 0.0f), 1000.0f));

    Open.SpawnSites.Add({ EBreakerZoneMarkerRole::SpawnSite, FName(TEXT("sub")), FVector(200.0f, 0.0f, 0.0f), 1 });
    TestFalse(TEXT("two spawn sites with ONE index in one yard is a naming mistake"), Open.IsComplete(Reason));
    TestTrue(FString::Printf(TEXT("and the reason names the index: %s"), *Reason), Reason.Contains(TEXT("1")));

    // A spawn site names a yard the way a door does; an unanchored one is
    // refused for the same reason.
    FBreakerZoneMarkers Unframed;
    Unframed.All.Add({ EBreakerZoneMarkerRole::PlayerStart, NAME_None, FVector::ZeroVector });
    Unframed.SpawnSites.Add({ EBreakerZoneMarkerRole::SpawnSite, FName(TEXT("north")), FVector(90.0f, 0.0f, 0.0f), 0 });
    TestFalse(TEXT("a spawn site in a yard nothing anchors is refused"), Unframed.IsComplete(Reason));
    TestTrue(FString::Printf(TEXT("and the reason names the yard: %s"), *Reason), Reason.Contains(TEXT("north")));
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

// ---------------------------------------------------------------------------
// WHICH YARD A RIFT BUILDS FROM.
//
// The ruined twin is composed by `python Scripts/compose_fernhall.py --ruined`
// into Assets/zones/fernhall_rift.glb and imported to its own mesh folder. The
// rule under test is deliberately the SELECTION and not the current contents,
// because the contents change the day the import lands and a test that pinned
// "the rift folder is empty" would have to be rewritten to allow the feature it
// exists to protect.
//
// The file header above says an absent asset folder is a FAILURE and not a
// skip. That still holds — for the LIVING yard, which is committed. The ruined
// twin is the one case where absence is a legal state, because the fallback
// makes it inert rather than broken: a checkout without the import builds the
// rift exactly as it builds one today.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerFernhallRuinedFolderTest,
    "RiorsEdge.Zone.Fernhall.RuinedFolder",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerFernhallRuinedFolderTest::RunTest(const FString& Parameters)
{
    const FString Living = UBreakerZoneBuilder::FernhallMeshFolder();
    const FString Ruined = UBreakerZoneBuilder::FernhallRiftMeshFolder();
    TestNotEqual(TEXT("the two yards are different folders"), Living, Ruined);

    // The ordinary world never asks for the ruin. Under O268 the rift and the
    // world run different rules on the same geometry, and this is the seam that
    // keeps a cleared yard from inheriting a ruin it was never in.
    TestEqual(TEXT("the ordinary yard builds from the living folder"),
        FString(UBreakerZoneBuilder::FernhallFolderFor(false)), Living);

    // A rift builds from ONE OF THE TWO, and whichever it picks has pieces in
    // it. That is the whole contract: it holds before the import (falls back to
    // the living yard) and after it (takes the ruin), so this assertion does
    // not have to move when the twin lands.
    const FString Chosen = UBreakerZoneBuilder::FernhallFolderFor(true);
    TestTrue(TEXT("a rift builds from one of the two authored yards"),
        Chosen == Living || Chosen == Ruined);
    TArray<FBreakerZonePiece> Pieces;
    TestTrue(TEXT("and never from a folder with nothing in it"),
        UBreakerZoneBuilder::CollectZonePieces(Chosen, Pieces) && Pieces.Num() > 0);
    AddInfo(FString::Printf(TEXT("A rift currently builds from %s (%d pieces)."), *Chosen, Pieces.Num()));
    return true;
}

// ---------------------------------------------------------------------------
// TWO FLOORS ON ONE PLANE IS A FLICKER. Every walkable slab in Fernhall shares
// one top face, so a seam slab that starts at a wall's inner face and runs
// across the yard slab beyond it is a coplanar fight, painted a different
// colour — the ground the owner walked over into the marshalling yard. A seam
// abuts its yard; it never lies on it. Measured over both folders, because the
// ruin is the same floor.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerFernhallFloorsDisjointTest,
    "RiorsEdge.Zone.Fernhall.FloorsDisjoint",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerFernhallFloorsDisjointTest::RunTest(const FString& Parameters)
{
    // Tops within half a centimetre are one plane; an overlap under a
    // centimetre on either axis is an abutment, not a fight.
    constexpr float BreakerFloorSamePlaneCm = 0.5f;
    constexpr float BreakerFloorAbutCm = 1.0f;
    for (const FString& Folder : { UBreakerZoneBuilder::FernhallMeshFolder(), UBreakerZoneBuilder::FernhallRiftMeshFolder() })
    {
        TArray<FBreakerZonePiece> Pieces;
        if (!TestTrue(FString::Printf(TEXT("%s collects"), *Folder), UBreakerZoneBuilder::CollectZonePieces(Folder, Pieces))) return false;
        TArray<const FBreakerZonePiece*> Floors;
        for (const FBreakerZonePiece& Piece : Pieces)
            if (Piece.Name.StartsWith(TEXT("flr_"))) Floors.Add(&Piece);
        TestTrue(FString::Printf(TEXT("%s has floors"), *Folder), Floors.Num() > 0);
        for (int32 A = 0; A < Floors.Num(); ++A)
        {
            for (int32 B = A + 1; B < Floors.Num(); ++B)
            {
                const FBreakerZonePiece& P = *Floors[A];
                const FBreakerZonePiece& Q = *Floors[B];
                const float TopP = P.Origin.Z + P.Extent.Z;
                const float TopQ = Q.Origin.Z + Q.Extent.Z;
                if (FMath::Abs(TopP - TopQ) > BreakerFloorSamePlaneCm) continue;
                const float OverlapX = FMath::Min(P.Origin.X + P.Extent.X, Q.Origin.X + Q.Extent.X) - FMath::Max(P.Origin.X - P.Extent.X, Q.Origin.X - Q.Extent.X);
                const float OverlapY = FMath::Min(P.Origin.Y + P.Extent.Y, Q.Origin.Y + Q.Extent.Y) - FMath::Max(P.Origin.Y - P.Extent.Y, Q.Origin.Y - Q.Extent.Y);
                TestTrue(FString::Printf(TEXT("%s: %s and %s share a plane at Z %.1f and abut rather than overlap (overlap %.0f x %.0f cm)"),
                    *Folder, *P.Name, *Q.Name, TopP, OverlapX, OverlapY),
                    OverlapX <= BreakerFloorAbutCm || OverlapY <= BreakerFloorAbutCm);
            }
        }
    }
    return true;
}

// ---------------------------------------------------------------------------
// EVERY DECK AND DOCK IS REACHED ON FOOT (O278). A step is under 45 cm, and a
// raised slab the composer places is reachable by a stair the movement
// component can climb — not a ramp mesh whose collision is whatever the kit
// shipped, and not four treads at 85 cm that the character walks into like a
// wall. The stair is measured as the composer authors it: a run of solid box
// treads named after the slab they climb to, read through the same
// CollectZonePieces bounds the runtime spawner uses.
//
// THE CEILING IS THE MOVEMENT DEFAULT, READ FROM THE DEFAULT OBJECT. If the
// character's MaxStepHeight is 45 the assertion is 45; a tread the composer
// authors at 46 goes red here, and the fix is the tread, never this file. The
// composer's own rise (35 cm) is deliberately under the ceiling so the stair
// reads as a stair, and that margin is the composer's to keep, not this test's
// to enforce.
//
// Measured over both folders, like FloorsDisjoint, because the ruin is the
// same yard with dressing over it and a dock nobody can climb in the rift is
// the same defect as one nobody can climb in the world.
// ---------------------------------------------------------------------------
namespace
{
    // Signed overlap of two pieces' boxes along one axis: positive is shared
    // span, negative is the gap between them.
    float BreakerFernhallAxisOverlap(const FBreakerZonePiece& P, const FBreakerZonePiece& Q, int32 Axis)
    {
        return FMath::Min(P.Origin[Axis] + P.Extent[Axis], Q.Origin[Axis] + Q.Extent[Axis])
            - FMath::Max(P.Origin[Axis] - P.Extent[Axis], Q.Origin[Axis] - Q.Extent[Axis]);
    }

    // THE NAME CONTRACT with compose_fernhall.py: `flr_<tag>_deck` is climbed
    // by `flr_<tag>_step<i>`, `flr_<tag>_fardeck` by `flr_<tag>_fstep<i>`,
    // `flr_<tag>_dock` by `flr_<tag>_dockstep<i>`. Returns the tread prefix
    // for a slab of one of those three kinds, or empty for any other piece.
    // `_fardeck` is tried before `_deck` because the second is not a suffix
    // of the first only by the underscore, and that is too thin to lean on.
    FString BreakerFernhallTreadPrefixFor(const FString& SlabName)
    {
        if (!SlabName.StartsWith(TEXT("flr_"))) return FString();
        struct FKind { const TCHAR* Suffix; const TCHAR* Treads; };
        static const FKind Kinds[] = {
            { TEXT("_fardeck"), TEXT("_fstep") },
            { TEXT("_deck"), TEXT("_step") },
            { TEXT("_dock"), TEXT("_dockstep") },
        };
        for (const FKind& Kind : Kinds)
        {
            if (SlabName.EndsWith(Kind.Suffix))
            {
                return SlabName.LeftChop(FCString::Strlen(Kind.Suffix)) + Kind.Treads;
            }
        }
        return FString();
    }

    float BreakerFernhallTopZ(const FBreakerZonePiece& Piece)
    {
        return Piece.Origin.Z + Piece.Extent.Z;
    }

    // THE TREADS OF ONE RUN: the pieces named for the slab with a bare index
    // after the prefix — `..._step0`, never `..._stepladder` — sorted by their
    // top face, which is the order a foot meets them.
    void BreakerFernhallCollectTreads(const TArray<FBreakerZonePiece>& Pieces, const FString& TreadPrefix,
        TArray<const FBreakerZonePiece*>& OutTreads)
    {
        OutTreads.Reset();
        for (const FBreakerZonePiece& Piece : Pieces)
        {
            if (Piece.Name.StartsWith(TreadPrefix) && Piece.Name.Mid(TreadPrefix.Len()).IsNumeric()) OutTreads.Add(&Piece);
        }
        OutTreads.Sort([](const FBreakerZonePiece& A, const FBreakerZonePiece& B)
        {
            return BreakerFernhallTopZ(A) < BreakerFernhallTopZ(B);
        });
    }

    // THE GROUND A FIRST TREAD STANDS ON, found rather than assumed at zero:
    // the highest flr_ piece under the tread's top whose footprint holds the
    // tread's centre, skipping whatever the caller names as the stair's own
    // (its slab, its treads). Every yard sits on one plane today, and the day
    // one does not this still measures the step a foot actually takes.
    const FBreakerZonePiece* BreakerFernhallGroundUnder(const TArray<FBreakerZonePiece>& Pieces,
        const FBreakerZonePiece& First, float SamePlaneCm, TFunctionRef<bool(const FBreakerZonePiece&)> IsOwn)
    {
        const FBreakerZonePiece* Ground = nullptr;
        const float FirstTop = BreakerFernhallTopZ(First);
        for (const FBreakerZonePiece& Floor : Pieces)
        {
            if (!Floor.Name.StartsWith(TEXT("flr_")) || IsOwn(Floor)) continue;
            const float FloorTop = BreakerFernhallTopZ(Floor);
            if (FloorTop > FirstTop - SamePlaneCm) continue;
            if (FMath::Abs(First.Origin.X - Floor.Origin.X) > Floor.Extent.X
                || FMath::Abs(First.Origin.Y - Floor.Origin.Y) > Floor.Extent.Y) continue;
            if (!Ground || FloorTop > BreakerFernhallTopZ(*Ground)) Ground = &Floor;
        }
        return Ground;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerFernhallDecksClimbableTest,
    "RiorsEdge.Zone.Fernhall.DecksClimbable",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerFernhallDecksClimbableTest::RunTest(const FString& Parameters)
{
    // Read, never typed: the character's own ceiling on a step.
    const UBreakerCharacterMovementComponent* Movement = GetDefault<UBreakerCharacterMovementComponent>();
    if (!TestNotNull(TEXT("the movement component has a default object"), Movement)) return false;
    const float MaxStepCm = Movement->MaxStepHeight;
    TestTrue(FString::Printf(TEXT("the movement default's MaxStepHeight is a positive figure (%.1f)"), MaxStepCm),
        MaxStepCm > 0.0f);

    // The same tolerances FloorsDisjoint measures by: an overlap or a gap
    // under a centimetre is an abutment, not a fight and not a hole.
    constexpr float BreakerFloorAbutCm = 1.0f;
    constexpr float BreakerFloorSamePlaneCm = 0.5f;

    for (const FString& Folder : { UBreakerZoneBuilder::FernhallMeshFolder(), UBreakerZoneBuilder::FernhallRiftMeshFolder() })
    {
        TArray<FBreakerZonePiece> Pieces;
        if (!TestTrue(FString::Printf(TEXT("%s collects"), *Folder), UBreakerZoneBuilder::CollectZonePieces(Folder, Pieces))) return false;

        TArray<const FBreakerZonePiece*> Solids;
        for (const FBreakerZonePiece& Piece : Pieces)
        {
            if (Piece.Name.StartsWith(TEXT("wall_")) || Piece.Name.StartsWith(TEXT("blk_"))) Solids.Add(&Piece);
        }

        int32 Slabs = 0;
        for (const FBreakerZonePiece& Slab : Pieces)
        {
            const FString TreadPrefix = BreakerFernhallTreadPrefixFor(Slab.Name);
            if (TreadPrefix.IsEmpty()) continue;
            ++Slabs;
            const FString Where = FString::Printf(TEXT("%s: %s"), *Folder, *Slab.Name);
            const float SlabTop = BreakerFernhallTopZ(Slab);

            // (d) The stair exists: at least one tread named for this slab,
            // in the order a foot meets them.
            TArray<const FBreakerZonePiece*> Treads;
            BreakerFernhallCollectTreads(Pieces, TreadPrefix, Treads);
            if (!TestTrue(FString::Printf(TEXT("%s has at least one tread named %s<i> (slab top Z %.1f)"),
                *Where, *TreadPrefix, SlabTop), Treads.Num() > 0))
            {
                continue;
            }
            const FBreakerZonePiece& First = *Treads[0];
            const FBreakerZonePiece& Last = *Treads.Last();

            // (a) THE FIRST RISE IS FROM THE GROUND THE TREAD STANDS ON, found
            // rather than assumed at zero.
            const float FirstTop = BreakerFernhallTopZ(First);
            const FBreakerZonePiece* Ground = BreakerFernhallGroundUnder(Pieces, First, BreakerFloorSamePlaneCm,
                [&](const FBreakerZonePiece& Floor)
                {
                    return &Floor == &Slab
                        || (Floor.Name.StartsWith(TreadPrefix) && Floor.Name.Mid(TreadPrefix.Len()).IsNumeric());
                });
            if (TestNotNull(FString::Printf(TEXT("%s: a floor lies under the first tread %s (top Z %.1f at X %.0f Y %.0f)"),
                *Where, *First.Name, FirstTop, First.Origin.X, First.Origin.Y), Ground))
            {
                const float FirstRise = FirstTop - BreakerFernhallTopZ(*Ground);
                TestTrue(FString::Printf(TEXT("%s: first tread %s rises %.1f cm off %s (top %.1f), within MaxStepHeight %.1f"),
                    *Where, *First.Name, FirstRise, *Ground->Name, BreakerFernhallTopZ(*Ground), MaxStepCm),
                    FirstRise <= MaxStepCm);
            }
            for (int32 i = 1; i < Treads.Num(); ++i)
            {
                const float Rise = BreakerFernhallTopZ(*Treads[i]) - BreakerFernhallTopZ(*Treads[i - 1]);
                TestTrue(FString::Printf(TEXT("%s: %s -> %s rises %.1f cm (%.1f -> %.1f), within MaxStepHeight %.1f"),
                    *Where, *Treads[i - 1]->Name, *Treads[i]->Name, Rise,
                    BreakerFernhallTopZ(*Treads[i - 1]), BreakerFernhallTopZ(*Treads[i]), MaxStepCm),
                    Rise <= MaxStepCm);
            }
            const float LastRise = SlabTop - BreakerFernhallTopZ(Last);
            TestTrue(FString::Printf(TEXT("%s: last tread %s -> slab rises %.1f cm (%.1f -> %.1f), within MaxStepHeight %.1f"),
                *Where, *Last.Name, LastRise, BreakerFernhallTopZ(Last), SlabTop, MaxStepCm),
                LastRise <= MaxStepCm);
            TestTrue(FString::Printf(TEXT("%s: last tread %s is not above the slab it climbs to (%.1f -> %.1f)"),
                *Where, *Last.Name, BreakerFernhallTopZ(Last), SlabTop),
                LastRise >= -BreakerFloorSamePlaneCm);

            // (b) THE LAST TREAD MEETS THE SLAB'S EDGE: a shared span on one
            // axis and an abutment on the other, within a centimetre either
            // way. A tread inside the footprint is under the slab, not at it;
            // a tread a hand's width off is a gap the foot finds first.
            const float OverlapX = BreakerFernhallAxisOverlap(Last, Slab, 0);
            const float OverlapY = BreakerFernhallAxisOverlap(Last, Slab, 1);
            const float Abut = FMath::Min(OverlapX, OverlapY);
            const float Shared = FMath::Max(OverlapX, OverlapY);
            TestTrue(FString::Printf(TEXT("%s: last tread %s touches the slab's near edge within %.0f cm (overlap X %.1f, Y %.1f)"),
                *Where, *Last.Name, BreakerFloorAbutCm, OverlapX, OverlapY),
                Abut >= -BreakerFloorAbutCm && Shared > 0.0f);
            TestTrue(FString::Printf(TEXT("%s: last tread %s does not lie inside the slab footprint (overlap X %.1f, Y %.1f)"),
                *Where, *Last.Name, OverlapX, OverlapY),
                Abut <= BreakerFloorAbutCm);

            // (c) NO TREAD IS INSIDE A WALL OR A COVER BLOCK. The dock's
            // cheeks and front are wall_ and the lattice is blk_; a tread that
            // shares more than a centimetre with either on all three axes is a
            // stair the character cannot stand on.
            for (const FBreakerZonePiece* Tread : Treads)
            {
                for (const FBreakerZonePiece* Solid : Solids)
                {
                    const float X = BreakerFernhallAxisOverlap(*Tread, *Solid, 0);
                    const float Y = BreakerFernhallAxisOverlap(*Tread, *Solid, 1);
                    const float Z = BreakerFernhallAxisOverlap(*Tread, *Solid, 2);
                    const bool bIntersects = X > BreakerFloorAbutCm && Y > BreakerFloorAbutCm && Z > BreakerFloorAbutCm;
                    if (bIntersects)
                    {
                        AddError(FString::Printf(TEXT("%s: tread %s intersects %s (overlap X %.1f, Y %.1f, Z %.1f cm)"),
                            *Where, *Tread->Name, *Solid->Name, X, Y, Z));
                    }
                }
            }

            AddInfo(FString::Printf(TEXT("%s: %d treads, first top %.1f, last top %.1f, slab top %.1f, ceiling %.1f"),
                *Where, Treads.Num(), FirstTop, BreakerFernhallTopZ(Last), SlabTop, MaxStepCm));
        }
        // Not vacuous: the composer authors a deck, a far deck and a dock in
        // every yard, so a folder in which none matched is a folder this test
        // has stopped reading, not a folder with nothing to climb.
        TestTrue(FString::Printf(TEXT("%s has raised slabs to measure (%d)"), *Folder, Slabs), Slabs > 0);
    }
    return true;
}

// ---------------------------------------------------------------------------
// TWO APRONS ACROSS THE LANE, CLIMBED FROM ALL FOUR SIDES (O285). A raised
// apron is a third of a storey up, 8 m along the lane and 18 m across it,
// with a two-tread run on each of its four sides — so there is no side a body
// has to walk round to get up, and no side it cannot drop off and come back
// up. It is sited OFF every chest pair: an apron that straddles a cover pair
// turns two measured chest-high boxes into one mound, and the grammar the
// yard was validated under no longer describes the yard.
//
// THE NAME CONTRACT with compose_fernhall.py: `flr_<tag>_apron<i>` is the
// top, climbed by `flr_<tag>_apron<i>_nstep<j>` and `_fstep<j>` along the
// lane and `_lstep<j>` and `_rstep<j>` across it. The yard's lane axis is
// read off the top itself — 18 m across, 8 m along, so the shorter
// horizontal extent IS the lane — rather than off a marker, because the
// apron's shape is the same fact and this file need not agree with a second
// source about it. The entry yard's tag is `entry` on its slabs and absent
// on its chests (`blk_chest_n00`, `blk_chest_sub_n00`), which is the
// composer's own naming; both spell one yard here.
//
// The rises answer to the movement default, the abutments and the solids to
// the same centimetre DecksClimbable measures by, both folders.
// ---------------------------------------------------------------------------
namespace
{
    // `flr_<tag>_apron<i>` or `flr_apron<i>`: true for a top, never a tread
    // (`_apron0_nstep1` has no bare index after `apron`). OutTag is the yard's
    // tag, empty for a tagless name.
    bool BreakerFernhallIsApronTop(const FString& Name, FString& OutTag)
    {
        if (!Name.StartsWith(TEXT("flr_"))) return false;
        const FString Rest = Name.Mid(4);
        const int32 At = Rest.Find(TEXT("apron"), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
        if (At == INDEX_NONE) return false;
        const FString Index = Rest.Mid(At + 5);
        if (Index.IsEmpty() || !Index.IsNumeric()) return false;
        const FString Prefix = Rest.Left(At);
        if (Prefix.IsEmpty()) { OutTag.Reset(); return true; }
        if (!Prefix.EndsWith(TEXT("_"))) return false;
        OutTag = Prefix.LeftChop(1);
        return true;
    }

    // `blk_chest_[<tag>_](n|s)<NN>`: the yard tag, empty for the entry yard.
    bool BreakerFernhallIsChest(const FString& Name, FString& OutTag)
    {
        static const FString ChestPrefix = TEXT("blk_chest_");
        if (!Name.StartsWith(ChestPrefix)) return false;
        const FString Rest = Name.Mid(ChestPrefix.Len());
        const int32 Cut = Rest.Find(TEXT("_"), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
        OutTag = Cut == INDEX_NONE ? FString() : Rest.Left(Cut);
        return true;
    }

    // One key per yard across both spellings: the composer's shape pass names
    // the entry yard `entry`, its cover pass names it nothing.
    FString BreakerFernhallYardKey(const FString& Tag)
    {
        return Tag == TEXT("entry") ? FString() : Tag;
    }

    const TCHAR* BreakerFernhallYardLabel(const FString& Key)
    {
        return Key.IsEmpty() ? TEXT("<entry>") : *Key;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerFernhallApronsClimbableTest,
    "RiorsEdge.Zone.Fernhall.ApronsClimbable",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerFernhallApronsClimbableTest::RunTest(const FString& Parameters)
{
    // Read, never typed: the character's own ceiling on a step.
    const UBreakerCharacterMovementComponent* Movement = GetDefault<UBreakerCharacterMovementComponent>();
    if (!TestNotNull(TEXT("the movement component has a default object"), Movement)) return false;
    const float MaxStepCm = Movement->MaxStepHeight;
    TestTrue(FString::Printf(TEXT("the movement default's MaxStepHeight is a positive figure (%.1f)"), MaxStepCm),
        MaxStepCm > 0.0f);

    constexpr float BreakerFloorAbutCm = 1.0f;
    constexpr float BreakerFloorSamePlaneCm = 0.5f;
    // O285: two aprons a yard, each 18 m across the lane.
    constexpr int32 BreakerApronsPerYard = 2;
    constexpr float BreakerApronMaxLatHalfCm = 900.0f;

    // The four runs, with the axis each climbs along: n/f along the lane,
    // l/r across it.
    struct FRun { const TCHAR* Suffix; bool bAlongFwd; };
    static const FRun Runs[] = {
        { TEXT("_nstep"), true },
        { TEXT("_fstep"), true },
        { TEXT("_lstep"), false },
        { TEXT("_rstep"), false },
    };

    for (const FString& Folder : { UBreakerZoneBuilder::FernhallMeshFolder(), UBreakerZoneBuilder::FernhallRiftMeshFolder() })
    {
        TArray<FBreakerZonePiece> Pieces;
        if (!TestTrue(FString::Printf(TEXT("%s collects"), *Folder), UBreakerZoneBuilder::CollectZonePieces(Folder, Pieces))) return false;

        TArray<const FBreakerZonePiece*> Solids;
        TMap<FString, TArray<const FBreakerZonePiece*>> ChestsByYard;
        for (const FBreakerZonePiece& Piece : Pieces)
        {
            if (Piece.Name.StartsWith(TEXT("wall_")) || Piece.Name.StartsWith(TEXT("blk_"))) Solids.Add(&Piece);
            FString ChestTag;
            if (BreakerFernhallIsChest(Piece.Name, ChestTag)) ChestsByYard.FindOrAdd(BreakerFernhallYardKey(ChestTag)).Add(&Piece);
        }

        TMap<FString, int32> ApronsByYard;
        int32 Aprons = 0;
        for (const FBreakerZonePiece& Top : Pieces)
        {
            FString Tag;
            if (!BreakerFernhallIsApronTop(Top.Name, Tag)) continue;
            ++Aprons;
            const FString Yard = BreakerFernhallYardKey(Tag);
            ++ApronsByYard.FindOrAdd(Yard);
            const FString Where = FString::Printf(TEXT("%s: %s"), *Folder, *Top.Name);
            const float TopZ = BreakerFernhallTopZ(Top);

            // THE LANE AXIS, off the top's own shape: 8 m along the lane, 18 m
            // across it, so the shorter horizontal extent is fwd. Equal
            // extents would be a square nobody authored, and no axis to read.
            if (!TestTrue(FString::Printf(TEXT("%s: the top is longer across the lane than along it (extent X %.0f, Y %.0f)"),
                *Where, Top.Extent.X, Top.Extent.Y), Top.Extent.X != Top.Extent.Y))
            {
                continue;
            }
            const int32 Fwd = Top.Extent.X < Top.Extent.Y ? 0 : 1;
            const int32 Lat = 1 - Fwd;

            // (d) Shipped shape: the apron never reaches wider across the lane
            // than the ruling gives it.
            TestTrue(FString::Printf(TEXT("%s: lat half-extent %.1f cm is within %.0f"), *Where, Top.Extent[Lat], BreakerApronMaxLatHalfCm),
                Top.Extent[Lat] <= BreakerApronMaxLatHalfCm);

            // The apron's span along the lane, grown by its fwd-axis runs as
            // they are found: the interval a chest pair must lie clear of.
            float FwdMin = Top.Origin[Fwd] - Top.Extent[Fwd];
            float FwdMax = Top.Origin[Fwd] + Top.Extent[Fwd];

            TArray<const FBreakerZonePiece*> AllTreads;
            for (const FRun& Run : Runs)
            {
                const FString TreadPrefix = Top.Name + Run.Suffix;
                TArray<const FBreakerZonePiece*> Treads;
                BreakerFernhallCollectTreads(Pieces, TreadPrefix, Treads);
                // (a) THE RUN EXISTS — one of exactly four, each found by its
                // own suffix.
                if (!TestTrue(FString::Printf(TEXT("%s has a run named %s<j> (top Z %.1f)"), *Where, *TreadPrefix, TopZ), Treads.Num() > 0))
                {
                    continue;
                }
                AllTreads.Append(Treads);
                const FBreakerZonePiece& First = *Treads[0];
                const FBreakerZonePiece& Last = *Treads.Last();
                const float FirstTop = BreakerFernhallTopZ(First);

                // (a) Every rise within the movement default: off the ground
                // the first tread stands on, tread to tread, and onto the top.
                const FBreakerZonePiece* Ground = BreakerFernhallGroundUnder(Pieces, First, BreakerFloorSamePlaneCm,
                    [&](const FBreakerZonePiece& Floor)
                    {
                        return &Floor == &Top || Floor.Name.StartsWith(Top.Name + TEXT("_"));
                    });
                if (TestNotNull(FString::Printf(TEXT("%s: a floor lies under the first tread %s (top Z %.1f at X %.0f Y %.0f)"),
                    *Where, *First.Name, FirstTop, First.Origin.X, First.Origin.Y), Ground))
                {
                    const float FirstRise = FirstTop - BreakerFernhallTopZ(*Ground);
                    TestTrue(FString::Printf(TEXT("%s: first tread %s rises %.1f cm off %s (top %.1f), within MaxStepHeight %.1f"),
                        *Where, *First.Name, FirstRise, *Ground->Name, BreakerFernhallTopZ(*Ground), MaxStepCm),
                        FirstRise <= MaxStepCm);
                }
                for (int32 i = 1; i < Treads.Num(); ++i)
                {
                    const float Rise = BreakerFernhallTopZ(*Treads[i]) - BreakerFernhallTopZ(*Treads[i - 1]);
                    TestTrue(FString::Printf(TEXT("%s: %s -> %s rises %.1f cm (%.1f -> %.1f), within MaxStepHeight %.1f"),
                        *Where, *Treads[i - 1]->Name, *Treads[i]->Name, Rise,
                        BreakerFernhallTopZ(*Treads[i - 1]), BreakerFernhallTopZ(*Treads[i]), MaxStepCm),
                        Rise <= MaxStepCm);
                }
                const float LastRise = TopZ - BreakerFernhallTopZ(Last);
                TestTrue(FString::Printf(TEXT("%s: last tread %s -> top rises %.1f cm (%.1f -> %.1f), within MaxStepHeight %.1f"),
                    *Where, *Last.Name, LastRise, BreakerFernhallTopZ(Last), TopZ, MaxStepCm),
                    LastRise <= MaxStepCm);
                TestTrue(FString::Printf(TEXT("%s: last tread %s is not above the top it climbs to (%.1f -> %.1f)"),
                    *Where, *Last.Name, BreakerFernhallTopZ(Last), TopZ),
                    LastRise >= -BreakerFloorSamePlaneCm);

                // (b) THE LAST TREAD MEETS THE TOP'S EDGE ON THE RUN'S OWN
                // AXIS: an abutment within a centimetre along the axis it
                // climbs, a shared span across it. Inside the footprint is
                // under the apron, not at it; a hand's width off is a gap.
                const int32 Axis = Run.bAlongFwd ? Fwd : Lat;
                const int32 Across = 1 - Axis;
                const float Along = BreakerFernhallAxisOverlap(Last, Top, Axis);
                const float Shared = BreakerFernhallAxisOverlap(Last, Top, Across);
                TestTrue(FString::Printf(TEXT("%s: last tread %s touches the top's edge within %.0f cm on its axis (along %.1f, across %.1f)"),
                    *Where, *Last.Name, BreakerFloorAbutCm, Along, Shared),
                    Along >= -BreakerFloorAbutCm && Shared > 0.0f);
                TestTrue(FString::Printf(TEXT("%s: last tread %s does not lie inside the top's footprint (along %.1f, across %.1f)"),
                    *Where, *Last.Name, Along, Shared),
                    Along <= BreakerFloorAbutCm);

                if (Run.bAlongFwd)
                {
                    for (const FBreakerZonePiece* Tread : Treads)
                    {
                        FwdMin = FMath::Min(FwdMin, static_cast<float>(Tread->Origin[Fwd] - Tread->Extent[Fwd]));
                        FwdMax = FMath::Max(FwdMax, static_cast<float>(Tread->Origin[Fwd] + Tread->Extent[Fwd]));
                    }
                }
                AddInfo(FString::Printf(TEXT("%s%s: %d treads, first top %.1f, last top %.1f, top %.1f, ceiling %.1f"),
                    *Where, Run.Suffix, Treads.Num(), FirstTop, BreakerFernhallTopZ(Last), TopZ, MaxStepCm));
            }

            // (a) EXACTLY four: every piece named under this top is a tread of
            // one of the four runs. A fifth run, or a tread with a suffix the
            // contract does not name, is a stair nobody measured.
            int32 NamedUnderTop = 0;
            for (const FBreakerZonePiece& Piece : Pieces)
            {
                if (Piece.Name.StartsWith(Top.Name + TEXT("_"))) ++NamedUnderTop;
            }
            TestEqual(FString::Printf(TEXT("%s: every piece named under the top is a tread of its four runs (%d named, %d matched)"),
                *Where, NamedUnderTop, AllTreads.Num()), NamedUnderTop, AllTreads.Num());

            // (c) NEITHER THE TOP NOR A TREAD IS INSIDE A WALL OR A COVER
            // BLOCK: more than a centimetre shared on all three axes is a
            // piece the character cannot stand on.
            AllTreads.Add(&Top);
            for (const FBreakerZonePiece* Piece : AllTreads)
            {
                for (const FBreakerZonePiece* Solid : Solids)
                {
                    const float X = BreakerFernhallAxisOverlap(*Piece, *Solid, 0);
                    const float Y = BreakerFernhallAxisOverlap(*Piece, *Solid, 1);
                    const float Z = BreakerFernhallAxisOverlap(*Piece, *Solid, 2);
                    if (X > BreakerFloorAbutCm && Y > BreakerFloorAbutCm && Z > BreakerFloorAbutCm)
                    {
                        AddError(FString::Printf(TEXT("%s: %s intersects %s (overlap X %.1f, Y %.1f, Z %.1f cm)"),
                            *Where, *Piece->Name, *Solid->Name, X, Y, Z));
                    }
                }
            }

            // (d) SITED OFF EVERY CHEST PAIR: the apron's span along the lane,
            // treads included, shares nothing past an abutment with any chest
            // box in its yard. Measured against the yard's chests by name, and
            // a yard with no chests to measure is a yard this test cannot see.
            const TArray<const FBreakerZonePiece*>* Chests = ChestsByYard.Find(Yard);
            if (TestTrue(FString::Printf(TEXT("%s: yard '%s' has chest boxes to site against"), *Where, BreakerFernhallYardLabel(Yard)),
                Chests && Chests->Num() > 0))
            {
                for (const FBreakerZonePiece* Chest : *Chests)
                {
                    const float Overlap = FMath::Min(FwdMax, static_cast<float>(Chest->Origin[Fwd] + Chest->Extent[Fwd]))
                        - FMath::Max(FwdMin, static_cast<float>(Chest->Origin[Fwd] - Chest->Extent[Fwd]));
                    TestTrue(FString::Printf(TEXT("%s: lane span %.0f..%.0f is clear of %s (%.0f..%.0f), overlap %.1f cm"),
                        *Where, FwdMin, FwdMax, *Chest->Name,
                        Chest->Origin[Fwd] - Chest->Extent[Fwd], Chest->Origin[Fwd] + Chest->Extent[Fwd], Overlap),
                        Overlap <= BreakerFloorAbutCm);
                }
            }
        }

        // (d) EXACTLY TWO A YARD, in every yard the cover pass authored — a
        // yard with chests and no aprons is the ruling unmet, not a yard
        // outside it.
        TSet<FString> Yards;
        for (const auto& Pair : ChestsByYard) Yards.Add(Pair.Key);
        for (const auto& Pair : ApronsByYard) Yards.Add(Pair.Key);
        for (const FString& Yard : Yards)
        {
            const int32* Count = ApronsByYard.Find(Yard);
            TestEqual(FString::Printf(TEXT("%s: yard '%s' has exactly %d aprons"), *Folder, BreakerFernhallYardLabel(Yard), BreakerApronsPerYard),
                Count ? *Count : 0, BreakerApronsPerYard);
        }
        // Not vacuous: a folder in which no apron matched is a folder this
        // test has stopped reading, not a yard with nothing to climb.
        TestTrue(FString::Printf(TEXT("%s has aprons to measure (%d)"), *Folder, Aprons), Aprons > 0);
    }
    return true;
}

// ---------------------------------------------------------------------------
// NO SLAB STANDS INSIDE A WALL (O278). The composer's probe measured every
// deck, catwalk and dock against every wall_ box EXCEPT the flank buildings,
// and recorded why: the decks lie at lat 19..27 against a flank inner face at
// 23.5, so every one of them ran into its building by up to 3.5 m since the
// shape pass was authored. O278 ends the exemption — every deck is reachable,
// and a slab whose outer metres are inside a wall is a slab whose outer
// metres nobody can stand on. So this measures what the probe declined to:
// every raised slab against EVERY wall_ box, both folders, no FLANK set.
//
// The same centimetre DecksClimbable and FloorsDisjoint measure by: an
// overlap of a centimetre or less on any one axis is an abutment, not a
// slab in a wall.
// ---------------------------------------------------------------------------
namespace
{
    // THE SLABS, by the composer's own suffixes: `flr_<tag>_deck`,
    // `flr_<tag>_fardeck`, `flr_<tag>_catwalk`, `flr_<tag>_dock`. A tread is
    // not a slab (`_dockstep0` does not end in `_dock`), and DecksClimbable
    // already measures every tread against every wall.
    bool BreakerFernhallIsRaisedSlab(const FString& Name)
    {
        if (!Name.StartsWith(TEXT("flr_"))) return false;
        static const TCHAR* Suffixes[] = { TEXT("_deck"), TEXT("_fardeck"), TEXT("_catwalk"), TEXT("_dock") };
        for (const TCHAR* Suffix : Suffixes)
        {
            if (Name.EndsWith(Suffix)) return true;
        }
        return false;
    }

    // WHICH wall_ PIECES ARE STOREYED (O278): the ones the composer builds as a
    // building — stacked and tiled, never scaled — as opposed to the ones that
    // are a face, a leg or a seam's blind. Read off compose_fernhall.py's
    // place() names:
    //
    //   Perimeter  wall_<flank><NN>     perimeter(): wall_n03, wall_sub_s07,
    //                                   wall_dep_n05, wall_siding_n09 ...
    //   Tower      wall_<flank><NN>t    perimeter()'s "tower" second mass
    //   EndWall    wall_[<yard>_](w|e)[_(n|s)]
    //                                   wall_w, wall_e_s, wall_sub_e,
    //                                   wall_dep_w_n, wall_siding_e_s ...
    //   Mass       wall_<tag>_mass<i>, wall_<tag>_massb<i>   shape_pass()
    //
    // EXCLUDED, explicitly, by prefix or by the segment they carry:
    //   wall_seam*      seam walls (wall_seam_*, wall_seam2_*, wall_seam3_*)
    //   *_gleg<i><j>    gantry legs (shape_pass)
    //   *_baycheek<n>, *_bayback, *_bayside<n>   bay faces and cheeks (work_pass)
    //   *_dockface<n>, *_dockfront               dock faces (work_pass)
    // Piers are dress_ pieces and never reach this classifier.
    //
    // A wall_ name that is none of the above is REFUSED (Unknown), not
    // skipped: a new wall family arrives here as a red with its name on it,
    // and the author decides which list it joins.
    enum class EBreakerFernhallWallFamily : uint8
    {
        NotStoreyed,
        Perimeter,
        Tower,
        EndWall,
        Mass,
        Unknown,
    };

    EBreakerFernhallWallFamily BreakerFernhallWallFamilyOf(const FString& Name)
    {
        if (!Name.StartsWith(TEXT("wall_"))) return EBreakerFernhallWallFamily::NotStoreyed;
        if (Name.StartsWith(TEXT("wall_seam"))) return EBreakerFernhallWallFamily::NotStoreyed;
        if (Name.Contains(TEXT("_gleg"))) return EBreakerFernhallWallFamily::NotStoreyed;
        if (Name.Contains(TEXT("_bay"))) return EBreakerFernhallWallFamily::NotStoreyed;
        if (Name.Contains(TEXT("_dock"))) return EBreakerFernhallWallFamily::NotStoreyed;

        TArray<FString> Segments;
        Name.Mid(5).ParseIntoArray(Segments, TEXT("_"), true);
        if (Segments.Num() == 0) return EBreakerFernhallWallFamily::Unknown;
        const FString& Last = Segments.Last();

        // wall_<tag>_mass<i> / wall_<tag>_massb<i>: exactly a yard tag and a mass.
        if (Segments.Num() == 2 && Last.StartsWith(TEXT("mass")))
        {
            return EBreakerFernhallWallFamily::Mass;
        }

        // wall_<flank><NN> / wall_<flank><NN>t: the last segment is a flank
        // letter and two digits, with or without the tower's trailing t, and
        // at most one yard tag before it (wall_n03, wall_sub_n02, wall_siding_s10t).
        {
            FString Bay = Last;
            const bool bTower = Bay.EndsWith(TEXT("t"));
            if (bTower) Bay.LeftChopInline(1);
            const bool bFlank = Bay.Len() == 3
                && (Bay[0] == TEXT('n') || Bay[0] == TEXT('s'))
                && Bay.Mid(1).IsNumeric();
            if (bFlank && Segments.Num() <= 2)
            {
                return bTower ? EBreakerFernhallWallFamily::Tower : EBreakerFernhallWallFamily::Perimeter;
            }
        }

        // wall_[<yard>_](w|e)[_(n|s)]: an end wall or one of its stubs.
        {
            const auto IsEnd = [](const FString& S) { return S == TEXT("w") || S == TEXT("e"); };
            const auto IsStub = [](const FString& S) { return S == TEXT("n") || S == TEXT("s"); };
            const int32 EndIndex = IsEnd(Last) ? Segments.Num() - 1
                : (Segments.Num() >= 2 && IsStub(Last) && IsEnd(Segments[Segments.Num() - 2])) ? Segments.Num() - 2
                : INDEX_NONE;
            if (EndIndex != INDEX_NONE && EndIndex <= 1)
            {
                return EBreakerFernhallWallFamily::EndWall;
            }
        }
        return EBreakerFernhallWallFamily::Unknown;
    }

    const TCHAR* BreakerFernhallWallFamilyName(EBreakerFernhallWallFamily Family)
    {
        switch (Family)
        {
        case EBreakerFernhallWallFamily::Perimeter: return TEXT("perimeter");
        case EBreakerFernhallWallFamily::Tower: return TEXT("tower");
        case EBreakerFernhallWallFamily::EndWall: return TEXT("end wall");
        case EBreakerFernhallWallFamily::Mass: return TEXT("mass");
        case EBreakerFernhallWallFamily::Unknown: return TEXT("unknown");
        default: return TEXT("not storeyed");
        }
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerFernhallSlabsClearWallsTest,
    "RiorsEdge.Zone.Fernhall.SlabsClearWalls",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerFernhallSlabsClearWallsTest::RunTest(const FString& Parameters)
{
    constexpr float BreakerFloorAbutCm = 1.0f;
    for (const FString& Folder : { UBreakerZoneBuilder::FernhallMeshFolder(), UBreakerZoneBuilder::FernhallRiftMeshFolder() })
    {
        TArray<FBreakerZonePiece> Pieces;
        if (!TestTrue(FString::Printf(TEXT("%s collects"), *Folder), UBreakerZoneBuilder::CollectZonePieces(Folder, Pieces))) return false;

        // EVERY wall_ box. Not the composer's FLANK-less set: the flank
        // buildings are the walls the slabs were authored into.
        TArray<const FBreakerZonePiece*> Walls;
        for (const FBreakerZonePiece& Piece : Pieces)
        {
            if (Piece.Name.StartsWith(TEXT("wall_"))) Walls.Add(&Piece);
        }
        TestTrue(FString::Printf(TEXT("%s has walls to measure against (%d)"), *Folder, Walls.Num()), Walls.Num() > 0);

        int32 Slabs = 0;
        int32 Hits = 0;
        for (const FBreakerZonePiece& Slab : Pieces)
        {
            if (!BreakerFernhallIsRaisedSlab(Slab.Name)) continue;
            ++Slabs;
            for (const FBreakerZonePiece* Wall : Walls)
            {
                const float X = BreakerFernhallAxisOverlap(Slab, *Wall, 0);
                const float Y = BreakerFernhallAxisOverlap(Slab, *Wall, 1);
                const float Z = BreakerFernhallAxisOverlap(Slab, *Wall, 2);
                const bool bIntersects = X > BreakerFloorAbutCm && Y > BreakerFloorAbutCm && Z > BreakerFloorAbutCm;
                if (bIntersects)
                {
                    ++Hits;
                    AddError(FString::Printf(TEXT("%s: slab %s intersects %s (overlap X %.1f, Y %.1f, Z %.1f cm)"),
                        *Folder, *Slab.Name, *Wall->Name, X, Y, Z));
                }
            }
        }
        // Not vacuous: a deck, a catwalk, a far deck and a dock in every yard.
        TestTrue(FString::Printf(TEXT("%s has raised slabs to measure (%d)"), *Folder, Slabs), Slabs > 0);
        AddInfo(FString::Printf(TEXT("%s: %d slabs against %d walls, %d intersections"), *Folder, Slabs, Walls.Num(), Hits));
    }
    return true;
}

// ---------------------------------------------------------------------------
// A WALL IS STOREYS (O278). A storey is 3 m and a tile is never stretched past
// its own proportions: a building is a stack of native wall tiles, so its
// collision box is exactly storeys x one tile tall. The tile the composer
// stacks is 3.03 m high, so 7 m, 11.5 m and 6.5 m — the skyline's old
// scaled-box heights — are each a wall that was scaled, and each goes red
// here by name. The seam walls, bay and dock faces and gantry legs are not
// buildings and are not measured; the classifier above says which is which.
//
// Five centimetres is the same tolerance PieceContract grants a cover box
// against its authored height.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerFernhallWallsAreStoreysTest,
    "RiorsEdge.Zone.Fernhall.WallsAreStoreys",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerFernhallWallsAreStoreysTest::RunTest(const FString& Parameters)
{
    // The native height of the kit's wall tile (O278): a storey, as built.
    constexpr float BreakerFernhallStoreyCm = 303.0f;
    constexpr float BreakerFernhallStoreyToleranceCm = 5.0f;
    for (const FString& Folder : { UBreakerZoneBuilder::FernhallMeshFolder(), UBreakerZoneBuilder::FernhallRiftMeshFolder() })
    {
        TArray<FBreakerZonePiece> Pieces;
        if (!TestTrue(FString::Printf(TEXT("%s collects"), *Folder), UBreakerZoneBuilder::CollectZonePieces(Folder, Pieces))) return false;

        int32 Matched = 0;
        for (const FBreakerZonePiece& Piece : Pieces)
        {
            const EBreakerFernhallWallFamily Family = BreakerFernhallWallFamilyOf(Piece.Name);
            if (Family == EBreakerFernhallWallFamily::NotStoreyed) continue;
            if (Family == EBreakerFernhallWallFamily::Unknown)
            {
                AddError(FString::Printf(TEXT("%s: %s is a wall_ piece of no known family; name it storeyed or excluded"),
                    *Folder, *Piece.Name));
                continue;
            }
            ++Matched;
            const float HeightCm = 2.0f * Piece.Extent.Z;
            const int32 Storeys = FMath::RoundToInt(HeightCm / BreakerFernhallStoreyCm);
            const float OffCm = HeightCm - static_cast<float>(Storeys) * BreakerFernhallStoreyCm;
            TestTrue(FString::Printf(TEXT("%s: %s (%s) is %.1f cm tall: %d storeys of %.0f, off by %.1f cm, within %.0f"),
                *Folder, *Piece.Name, BreakerFernhallWallFamilyName(Family), HeightCm, Storeys,
                BreakerFernhallStoreyCm, OffCm, BreakerFernhallStoreyToleranceCm),
                Storeys >= 1 && FMath::Abs(OffCm) <= BreakerFernhallStoreyToleranceCm);
        }
        // Not vacuous: every yard has flanks, end walls and a mass.
        TestTrue(FString::Printf(TEXT("%s has storeyed walls to measure (%d)"), *Folder, Matched), Matched > 0);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerFernhallSurfaceRepairTest,
    "RiorsEdge.World.Fernhall.SurfaceRepairBounds", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerFernhallSurfaceRepairTest::RunTest(const FString& Parameters)
{
    TArray<FBreakerZonePiece> Pieces;
    if (!TestTrue(TEXT("Shipped yard loads"), UBreakerZoneBuilder::CollectZonePieces(UBreakerZoneBuilder::FernhallMeshFolder(),Pieces))) return false;
    const auto* Piece=Pieces.FindByPredicate([](const auto& P){return P.Name==TEXT("wall_n03");});
    if (!TestNotNull(TEXT("Authored facade exists"),Piece)) return false;
    auto* World=UWorld::CreateWorld(EWorldType::Game,false);
    if (!World) return false;
    ON_SCOPE_EXIT { World->DestroyWorld(false); };
    auto* Actor=World->SpawnActor<AStaticMeshActor>();
    auto* Component=Actor->GetStaticMeshComponent();Component->SetMobility(EComponentMobility::Movable);
    auto* Original=Cast<UStaticMesh>(Piece->MeshPath.TryLoad());
    if (!TestNotNull(TEXT("Original facade loads"),Original)) return false;
    const FBox Before=Original->GetBoundingBox();Component->SetStaticMesh(Original);
    BreakerRepairFernhallSurface(Component,*Piece);Component->UpdateBounds();
    TestTrue(TEXT("Facade becomes a coherent structural mesh"),Component->GetStaticMesh()!=Original);
    TestTrue(TEXT("Repair retains authored minimum boundary"),Component->Bounds.GetBox().Min.Equals(Before.Min,.5f));
    TestTrue(TEXT("Repair retains authored maximum boundary"),Component->Bounds.GetBox().Max.Equals(Before.Max,.5f));
    TestNotNull(TEXT("Shipped textured material binds"),Component->GetMaterial(0));
    return true;
}
