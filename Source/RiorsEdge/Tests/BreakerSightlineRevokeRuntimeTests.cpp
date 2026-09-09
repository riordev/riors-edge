#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Abilities/BreakerAbilityComponent.h"
#include "Abilities/BreakerAbilityDefinition.h"
#include "Abilities/BreakerAbilityStateComponent.h"
#include "Abilities/BreakerAbility_Sightline.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Classes/BreakerMomentumComponent.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerEnemy.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Items/BreakerEquipmentComponent.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Progression/BreakerCoreWheelMath.h"
#include "Progression/BreakerExperience.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionLibrary.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerSightlineRevokeRuntimeTest,
    "RiorsEdge.Abilities.Swift.SightlineRevokeRuntime", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerSightlineRevokeRuntimeTest::RunTest(const FString&)
{
    UWorld::InitializationValues Init; Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    auto* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
    if(!World)return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);World->InitializeActorsForPlay(FURL());
    const uint64 Frame=GFrameCounter;
    ON_SCOPE_EXIT{World->DestroyWorld(false);GEngine->DestroyWorldContext(World);GFrameCounter=Frame;};
    auto* Player=World->SpawnActor<ABreakerCharacter>(FVector(0,0,100),FRotator::ZeroRotator);
    if(!Player)return false;
    Player->bRefuseSavesForPendingCharacter=true;Player->SetActorTickEnabled(false);
    Player->GetBreakerMovement()->SetComponentTickEnabled(false);
    auto* ASC=Player->GetAbilitySystemComponent();auto* Attr=Player->GetAttributes();
    ASC->InitAbilityActorInfo(Player,Player);ASC->AddAttributeSetSubobject(Attr);Player->GetCombat()->BindAttributes(Attr);
    auto* Progression=Player->GetProgression();Progression->BindAttributes(Attr);
    if(!TestTrue(TEXT("Choose actual Swift"),Progression->ChoosePermanentClassById(EBreakerClassId::Swift)))return false;
    Progression->AwardExperience(UBreakerExperienceLibrary::TotalXpToReachLevel(20,Progression->ExperienceCurve));
    auto* Abilities=Player->GetAbilities();FText Reason;
    if(!TestTrue(TEXT("Earned token unlocks Sightline"),Progression->SpendAbilityToken(TEXT("Swift.Sightline"),Reason)))return false;
    if(!TestTrue(TEXT("Equip actual purchased Sightline"),Abilities->TryEquipAbility(EBreakerAbilitySlot::ClassAbilityOne,TEXT("Swift.Sightline"),Reason)))return false;
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
    int32 Base = 0;
    auto Cast=[&]()
    {
        const float Price=Abilities->GetResourceCostForSlot(EBreakerAbilitySlot::ClassAbilityOne);
        auto* Move=Player->GetBreakerMovement();Move->SetMovementMode(MOVE_Walking);Move->Velocity=FVector(Move->WalkSpeed,0,0);
        for(int32 I=0;I<40;++I){Player->SetActorLocation(Player->GetActorLocation()+Move->Velocity);Momentum->AdvanceLoop(1);}
        Move->StopMovementImmediately();
        const float Before=Momentum->GetMomentum();
        if(!TestTrue(TEXT("Native traversal funds Sightline"),Before>=Price))return false;
        // Earned Momentum itself grants pierce. Measure after traversal, so
        // the test isolates Sightline without discarding the native class lane.
        Base = Player->GetWeapon()->GetShotChannels().PierceCount;
        if(!TestTrue(TEXT("Actual equipped Sightline activates"),Abilities->TryActivateSlot(EBreakerAbilitySlot::ClassAbilityOne)))return false;
        TestEqual(TEXT("Sightline pays its actual quote"),Before-Momentum->GetMomentum(),Price,.001f);
        TestTrue(TEXT("Actual Sightline window exists"),State->IsWindowActive(UBreakerAbility_Sightline::WindowKey()));
        return true;
    };
    if(!BuyAfterimage())return false;
    auto* Weapon=Player->GetWeapon();
    if(!Cast())return false;
    TestEqual(TEXT("Full paid contribution"),Weapon->GetShotChannels().PierceCount,Base+UBreakerAbility_Sightline::AllTargetsPierceCount);
    Clock(State->GetWindowRemaining(UBreakerAbility_Sightline::WindowKey())+.05f);
    TestFalse(TEXT("Ordinary permission has expired"),State->IsWindowActive(UBreakerAbility_Sightline::WindowKey()));
    TestEqual(TEXT("Natural tail halves discrete contribution"),Weapon->GetShotChannels().PierceCount,Base+UBreakerAbility_Sightline::AllTargetsPierceCount/2);
    auto* Spec=ASC->FindAbilitySpecFromClass(UBreakerAbility_Sightline::StaticClass());
    if(!TestNotNull(TEXT("Actual purchased grant survives normal expiry"),Spec))return false;
    TestFalse(TEXT("Instant activation is inactive during tail"),Spec->IsActive());
    ASC->ClearAbility(Spec->Handle);
    TestEqual(TEXT("Grant removal immediately revokes owned tail"),Weapon->GetShotChannels().PierceCount,Base);
    Clock(2.05f);
    TestEqual(TEXT("Later callbacks do not resurrect revoked contribution"),Weapon->GetShotChannels().PierceCount,Base);
    return true;
}
#endif
