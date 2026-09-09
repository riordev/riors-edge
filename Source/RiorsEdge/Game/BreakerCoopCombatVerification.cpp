#include "Game/BreakerCoopCombatVerification.h"
#include "Game/BreakerCoopCombatTest.h"
#include "Game/BreakerGameInstance.h"
#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerEnemy.h"
#include "Combat/BreakerCombatComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Weapons/BreakerWeaponComponent.h"
#include "AbilitySystemComponent.h"
#include "Abilities/BreakerAbilityComponent.h"
#include "Abilities/BreakerAbilityDefinition.h"
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
 if(IsValid(Guest) && Guest->GetCombat())
 {
  Guest->GetCombat()->OnDeath.RemoveDynamic(this,&ThisClass::ObserveGuestDeath);
  Guest->GetCombat()->OnVitalsRestored.RemoveDynamic(this,&ThisClass::ObserveGuestRestore);
 }
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
  bTargetDefeated=true;
 }
}
void ABreakerCoopCombatVerification::ObserveGuestDeath(){++GuestDeaths;}
void ABreakerCoopCombatVerification::ObserveGuestRestore(){++GuestRestores;}
void ABreakerCoopCombatVerification::ServerBeginGuestDeathCheck_Implementation()
{
 // This actor exists only under the explicit isolated verification flag. The
 // owner RPC follows the guest's actual target-death receipt, never a timer.
 if(!HasAuthority() || bExpired || !bTargetDefeated || bGuestDeathIssued || !IsValid(Guest)
  || GetOwner()!=Guest->GetController() || !Guest->GetCombat() || Guest->GetCombat()->IsDead())return;
 bGuestDeathIssued=true;
 GuestDeathPosition=Guest->GetActorLocation();
 FBreakerDamageRequest Hit;
 Hit.BaseDamage=1000000.0f; // O2 PLACEHOLDER verification-only environmental lethal hit.
 Hit.bCanCritical=false;Hit.bCanBeAvoided=false;Hit.bBypassShield=true;Hit.SetInstigator(this);
 Guest->GetCombat()->ReceiveDamage(Hit);
 UE_LOG(LogTemp,Display,TEXT("[CoopVerify] server guest-lethal profile=%s health=%.2f death-events=%d awaiting=%d"),
  *Guest->GetCoopCombatProfileId().ToString(),Guest->GetAttributes()->GetHealth(),GuestDeaths,Guest->IsAwaitingRespawn());
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
   GuestStart=Guest->GetActorLocation();SetOwner(Guest->GetController());ForceNetUpdate();
  }
  if(!bGuestObserversBound && Guest->GetCombat())
  {
   Guest->GetCombat()->OnDeath.AddDynamic(this,&ThisClass::ObserveGuestDeath);
   Guest->GetCombat()->OnVitalsRestored.AddDynamic(this,&ThisClass::ObserveGuestRestore);
   bGuestObserversBound=true;
  }
  if(bGuestDeathIssued)
  {
   if(GuestDeaths==1 && GuestRestores==1 && Guest->GetAttributes()->GetHealth()>0 && !Guest->IsAwaitingRespawn())
   {
    UE_LOG(LogTemp,Display,TEXT("[CoopVerify] server guest-respawn profile=%s health=%.2f death-events=%d restore-events=%d distance=%.2f"),
     *Guest->GetCoopCombatProfileId().ToString(),Guest->GetAttributes()->GetHealth(),GuestDeaths,GuestRestores,
     FVector::Dist(GuestDeathPosition,Guest->GetActorLocation()));
    bExpired=true;
   }
   return;
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
 if(!bGuestObserversBound && Guest->GetCombat())
 {
  Guest->GetCombat()->OnDeath.AddDynamic(this,&ThisClass::ObserveGuestDeath);
  Guest->GetCombat()->OnVitalsRestored.AddDynamic(this,&ThisClass::ObserveGuestRestore);
  bGuestObserversBound=true;
 }
 if(bGuestDeathRequested)
 {
  StopGuestFire();
  const float GuestHealth=Guest->GetAttributes()->GetHealth();
  if(!bGuestDeadSeen && GuestHealth<=0 && Guest->IsAwaitingRespawn() && !Guest->InputEnabled())
  {
   bGuestDeadSeen=true;
   UE_LOG(LogTemp,Display,TEXT("[CoopVerify] client guest-dead-presentation profile=%s health=%.2f awaiting=1 input=0 death-events=%d restore-events=%d"),
    *Guest->GetCoopCombatProfileId().ToString(),GuestHealth,GuestDeaths,GuestRestores);
  }
  if(bGuestDeadSeen && GuestHealth>0 && !Guest->IsAwaitingRespawn() && Guest->InputEnabled())
  {
   UE_LOG(LogTemp,Display,TEXT("[CoopVerify] client guest-revived-presentation profile=%s health=%.2f awaiting=0 input=1 death-events=%d restore-events=%d"),
    *Guest->GetCoopCombatProfileId().ToString(),GuestHealth,GuestDeaths,GuestRestores);
   bExpired=true;
  }
  return;
 }
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
 // Observe the owner-facing getters only after replicated metadata AND real
 // GAS specs agree. This gates ordinary smoke input, so missing metadata fails
 // by timeout instead of a reflection-only success or a manufactured grant.
 if(!bSlotMetadataLogged)
 {
  auto* Abilities=Guest->FindComponentByClass<UBreakerAbilityComponent>();
  if(!Abilities)return;
  const auto One=EBreakerAbilitySlot::ClassAbilityOne;
  const auto Two=EBreakerAbilitySlot::ClassAbilityTwo;
  const auto Ultimate=EBreakerAbilitySlot::Ultimate;
  const auto* First=Abilities->GetDefinitionForSlot(One);
  const auto* Ult=Abilities->GetDefinitionForSlot(Ultimate);
  if(Abilities->GetAbilityIdForSlot(One)!=FName(TEXT("Swift.Slipcut"))
   || Abilities->GetAbilityIdForSlot(Ultimate)!=FName(TEXT("Swift.Overdrive"))
   || !First || First->AbilityId!=Abilities->GetAbilityIdForSlot(One)
   || !Ult || Ult->AbilityId!=Abilities->GetAbilityIdForSlot(Ultimate)
   || !Abilities->IsSlotImplemented(One) || !Abilities->IsSlotImplemented(Ultimate)
   || !Abilities->IsSlotGranted(One) || !Abilities->IsSlotGranted(Ultimate)
   || !Abilities->GetAbilityIdForSlot(Two).IsNone() || Abilities->GetDefinitionForSlot(Two)
   || Abilities->IsSlotGranted(Two)
   || !Abilities->GetAbilityIdForSlot(static_cast<EBreakerAbilitySlot>(255)).IsNone())return;
  bSlotMetadataLogged=true;
  UE_LOG(LogTemp,Display,TEXT("[CoopVerify] client owner-slot-definitions profile=%s first=%s second=none ultimate=%s real-specs=matched"),
   *Guest->GetCoopCombatProfileId().ToString(),*First->AbilityId.ToString(),*Ult->AbilityId.ToString());
 }
 // Ordinary movement consumer; replicated movement is independently observed
 // by authority above. No teleport, movement multicast or synthetic result.
 if(PC->GetPawn()!=Guest || PC->GetViewTarget()!=Guest)return;
 if(!bMovementInputStarted)
 {
  bMovementInputStarted=true;
  Guest->AddMovementInput(FVector::ForwardVector,1);
  return; // The preceding frame elapsed before input began; do not charge it.
 }
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
  if(GetOwner()==PC)
  {
   bGuestDeathRequested=true;
   ServerBeginGuestDeathCheck();
  }
  return;
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
