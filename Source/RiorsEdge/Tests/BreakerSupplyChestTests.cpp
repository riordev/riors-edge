#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Game/BreakerZoneBuilder.h"
#include "HAL/FileManager.h"
#include "Interaction/BreakerSupplyChest.h"
#include "Interaction/BreakerSupplyChestMath.h"
#include "Items/BreakerDropTable.h"
#include "Items/BreakerItemTypes.h"
#include "Misc/PackageName.h"
#include "Progression/BreakerRiftRewardMath.h"

// ---------------------------------------------------------------------------
// WHAT IS IN A CHEST, AND WHERE IT STANDS. O275, assertable without a world:
//
//   THE FLOOR       a chest never pays nothing, and the floor climbs with the yard
//   THE ITEM        rolled at the completion floor, gated by the yard's level
//   THE SITE        behind full-height cover or inside a bay, never on the lane
//   THE SEED PICKS  which sites, distinct, and a yard offers enough of them
//
// What is NOT assertable here is whether finding one feels worth the walk.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerSupplyChestTest,
    "RiorsEdge.Items.SupplyChest.Contents",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
    // A yard as the composer writes one, in metres from the yard's anchor:
    // (along, lateral). The entry yard's six full-height breaks and its bay,
    // read off compose_fernhall.py so the synthetic sweep below measures the
    // same shapes the shipped yard offers.
    struct FBreakerSupplyChestYard
    {
        FVector Origin;
        FVector Forward;
        TArray<FBreakerZonePiece> Pieces;

        FVector Right() const { return BreakerSupplyChest::LateralAxis(Forward); }

        void Place(const TCHAR* Name, float AlongM, float LateralM, const FVector& ExtentCm)
        {
            FBreakerZonePiece& Piece = Pieces.AddDefaulted_GetRef();
            Piece.Name = Name;
            Piece.Extent = ExtentCm;
            // Grounded, the way the composer grounds every piece: the base sits
            // on the yard floor and the origin is half the height above it.
            Piece.Origin = Origin + Forward * (AlongM * 100.0f) + Right() * (LateralM * 100.0f)
                + FVector(0.0f, 0.0f, ExtentCm.Z);
        }
    };

    FBreakerSupplyChestYard BreakerSupplyChestEntryYard(const FVector& Origin, const FVector& Forward)
    {
        FBreakerSupplyChestYard Yard;
        Yard.Origin = Origin;
        Yard.Forward = Forward;
        // Six 3 x 4 x 3 m full-height breaks, at the composer's positions.
        const float Breaks[][2] = { {32, 17}, {32, -17}, {62, 17}, {62, -17}, {86, 17}, {89, -18} };
        for (int32 Index = 0; Index < UE_ARRAY_COUNT(Breaks); ++Index)
        {
            Yard.Place(*FString::Printf(TEXT("blk_full_break%02d"), Index), Breaks[Index][0], Breaks[Index][1],
                FVector(150.0f, 150.0f, 200.0f));
        }
        // The bay: fourteen along, ten deep, its slab 0.3 m thick, at 21 m off
        // the lane. Its extent is world-aligned, so it is expressed along the
        // frame the yard is built in.
        {
            const FVector Right = Yard.Right();
            const FVector Extent(
                FMath::Abs(Forward.X) * 700.0f + FMath::Abs(Right.X) * 500.0f,
                FMath::Abs(Forward.Y) * 700.0f + FMath::Abs(Right.Y) * 500.0f, 15.0f);
            Yard.Place(TEXT("flr_entry_bay"), 44, 21, Extent);
        }
        // Things that are NOT sites: chest-high cover, the bay's own walls and
        // roof, dressing, the yard floor. A site pick that read any of these
        // would put a chest on open ground or on a roof.
        Yard.Place(TEXT("blk_chest_n00"), 20, 10.5f, FVector(150.0f, 60.0f, 60.0f));
        Yard.Place(TEXT("wall_entry_bayback"), 44, 26, FVector(700.0f, 30.0f, 300.0f));
        Yard.Place(TEXT("flr_entry_bayroof"), 44, 21, FVector(700.0f, 500.0f, 20.0f));
        Yard.Place(TEXT("dress_entry_baycrate0"), 39.6f, 23.6f, FVector(65.0f, 65.0f, 65.0f));
        Yard.Place(TEXT("flr_yard"), 50, 0, FVector(5000.0f, 2500.0f, 10.0f));
        return Yard;
    }

    const FBreakerZonePiece* BreakerSupplyChestFindPiece(const TArray<FBreakerZonePiece>& Pieces, FName Name)
    {
        for (const FBreakerZonePiece& Piece : Pieces)
            if (Piece.Name == Name.ToString()) return &Piece;
        return nullptr;
    }

    // The chest's parts are protected members of the NPC it derives from, so
    // they are found the way a capture would find them: by what mesh each
    // component carries. Bounds are brought into the ACTOR'S frame by walking
    // the attachment chain the constructor set up (the lid hangs off the
    // body, the body off the capsule), so "the lid sits on the body" is a
    // statement about where they are, not about which is parented to which.
    const UStaticMeshComponent* BreakerSupplyChestFindByMeshPath(
        const TArray<UStaticMeshComponent*>& Meshes, const TCHAR* PathPrefix)
    {
        for (const UStaticMeshComponent* Mesh : Meshes)
            if (Mesh && Mesh->GetStaticMesh() && Mesh->GetStaticMesh()->GetPathName().StartsWith(PathPrefix)) return Mesh;
        return nullptr;
    }

    FTransform BreakerSupplyChestActorSpace(const USceneComponent* Component)
    {
        FTransform ToActor = Component->GetRelativeTransform();
        for (const USceneComponent* Parent = Component->GetAttachParent(); Parent; Parent = Parent->GetAttachParent())
            ToActor = ToActor * Parent->GetRelativeTransform();
        return ToActor;
    }

    FBox BreakerSupplyChestActorBounds(const UStaticMeshComponent* Component)
    {
        return Component->GetStaticMesh()->GetBounds().GetBox().TransformBy(BreakerSupplyChestActorSpace(Component));
    }

    // The band is the ONE engine cube left on the chest with any size to it:
    // the console body's cube is gone (hidden, unmeshed or collapsed), the
    // mote is a sphere, the crate and its lid are named props.
    void BreakerSupplyChestCollectBands(const TArray<UStaticMeshComponent*>& Meshes, TArray<const UStaticMeshComponent*>& OutBands)
    {
        for (const UStaticMeshComponent* Mesh : Meshes)
        {
            if (!Mesh || !Mesh->GetStaticMesh()) continue;
            if (Mesh->GetStaticMesh()->GetPathName() != TEXT("/Engine/BasicShapes/Cube.Cube")) continue;
            if (Mesh->GetRelativeScale3D().IsNearlyZero() || !Mesh->GetVisibleFlag()) continue;
            OutBands.Add(Mesh);
        }
    }
}

