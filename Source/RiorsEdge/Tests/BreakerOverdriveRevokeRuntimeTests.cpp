#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Abilities/BreakerAbilityComponent.h"
#include "Abilities/BreakerAbilityDefinition.h"
#include "Abilities/BreakerAbilityStateComponent.h"
#include "Abilities/BreakerAbility_Overdrive.h"
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
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerOverdriveRevokeRuntimeTest,
    "RiorsEdge.Abilities.Swift.OverdriveRevokeRuntime", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerOverdriveRevokeRuntimeTest::RunTest(const FString&)
{
    for (bool bRemoveDuringTail : {false, true})
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
    if(!TestTrue(TEXT("Actual class ultimate is unlocked without a token purchase"),Progression->IsAbilityUnlocked(TEXT("Swift.Overdrive"))))return false;
    if(!TestTrue(TEXT("Equip actual class ultimate Overdrive"),Abilities->TryEquipAbility(EBreakerAbilitySlot::Ultimate,TEXT("Swift.Overdrive"),Reason)))return false;
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
    float Base = 1.f;
    auto Cast=[&]()
    {
        const float Price=Abilities->GetResourceCostForSlot(EBreakerAbilitySlot::Ultimate);
        auto* Move=Player->GetBreakerMovement();Move->SetMovementMode(MOVE_Walking);Move->Velocity=FVector(Move->WalkSpeed,0,0);
        for(int32 I=0;I<40;++I){Player->SetActorLocation(Player->GetActorLocation()+Move->Velocity);Momentum->AdvanceLoop(1);}
        Move->StopMovementImmediately();
        const float Before=Momentum->GetMomentum();
        if(!TestTrue(TEXT("Native traversal funds Overdrive"),Before>=Price))return false;
        // Preserve native class/gear outgoing More. Only the owned Overdrive factor
        // changes across the window and its half-strength contribution tail.
        Base = Player->GetCombat()->GetComposedMoreMultiplier();
        if(!TestTrue(TEXT("Actual equipped Overdrive activates"),Abilities->TryActivateSlot(EBreakerAbilitySlot::Ultimate)))return false;
        TestEqual(TEXT("Overdrive pays its actual quote"),Before-Momentum->GetMomentum(),Price,.001f);
        TestTrue(TEXT("Actual Overdrive window exists"),State->IsWindowActive(UBreakerAbility_Overdrive::WindowKey()));
        return true;
    };
    if(!BuyAfterimage())return false;
    auto* Combat=Player->GetCombat();
    if(!Cast())return false;
    TestEqual(TEXT("Full paid contribution"),Combat->GetComposedMoreMultiplier(),Base*UBreakerAbility_Overdrive::OutgoingMoreMultiplier,.001f);
    if (bRemoveDuringTail)
    {
    Clock(State->GetWindowRemaining(UBreakerAbility_Overdrive::WindowKey())+.05f);
    TestFalse(TEXT("Ordinary permission has expired"),State->IsWindowActive(UBreakerAbility_Overdrive::WindowKey()));
    TestEqual(TEXT("Natural tail halves only outgoing More contribution"),Combat->GetComposedMoreMultiplier(),Base*(1.f+(UBreakerAbility_Overdrive::OutgoingMoreMultiplier-1.f)*.5f),.001f);
    }
    auto* Spec=ASC->FindAbilitySpecFromClass(UBreakerAbility_Overdrive::StaticClass());
    if(!TestNotNull(TEXT("Actual class ultimate grant survives normal expiry"),Spec))return false;
    TestFalse(TEXT("Ordinary activation is inactive while its lease remains"),Spec->IsActive());
    ASC->ClearAbility(Spec->Handle);
    TestEqual(TEXT("Grant removal immediately revokes owned tail"),Combat->GetComposedMoreMultiplier(),Base,.001f);
    TestFalse(TEXT("Grant removal closes ordinary permission"),State->IsWindowActive(UBreakerAbility_Overdrive::WindowKey()));
    TestEqual(TEXT("Revocation clears effective floor without refilling raw resource"),Momentum->GetMomentumState(),EBreakerMomentumState::Settled);
    TestEqual(TEXT("Revocation never refunds spent Momentum"),Momentum->GetMomentum(),0.f,.001f);
    TestEqual(TEXT("Revocation ends generation override"),Momentum->GetGenerationMultiplier(),1.f,.001f);
    Clock(12.f);
    TestEqual(TEXT("Later callbacks do not resurrect revoked contribution"),Combat->GetComposedMoreMultiplier(),Base,.001f);
    }
    return true;
}
#endif
