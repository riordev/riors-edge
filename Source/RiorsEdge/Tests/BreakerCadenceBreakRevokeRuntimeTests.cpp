#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Abilities/BreakerAbilityComponent.h"
#include "Abilities/BreakerAbilityDefinition.h"
#include "Abilities/BreakerAbilityStateComponent.h"
#include "Abilities/BreakerAbility_CadenceBreak.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Classes/BreakerMomentumComponent.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerEnemy.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PawnMovementComponent.h"
#include "Items/BreakerEquipmentComponent.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Progression/BreakerCoreWheelMath.h"
#include "Progression/BreakerExperience.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionLibrary.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerCadenceBreakRevokeRuntimeTest,
    "RiorsEdge.Abilities.Swift.CadenceBreakRevokeRuntime", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerCadenceBreakRevokeRuntimeTest::RunTest(const FString&)
{
    for (const bool bDuringTail : {false,true})
    {
    UWorld::InitializationValues Init; Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    auto* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
    if(!World)return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);World->InitializeActorsForPlay(FURL());
    const uint64 Frame=GFrameCounter;
    ON_SCOPE_EXIT{World->DestroyWorld(false);GEngine->DestroyWorldContext(World);GFrameCounter=Frame;};
    auto* Player=World->SpawnActor<ABreakerCharacter>(FVector(0,0,100),FRotator::ZeroRotator);
    if(!Player)return false;
    Player->bRefuseSavesForPendingCharacter=true;
    Player->SetActorTickEnabled(false);
    Player->GetBreakerMovement()->SetComponentTickEnabled(false);
    auto* ASC=Player->GetAbilitySystemComponent();auto* Attr=Player->GetAttributes();
    ASC->InitAbilityActorInfo(Player,Player);ASC->AddAttributeSetSubobject(Attr);Player->GetCombat()->BindAttributes(Attr);
    auto* Progression=Player->GetProgression();Progression->BindAttributes(Attr);
    if(!TestTrue(TEXT("Choose actual Swift"),Progression->ChoosePermanentClassById(EBreakerClassId::Swift)))return false;
    Progression->AwardExperience(UBreakerExperienceLibrary::TotalXpToReachLevel(20,Progression->ExperienceCurve));
    auto* Abilities=Player->GetAbilities();FText Reason;
    if(!TestTrue(TEXT("Earned token unlocks CadenceBreak"),Progression->SpendAbilityToken(TEXT("Swift.CadenceBreak"),Reason)))return false;
    if(!TestTrue(TEXT("Equip actual purchased CadenceBreak"),Abilities->TryEquipAbility(EBreakerAbilitySlot::ClassAbilityOne,TEXT("Swift.CadenceBreak"),Reason)))return false;
    Abilities->RefreshGrants();
    auto* Momentum=Player->GetMomentum();Momentum->BindAttributes(Attr);Momentum->SetComponentTickEnabled(false);
    auto* State=UBreakerAbilityStateComponent::FindOrAdd(Player);
    State->SetComponentTickEnabled(false);
    auto Clock=[&](float Seconds){for(float T=0;T<Seconds;){const float Step=FMath::Min(.01f,Seconds-T);++GFrameCounter;World->Tick(LEVELTICK_All,Step);State->TickComponent(Step,LEVELTICK_All,nullptr);T+=Step;}};
    auto BuyAfterimage=[&]()
    {
        auto* Tree=UBreakerProgressionLibrary::GetCoreSliceTree();
        for(const TCHAR* Id:{TEXT("Core.Duration.Hold"),TEXT("Core.Duration.Extend"),TEXT("Core.Duration.Uptime"),
            TEXT("Core.Duration.Settle"),TEXT("Core.Duration.Standing"),TEXT("Core.Duration.Afterimage")})
            if(!TestTrue(FString::Printf(TEXT("Pay actual Core route %s"),Id),Progression->PurchaseNode(Tree,Id,Reason)))return false;
        return true;
    };
    auto FlatContribution=[&]()
    {
        FBreakerDamageRequest Request; Request.BaseDamage=0; Request.Delivery=EBreakerDamageDelivery::Weapon;
        Player->GetCombat()->ApplyOutgoingModifiers(Request);
        return Request.BaseDamage;
    };
    float Base = 0;
    auto ActivateCadence=[&]()
    {
        const float Price=Abilities->GetResourceCostForSlot(EBreakerAbilitySlot::ClassAbilityOne);
        auto* Move=Player->GetBreakerMovement();Move->SetMovementMode(MOVE_Walking);Move->Velocity=FVector(Move->WalkSpeed,0,0);
        for(int32 I=0;I<40;++I){Player->SetActorLocation(Player->GetActorLocation()+Move->Velocity);Momentum->AdvanceLoop(1);}
        Move->StopMovementImmediately();
        const float Before=Momentum->GetMomentum();
        if(!TestTrue(TEXT("Native traversal funds CadenceBreak"),Before>=Price))return false;
        Base = FlatContribution();
        if(!TestTrue(TEXT("Actual equipped CadenceBreak activates"),Abilities->TryActivateSlot(EBreakerAbilitySlot::ClassAbilityOne)))return false;
        TestEqual(TEXT("CadenceBreak pays its actual quote"),Before-Momentum->GetMomentum(),Price,.001f);
        TestTrue(TEXT("Actual CadenceBreak window exists"),State->IsWindowActive(UBreakerAbility_CadenceBreak::WindowKey()));
        return true;
    };
    if(!BuyAfterimage())return false;
    auto* Weapon=Player->GetWeapon();
    Weapon->BeginPlay(); // Run ordinary magazine initialization without character save loading.
    if(!ActivateCadence())return false;
    FVector Eye; FRotator Aim; Player->GetActorEyesViewPoint(Eye,Aim);
    // Real enemy and unmodified weapon: no synthetic OnShot, damage, crit,
    // ammunition or streak grant. Level40 provides an ordinary durable target.
    auto* Enemy=World->SpawnActor<ABreakerEnemy>(Eye+Aim.Vector()*300.f,FRotator::ZeroRotator);
    if(!TestNotNull(TEXT("Native firing target"),Enemy))return false;
    Enemy->ConfigureCrowdProbe(); Enemy->SetAreaLevel(40); Enemy->DispatchBeginPlay();
    Enemy->SetActorTickEnabled(false);
    if(auto* Move=Enemy->GetMovementComponent())Move->SetComponentTickEnabled(false);
    auto* Spec=ASC->FindAbilitySpecFromClass(UBreakerAbility_CadenceBreak::StaticClass());
    auto* Instance=Spec?Cast<UBreakerAbility_CadenceBreak>(Spec->GetPrimaryInstance()):nullptr;
    if(!TestNotNull(TEXT("Actual purchased ability instance"),Instance))return false;
    const auto* EnemyAttributes=Enemy->GetAbilitySystemComponent()->GetSet<UBreakerAttributeSet>();
    if(!TestNotNull(TEXT("Native target attributes"),EnemyAttributes))return false;
    const float HealthBefore=EnemyAttributes->GetHealth();
    Weapon->StartFire(); Weapon->StopFire();
    if(!TestTrue(TEXT("Real shot deals accepted enemy health damage"),EnemyAttributes->GetHealth()<HealthBefore))return false;
    if(!TestTrue(TEXT("Native traced shot earns a stack"),Weapon->GetLastShot().bHit && Weapon->GetLastShot().HitActor==Enemy && Instance->GetStacks()>0))return false;
    const float Earned=Instance->GetStacks()*Instance->FlatDamagePerStack;
    if(!TestTrue(TEXT("Authored earned flat lane is nonzero"),Earned>0))return false;
    TestEqual(TEXT("Full OnShot-earned contribution reaches outgoing reader"),FlatContribution(),Base+Earned,.001f);
    if(bDuringTail)
    {
        Clock(State->GetWindowRemaining(UBreakerAbility_CadenceBreak::WindowKey())+.05f);
        TestFalse(TEXT("Ordinary stacking permission has expired"),State->IsWindowActive(UBreakerAbility_CadenceBreak::WindowKey()));
        TestEqual(TEXT("Natural tail halves only the earned flat contribution"),FlatContribution(),Base+Earned*.5f,.001f);
    }
    if(!TestNotNull(TEXT("Actual purchased grant survives normal expiry"),Spec))return false;
    TestFalse(TEXT("Instant activation is inactive during tail"),Spec->IsActive());
    ASC->ClearAbility(Spec->Handle);
    TestEqual(TEXT("Grant removal immediately revokes owned tail"),FlatContribution(),Base,.001f);
    TestFalse(TEXT("Revoke closes stacking permission"),State->IsWindowActive(UBreakerAbility_CadenceBreak::WindowKey()));
    Clock(.25f);
    Weapon->StartFire(); Weapon->StopFire();
    TestEqual(TEXT("Subsequent actual shot cannot restore revoked stacks"),FlatContribution(),Base,.001f);
    Clock(4.05f);
    TestEqual(TEXT("Later callbacks do not resurrect revoked contribution"),FlatContribution(),Base,.001f);
    }
    return true;
}
#endif
