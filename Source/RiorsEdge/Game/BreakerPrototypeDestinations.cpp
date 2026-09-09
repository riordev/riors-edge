#include "Game/BreakerPrototypeDestinations.h"
#include "Game/BreakerContainmentHunt.h"
#include "Game/BreakerEnvironmentDressing.h"
#include "Combat/BreakerEnemy.h"
#include "Combat/BreakerRangedEnemy.h"
#include "Combat/BreakerSkirmisherEnemy.h"
#include "Combat/BreakerWardenEnemy.h"
#include "Playtest/BreakerKillTelemetryComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/PointLight.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Interaction/BreakerFernhallCache.h"
#include "Interaction/BreakerBasinRecorder.h"
#include "Interaction/BreakerCoastalUplink.h"
#include "Interaction/BreakerMeridianGroundCrew.h"
#include "Interaction/BreakerTravelPoint.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/PackageName.h"

const TArray<BreakerPrototypeDestinations::FDefinition>& BreakerPrototypeDestinations::All()
{
    // O2 PLACEHOLDER: authored district positions and fixed level brackets.
    // Campaign side destinations; ordinary patrols never read
    // player level. Return visits retain these levels and normal loot gates.
    static const TArray<FDefinition> Definitions = {
        {TEXT("RedBasin"), TEXT("Lvl_RedBasin"), TEXT("Red Basin"),
            TEXT("Areas 12-16 / Scorched farmland. Recover the survey recorder, then extract it at the crater relay."),
            {FVector(2200,0,0), FVector(8500,2000,0), FVector(15000,-500,0)},
            {TEXT("Scorched Fields"), TEXT("Burned Homestead"), TEXT("Impact Basin")}, {12,14,16}},
        {TEXT("StationZero"), TEXT("Lvl_StationZero"), TEXT("Station Zero"),
            TEXT("Areas 24-28 / Overrun research hub. Hunt the Containment Custodian; supply lockers are optional."),
            {FVector(2000,0,0), FVector(8000,-1200,0), FVector(12500,3600,0)},
            {TEXT("Research Reception"), TEXT("Specimen Gardens"), TEXT("Containment Laboratory")}, {24,26,28}},
        {TEXT("PortMeridian"), TEXT("Lvl_PortMeridian"), TEXT("Port Meridian"),
            TEXT("Areas 34-38 / Destroyed airport. Guide the ground crew from Departures to Maintenance Hangar; supply caches are optional."),
            {FVector(2200,0,0), FVector(8800,0,0), FVector(15400,0,0)},
            {TEXT("Departures Terminal"), TEXT("Broken Apron"), TEXT("Maintenance Hangar")}, {34,36,38}},
        {TEXT("BrokenCoast"), TEXT("Lvl_BrokenCoast"), TEXT("Broken Coast"),
            TEXT("Areas 42-46 / Flat shoreline. Restore the Signal Point coastal uplink; supply caches are optional."),
            {FVector(2200,0,0), FVector(8800,0,0), FVector(15400,0,0)},
            {TEXT("Strand Landing"), TEXT("Broken Jetty"), TEXT("Signal Point")}, {42,44,46}},
        {TEXT("Shatterpoint"), TEXT("Lvl_Shatterpoint"), TEXT("Shatterpoint"),
            TEXT("Areas 48-52 / Ruined streets. Recover supplies from Broken Avenue, Civic Square and Overpass Market."),
            {FVector(2200,0,0), FVector(8800,900,0), FVector(15400,-200,0)},
            {TEXT("Broken Avenue"), TEXT("Civic Square"), TEXT("Overpass Market")}, {48,50,52}}
    };
    return Definitions;
}
const BreakerPrototypeDestinations::FDefinition* BreakerPrototypeDestinations::Find(FName Id)
{
    return All().FindByPredicate([Id](const FDefinition& D) { return D.Id == Id; });
}
const BreakerPrototypeDestinations::FDefinition* BreakerPrototypeDestinations::ForWorld(const UObject* Context)
{
    const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(Context, EGetWorldErrorMode::ReturnNull) : nullptr;
    if (!World) return nullptr;
    FString Name = World->GetMapName(); Name.RemoveFromStart(World->StreamingLevelsPrefix);
    return All().FindByPredicate([&](const FDefinition& D) { return D.MapName == Name; });
}
bool BreakerPrototypeDestinations::HasMapPackage(const FDefinition& Definition)
{
    return FPackageName::DoesPackageExist(TEXT("/Game/Breaker/Maps/") + Definition.MapName);
}

