#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Game/BreakerLocalMapComponent.h"
#include "Interaction/BreakerRiftDoor.h"
#include "Save/BreakerSaveGame.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Kismet/GameplayStatics.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerLocalMapTest, "RiorsEdge.World.LocalMap.DiscoveryAndTracking",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerLocalMapTest::RunTest(const FString&)
{
    UWorld::InitializationValues Init; Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!World) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
    auto* Owner = World->SpawnActor<AActor>();
    auto* Map = NewObject<UBreakerLocalMapComponent>(Owner); Map->RegisterComponent();
    auto* Door = World->SpawnActor<ABreakerRiftDoor>();
    Door->Rift.EncounterId = TEXT("fernhall.entry"); Door->Rift.AreaLevel = 5;
    Door->Rift.AreaName = FText::FromString(TEXT("Entry Rift"));
    Door->SetActorLocation(FVector(5000,0,0));
    const auto Markers = Map->GetMarkers();
    if (!TestEqual(TEXT("Actual door produces one map marker"), Markers.Num(), 1)) return false;
    const FName Id = Markers[0].Id;
    TestFalse(TEXT("Unknown location cannot be tracked"), Map->Track(Id));
    TestFalse(TEXT("Distant sites remain undiscovered"), Map->DiscoverNearby(FVector::ZeroVector));
    TestTrue(TEXT("Approaching real door discovers it"), Map->DiscoverNearby(FVector(4000,0,0)));
    TestFalse(TEXT("No repeated discovery"), Map->DiscoverNearby(FVector(4000,0,0)));
    TestTrue(TEXT("Discovered real door can be tracked"), Map->Track(Id));
    FBreakerLocalMapMarker Target;
    TestTrue(TEXT("Track resolves current live actor"), Map->GetTrackedMarker(Target));
    TestEqual(TEXT("Rift difficulty is from its actual definition"), Target.Detail.ToString(), Door->GetDisplayDetail().ToString());
    Door->SetActorLocation(FVector(5200,0,0));
    Map->GetTrackedMarker(Target);
    TestEqual(TEXT("Position follows actual authored actor"), Target.Location.X, 5200.0);
    auto* Save = NewObject<UBreakerSaveGame>(); Save->DiscoveredMapSites = Map->GetDiscovered(); Save->TrackedMapSite = Id;
    TArray<uint8> Bytes;
    TestTrue(TEXT("Exploration serializes through actual save payload"), UGameplayStatics::SaveGameToMemory(Save, Bytes));
    auto* Restored = Cast<UBreakerSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes));
    if (!TestNotNull(TEXT("Save deserializes"), Restored)) return false;
    Map->Restore(Restored->DiscoveredMapSites, Restored->TrackedMapSite);
    TestTrue(TEXT("Discovery survives round trip"), Map->IsDiscovered(Id));
    TestEqual(TEXT("Tracking survives round trip"), Map->GetTracked(), Id);
    Door->Destroy();
    TestFalse(TEXT("Removed site never yields a stale waypoint"), Map->GetTrackedMarker(Target));
    TestTrue(TEXT("Can clear tracking"), Map->Track(NAME_None));
    auto* GateA = World->SpawnActor<ABreakerTravelPoint>();
    auto* GateB = World->SpawnActor<ABreakerTravelPoint>();
    GateB->SetActorLocation(FVector(8000,0,0));
    const auto Gates = Map->GetMarkers();
    TestEqual(TEXT("Both actual generic gates represented"), Gates.Num(), 2);
    if (Gates.Num() == 2) TestTrue(TEXT("Distinct gate sites have distinct discovery keys"), Gates[0].Id != Gates[1].Id);
    Map->DiscoverNearby(GateA->GetActorLocation());
    int32 DiscoveredGates = 0;
    for (const auto& Gate : Gates) DiscoveredGates += Map->IsDiscovered(Gate.Id) ? 1 : 0;
    TestEqual(TEXT("Nearby gate discovery does not reveal distant same-class gate"), DiscoveredGates, 1);
    return true;
}
#endif
