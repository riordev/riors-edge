#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Game/BreakerHubBuilder.h"
#include "Interaction/BreakerNPC.h"
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
            FHitResult Hit;
            TestFalse(TEXT("A player can approach within interaction range"),
                World->SweepSingleByChannel(Hit, Center + ArrivalDirection * 250.0f,
                    Center + ArrivalDirection * 100.0f, FQuat::Identity, ECC_Pawn, Capsule, Query));
        }
        TestEqual(TEXT("The hub retains exactly its two existing vendors"), Vendors, 2);
        const FVector Arrival = UBreakerHubBuilder::ArrivalTransform(Origin).GetLocation();
        TestFalse(TEXT("Arrival remains clear"), World->OverlapBlockingTestByChannel(
            Arrival, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(34.0f, 88.0f)));
    }
    return true;
}
#endif
