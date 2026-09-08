#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BreakerLocalMapComponent.generated.h"

struct FBreakerLocalMapMarker
{
    FName Id;
    FText Label;
    FText Detail;
    FVector Location = FVector::ZeroVector;
    bool bRift = false;
};

// Local geography is read from live services and authored floor meshes.
// Discovery carries stable site keys between ordinary character saves.
UCLASS()
class RIORSEDGE_API UBreakerLocalMapComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UBreakerLocalMapComponent();
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
    TArray<FBreakerLocalMapMarker> GetMarkers() const;
    TArray<FBox2D> GetGround() const;
    bool IsDiscovered(FName Id) const { return Discovered.Contains(Id); }
    const TArray<FName>& GetDiscovered() const { return Discovered; }
    FName GetTracked() const { return Tracked; }
    void Restore(const TArray<FName>& Sites, FName Target) { Discovered = Sites; Tracked = Target; }
    bool Track(FName Id);
    bool DiscoverNearby(FVector Location);
    bool GetTrackedMarker(FBreakerLocalMapMarker& Out) const;
    FText GetRegionName() const;
private:
    UPROPERTY() TArray<FName> Discovered;
    UPROPERTY() FName Tracked;
};
