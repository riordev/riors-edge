#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerEnemy.h"
#include "Combat/BreakerRangedEnemy.h"
#include "Components/CapsuleComponent.h"
#include "Game/BreakerZoneBuilder.h"
#include "Game/BreakerFernhallCourtyardBuilder.h"
#include "Game/BreakerLocalMapComponent.h"
#include "Engine/World.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerFernhallCourtyardTest, "RiorsEdge.World.Fernhall.CourtyardRoute",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerFernhallCourtyardTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Init; Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    auto* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
    if (!TestNotNull(TEXT("isolated assembled world"),World)) return false;
    ON_SCOPE_EXIT { World->DestroyWorld(false); };
    TArray<FBreakerZonePiece> Pieces;
    if (!TestTrue(TEXT("shipped Fernhall assets collected"),UBreakerZoneBuilder::CollectZonePieces(UBreakerZoneBuilder::FernhallMeshFolder(),Pieces))) return false;
    BreakerFernhallCourtyard::FPlan Plan; FString Error;
    if (!TestTrue(TEXT("courtyard plan resolves shipped boundary"),BreakerFernhallCourtyard::MakePlan(Pieces,Plan,Error))) return false;
    FBreakerZoneMarkers Markers;
    if (!TestTrue(TEXT("actual complete Fernhall assembly"),UBreakerZoneBuilder::BuildFernhallYard(World,Markers))) return false;
    auto* MapOwner=World->SpawnActor<ABreakerCharacter>(FVector(0,0,10000),FRotator::ZeroRotator);
    if (!TestNotNull(TEXT("native map owner without save startup"),MapOwner)) return false;
    const auto MapGround=MapOwner->GetLocalMap()->GetGround();
    for (const auto& Footprint:Plan.GroundFootprints)
        TestTrue(TEXT("local map includes each actual courtyard floor footprint"),MapGround.ContainsByPredicate([&](const FBox2D& Ground)
        { return Ground.Min.Equals(FVector2D(Footprint.Min),.1) && Ground.Max.Equals(FVector2D(Footprint.Max),.1); }));
    TArray<FBox2D> FloorFootprints;
    for (const auto& Footprint:Plan.GroundFootprints) FloorFootprints.Emplace(FVector2D(Footprint.Min),FVector2D(Footprint.Max));
    for (const auto& Piece:Pieces)
        if (Piece.Name.StartsWith(TEXT("flr_"))) FloorFootprints.Emplace(FVector2D(Piece.Origin-Piece.Extent),FVector2D(Piece.Origin+Piece.Extent));
    // Only comparisons involving added floors: retain existing imported geometry semantics.
    for (int32 A=0;A<Plan.GroundFootprints.Num();++A)
        for (int32 B=A+1;B<FloorFootprints.Num();++B)
        {
            const auto& First=FloorFootprints[A]; const auto& Second=FloorFootprints[B];
            const double Width=FMath::Min(First.Max.X,Second.Max.X)-FMath::Max(First.Min.X,Second.Min.X);
            const double Height=FMath::Min(First.Max.Y,Second.Max.Y)-FMath::Max(First.Min.Y,Second.Min.Y);
            TestFalse(TEXT("new floors share edges without positive-area coplanar overlap"),Width>.1 && Height>.1);
        }
    const auto* PlayerCapsule=GetDefault<ABreakerCharacter>()->GetCapsuleComponent();
    const float Radius=PlayerCapsule->GetUnscaledCapsuleRadius();
    const float HalfHeight=PlayerCapsule->GetUnscaledCapsuleHalfHeight();
    const FCollisionShape Capsule=FCollisionShape::MakeCapsule(Radius,HalfHeight);
    FCollisionQueryParams Query(SCENE_QUERY_STAT(FernhallCourtyardRoute),false);
    const FCollisionObjectQueryParams StaticObjects(ECC_WorldStatic);
    auto FloorAt=[&](FVector Feet,const FString& Label)
    {
        bool Good=true;
        for (FVector Offset : {FVector::ZeroVector,FVector(Radius,0,0),FVector(-Radius,0,0),FVector(0,Radius,0),FVector(0,-Radius,0)})
        {
            FHitResult Hit;
            const bool Supported=World->LineTraceSingleByObjectType(Hit,Feet+Offset+FVector(0,0,20),Feet+Offset-FVector(0,0,30),StaticObjects,Query);
            Good &= TestTrue(FString::Printf(TEXT("%s floor footprint probe=%s feetZ=%.4f hit=%d actor=%s point=%s normal=%s"),
                *Label,*(Feet+Offset).ToString(),Feet.Z,Supported,*GetNameSafe(Hit.GetActor()),*Hit.ImpactPoint.ToString(),*Hit.ImpactNormal.ToString()),
                Supported && Hit.ImpactNormal.Z>=.7f && FMath::Abs(Hit.ImpactPoint.Z-Feet.Z)<2);
        }
        return Good;
    };
    if (!TestTrue(TEXT("authored route has dogleg"),Plan.RoutePoints.Num()>=4)) return false;
    for (int32 Direction : {1,-1})
        for (int32 Segment=1;Segment<Plan.RoutePoints.Num();++Segment)
        {
            const int32 A=Direction>0 ? Segment-1 : Plan.RoutePoints.Num()-Segment;
            const int32 B=A+Direction;
            const FVector Start=Plan.RoutePoints[A], End=Plan.RoutePoints[B];
            FHitResult Block;
            const bool bBlocked=World->SweepSingleByChannel(Block,Start+FVector(0,0,HalfHeight+2),End+FVector(0,0,HalfHeight+2),FQuat::Identity,ECC_Pawn,Capsule,Query);
            TestFalse(FString::Printf(TEXT("route %d segment%d capsule sweep blocked by %s"),Direction,Segment,*GetNameSafe(Block.GetActor())),
                bBlocked);
            const int32 Samples=FMath::CeilToInt(FVector::Distance(Start,End)/50.f);
            for (int32 Sample=0;Sample<=Samples;++Sample)
                if (!FloorAt(FMath::Lerp(Start,End,static_cast<float>(Sample)/Samples),FString::Printf(TEXT("route%d segment%d sample%d"),Direction,Segment,Sample))) return false;
        }
    FHitResult Sight;
    TestTrue(TEXT("dogleg blocks direct mouth-to-courtyard eye line"),World->LineTraceSingleByObjectType(Sight,
        Plan.RoutePoints[0]+FVector(0,0,160),Plan.RoutePoints.Last()+FVector(0,0,160),StaticObjects,Query));
    FHitResult InnerSight;
    TestTrue(TEXT("inside corridor direct eye line is blocked by dogleg"),World->LineTraceSingleByObjectType(InnerSight,
        Plan.At(150,0,160),Plan.At(1600,2200,160),StaticObjects,Query));
    TestTrue(TEXT("inner dogleg blocker belongs to actual new architecture"),InnerSight.GetActor() && InnerSight.GetActor()->Tags.Contains(TEXT("FernhallMaintenance")));
    FHitResult OuterWall;
    TestTrue(TEXT("outer corridor corner cannot leak a player capsule"),World->SweepSingleByChannel(OuterWall,
        Plan.At(1950,-350,HalfHeight+2),Plan.At(1950,-650,HalfHeight+2),FQuat::Identity,ECC_Pawn,Capsule,Query));
    TArray<FVector> SpawnFeet=Plan.MeleeSpawns; SpawnFeet.Add(Plan.RangedSpawn);
    TestEqual(TEXT("authored courtyard has four encounter positions"),SpawnFeet.Num(),4);
    for (int32 Index=0;Index<SpawnFeet.Num();++Index)
    {
        const auto* Enemy=Index==SpawnFeet.Num()-1 ? static_cast<const ABreakerEnemy*>(GetDefault<ABreakerRangedEnemy>()) : GetDefault<ABreakerEnemy>();
        const auto* Body=Enemy->FindComponentByClass<UCapsuleComponent>();
        if (!TestNotNull(TEXT("shipped enemy capsule"),Body)) return false;
        const FVector Center=SpawnFeet[Index]+FVector(0,0,Body->GetUnscaledCapsuleHalfHeight()+2);
        TestFalse(TEXT("courtyard spawn clear of walls and cover"),World->OverlapBlockingTestByChannel(Center,FQuat::Identity,ECC_Pawn,
            FCollisionShape::MakeCapsule(Body->GetUnscaledCapsuleRadius(),Body->GetUnscaledCapsuleHalfHeight()),Query));
        if (!FloorAt(SpawnFeet[Index],TEXT("encounter spawn"))) return false;
    }
    for (const TCHAR* Name : {TEXT("flr_yard"),TEXT("flr_yard_sub")})
    {
        const auto* Floor=Pieces.FindByPredicate([Name](const auto& Piece){return Piece.Name==Name;});
        if (!TestNotNull(TEXT("original yard floor retained"),Floor)) return false;
        // Select the near corner, away from authored central cover.
        const FVector Feet=Floor->Origin+FVector(-Floor->Extent.X+700,-Floor->Extent.Y+700,Floor->Extent.Z);
        if (!FloorAt(Feet,FString(Name))) return false;
    }
    return true;
}
#endif
