#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Combat/BreakerCombatTypes.h"
#include "BreakerCoopCombatVerification.generated.h"
class ABreakerCharacter;
class ABreakerEnemy;
class UGameplayAbility;
// Explicit nonsaving, two-process smoke driver. No ammo/resource/progression
// grants. After the real weapon kill, a verification-only environmental damage
// request exercises guest death and the existing authority respawn timer.
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
 UFUNCTION(Server, Reliable) void ServerBeginGuestDeathCheck();
 UFUNCTION() void ObserveGuestDeath();
 UFUNCTION() void ObserveGuestRestore();
 void ObserveAbilityStart(UGameplayAbility* Ability);
 void ObserveAbilityEnd(UGameplayAbility* Ability);
 void BindAbilityObservers();
 void PredictionCaughtUp();
 void PredictionRejected();
 UPROPERTY(Replicated) bool bServerSlipcutVerified=false;
 bool bAbilityObserversBound=false,bAbilityRequested=false,bPredictionCaughtUp=false,bPredictionRejected=false,bAbilityReconciled=false;
 int32 SlipcutStarts=0,SlipcutEnds=0;
 int32 ObservedPredictionKey=0;
 float AbilityResourceBefore=0,AbilityRateBefore=1,AbilitySettleSeconds=0,AbilityMoveSeconds=0;
 bool SpawnTarget();
 void StopGuestFire();
 float Age=0,MoveAge=0,AimAge=0;
 bool bSlotMetadataLogged=false,bMovementInputStarted=false;
 bool bMoved=false,bPresenceLogged=false,bInitialHealthSeen=false,bFiring=false;
 bool bHealthReceiptLogged=false,bDeathReceiptLogged=false,bExpired=false;
 int32 GuestWeaponHits=0;
 int32 GuestDeaths=0,GuestRestores=0;
 int32 DeathsBeforeRequestedCheck=0,RestoresBeforeRequestedCheck=0;
 bool bTargetDefeated=false,bGuestDeathIssued=false,bGuestDeathRequested=false;
 bool bGuestObserversBound=false,bGuestDeadSeen=false;
 FVector GuestDeathPosition=FVector::ZeroVector;
};