BreakerPrototypeDestinations::FLayout BreakerPrototypeDestinations::Build(UWorld* World, FName Id)
{
    FLayout Result;
    const FDefinition* Definition = Find(Id);
    if (!World || !Definition || World->GetNetMode() == NM_Client) return Result;
    // A second player arriving must never duplicate a district or its rewards.
    for (TActorIterator<AActor> It(World); It; ++It)
        if (It->ActorHasTag(TEXT("PrototypeDestination.Root"))) return Result;
    UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));
    UStaticMesh* Cylinder = LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr,TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    if (!Cube || !Cylinder || !Material) return Result;
    const bool bBasin = Id == TEXT("RedBasin");
    const bool bStation = Id == TEXT("StationZero");
    const bool bAirport = Id == TEXT("PortMeridian");
    const bool bCoast = Id == TEXT("BrokenCoast");
    const bool bCity = Id == TEXT("Shatterpoint");
    // O2 PLACEHOLDER palette/placement: scorched soil versus cool lab panels,
    // with amber route lamps. Rift teal remains reserved for travel objects.
    const FLinearColor Ground = bBasin ? FLinearColor(.16f,.085f,.05f) : bAirport ? FLinearColor(.13f,.14f,.15f) : bCoast ? FLinearColor(.31f,.29f,.23f) : bCity ? FLinearColor(.12f,.12f,.13f) : FLinearColor(.09f,.13f,.19f);
    const FLinearColor Wall = bBasin ? FLinearColor(.27f,.14f,.08f) : bAirport ? FLinearColor(.29f,.30f,.28f) : bCoast ? FLinearColor(.28f,.31f,.30f) : bCity ? FLinearColor(.25f,.23f,.24f) : FLinearColor(.18f,.25f,.34f);
    const FLinearColor Trim = bBasin ? FLinearColor(.15f,.12f,.10f) : bAirport ? FLinearColor(.52f,.40f,.16f) : bCoast ? FLinearColor(.34f,.26f,.17f) : bCity ? FLinearColor(.38f,.34f,.27f) : FLinearColor(.42f,.49f,.55f);
    bool bGeometryValid = true;
    int32 RecorderCount=0;
    auto Shape = [&](const TCHAR* Name,UStaticMesh* Mesh,FVector At,FVector Size,FLinearColor Color,
        bool bGround=false,FRotator Rotation=FRotator::ZeroRotator)
    {
        auto* Actor = World->SpawnActor<AStaticMeshActor>(At,Rotation);
        if (!Actor) { bGeometryValid=false; return Actor; }
        Actor->Tags.Add(TEXT("PrototypeDestination.Geometry"));
        if (bGround) Actor->Tags.Add(TEXT("BreakerMapGround"));
#if WITH_EDITOR
        Actor->SetActorLabel(FString::Printf(TEXT("%s.%s"),*Id.ToString(),Name));
#endif
        auto* Component=Actor->GetStaticMeshComponent();
        Component->SetMobility(EComponentMobility::Movable); Component->SetStaticMesh(Mesh);
        Component->SetWorldScale3D(Size/100.f); Component->SetCollisionProfileName(TEXT("BlockAll"));
        Component->SetCanEverAffectNavigation(true);
        auto* Tint=UMaterialInstanceDynamic::Create(Material,Component); Tint->SetVectorParameterValue(TEXT("Color"),Color);
        Component->SetMaterial(0,Tint); Component->SetMobility(EComponentMobility::Static); Actor->SetActorTickEnabled(false);
        return Actor;
    };
    auto Dress = [&](const TCHAR* Asset,FVector At,float Height,float Yaw)
    {
        if (auto* Actor=BreakerPlaceEnvironmentDressing(World,Asset,At,Height,Yaw))
        { Actor->Tags.Add(TEXT("PrototypeDestination.Dressing")); ++Result.DressingCount; }
    };
    if(bCoast)
    {
        // O2 PLACEHOLDER: continuous flat coastal ground backs every district
        // and connector. The visual sea lies outside it, below walking height.
        // It is not a swimming surface, a new hazard or a playable map ground.
        const float MinX=ArrivalLocation().X-800.f;
        const float MaxX=Definition->Districts.Last().X+3400.f;
        const float MidX=(MinX+MaxX)*.5f;
        const float Length=MaxX-MinX;
        if(auto* Land=Shape(TEXT("CoastalPlain"),Cube,FVector(MidX,0,-101),FVector(Length,5600,200),Ground,true))
            Land->Tags.Add(TEXT("Destination.Landmark.CoastalPlain"));
        if(auto* Sea=Shape(TEXT("OceanSurface"),Cube,FVector(MidX,-33000,-250),FVector(90000,60400,20),FLinearColor(.07f,.12f,.18f)))
        {
            Sea->SetActorEnableCollision(false);
            Sea->GetStaticMeshComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            Sea->GetStaticMeshComponent()->SetCanEverAffectNavigation(false);
            Sea->Tags.Add(TEXT("Destination.Landmark.Ocean"));
        }
        // The sea-facing edge is a visible low concrete seawall, outside
        // guard/cache/return routes. No invisible blocking volume is authored.
        Shape(TEXT("CoastalSeaWall"),Cube,FVector(MidX,-2710,95),FVector(Length,150,190),Wall);
    }
    Result.Arrival=ArrivalLocation();
    // O2 PLACEHOLDER: continuous approach floor reaches the first court;
    // safety comes from distance, not invulnerability or disabled encounters.
    const FVector ApproachStart(Result.Arrival.X-600,0,0);
    const FVector ApproachEnd=Definition->Districts[0];
    const FVector ApproachDelta=ApproachEnd-ApproachStart;
    Shape(TEXT("ArrivalApproach"),Cube,(ApproachStart+ApproachEnd)*.5f-FVector(0,0,70),
        FVector(ApproachDelta.Size2D()+600,2200,140),Ground,true,ApproachDelta.Rotation());
    Result.WalkingRoute={Result.Arrival};
    TArray<TArray<ABreakerEnemy*>> MeridianPockets;
    // O2 PLACEHOLDER: centimetre-authored wide walking route; ledges and cover
    // are optional. No destination requires dash, wall-run or a class.
    for (int32 Pocket=0;Pocket<3;++Pocket)
    {
        const FVector C=Definition->Districts[Pocket];
        auto* Floor=Shape(TEXT("DistrictGround"),Cube,C+FVector(0,0,-100),FVector(6200,5200,200),Ground,true);
        if (Pocket==0 && Floor) Floor->Tags.Add(TEXT("PrototypeDestination.Root"));
        Result.WalkingRoute.Add(C+FVector(0,0,100));
        for (int32 Side : {-1,1})
        {
            if (bBasin)
            {
                // Scorched perimeter banks frame the fields; open mouths retain
                // the central route and a wide outer flanking lane.
                for (int32 Rock=0;Rock<6;++Rock)
                {
                    const float X=C.X-2500+Rock*1000;
                    // O2 PLACEHOLDER: low field banks open the farm skyline;
                    // taller ejecta only surround the impact district.
                    const float BankHeight=Pocket==2 ? 1100+Rock%2*280 : 360+Rock%2*80;
                    Shape(TEXT("ScorchedBoundaryBank"),Cube,FVector(X,C.Y+Side*2850,BankHeight*.5f),
                        FVector(1300,850,BankHeight),Wall,false,FRotator(0,Side*(9+Rock*3),Side*5));
                    Shape(TEXT("BurnedEarthShelf"),Cube,FVector(X+120,C.Y+Side*2450,170),FVector(700,500,340),Wall*.85f);
                }
                Dress(TEXT("DeadTree_1"),C+FVector(-1200,Side*1800,0),850,Side*35);
                Dress(TEXT("DeadTree_1"),C+FVector(2500,Side*2300,0),420,Side*65);
                if (Pocket==0)
                    for (int32 Row=0;Row<5;++Row)
                    {
                        // O2 PLACEHOLDER: charred parallel crop beds, decorative
                        // so they cannot change the existing standing route.
                        if (auto* Bed=Shape(TEXT("ScorchedCropRow"),Cube,C+FVector(0,Side*(1700+Row*95),5),
                            FVector(4200,32,10),FLinearColor(.045f,.028f,.018f)))
                        { Bed->SetActorEnableCollision(false); Bed->Tags.Add(TEXT("Destination.Landmark.ScorchedFields")); }
                    }
            }
            else if (bStation)
            {
                // Lab wings and broken roof bays frame an overrun research hub;
                // the central specimen garden remains open to the sky.
                for (int32 Bay=0;Bay<5;++Bay)
                {
                    const FVector Post=C+FVector(-2200+Bay*1100,Side*2100,0);
                    Shape(TEXT("SteelColumn"),Cube,Post+FVector(0,0,350),FVector(140,140,700),Trim);
                    Dress(TEXT("Column_MetalSupport"),Post+FVector(0,Side*180,0),700,90);
                    if (Pocket!=1 && Bay%2==0)
                        Shape(TEXT("RoofBay"),Cube,Post+FVector(0,-Side*450,720),FVector(1050,1200,80),Wall);
                }
                for (int32 Panel=-1;Panel<=1;++Panel)
                    Shape(TEXT("LaboratoryWall"),Cube,C+FVector(Panel*1100,Side*2500,240),FVector(850,100,480),Wall);
                Dress(TEXT("Door_Metal"),C+FVector(0,Side*2430,0),310,Side>0?180:0);
                // O2 PLACEHOLDER: existing primitive laboratory workstations,
                // outside the central route and each authored guard position.
                // Keep the final laboratory's southwest diagonal entry clear.
                const float BenchX=(Pocket==2 && Side<0) ? 0.f : -1500.f; // O2 PLACEHOLDER
                const FVector Bench=C+FVector(BenchX,Side*1750,0);
                if (auto* Table=Shape(TEXT("ResearchBench"),Cube,Bench+FVector(0,0,90),FVector(650,260,35),Trim))
                    Table->Tags.Add(TEXT("Destination.Landmark.ResearchBench"));
                for (int32 Instrument=0;Instrument<3;++Instrument)
                {
                    Shape(TEXT("InstrumentHousing"),Cube,Bench+FVector(-210+Instrument*210,0,155),FVector(125,100,95),Wall);
                    Shape(TEXT("InstrumentDisplay"),Cube,Bench+FVector(-210+Instrument*210,-Side*53,165),FVector(100,8,60),FLinearColor(.18f,.35f,.32f));
                }
                Dress(TEXT("Bush_Common"),C+FVector(-2100,Side*2300,0),180,Side*35);
                if (Pocket==1)
                {
                    if (auto* Planter=Shape(TEXT("SpecimenPlanter"),Cube,C+FVector(-2400,Side*1600,65),FVector(650,650,130),Trim))
                        Planter->Tags.Add(TEXT("Destination.Landmark.SpecimenGarden"));
                    Dress(TEXT("CommonTree_1"),C+FVector(-2400,Side*1600,130),950,Side*40);
                    Dress(TEXT("Bush_Common"),C+FVector(1500,Side*2200,0),270,Side*70);
                }
            }
            else if (bAirport)
            {
                // O2 PLACEHOLDER: destroyed terminal facade and hangar portals
                // frame a continuous central aisle; the apron stays open sky.
                if (Pocket!=1)
                    for (int32 Bay=0;Bay<5;++Bay)
                    {
                        const FVector Post=C+FVector(-2200+Bay*1100,Side*2450,0);
                        const float Height=Pocket==0 ? 800.f : 1300.f;
                        Shape(TEXT("AirportPortalPost"),Cube,Post+FVector(0,0,Height*.5f),FVector(130,130,Height),Wall);
                        Dress(TEXT("Column_MetalSupport"),Post+FVector(0,Side*160,0),Height,90);
                        if (Bay%2==0)
                            if (auto* Roof=Shape(TEXT("BrokenAirportRoof"),Cube,C+FVector(-2200+Bay*1100,Side*1400,Height),
                                FVector(950,2200,90),Wall,false,FRotator(0,0,Side*(Bay==2?7:0))))
                                Roof->Tags.Add(Pocket==0 ? TEXT("Destination.Landmark.DeparturesTerminal") : TEXT("Destination.Landmark.MaintenanceHangar"));
                        if (Pocket==0 && Bay!=2)
                            Shape(TEXT("ShatteredTerminalWall"),Cube,Post+FVector(350,0,230),FVector(600,100,460),Wall);
                    }
                else
                    for (int32 Mark=0;Mark<6;++Mark)
                        if (auto* Paint=Shape(TEXT("ApronLaneMark"),Cube,C+FVector(-2200+Mark*850,Side*2200,1),
                            FVector(500,30,2),Trim)) Paint->SetActorEnableCollision(false);
                if(Pocket==0)
                {
                    // O2 PLACEHOLDER: vacant check-in desks face the open aisle;
                    // cache on the opposite end remains physically accessible.
                    for(int32 Desk=0;Desk<3;++Desk)
                        Shape(TEXT("CheckInDesk"),Cube,C+FVector(-1900+Desk*500,Side*1900,65),FVector(380,240,130),Trim);
                }
                Dress(TEXT("Column_Pipes"),C+FVector(2600,Side*2450,0),420,Side*90);
            }
            else if(bCoast)
            {
                // O2 PLACEHOLDER: a flat, open shoreline with low weathered
                // piles rather than tall enclosing terrain or city walls.
                for(int32 Pile=0;Pile<5;++Pile)
                {
                    const FVector At=C+FVector(-2200+Pile*1100,Side*2350,0);
                    Shape(TEXT("MooringBollard"),Cylinder,At+FVector(0,0,80),FVector(100,100,160),Trim);
                    if(Side>0 && Pile%2==0)
                        Dress(TEXT("Bush_Common"),At+FVector(0,150,0),220,Pile*35);
                }
                Dress(TEXT("Column_Pipes"),C+FVector(-2100,Side*2100,0),300,Side*90);
                if(Pocket==0)
                    Shape(TEXT("LandingSupplyStack"),Cube,C+FVector(-1600,Side*1900,85),FVector(650,400,170),Trim);
            }
            else if(bCity)
            {
                // O2 PLACEHOLDER: irregular city frontages face the open road.
                // Building collision remains outside all guard/cache approaches.
                for(int32 Block=0;Block<4;++Block)
                {
                    const FVector At=C+FVector(-2400+Block*1600,Side*2850,0);
                    const float Height=1500.f+((Block+Pocket)%3)*650.f;
                    if(auto* Building=Shape(TEXT("RuinedCityBlock"),Cube,At+FVector(0,0,Height*.5f),FVector(1150,950,Height),Wall))
                        Building->Tags.Add(TEXT("Destination.Landmark.CityBlocks"));
                    for(int32 Storey=0;Storey<3;++Storey)
                        if(auto* Windows=Shape(TEXT("DarkWindowBand"),Cube,At+FVector(0,-Side*479,450+Storey*550),
                            FVector(920,8,180),FLinearColor(.045f,.055f,.07f)))Windows->SetActorEnableCollision(false);
                    Shape(TEXT("FracturedRoofParapet"),Cube,At+FVector(160,0,Height+90),FVector(500,1000,180),Trim,false,FRotator(0,0,Side*6));
                    Dress(TEXT("Column_Pipes"),At+FVector(-430,-Side*500,0),800,Side*90);
                }
                // Sidewalk slabs are flush with the existing walkable district.
                Shape(TEXT("CitySidewalk"),Cube,C+FVector(0,Side*2150,-10),FVector(6000,650,20),Trim,true);
                if(Pocket==2)
                    for(int32 Stall=0;Stall<3;++Stall)
                    {
                        const FVector At=C+FVector(-2000+Stall*700,Side*1900,0);
                        Shape(TEXT("AbandonedMarketCounter"),Cube,At+FVector(0,0,65),FVector(450,260,130),Trim);
                        Shape(TEXT("TornMarketCanopy"),Cube,At+FVector(0,0,320),FVector(580,420,35),Wall,false,FRotator(0,0,Side*8));
                    }
            }
            // Real low cover and a full-height line break, with side approaches.
            // O2 PLACEHOLDER: keep the laboratory southwest arrival diagonal
            // outside the full standing-capsule footprint of this cover.
            const float CoverX=(bStation && Pocket==2 && Side<0) ? 850.f : -850.f;
            Shape(TEXT("LowCover"),Cube,C+FVector(CoverX,Side*600,55),FVector(550,240,110),Trim);
            Shape(TEXT("SightBreak"),Cube,C+FVector(100,Side*1150,200),FVector(240,650,400),Wall);
            auto* Lamp=World->SpawnActor<APointLight>(C+FVector(-1800,Side*1600,430),FRotator::ZeroRotator);
            if (Lamp)
            {
                Lamp->GetLightComponent()->SetMobility(EComponentMobility::Movable);
                CastChecked<UPointLightComponent>(Lamp->GetLightComponent())->SetLightColor(FLinearColor(1.f,.58f,.20f));
                CastChecked<UPointLightComponent>(Lamp->GetLightComponent())->SetIntensity(2800.f);
                CastChecked<UPointLightComponent>(Lamp->GetLightComponent())->SetAttenuationRadius(1500.f);
            }
        }
        if (bBasin && Pocket==1)
        {
            // O2 PLACEHOLDER: a charred barn frame and partly collapsed gable
            // establish a ruined farm rather than extraction machinery.
            const FVector Barn=C+FVector(1800,-1750,0);
            const FLinearColor Char(.055f,.037f,.025f);
            for (int32 X : {-1,1}) for (int32 Y : {-1,1})
                if (auto* Post=Shape(TEXT("BurnedBarnPost"),Cube,Barn+FVector(X*300,Y*300,300),FVector(65,65,600),Char))
                    Post->Tags.Add(TEXT("Destination.Landmark.BurnedHomestead"));
            Shape(TEXT("BarnRidge"),Cube,Barn+FVector(0,0,770),FVector(720,75,75),Char);
            for (int32 Slope : {-1,1})
                Shape(TEXT("BrokenBarnGable"),Cube,Barn+FVector(-140,Slope*170,675),FVector(380,420,45),Char,false,FRotator(0,0,Slope*30));
            Shape(TEXT("FallenBarnBeam"),Cube,Barn+FVector(210,0,180),FVector(65,620,65),Char,false,FRotator(0,15,28));
        }
        else if (bBasin && Pocket==2)
        {
            // O2 PLACEHOLDER: a real raised, broken impact rim enclosing a
            // depressed dark floor; kept clear of route, guards and cache.
            const FVector Impact=C+FVector(1800,-1650,0);
            if (auto* Basin=Shape(TEXT("ImpactBasinFloor"),Cylinder,Impact+FVector(0,0,9),FVector(1200,1200,18),FLinearColor(.035f,.028f,.025f)))
                Basin->Tags.Add(TEXT("Destination.Landmark.ImpactCrater"));
            for (int32 Segment=0;Segment<12;++Segment)
            {
                const float Angle=Segment*30.f;
                const FVector Radial(FMath::Cos(FMath::DegreesToRadians(Angle)),FMath::Sin(FMath::DegreesToRadians(Angle)),0);
                if (auto* Rim=Shape(TEXT("FracturedImpactRim"),Cube,Impact+Radial*650+FVector(0,0,115),
                    FVector(340,220,230),Wall,false,FRotator(0,Angle+90,Segment%2?14:-14)))
                    Rim->Tags.Add(TEXT("Destination.Landmark.ImpactCrater"));
            }
        }
        else if (bStation && Pocket==2)
        {
            // O2 PLACEHOLDER: a breached specimen containment chamber, with
            // instrument rings and growth escaping the service collar.
            const FVector At=C+FVector(1900,-1500,0);
            if (auto* Chamber=Shape(TEXT("ContainmentChamber"),Cylinder,At+FVector(0,0,750),FVector(1150,1150,1500),Wall))
                Chamber->Tags.Add(TEXT("Destination.Landmark.ContainmentLaboratory"));
            for (int32 Band=0;Band<4;++Band)
                Shape(TEXT("ContainmentInstrumentRing"),Cylinder,At+FVector(0,0,230+Band*350),FVector(1250,1250,70),Trim);
            Dress(TEXT("Column_Pipes"),At+FVector(-800,0,0),1100,180);
            Dress(TEXT("Bush_Common"),At+FVector(0,0,1500),550,0);
            Dress(TEXT("CommonTree_1"),At+FVector(350,0,1500),1100,45);
        }
        if(bAirport)
        {
            // O2 PLACEHOLDER: airport-specific silhouettes use existing native
            // primitives. The wreck is outside all guard/cache/central routes.
            if(Pocket==1)
            {
                const FVector Wreck=C+FVector(1700,-1950,0);
                auto* Hull=Shape(TEXT("BrokenAircraftFuselage"),Cylinder,Wreck+FVector(0,0,210),
                    FVector(350,350,1900),FLinearColor(.46f,.47f,.43f),false,FRotator(90,8,0));
                if(Hull)Hull->Tags.Add(TEXT("Destination.Landmark.AircraftWreck"));
                Shape(TEXT("TornAircraftWing"),Cube,Wreck+FVector(-150,-140,155),FVector(700,950,45),Wall,false,FRotator(0,-18,12));
                Shape(TEXT("AircraftTailFin"),Cube,Wreck+FVector(700,20,410),FVector(330,65,470),Trim,false,FRotator(0,8,0));
            }
            if(Pocket==0)
                Shape(TEXT("DeparturesSign"),Cube,C+FVector(-2500,0,760),FVector(100,1800,240),Trim);
            if(Pocket==2)
            {
                for(int32 Bay=0;Bay<3;++Bay)
                    Shape(TEXT("HangarRoofTruss"),Cube,C+FVector(-2100+Bay*2100,0,1400),FVector(110,5100,130),Trim);
                Shape(TEXT("CollapsedHangarDoor"),Cube,C+FVector(2450,-2050,370),FVector(110,900,800),Wall,false,FRotator(0,10,15));
                Dress(TEXT("Column_Pipes"),C+FVector(-2200,-2000,0),750,90);
            }
        }
        if(bCoast)
        {
            // O2 PLACEHOLDER: noninteractive coastal landmarks keep all real
            // objectives on the continuous shore, not the broken offshore pier.
            if(Pocket==0)
            {
                const FVector At=C+FVector(1800,-1900,0);
                if(auto* Hull=Shape(TEXT("StrandedBoatHull"),Cube,At+FVector(0,0,140),FVector(1500,470,240),Trim,false,FRotator(0,8,-10)))
                    Hull->Tags.Add(TEXT("Destination.Landmark.StrandLanding"));
                Shape(TEXT("BoatCabin"),Cube,At+FVector(200,0,330),FVector(430,350,300),Wall,false,FRotator(0,8,-10));
                Shape(TEXT("BoatMast"),Cylinder,At+FVector(-300,0,640),FVector(35,35,1000),Trim,false,FRotator(0,0,-10));
            }
            if(Pocket==1)
            {
                for(int32 Segment=0;Segment<5;++Segment)
                {
                    const FVector At=C+FVector(1900,-2100-Segment*600,0);
                    // A separated pier silhouette beyond the seawall is
                    // decorative: there is no missing objective to jump to.
                    if(auto* Deck=Shape(TEXT("BrokenPierDeck"),Cube,At+FVector(0,0,-30-Segment*25),FVector(650,450,80),Trim,false,FRotator(Segment*3,0,0)))
                    {Deck->SetActorEnableCollision(false);Deck->Tags.Add(TEXT("Destination.Landmark.BrokenJetty"));}
                    for(int32 Side:{-1,1})
                        if(auto* Pile=Shape(TEXT("PierTimberPile"),Cylinder,At+FVector(Side*290,0,-160),FVector(95,95,620),Trim))
                            Pile->SetActorEnableCollision(false);
                }
            }
            if(Pocket==2)
            {
                const FVector At=C+FVector(1900,-1900,0);
                if(auto* Tower=Shape(TEXT("SignalBeaconTower"),Cylinder,At+FVector(0,0,750),FVector(560,560,1500),Wall))
                    Tower->Tags.Add(TEXT("Destination.Landmark.SignalPoint"));
                Shape(TEXT("BeaconLanternRoom"),Cylinder,At+FVector(0,0,1580),FVector(800,800,220),Trim);
                Shape(TEXT("BeaconLens"),Cylinder,At+FVector(0,0,1740),FVector(480,480,130),FLinearColor(.75f,.62f,.35f));
                Shape(TEXT("BeaconCap"),Cylinder,At+FVector(0,0,1850),FVector(850,850,90),Wall);
            }
        }
        if(bCity)
        {
            // O2 PLACEHOLDER: each street block has a distinct landmark, all
            // beyond the existing standing route and guarded supply locations.
            if(Pocket==0)
                if(auto* Sign=Shape(TEXT("BrokenAvenueGantry"),Cube,C+FVector(-2200,0,650),FVector(140,3200,220),Trim,false,FRotator(0,0,6)))
                    Sign->Tags.Add(TEXT("Destination.Landmark.BrokenAvenue"));
            if(Pocket==1)
            {
                const FVector At=C+FVector(1900,-1900,0);
                if(auto* Monument=Shape(TEXT("ShatteredCivicMonument"),Cube,At+FVector(0,0,680),FVector(520,520,1360),Wall))
                    Monument->Tags.Add(TEXT("Destination.Landmark.CivicSquare"));
                Shape(TEXT("CivicClockFace"),Cylinder,At+FVector(-285,0,1050),FVector(340,340,40),Trim,false,FRotator(90,0,0));
                Shape(TEXT("FallenMonumentCrown"),Cube,At+FVector(-650,0,130),FVector(470,470,260),Trim,false,FRotator(0,25,15));
            }
            if(Pocket==2)
            {
                if(auto* Deck=Shape(TEXT("FracturedOverpassDeck"),Cube,C+FVector(200,0,1020),FVector(1500,6500,180),Wall))
                    Deck->Tags.Add(TEXT("Destination.Landmark.OverpassMarket"));
                for(int32 Side:{-1,1})
                {
                    Shape(TEXT("OverpassPier"),Cube,C+FVector(200,Side*2230,450),FVector(220,280,900),Trim);
                    Shape(TEXT("OverpassBrokenRail"),Cube,C+FVector(-450,Side*1700,1210),FVector(100,1800,230),Trim,false,FRotator(0,0,Side*8));
                }
            }
        }
        TArray<ABreakerEnemy*> Guards;
        const FVector Offsets[]={FVector(-900,-350,100),FVector(50,450,100),FVector(1250,-500,100),
            FVector(-1200,900,100),FVector(1400,700,100),FVector(450,-1450,100)};
        for (int32 Index=0;Index<6;++Index)
        {
            TSubclassOf<ABreakerEnemy> Class=ABreakerEnemy::StaticClass();
            if (Index==2 || (Pocket==0 && Index==4)) Class=ABreakerRangedEnemy::StaticClass();
            if (Index==3 && (Pocket>0 || !bBasin)) Class=ABreakerWardenEnemy::StaticClass();
            if (Index==5 || (Pocket==2 && Index==4)) Class=ABreakerSkirmisherEnemy::StaticClass();
            // O2 PLACEHOLDER: street firing line, armoured civic pair, then
            // fast underpass flankers, using existing chassis and rewards.
            if(bCity && Pocket==0 && Index==3)Class=ABreakerRangedEnemy::StaticClass();
            if(bCity && Pocket==1 && Index==4)Class=ABreakerWardenEnemy::StaticClass();
            if(bCity && Pocket==2 && Index==0)Class=ABreakerSkirmisherEnemy::StaticClass();
            FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            const FVector At=C+Offsets[Index];
            auto* Enemy=World->SpawnActor<ABreakerEnemy>(Class,At,FRotator(0,180,0),Params);
            if (!Enemy) continue;
            Enemy->ConfigureWave(Definition->AreaLevels[Pocket]);
            auto* Capsule=Enemy->FindComponentByClass<UCapsuleComponent>();
            FCollisionQueryParams Query(SCENE_QUERY_STAT(PrototypeEnemyFloor),false,Enemy);
            FHitResult EnemyFloor;
            if (!Capsule || !World->LineTraceSingleByObjectType(EnemyFloor,At+FVector(0,0,500),At-FVector(0,0,500),
                FCollisionObjectQueryParams(ECC_WorldStatic),Query) || EnemyFloor.ImpactNormal.Z<.7f)
            { Enemy->Destroy(); continue; }
            const FVector Stand=EnemyFloor.ImpactPoint+FVector(0,0,Capsule->GetScaledCapsuleHalfHeight()+2);
            if (World->OverlapBlockingTestByChannel(Stand,FQuat::Identity,ECC_Pawn,
                FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(),Capsule->GetScaledCapsuleHalfHeight()),Query))
            { Enemy->Destroy(); continue; }
            Enemy->SetActorLocation(Stand);
            Enemy->ConfigureEncounter(Stand,Index*1.3f);
            Enemy->Tags.Add(FName(*FString::Printf(TEXT("Destination.%s.Pocket.%d"),*Id.ToString(),Pocket)));
            // O2 PLACEHOLDER authored target assignment: the laboratory Warden
            // is the Containment Custodian; five surrounding guards are optional.
            if (bStation && Pocket==2 && Index==3) UBreakerContainmentHunt::AttachTo(Enemy);
            UBreakerKillTelemetryComponent::AttachTo(Enemy);
            Guards.Add(Enemy); Result.Enemies.Add(Enemy);
        }
        // O2 PLACEHOLDER placement: recovery in district two, extraction in
        // district three. Existing central walking route remains unchanged.
        if(bBasin&&Pocket>0)
            if(auto* Recorder=World->SpawnActor<ABreakerBasinRecorder>(C+FVector(1700,Pocket==2?-650:-900,100),FRotator(0,180,0)))
            { ++RecorderCount;if(Pocket==2)Recorder->ConfigureExtraction(); }
        if(bAirport)MeridianPockets.Add(Guards);
        const FVector CacheAt=C+FVector(1000,1700,90);
        if (auto* Cache=World->SpawnActor<ABreakerFernhallCache>(CacheAt,FRotator(0,180,0)))
        {
            Cache->DisplayName=FText::FromString(Definition->DistrictNames[Pocket]+(bStation?TEXT(" research supply locker"):TEXT(" supply cache")));
            Cache->Tags.Remove(TEXT("Fernhall.Cache"));
            Cache->Tags.Add(TEXT("PrototypeDestination.Cache"));
            Cache->Tags.Add(FName(*FString::Printf(TEXT("Destination.Site.%s.%d"),*Id.ToString(),Pocket)));
            Cache->Configure(Definition->AreaLevels[Pocket],Guards,6);
            Result.Caches.Add(Cache);
        }
    }
    // O2 PLACEHOLDER: inland approach to the existing Signal Point tower;
    // finite level-46 defenders stay unchanged and may already be defeated.
    bool bUplinkPresent=!bCoast;
    if(bCoast)
        if(auto* Uplink=World->SpawnActor<ABreakerCoastalUplink>(Definition->Districts.Last()+FVector(1900,-1100,100),FRotator(0,180,0)))
            bUplinkPresent=true;
    bool bCrewPresent=!bAirport;
    if(bAirport)
        if(auto* Crew=World->SpawnActor<ABreakerMeridianGroundCrew>(Definition->Districts[0]+GetDefault<ABreakerMeridianGroundCrew>()->ShelterOffset,FRotator::ZeroRotator))
        {
            bCrewPresent=Crew->ConfigureMeridian(Definition->Districts,MeridianPockets);
            if(!bCrewPresent)Crew->Destroy();
        }
    // Wide connectors overlap both districts. The laboratory turns north;
    // the farm track bends toward the impact basin.
    for (int32 Link=0;Link<2;++Link)
    {
        const FVector A=Definition->Districts[Link], B=Definition->Districts[Link+1];
        const FVector Delta=B-A; const FVector Mid=(A+B)*.5f;
        Shape(TEXT("ConnectingRoute"),Cube,Mid-FVector(0,0,70),FVector(Delta.Size2D()+600,1700,140),Ground,true,Delta.Rotation());
        Result.WalkingRoute.Insert(Mid+FVector(0,0,112),2+Link*2);
        for (int32 Sign : {-1,1})
        {
            const FVector Right=FRotationMatrix(Delta.Rotation()).GetUnitAxis(EAxis::Y);
            Dress(bBasin?TEXT("DeadTree_1"):TEXT("Column_MetalSupport"),Mid+Right*Sign*1200,1000,Delta.Rotation().Yaw);
        }
    }
    for (const FVector At : {Result.Arrival+FVector(100,-850,0),Definition->Districts.Last()+FVector(2200,1700,100)})
        if (auto* Gate=World->SpawnActor<ABreakerTravelPoint>(At,FRotator::ZeroRotator))
        {
            Gate->ExcludedDestinationId=Id;
            Gate->Tags.Add(TEXT("PrototypeDestination.ReturnGate"));
            Result.Gates.Add(Gate);
        }
    Result.bComplete=bGeometryValid && bUplinkPresent && bCrewPresent && (!bBasin || RecorderCount==2) && Result.Enemies.Num()==18 && Result.Caches.Num()==3 && Result.Gates.Num()==2 && Result.DressingCount>=6;
    UE_LOG(LogTemp,Display,TEXT("[PrototypeDestination] %s complete=%d fixed=%d-%d enemies=%d caches=%d dressing=%d"),
        *Definition->DisplayName,Result.bComplete,Definition->AreaLevels[0],Definition->AreaLevels.Last(),Result.Enemies.Num(),Result.Caches.Num(),Result.DressingCount);
    return Result;
}
