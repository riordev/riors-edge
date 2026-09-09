#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Combat/BreakerCombatTypes.h"
#include "BreakerCoopCombatVerification.generated.h"
class ABreakerCharacter;
class ABreakerEnemy;
// Explicit nonsaving, two-process smoke driver. Never grants damage, ammo,
// resources or progression. Completion is observed from real combat/replication.
UCLASS()
class RIORSEDGE_API ABreakerCoopCombatVerification : public AActor
{
 GENERATED_BODY()
public:
 ABreakerCoopCombatVerification();
 static void StartForWorld(UWorld* World);
 virtual void Tick(float DeltaSeconds) override;
 virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
 virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
 UPROPERTY(Replicated) TObjectPtr<ABreakerCharacter> Guest;
 UPROPERTY(Replicated) TObjectPtr<ABreakerEnemy> Target;
 UPROPERTY(Replicated) FVector GuestStart=FVector::ZeroVector;
 UPROPERTY(Replicated) float InitialHealth=0;
 UFUNCTION() void ObserveDamage(const FBreakerHitContext& Hit);
 UFUNCTION() void ObserveDeath();
 bool SpawnTarget();
 void StopGuestFire();
 float Age=0,MoveAge=0,AimAge=0;
 bool bMoved=false,bPresenceLogged=false,bInitialHealthSeen=false,bFiring=false;
 bool bHealthReceiptLogged=false,bDeathReceiptLogged=false,bExpired=false;
 int32 GuestWeaponHits=0;
};
