#include "Game/BreakerCoopCombatVerification.h"
#include "Game/BreakerCoopCombatTest.h"
#include "Game/BreakerGameInstance.h"
#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerEnemy.h"
#include "Combat/BreakerCombatComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Weapons/BreakerWeaponComponent.h"
#include "AbilitySystemComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Net/UnrealNetwork.h"

ABreakerCoopCombatVerification::ABreakerCoopCombatVerification()
{
 bReplicates=true; bAlwaysRelevant=true; PrimaryActorTick.bCanEverTick=true;
}
void ABreakerCoopCombatVerification::StartForWorld(UWorld* World)
{
 if(!World || World->GetNetMode()!=NM_ListenServer || !BreakerCoopCombatTest::IsEnabled(World)
  || !UBreakerGameInstance::IsFernhallMap(World)
  || !FParse::Param(FCommandLine::Get(),TEXT("BreakerCoopCombatVerify")))return;
 for(TActorIterator<ABreakerCoopCombatVerification> It(World);It;++It)return;
 World->SpawnActor<ABreakerCoopCombatVerification>();
}
void ABreakerCoopCombatVerification::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
 Super::GetLifetimeReplicatedProps(OutLifetimeProps);
 DOREPLIFETIME(ABreakerCoopCombatVerification,Guest);
 DOREPLIFETIME(ABreakerCoopCombatVerification,Target);
 DOREPLIFETIME(ABreakerCoopCombatVerification,GuestStart);
 DOREPLIFETIME(ABreakerCoopCombatVerification,InitialHealth);
}
void ABreakerCoopCombatVerification::StopGuestFire()
{
 if(IsValid(Guest) && Guest->IsLocallyControlled())
  if(auto* Weapon=Guest->FindComponentByClass<UBreakerWeaponComponent>())Weapon->StopFire();
 bFiring=false;
}
void ABreakerCoopCombatVerification::EndPlay(const EEndPlayReason::Type Reason)
{
 StopGuestFire(); Super::EndPlay(Reason);
}
bool ABreakerCoopCombatVerification::SpawnTarget()
{
 // O2 PLACEHOLDER smoke placement only: choose a normal target on native
 // floor and with a clear shot. No special enemy stat or collider overrides.
 const auto* Body=GetDefault<ABreakerEnemy>()->FindComponentByClass<UCapsuleComponent>();
 if(!Body)return false;
 const float Half=Body->GetScaledCapsuleHalfHeight(),Radius=Body->GetScaledCapsuleRadius();
 FCollisionQueryParams Query(SCENE_QUERY_STAT(CoopVerifyPlacement),false,Guest);
 for(int32 Direction=0;Direction<8;++Direction)
 {
  const float Radians=Direction*PI*.25f;
  const FVector Around=Guest->GetActorLocation()+FVector(FMath::Cos(Radians),FMath::Sin(Radians),0)*750;
  FHitResult Floor;
  if(!GetWorld()->LineTraceSingleByObjectType(Floor,Around+FVector(0,0,300),Around-FVector(0,0,400),FCollisionObjectQueryParams(ECC_WorldStatic),Query)
   || Floor.ImpactNormal.Z<.7f)continue;
  const FVector At=Floor.ImpactPoint+FVector(0,0,Half+2);
  if(GetWorld()->OverlapBlockingTestByChannel(At,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(Radius,Half),Query))continue;
  FVector Eye;FRotator View;Guest->GetActorEyesViewPoint(Eye,View);
  if(GetWorld()->LineTraceTestByObjectType(Eye,At,FCollisionObjectQueryParams(ECC_WorldStatic),Query))continue;
  const FTransform Spawn((Guest->GetActorLocation()-At).Rotation(),At);
  auto* Enemy=GetWorld()->SpawnActorDeferred<ABreakerEnemy>(ABreakerEnemy::StaticClass(),Spawn,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
  if(!Enemy)return false;
  Enemy->ConfigureCrowdProbe(); Enemy->SetAreaLevel(1); Enemy->FinishSpawning(Spawn);
  auto* Combat=Enemy->FindComponentByClass<UBreakerCombatComponent>();
  const auto* Attributes=Enemy->GetAbilitySystemComponent()->GetSet<UBreakerAttributeSet>();
  if(!Combat || !Attributes){Enemy->Destroy();return false;}
  Target=Enemy;InitialHealth=Attributes->GetHealth();
  Combat->OnDamageTaken.AddDynamic(this,&ThisClass::ObserveDamage);
  Combat->OnDeath.AddDynamic(this,&ThisClass::ObserveDeath);
  ForceNetUpdate();
  UE_LOG(LogTemp,Display,TEXT("[CoopVerify] server target=%s health=%.2f normal-level=1"),*Enemy->GetName(),InitialHealth);
  return true;
 }
 return false;
}
void ABreakerCoopCombatVerification::ObserveDamage(const FBreakerHitContext& Hit)
{
 if(!HasAuthority() || Hit.Instigator.Get()!=Guest.Get() || Hit.Delivery!=EBreakerDamageDelivery::Weapon || Hit.Result.HealthDamage<=0)return;
 ++GuestWeaponHits;
 UE_LOG(LogTemp,Display,TEXT("[CoopVerify] server guest-weapon-hit profile=%s target=%s damage=%.2f hits=%d"),
  *Guest->GetCoopCombatProfileId().ToString(),*GetNameSafe(Target),Hit.Result.HealthDamage,GuestWeaponHits);
}
void ABreakerCoopCombatVerification::ObserveDeath()
{
 if(!HasAuthority() || !IsValid(Target) || !IsValid(Guest))return;
 auto* Combat=Target->FindComponentByClass<UBreakerCombatComponent>();
 if(Combat && Combat->IsDead() && Combat->GetLastDamageInstigator()==Guest.Get() && GuestWeaponHits>0)
 {
  UE_LOG(LogTemp,Display,TEXT("[CoopVerify] server target-death profile=%s target=%s guest-weapon-hits=%d"),
   *Guest->GetCoopCombatProfileId().ToString(),*Target->GetName(),GuestWeaponHits);
  bExpired=true;
 }
}
void ABreakerCoopCombatVerification::Tick(float DeltaSeconds)
{
 Super::Tick(DeltaSeconds);
 if(bExpired || !GetWorld() || !BreakerCoopCombatTest::IsEnabled(GetWorld()))return;
 Age+=DeltaSeconds;
 // O2 PLACEHOLDER smoke timeout: bounded failure rather than a passing flag.
 if(Age>90){StopGuestFire();bExpired=true;UE_LOG(LogTemp,Warning,TEXT("[CoopVerify] timeout"));return;}
 if(HasAuthority())
 {
  if(!IsValid(Guest))
  {
   TSet<FGuid> Profiles;
   for(TActorIterator<ABreakerCharacter> It(GetWorld());It;++It)
   {
    if(!It->GetCoopCombatProfileId().IsValid() || !It->GetController())continue;
    Profiles.Add(It->GetCoopCombatProfileId());
    if(!It->GetController()->IsLocalController())Guest=*It;
   }
   if(Profiles.Num()<2 || !Guest){Guest=nullptr;return;}
   GuestStart=Guest->GetActorLocation();ForceNetUpdate();
  }
  if(!bMoved && FVector::Dist2D(Guest->GetActorLocation(),GuestStart)>50)
  {
   bMoved=true;
   UE_LOG(LogTemp,Display,TEXT("[CoopVerify] server guest-movement profile=%s distance=%.2f"),*Guest->GetCoopCombatProfileId().ToString(),FVector::Dist2D(Guest->GetActorLocation(),GuestStart));
  }
  if(bMoved && !Target && InitialHealth==0)
  {
   if(!SpawnTarget()){bExpired=true;UE_LOG(LogTemp,Warning,TEXT("[CoopVerify] no legal target placement"));}
  }
  return;
 }
 if(!IsValid(Guest) || !Guest->IsLocallyControlled() || GetNetMode()!=NM_Client)return;
 auto* PC=Cast<APlayerController>(Guest->GetController());
 auto* Weapon=Guest->FindComponentByClass<UBreakerWeaponComponent>();
 if(!PC || !Weapon)return;
 if(Guest->IsWeaponsHolstered() || Guest->GetCombat()->IsDead()){StopGuestFire();return;}
 if(!bPresenceLogged)
 {
  TSet<FGuid> Profiles;
  for(TActorIterator<ABreakerCharacter> It(GetWorld());It;++It)
   if(It->GetCoopCombatProfileId().IsValid())Profiles.Add(It->GetCoopCombatProfileId());
  if(Profiles.Num()<2)return;
  bPresenceLogged=true;
  UE_LOG(LogTemp,Display,TEXT("[CoopVerify] client distinct-player-presence count=%d profile=%s"),Profiles.Num(),*Guest->GetCoopCombatProfileId().ToString());
 }
 // Ordinary movement consumer; replicated movement is independently observed
 // by authority above. No teleport, movement multicast or synthetic result.
 if(MoveAge<.5f){Guest->AddMovementInput(FVector::ForwardVector,1);MoveAge+=DeltaSeconds;return;}
 if(!IsValid(Target)){StopGuestFire();return;}
 const auto* ASC=Target->GetAbilitySystemComponent();
 const auto* Attr=ASC?ASC->GetSet<UBreakerAttributeSet>():nullptr;
 if(!Attr || InitialHealth<=0)return;
 const float Health=Attr->GetHealth();
 if(!bInitialHealthSeen)
 {
  if(!FMath::IsNearlyEqual(Health,InitialHealth,.01f))return;
  bInitialHealthSeen=true;
  UE_LOG(LogTemp,Display,TEXT("[CoopVerify] client initial-health target=%s health=%.2f"),*Target->GetName(),Health);
 }
 if(Health<InitialHealth && !bHealthReceiptLogged)
 {
  bHealthReceiptLogged=true;
  UE_LOG(LogTemp,Display,TEXT("[CoopVerify] client replicated-health target=%s health=%.2f initial=%.2f"),*Target->GetName(),Health,InitialHealth);
 }
 if(Health<=0)
 {
  StopGuestFire();
  if(!bDeathReceiptLogged){bDeathReceiptLogged=true;UE_LOG(LogTemp,Display,TEXT("[CoopVerify] client replicated-death target=%s health=%.2f"),*Target->GetName(),Health);}
  bExpired=true;return;
 }
 FVector Eye;FRotator View;PC->GetPlayerViewPoint(Eye,View);
 PC->SetControlRotation((Target->GetActorLocation()-Eye).Rotation());
 AimAge+=DeltaSeconds;
 if(!bFiring && AimAge>.35f)
 {
  // Same public input consumer used by Character::StartFire; on this
  // autonomous client it calls the existing reliable ServerStartFire RPC.
  Weapon->StartFire();bFiring=true;
  UE_LOG(LogTemp,Display,TEXT("[CoopVerify] client fire-request profile=%s magazine=%d"),*Guest->GetCoopCombatProfileId().ToString(),Weapon->GetMagazineAmmo());
 }
 if(bFiring && Weapon->GetMagazineAmmo()==0 && !Weapon->IsReloading())
 { Weapon->StopFire();bFiring=false;AimAge=0;Weapon->StartReload(); }
}