// ---------------------------------------------------------------------------
// A CRATE WITH A LID, NOT A CUBE (O280). What the default object ships, read
// off its own components: a crate body, a lid resting on the crate's top, and
// the gold band (O179) at the seam between them rather than floating in the
// air where the console's trim used to be. Reachable without a world because
// the parts are wired in the constructor.
//
// What is NOT assertable here is whether the crate reads as a chest from the
// lane, and whether the band is gold ONCE THE NPC'S SASH PAINT HAS RUN — that
// needs BeginPlay and lives in RiorsEdge.Campaign.FernhallCacheRuntime.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerSupplyChestCrateWithALidTest,
    "RiorsEdge.Items.SupplyChest.CrateWithALid",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerSupplyChestCrateWithALidTest::RunTest(const FString& Parameters)
{
    const ABreakerSupplyChest* Chest = GetDefault<ABreakerSupplyChest>();
    if (!TestNotNull(TEXT("the chest has a default object"), Chest)) return false;
    TArray<UStaticMeshComponent*> Meshes;
    Chest->GetComponents<UStaticMeshComponent>(Meshes);

    // ---- THE PARTS ----------------------------------------------------------
    const UStaticMeshComponent* Body = BreakerSupplyChestFindByMeshPath(Meshes, TEXT("/Game/Breaker/Meshes/props/props/StaticMeshes/prop_chest_body"));
    const UStaticMeshComponent* Lid = BreakerSupplyChestFindByMeshPath(Meshes, TEXT("/Game/Breaker/Meshes/props/props/StaticMeshes/prop_chest_lid"));
    if (!TestNotNull(TEXT("the chest carries the crate body prop (O280)"), Body)) return false;
    if (!TestNotNull(TEXT("and the crate lid prop (O280)"), Lid)) return false;
    TestTrue(TEXT("the body and the lid are two components, not one"), Body != Lid);

    // ---- A CRATE: WIDER THAN TALL ------------------------------------------
    const FBox BodyBounds = BreakerSupplyChestActorBounds(Body);
    const FBox LidBounds = BreakerSupplyChestActorBounds(Lid);
    const FVector BodySize = BodyBounds.GetSize();
    const FVector LidSize = LidBounds.GetSize();
    AddInfo(FString::Printf(TEXT("CHEST CRATE  body %.1f x %.1f x %.1f cm, top at %.1f; lid %.1f x %.1f x %.1f cm, bottom at %.1f, top at %.1f"),
        BodySize.X, BodySize.Y, BodySize.Z, BodyBounds.Max.Z, LidSize.X, LidSize.Y, LidSize.Z, LidBounds.Min.Z, LidBounds.Max.Z));
    TestTrue(TEXT("the body has size"), BodySize.GetMin() > KINDA_SMALL_NUMBER);
    TestTrue(TEXT("the lid has size"), LidSize.GetMin() > KINDA_SMALL_NUMBER);
    TestTrue(TEXT("the body is wider than it is tall: a crate, not a console"),
        FMath::Max(BodySize.X, BodySize.Y) > BodySize.Z);

    // ---- WITH A LID: SEATED ON THE BODY --------------------------------------
    // The kit's chest is a box whose back and rim reach the lid's top, and a
    // lid that hinges partway down the back and closes flush with that rim:
    // the lid's top meets the body's top within 2 cm and its hinge sits in
    // the body's upper half. A lid floating above the crate or sunk into it
    // is the defect this pins; the hinge being the lid mesh's origin is what
    // lets it swing without the seam moving.
    TestTrue(*FString::Printf(TEXT("the lid closes flush with the body's top (gap %.2f cm)"),
        LidBounds.Max.Z - BodyBounds.Max.Z),
        FMath::Abs(LidBounds.Max.Z - BodyBounds.Max.Z) <= 2.0f);
    TestTrue(TEXT("the lid's hinge sits in the body's upper half"),
        LidBounds.Min.Z > BodyBounds.Min.Z + 0.5f * (BodyBounds.Max.Z - BodyBounds.Min.Z) && LidBounds.Min.Z < BodyBounds.Max.Z);

    // ---- THE BAND, AT THE SEAM ----------------------------------------------
    // If the cube band is still the band. Zero of them is a chest whose band
    // is part of the props and is fine; two is the console's cube still
    // standing inside the crate.
    TArray<const UStaticMeshComponent*> Bands;
    BreakerSupplyChestCollectBands(Meshes, Bands);
    if (Bands.Num() == 0)
    {
        AddInfo(TEXT("CHEST CRATE  no cube band on the default object; the band is a prop or gone"));
    }
    else if (TestEqual(TEXT("exactly one cube band remains: the console's cube is gone"), Bands.Num(), 1))
    {
        const FBox BandBounds = BreakerSupplyChestActorBounds(Bands[0]);
        const float BandCentreZ = BandBounds.GetCenter().Z;
        AddInfo(FString::Printf(TEXT("CHEST CRATE  band centre at %.1f between body top %.1f and lid top %.1f"),
            BandCentreZ, BodyBounds.Max.Z, LidBounds.Max.Z));
        // The seam is the hinge line, the lid's bottom; a band centred there is
        // the honest case, so the lower edge carries float slop of 1 cm; the
        // upper edge is the lid's top.
        TestTrue(TEXT("the band's centre is no lower than the lid's bottom: at the seam, not around the belly"),
            BandCentreZ >= LidBounds.Min.Z - 1.0f);
        TestTrue(TEXT("and no higher than the lid's top: not floating above the chest"),
            BandCentreZ <= LidBounds.Max.Z);
    }
    return true;
}

