#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Game/BreakerHubBuilder.h"
#include "Interaction/BreakerNPC.h"
#include "Interaction/BreakerStashPoint.h"
#include "Interaction/BreakerTravelPoint.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerHubStationClearanceTest,
    "RiorsEdge.Hub.StationClearance", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerHubStationClearanceTest::RunTest(const FString& Parameters)
{
    for (const float Yaw : { 0.0f, 90.0f })
    {
        UWorld::InitializationValues Initialization;
        Initialization.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
        UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true,
            ERHIFeatureLevel::Num, &Initialization);
        if (!TestNotNull(TEXT("Hub fixture world"), World)) return false;
        ON_SCOPE_EXIT { World->DestroyWorld(false); };
        const FTransform Origin(FRotator(0, Yaw, 0), FVector(100, 200, 40));
        if (!TestNotNull(TEXT("Hub keeps its travel point"), UBreakerHubBuilder::BuildHub(World, Origin))) return false;
        const FVector ArrivalDirection = -Origin.GetRotation().GetForwardVector();
        const FVector Arrival = UBreakerHubBuilder::ArrivalTransform(Origin).GetLocation();
        const FCollisionShape PlayerCapsule = FCollisionShape::MakeCapsule(34.0f, 88.0f);
        auto SweepRoute = [&](const TArray<FVector>& Points, const AActor* Service, const TCHAR* Route)
        {
            FCollisionQueryParams Query(SCENE_QUERY_STAT(BreakerHubRouteTest), false, Service);
            for (int32 Index = 1; Index < Points.Num(); ++Index)
            {
                FHitResult Hit;
                const bool bBlocked = World->SweepSingleByChannel(Hit, Points[Index - 1], Points[Index],
                    FQuat::Identity, ECC_Pawn, PlayerCapsule, Query);
                TestFalse(FString::Printf(TEXT("%s yaw %.0f segment %d clear (blocker %s)"),
                    Route, Yaw, Index, *GetNameSafe(Hit.GetActor())), bBlocked);
            }
        };
        auto At = [&](float Forward, float Lateral)
        { return Origin.TransformPosition(FVector(Forward, Lateral, 100)); };
        // Walk around the central obelisk before joining the vendor lanes.
        const TArray<FVector> MainRoute = { Arrival, At(-300, -250), At(300, -250), At(450, 0) };
        SweepRoute(MainRoute, nullptr, TEXT("Arrival to market street"));
        int32 Vendors = 0;
        for (TActorIterator<ABreakerNPC> It(World); It; ++It)
        {
            ABreakerNPC* NPC = *It;
            ++Vendors;
            const UCapsuleComponent* Body = NPC->FindComponentByClass<UCapsuleComponent>();
            if (!TestNotNull(TEXT("Vendor capsule"), Body)) return false;
            const FVector Center = NPC->GetActorLocation();
            TestEqual(TEXT("Vendor feet meet plaza, not counter or empty air"),
                Center.Z - Body->GetScaledCapsuleHalfHeight(), Origin.GetLocation().Z, 0.5);
            TestTrue(TEXT("Vendor faces arriving players"), FVector::DotProduct(NPC->GetActorForwardVector(), ArrivalDirection) > 0.99f);
            FCollisionQueryParams Query(SCENE_QUERY_STAT(BreakerHubStationTest), false, NPC);
            // Slightly inset from the floor contact: this tests obstacles, not
            // the intended contact between the capsule sole and plaza.
            const FCollisionShape Capsule = FCollisionShape::MakeCapsule(32.0f, 86.0f);
            TestFalse(TEXT("Vendor body is clear of station props"),
                World->OverlapBlockingTestByChannel(Center, FQuat::Identity, ECC_Pawn, Capsule, Query));
            const float Side = Origin.InverseTransformPosition(Center).Y < 0 ? -1.0f : 1.0f;
            TArray<FVector> VendorRoute = { At(450, 0), At(450, Side * 500), At(1100, Side * 900),
                Center + ArrivalDirection * 100.0f };
            SweepRoute(VendorRoute, NPC, Side < 0 ? TEXT("Market street to Kess") : TEXT("Market street to quartermaster"));
            FHitResult Hit;
            TestFalse(TEXT("A player can approach within interaction range"),
                World->SweepSingleByChannel(Hit, Center + ArrivalDirection * 250.0f,
                    Center + ArrivalDirection * 100.0f, FQuat::Identity, ECC_Pawn, Capsule, Query));
        }
        TestEqual(TEXT("The hub retains exactly its two existing vendors"), Vendors, 2);
        int32 Stashes = 0, Perimeter = 0, ForgeArea = 0, QuartermasterArea = 0;
        for (TActorIterator<AActor> It(World); It; ++It)
        {
            Perimeter += It->ActorHasTag(TEXT("HubGroundingPerimeter")) ? 1 : 0;
            ForgeArea += It->ActorHasTag(TEXT("HubForgeWorkArea")) ? 1 : 0;
            QuartermasterArea += It->ActorHasTag(TEXT("HubQuartermasterWorkArea")) ? 1 : 0;
        }
        TestTrue(TEXT("Built hub carries its outer settlement landmarks"), Perimeter > 0);
        TestTrue(TEXT("Built forge has a distinct work area"), ForgeArea > 0);
        TestTrue(TEXT("Built quartermaster has a distinct stock area"), QuartermasterArea > 0);
        for (TActorIterator<ABreakerStashPoint> It(World); It; ++It)
        {
            ++Stashes;
            const FVector Local = Origin.InverseTransformPosition(It->GetActorLocation());
            TestTrue(TEXT("Stash retains its original service position"), Local.Equals(FVector(1200, 0, 100), 0.5f));
            SweepRoute({At(450, 0), At(1000, 0)}, *It, TEXT("Market street to stash"));
        }
        TestEqual(TEXT("Grounding adds no extra stash interaction"), Stashes, 1);
        for (TActorIterator<ABreakerTravelPoint> It(World); It; ++It)
        {
            const FVector Approach = It->GetActorLocation() + Origin.GetRotation().GetForwardVector() * 100.0f;
            SweepRoute({ Arrival, Approach }, *It, TEXT("Arrival to travel point"));
        }
        TestFalse(TEXT("Arrival remains clear"), World->OverlapBlockingTestByChannel(
            Arrival, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(34.0f, 88.0f)));
    }
    return true;
}
#endif
