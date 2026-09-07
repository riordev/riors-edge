#pragma once

#include "CoreMinimal.h"
#include "Components/SkeletalMeshComponent.h"
#include "BreakerFirstPersonArms.generated.h"

class UAnimSequence;
class UBreakerWeaponComponent;
class UStaticMeshComponent;

// A separate cropped mesh: the third-person body never enters the camera rig.
UCLASS()
class RIORSEDGE_API UBreakerFirstPersonArms : public USkeletalMeshComponent
{
    GENERATED_BODY()
public:
    UBreakerFirstPersonArms();
    bool Configure(UStaticMeshComponent* Gun, const FVector& FiringGrip, const FVector& SupportGrip);
    void DeactivateArms(USceneComponent* Rig);
    void UpdateWeaponPose(const UBreakerWeaponComponent* Weapon);
    bool IsConfigured() const { return bConfigured; }

private:
    UPROPERTY() TObjectPtr<UAnimSequence> RifleIdle;
    UPROPERTY() TObjectPtr<UAnimSequence> RifleReload;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> HeldGun;
    bool bConfigured = false;
    bool bShowingReload = false;
};
