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
#include "Abilities/BreakerAbility_Slipcut.h"
#include "Abilities/BreakerAbilityStateComponent.h"
#include "GameplayPrediction.h"
#include "GameplayEffect.h"
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
 DOREPLIFETIME(ABreakerCoopCombatVerification,bServerSlipcutVerified);
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
 if(IsValid(Guest)) if(auto* ASC=Guest->GetAbilitySystemComponent())
 { ASC->AbilityActivatedCallbacks.RemoveAll(this); ASC->AbilityEndedCallbacks.RemoveAll(this); }
 StopGuestFire(); Super::EndPlay(Reason);
}
void ABreakerCoopCombatVerification::BindAbilityObservers()
{
 if(bAbilityObserversBound || !IsValid(Guest))return;
 auto* ASC=Guest->GetAbilitySystemComponent(); if(!ASC)return;
 ASC->AbilityActivatedCallbacks.AddUObject(this,&ThisClass::ObserveAbilityStart);
 ASC->AbilityEndedCallbacks.AddUObject(this,&ThisClass::ObserveAbilityEnd);
 bAbilityObserversBound=true;
}
void ABreakerCoopCombatVerification::ObserveAbilityStart(UGameplayAbility* Ability)
{
 if(!IsValid(Guest) || !Ability || !Ability->IsA<UBreakerAbility_Slipcut>())return;
 ++SlipcutStarts;
 AbilityResourceBefore=Guest->GetAttributes()->GetClassResource();
 AbilityRateBefore=Guest->GetWeapon()->GetFireRateMultiplier();
 auto Key=Ability->GetCurrentActivationInfo().GetActivationPredictionKey();
 ObservedPredictionKey=Key.Current;
 if(!HasAuthority() && Guest->IsLocallyControlled() && Key.IsValidKey())
 {
  // Observe GAS's real key retirement, never manufacture an acknowledgement.
  Key.NewCaughtUpDelegate().BindUObject(this,&ThisClass::PredictionCaughtUp);
  Key.NewRejectedDelegate().BindUObject(this,&ThisClass::PredictionRejected);
 }
}
void ABreakerCoopCombatVerification::ObserveAbilityEnd(UGameplayAbility* Ability)
{
 auto* Slipcut=Cast<UBreakerAbility_Slipcut>(Ability);
 if(!IsValid(Guest) || !Slipcut)return;
 ++SlipcutEnds;
 if(!HasAuthority())return;
 auto* Abilities=Guest->GetAbilities();
 const auto Slot=EBreakerAbilitySlot::ClassAbilityOne;
 const float Quoted=Abilities->GetCost(Slot);
 const float Debit=AbilityResourceBefore-Guest->GetAttributes()->GetClassResource();
 const auto* State=Guest->FindComponentByClass<UBreakerAbilityStateComponent>();
 const float Rate=Guest->GetWeapon()->GetFireRateMultiplier();
 if(SlipcutStarts!=1 || SlipcutEnds!=1 || ObservedPredictionKey<=0
  || !FMath::IsNearlyEqual(Debit,Quoted,.01f) || !FMath::IsNearlyEqual(Slipcut->GetLastPaidResourceCost(),Quoted,.01f)
  || Abilities->GetCooldownRemaining(Slot)<=0 || !State || !State->IsWindowActive(UBreakerAbility_Slipcut::WindowKey())
  || Rate<=AbilityRateBefore)return;
 bServerSlipcutVerified=true;ForceNetUpdate();
 UE_LOG(LogTemp,Display,TEXT("[CoopVerify] server guest-slipcut profile=%s key=%d starts=%d ends=%d quoted=%.2f debit=%.2f rate-before=%.3f rate-after=%.3f cooldown=%.3f window=1"),
  *Guest->GetCoopCombatProfileId().ToString(),ObservedPredictionKey,SlipcutStarts,SlipcutEnds,Quoted,Debit,AbilityRateBefore,Rate,Abilities->GetCooldownRemaining(Slot));
}
void ABreakerCoopCombatVerification::PredictionCaughtUp()
{
 bPredictionCaughtUp=true;
 UE_LOG(LogTemp,Display,TEXT("[CoopVerify] client slipcut-key-caught-up profile=%s key=%d"),*Guest->GetCoopCombatProfileId().ToString(),ObservedPredictionKey);
}
void ABreakerCoopCombatVerification::PredictionRejected()
{
 bPredictionRejected=true;
 UE_LOG(LogTemp,Warning,TEXT("[CoopVerify] client slipcut-key-rejected profile=%s key=%d"),*Guest->GetCoopCombatProfileId().ToString(),ObservedPredictionKey);
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
 // Earlier ordinary combat can kill the guest while movement funds Slipcut.
 // Keep those observations; measure this explicitly requested lifecycle from
 // its own living start instead of requiring an immortal prelude.
 DeathsBeforeRequestedCheck=GuestDeaths;RestoresBeforeRequestedCheck=GuestRestores;
 UE_LOG(LogTemp,Display,TEXT("[CoopVerify] server guest-check-baseline deaths=%d restores=%d"),GuestDeaths,GuestRestores);
 bGuestDeathIssued=true;
 GuestDeathPosition=Guest->GetActorLocation();
 FBreakerDamageRequest Hit;
 Hit.BaseDamage=1000000.0f; // O2 PLACEHOLDER verification-only environmental lethal hit.
 Hit.bCanCritical=false;Hit.bCanBeAvoided=false;Hit.bBypassShield=true;Hit.SetInstigator(this);
 Guest->GetCombat()->ReceiveDamage(Hit);
 UE_LOG(LogTemp,Display,TEXT("[CoopVerify] server guest-lethal profile=%s health=%.2f death-events=%d awaiting=%d"),
  *Guest->GetCoopCombatProfileId().ToString(),Guest->GetAttributes()->GetHealth(),GuestDeaths-DeathsBeforeRequestedCheck,Guest->IsAwaitingRespawn());
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
  BindAbilityObservers();
  if(!bGuestObserversBound && Guest->GetCombat())
  {
   Guest->GetCombat()->OnDeath.AddDynamic(this,&ThisClass::ObserveGuestDeath);
   Guest->GetCombat()->OnVitalsRestored.AddDynamic(this,&ThisClass::ObserveGuestRestore);
   bGuestObserversBound=true;
  }
  if(bGuestDeathIssued)
  {
   if(GuestDeaths-DeathsBeforeRequestedCheck==1 && GuestRestores-RestoresBeforeRequestedCheck==1 && Guest->GetAttributes()->GetHealth()>0 && !Guest->IsAwaitingRespawn())
   {
    UE_LOG(LogTemp,Display,TEXT("[CoopVerify] server guest-respawn profile=%s health=%.2f death-events=%d restore-events=%d distance=%.2f"),
     *Guest->GetCoopCombatProfileId().ToString(),Guest->GetAttributes()->GetHealth(),GuestDeaths-DeathsBeforeRequestedCheck,GuestRestores-RestoresBeforeRequestedCheck,
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
  if(bMoved && bServerSlipcutVerified && !Target && InitialHealth==0)
  {
   if(!SpawnTarget()){bExpired=true;UE_LOG(LogTemp,Warning,TEXT("[CoopVerify] no legal target placement"));}
  }
  return;
 }
 if(!IsValid(Guest) || !Guest->IsLocallyControlled() || GetNetMode()!=NM_Client)return;
 auto* PC=Cast<APlayerController>(Guest->GetController());
 auto* Weapon=Guest->FindComponentByClass<UBreakerWeaponComponent>();
 if(!PC || !Weapon)return;
 BindAbilityObservers();
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
 // Fund the actual starter through ordinary movement and replicated Momentum.
 // O2 PLACEHOLDER smoke route only: reverse along a short line instead of
 // running off the yard. Collision/anti-farm rules stay enabled; no credit grant.
 if(!bAbilityReconciled)
 {
  auto* Abilities=Guest->GetAbilities();auto* ASC=Guest->GetAbilitySystemComponent();
  const auto Slot=EBreakerAbilitySlot::ClassAbilityOne;
  if(!Abilities || !ASC)return;
  if(!bAbilityRequested)
  {
   const float Quoted=Abilities->GetCost(Slot);
   if(AbilitySettleSeconds<=0 && Guest->GetAttributes()->GetClassResource()<Quoted+5.f)
   {
    AbilityMoveSeconds+=DeltaSeconds;
    const float Direction=(static_cast<int32>(AbilityMoveSeconds/1.5f)%2)==0?1.f:-1.f;
    Guest->AddMovementInput(FVector::ForwardVector,Direction);
    AbilitySettleSeconds=0;return;
   }
   AbilitySettleSeconds+=DeltaSeconds;
   if(AbilitySettleSeconds<.15f)return;
   bAbilityRequested=true;
   const float Before=Guest->GetAttributes()->GetClassResource();
   const float Rate=Weapon->GetFireRateMultiplier();
   const bool Activated=Abilities->TryActivateSlot(Slot);
   const float After=Guest->GetAttributes()->GetClassResource();
   const bool RefusedSecond=!Abilities->TryActivateSlot(Slot);
   if(!Activated || SlipcutStarts!=1 || SlipcutEnds!=1 || ObservedPredictionKey<=0
    || !FMath::IsNearlyEqual(Before-After,Quoted,.01f) || !RefusedSecond
    || !FMath::IsNearlyEqual(Guest->GetAttributes()->GetClassResource(),After,.01f)
    || !FMath::IsNearlyEqual(Weapon->GetFireRateMultiplier(),Rate,.001f)
    || Abilities->GetCooldownRemaining(Slot)<=0)
   {bExpired=true;UE_LOG(LogTemp,Warning,TEXT("[CoopVerify] client slipcut-local-failed activated=%d starts=%d ends=%d key=%d debit=%.3f quoted=%.3f second-refused=%d"),Activated,SlipcutStarts,SlipcutEnds,ObservedPredictionKey,Before-After,Quoted,RefusedSecond);return;}
   UE_LOG(LogTemp,Display,TEXT("[CoopVerify] client guest-slipcut-local profile=%s key=%d starts=1 ends=1 quoted=%.2f debit=%.2f second-refused=1 authority-cadence-unchanged=1"),
    *Guest->GetCoopCombatProfileId().ToString(),ObservedPredictionKey,Quoted,Before-After);
   return;
  }
  if(bPredictionRejected){bExpired=true;return;}
  if(!bPredictionCaughtUp || !bServerSlipcutVerified)return;
  // Prediction catch-up must retire the temporary predicted cost modifier.
  // Compare actual replicated attribute base/current, not a smoke RPC snapshot
  // that would race the still-running native Momentum decay loop.
  for(const auto Handle:ASC->GetActiveEffects(FGameplayEffectQuery()))
  {
   const auto* Effect=ASC->GetActiveGameplayEffect(Handle);
   if(Effect && Effect->Spec.Def && Effect->Spec.Def->IsA<UBreakerAbilityCostEffect>()
    && Effect->PredictionKey.Current==ObservedPredictionKey)return;
  }
  const float Current=ASC->GetNumericAttribute(UBreakerAttributeSet::GetClassResourceAttribute());
  const float Base=ASC->GetNumericAttributeBase(UBreakerAttributeSet::GetClassResourceAttribute());
  if(!FMath::IsNearlyEqual(Current,Base,.01f) || Abilities->GetCooldownRemaining(Slot)<=0)return;
  bAbilityReconciled=true;
  UE_LOG(LogTemp,Display,TEXT("[CoopVerify] client guest-slipcut-reconciled profile=%s key=%d starts=%d ends=%d resource=%.3f base=%.3f cooldown=%.3f pending-predicted-cost=0"),
   *Guest->GetCoopCombatProfileId().ToString(),ObservedPredictionKey,SlipcutStarts,SlipcutEnds,Current,Base,Abilities->GetCooldownRemaining(Slot));
 }
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