bool FBreakerSupplyChestTest::RunTest(const FString& Parameters)
{
    using namespace BreakerSupplyChest;

    // ---- A CHEST NEVER PAYS NOTHING ---------------------------------------
    // The defect this pins actually shipped for one suite run: the chest paid
    // the kill roll straight through, a trash body is worth 0 to 1 Riftglass,
    // and a chest credited zero. A bad roll on a kill is one of forty; a bad
    // roll on a chest is the whole interaction.
    for (int32 AreaLevel = 1; AreaLevel <= 60; ++AreaLevel)
    {
        for (int32 PerKill = 0; PerKill <= 12; ++PerKill)
        {
            const int32 Paid = CurrencyPayout(PerKill, AreaLevel);
            if (!TestTrue(*FString::Printf(TEXT("a chest at level %d always pays something"), AreaLevel),
                Paid >= MinimumRiftglass)) return false;
            if (!TestTrue(TEXT("and never less than the kill it is worth several of"),
                Paid >= PerKill)) return false;
        }
        // A deeper yard is a longer walk, so the floor may not fall as the
        // level rises.
        if (AreaLevel > 1)
        {
            TestTrue(TEXT("the floor never falls as the yard gets deeper"),
                CurrencyPayout(0, AreaLevel) >= CurrencyPayout(0, AreaLevel - 1));
        }
    }
    // A negative roll is a defect upstream, not a debt: it cannot make a chest
    // charge the player.
    TestTrue(TEXT("a negative roll cannot become a negative payout"),
        CurrencyPayout(-40, 5) >= MinimumRiftglass);
    AddInfo(FString::Printf(TEXT("CHEST CURRENCY  level 5 pays %d-%d, level 13 pays %d-%d"),
        CurrencyPayout(0, 5), CurrencyPayout(1, 5), CurrencyPayout(0, 13), CurrencyPayout(1, 13)));

    // ---- THE ITEM IS AT THE COMPLETION FLOOR --------------------------------
    // O275: one item, at the codebase's one "decent" floor, gated by the
    // chest's own level exactly as a rift's offer is. Below the Exceptional
    // unlock the floor is Uncommon; the entry yard is level five and the
    // depot is past the unlock, so both rungs are exercised by shipped yards.
    // The default table, because that is the table the chest passes.
    TestEqual(TEXT("an AL5 chest's item is Uncommon"),
        static_cast<int32>(BreakerRiftReward::CompletionRarity(ChestItemLevel(5), FBreakerDropTableParams{})),
        static_cast<int32>(EBreakerItemRarity::Uncommon));
    TestEqual(TEXT("an AL9 chest's item is Exceptional"),
        static_cast<int32>(BreakerRiftReward::CompletionRarity(ChestItemLevel(9), FBreakerDropTableParams{})),
        static_cast<int32>(EBreakerItemRarity::Exceptional));
    TestEqual(TEXT("a chest never rolls below the ladder's first rung"), ChestItemLevel(0), 1);
    TestEqual(TEXT("and otherwise rolls at the yard's own level"), ChestItemLevel(7), 7);

    // ---- WHERE ONE STANDS: THE SITES ----------------------------------------
    // The rule, on a yard that is not axis-aligned: the sites must follow the
    // frame, not the world's X. The chest radius is what the game grants —
    // the chest's own capsule, read from its default object.
    const UCapsuleComponent* ChestBody = GetDefault<ABreakerSupplyChest>()->FindComponentByClass<UCapsuleComponent>();
    if (!TestNotNull(TEXT("the chest has a body to measure from"), ChestBody)) return false;
    const float Radius = ChestBody->GetScaledCapsuleRadius();

    // Every site is on the far side of its cover, or inside its bay, and
    // never nearer the lane than the piece it stands with.
    auto CheckSites = [&](const TCHAR* Label, const TArray<FBreakerZonePiece>& Pieces, const FVector& Origin,
        const FVector& Forward, const TArray<FBreakerChestSite>& Sites) -> bool
    {
        const FVector Right = LateralAxis(Forward);
        for (const FBreakerChestSite& Site : Sites)
        {
            const FBreakerZonePiece* Piece = BreakerSupplyChestFindPiece(Pieces, Site.Cover);
            if (!TestNotNull(*FString::Printf(TEXT("%s: site names a piece the yard has (%s)"), Label,
                *Site.Cover.ToString()), Piece)) return false;
            const float Sign = LateralSign(Piece->Origin, Origin, Forward);
            const FVector Offset = Site.Location - Piece->Origin;
            const float Away = FVector::DotProduct(Offset, Right) * Sign;
            const float Along = FVector::DotProduct(Offset, Forward);
            const float SiteLateral = FVector::DotProduct(Site.Location - Origin, Right);
            const float PieceLateral = FVector::DotProduct(Piece->Origin - Origin, Right);
            if (!TestTrue(*FString::Printf(TEXT("%s: %s stands further from the lane than its piece"), Label,
                *Site.Cover.ToString()), FMath::Abs(SiteLateral) > FMath::Abs(PieceLateral))) return false;
            if (IsFullCover(Piece->Name))
            {
                if (!TestTrue(*FString::Printf(TEXT("%s: %s is behind its cover by at least half-depth plus radius (%.0f)"),
                    Label, *Site.Cover.ToString(), Away),
                    Away >= HalfExtentAlong(Piece->Extent, Right) + Radius - KINDA_SMALL_NUMBER)) return false;
                if (!TestTrue(*FString::Printf(TEXT("%s: %s is squarely behind, not diagonal"), Label, *Site.Cover.ToString()),
                    FMath::IsNearlyZero(Along, 1.0f))) return false;
            }
            else if (!TestTrue(*FString::Printf(TEXT("%s: %s is a bay"), Label, *Site.Cover.ToString()), IsBay(Piece->Name)))
            {
                return false;
            }
            else
            {
                // Inside the footprint with the whole capsule, and pulled back
                // from the mouth rather than standing in it.
                if (!TestTrue(*FString::Printf(TEXT("%s: %s site is inside the bay's footprint"), Label, *Site.Cover.ToString()),
                    FMath::Abs(Along) <= HalfExtentAlong(Piece->Extent, Forward) - Radius
                    && FMath::Abs(FVector::DotProduct(Offset, Right)) <= HalfExtentAlong(Piece->Extent, Right) - Radius))
                    return false;
                if (!TestTrue(*FString::Printf(TEXT("%s: %s site is pulled back from the mouth"), Label, *Site.Cover.ToString()),
                    Away > 0.0f)) return false;
            }
            if (!TestTrue(*FString::Printf(TEXT("%s: %s site sits on its piece's ground"), Label, *Site.Cover.ToString()),
                FMath::IsNearlyEqual(Site.Location.Z, Piece->Origin.Z - Piece->Extent.Z, 1.0))) return false;
        }
        return true;
    };

    const FBreakerSupplyChestYard Synthetic = BreakerSupplyChestEntryYard(
        FVector(12000.0f, -3000.0f, 0.0f), FVector(0.0f, 1.0f, 0.0f));
    TArray<FBreakerChestSite> Sites;
    CollectChestSites(Synthetic.Pieces, Synthetic.Origin, Synthetic.Forward, Radius, Sites);
    // Six breaks and one bay; nothing else in the yard is a site.
    TestEqual(TEXT("the composed entry yard offers a site per break plus its bay"), Sites.Num(), 7);
    if (!CheckSites(TEXT("synthetic"), Synthetic.Pieces, Synthetic.Origin, Synthetic.Forward, Sites)) return false;
    for (const FBreakerChestSite& Site : Sites)
    {
        const FString Name = Site.Cover.ToString();
        TestFalse(TEXT("chest-high cover is not full-height cover"), Name.StartsWith(TEXT("blk_chest_")));
        TestFalse(TEXT("a bay's roof is not its floor"), Name.EndsWith(TEXT("_bayroof")));
        TestFalse(TEXT("the yard floor is the lane, not a site"), Name == TEXT("flr_yard"));
        TestFalse(TEXT("walls and dressing are not sites"),
            Name.StartsWith(TEXT("wall_")) || Name.StartsWith(TEXT("dress_")));
    }
    TestTrue(TEXT("the trace starts under a bay's roof"), SiteTraceHeightCm < 600.0f - 20.0f);
    TestTrue(TEXT("and above the bay's stock"), SiteTraceHeightCm > 130.0f);

    // ---- THE SEED PICKS -----------------------------------------------------
    // Distinct sites, every seed; and the seed genuinely decides, so a route
    // walked twice is not identical the third time.
    TArray<int32> TimesPicked;
    TimesPicked.SetNumZeroed(Sites.Num());
    for (int32 Seed = 0; Seed < 4000; ++Seed)
    {
        const TArray<FBreakerChestSite> Picked = PickChestSites(Sites, Seed, ChestsPerYard);
        if (!TestEqual(*FString::Printf(TEXT("seed %d picks a full complement"), Seed), Picked.Num(), ChestsPerYard))
            return false;
        for (int32 A = 0; A < Picked.Num(); ++A)
        {
            const int32 Which = Sites.IndexOfByPredicate([&](const FBreakerChestSite& Site)
            {
                return Site.Cover == Picked[A].Cover && Site.Location.Equals(Picked[A].Location);
            });
            if (!TestTrue(*FString::Printf(TEXT("seed %d picks only offered sites"), Seed), Which != INDEX_NONE)) return false;
            ++TimesPicked[Which];
            for (int32 B = A + 1; B < Picked.Num(); ++B)
            {
                if (!TestFalse(*FString::Printf(TEXT("seed %d never picks the same site twice"), Seed),
                    Picked[A].Cover == Picked[B].Cover || Picked[A].Location.Equals(Picked[B].Location))) return false;
            }
        }
        // Determinism: the same seed always stands its chests in the same places.
        const TArray<FBreakerChestSite> Again = PickChestSites(Sites, Seed, ChestsPerYard);
        for (int32 Index = 0; Index < Picked.Num(); ++Index)
            if (!TestTrue(TEXT("a seed always picks the same sites"), Again[Index].Cover == Picked[Index].Cover)) return false;
    }
    for (int32 Index = 0; Index < Sites.Num(); ++Index)
    {
        TestTrue(*FString::Printf(TEXT("site %s is picked by some seed (%d of 4000)"),
            *Sites[Index].Cover.ToString(), TimesPicked[Index]), TimesPicked[Index] > 0);
    }
    // A yard offering fewer sites than asked places what it has; a yard
    // offering none places none; asking for none gets none.
    TestEqual(TEXT("a short yard places what it has"), PickChestSites(Sites, 3, 99).Num(), Sites.Num());
    TestEqual(TEXT("an empty yard places nothing"), PickChestSites(TArray<FBreakerChestSite>(), 3, ChestsPerYard).Num(), 0);
    TestEqual(TEXT("asking for no chests places none"), PickChestSites(Sites, 3, 0).Num(), 0);
    TestEqual(TEXT("and a negative ask is no ask"), PickChestSites(Sites, 3, -2).Num(), 0);

    // ---- SHIPPED CONFIGURATION ----------------------------------------------
    // The shipped yards, through the SAME collection and the SAME yard
    // membership the spawner uses. The mesh folder ships in the repo, so it
    // is required here as every other Fernhall test requires it: the
    // shipped world has to offer every yard at least ChestsPerYard sites,
    // or the spawner's "placing what it has" log is the norm rather than
    // the exception.
    const FString Folder = UBreakerZoneBuilder::FernhallMeshFolder();
    TArray<FBreakerZonePiece> Pieces;
    if (!TestTrue(TEXT("the shipped yard's mesh folder collects"), UBreakerZoneBuilder::CollectZonePieces(Folder, Pieces)))
        return false;
    FBreakerZoneMarkers Markers;
    if (!TestTrue(TEXT("the shipped marker set is complete"), UBreakerZoneBuilder::ExtractMarkers(Pieces, Markers)))
        return false;
    const FName ChestYards[] = { NAME_None, FName(TEXT("substation")), FName(TEXT("depot")) };
    for (const FName Yard : ChestYards)
    {
        FVector2D Origin2D, Forward2D;
        if (!TestTrue(*FString::Printf(TEXT("yard '%s' has a frame"), *Yard.ToString()),
            UBreakerZoneBuilder::YardFrame(Markers, Yard, Origin2D, Forward2D))) return false;
        const FVector Forward(Forward2D.X, Forward2D.Y, 0.0f);
        const FVector Origin(Origin2D.X, Origin2D.Y, 0.0f);
        TArray<FBreakerZonePiece> Own;
        for (const FBreakerZonePiece& Piece : Pieces)
            if (UBreakerZoneBuilder::YardForPoint(Markers, Piece.Origin) == Yard) Own.Add(Piece);
        TArray<FBreakerChestSite> Offered;
        CollectChestSites(Own, Origin, Forward, Radius, Offered);
        int32 Bays = 0;
        for (const FBreakerChestSite& Site : Offered) Bays += IsBay(Site.Cover.ToString()) ? 1 : 0;
        AddInfo(FString::Printf(TEXT("CHEST SITES  yard '%s' offers %d (%d behind cover, %d in a bay) for %d chests"),
            *Yard.ToString(), Offered.Num(), Offered.Num() - Bays, Bays, ChestsPerYard));
        TestTrue(*FString::Printf(TEXT("shipped yard '%s' offers at least ChestsPerYard sites"), *Yard.ToString()),
            Offered.Num() >= ChestsPerYard);
        TestTrue(*FString::Printf(TEXT("shipped yard '%s' offers its bay"), *Yard.ToString()), Bays >= 1);
        if (!CheckSites(*Yard.ToString(), Own, Origin, Forward, Offered)) return false;
    }
    return true;
}

#endif   // WITH_DEV_AUTOMATION_TESTS
