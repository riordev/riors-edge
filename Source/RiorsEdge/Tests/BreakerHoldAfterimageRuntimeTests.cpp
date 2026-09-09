#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Abilities/BreakerAbilityComponent.h"
#include "Abilities/BreakerAbilityDefinition.h"
#include "Abilities/BreakerAbilityStateComponent.h"
#include "Abilities/BreakerTankAbilities.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Classes/BreakerGritComponent.h"
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
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerHoldAfterimageRuntimeTest,
    "RiorsEdge.Abilities.Tank.HoldAfterimageRuntime", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerHoldAfterimageRuntimeTest::RunTest(const FString&)
{
    UWorld::InitializationValues Init; Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    auto* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
    if(!World)return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);World->InitializeActorsForPlay(FURL());
    const uint64 Frame=GFrameCounter;
    ON_SCOPE_EXIT{World->DestroyWorld(false);GEngine->DestroyWorldContext(World);GFrameCounter=Frame;};
    auto* Player=World->SpawnActor<ABreakerCharacter>(FVector(0,0,100),FRotator::ZeroRotator);
    auto* Enemy=World->SpawnActor<ABreakerEnemy>(FVector(0,300,100),FRotator::ZeroRotator);
    if(!Player||!Enemy)return false;
    Player->bRefuseSavesForPendingCharacter=true;Player->SetActorTickEnabled(false);
    Player->GetBreakerMovement()->SetComponentTickEnabled(false);
    Enemy->ConfigureCrowdProbe();Enemy->DispatchBeginPlay();Enemy->SetActorTickEnabled(false);
    if(auto* Move=Enemy->FindComponentByClass<UPawnMovementComponent>()){Move->StopMovementImmediately();Move->SetComponentTickEnabled(false);}
    auto* ASC=Player->GetAbilitySystemComponent();auto* Attr=Player->GetAttributes();
    ASC->InitAbilityActorInfo(Player,Player);ASC->AddAttributeSetSubobject(Attr);Player->GetCombat()->BindAttributes(Attr);
    auto* Progression=Player->GetProgression();Progression->BindAttributes(Attr);
    if(!TestTrue(TEXT("Choose actual Tank"),Progression->ChoosePermanentClassById(EBreakerClassId::Tank)))return false;
    Progression->AwardExperience(UBreakerExperienceLibrary::TotalXpToReachLevel(20,Progression->ExperienceCurve));
    auto* Abilities=Player->GetAbilities();FText Reason;
    if(!TestTrue(TEXT("Equip actual free ultimate"),Abilities->TryEquipAbility(EBreakerAbilitySlot::Ultimate,TEXT("Tank.Hold"),Reason)))return false;
    Abilities->RefreshGrants();
    auto* Grit=Player->GetGrit();Grit->BindAttributes(Attr);Grit->SetComponentTickEnabled(false);
    auto* State=UBreakerAbilityStateComponent::FindOrAdd(Player);
    auto Clock=[&](float Seconds){for(float T=0;T<Seconds;){const float Step=FMath::Min(.01f,Seconds-T);++GFrameCounter;World->Tick(LEVELTICK_All,Step);T+=Step;}};
    auto BuyAfterimage=[&]()
    {
        auto* Tree=UBreakerProgressionLibrary::GetCoreSliceTree();
        for(const TCHAR* Id:{TEXT("Core.Duration.Hold"),TEXT("Core.Duration.Extend"),TEXT("Core.Duration.Uptime"),
            TEXT("Core.Duration.Settle"),TEXT("Core.Duration.Standing"),TEXT("Core.Duration.Afterimage")})
            if(!TestTrue(FString::Printf(TEXT("Pay actual Core route %s"),Id),Progression->PurchaseNode(Tree,Id,Reason)))return false;
        return true;
    };
    auto Cast=[&]()
    {
        const float Price=Abilities->GetResourceCostForSlot(EBreakerAbilitySlot::Ultimate);
        // Source contact and character proximity scan earn resource; no wallet edit.
        for(int32 I=0;I<100&&Grit->GetGrit()<Price;++I)
        {
            FBreakerDamageRequest Contact;Contact.BaseDamage=.01f;Contact.bCanCritical=false;Contact.bCanBeAvoided=false;Contact.SetInstigator(Enemy);
            Player->GetCombat()->ReceiveDamage(Contact);Player->Tick(.3f);Grit->AdvanceLoop(1.f);
        }
        const float Before=Grit->GetGrit();
        if(!TestTrue(TEXT("Native proximity/contact funds Hold"),Before>=Price))return false;
        if(!TestTrue(TEXT("Actual equipped Hold activates"),Abilities->TryActivateSlot(EBreakerAbilitySlot::Ultimate)))return false;
        TestEqual(TEXT("Hold pays its actual quote"),Before-Grit->GetGrit(),Price,.001f);
        TestTrue(TEXT("Actual Hold window exists"),State->IsWindowActive(UBreakerAbility_Hold::WindowKey()));
        return true;
    };
    const float Authored=GetDefault<UBreakerAbility_Hold>()->GenerationMultiplier;
    if(!Cast())return false;
    TestEqual(TEXT("Ordinary window uses authored generation"),Grit->GetGenerationMultiplier(),Authored,.001f);
    Clock(State->GetWindowRemaining(UBreakerAbility_Hold::WindowKey())+.05f);
    TestEqual(TEXT("No node means no tail"),Grit->GetGenerationMultiplier(),1.f,.001f);
    if(!BuyAfterimage()||!Cast())return false;
    Clock(State->GetWindowRemaining(UBreakerAbility_Hold::WindowKey())+.05f);
    TestEqual(TEXT("Tail halves the contribution above neutral"),Grit->GetGenerationMultiplier(),1.f+(Authored-1.f)*.5f,.001f);
    TestEqual(TEXT("Binary hit cap ends with the ordinary window"),Player->GetCombat()->GetIncomingHitCap(),0.f);
    Clock(2.05f);
    TestEqual(TEXT("Two-second generation tail expires"),Grit->GetGenerationMultiplier(),1.f,.001f);
    if(!Cast())return false;
    Clock(State->GetWindowRemaining(UBreakerAbility_Hold::WindowKey())+.05f);
    const auto Price=BreakerCoreRespecCost(Progression->GetCharacterLevel());
    auto* Equipment=Player->GetEquipment();const int32 BeforeWallet=Equipment->GetForgeWallet().Get();
    Equipment->GrantForgeCurrency(Price.Amount+1); // The wallet debit itself is the subject.
    if(!TestTrue(TEXT("Pay actual Core respec during tail"),Progression->RespecCore(Reason)))return false;
    TestEqual(TEXT("Actual respec price debited"),Equipment->GetForgeWallet().Get(),BeforeWallet+1);
    TestEqual(TEXT("Respec removes existing generation tail"),Grit->GetGenerationMultiplier(),1.f,.001f);
    if(!BuyAfterimage()||!Cast())return false;
    auto* HoldSpec=ASC->FindAbilitySpecFromClass(UBreakerAbility_Hold::StaticClass());
    if(!TestNotNull(TEXT("Equipped grant has an actual native spec"),HoldSpec))return false;
    const FGameplayAbilitySpecHandle HoldHandle=HoldSpec->Handle;
    ASC->CancelAbilityHandle(HoldHandle);
    TestEqual(TEXT("Explicit cancellation removes active generation without tail"),Grit->GetGenerationMultiplier(),1.f,.001f);
    TestEqual(TEXT("Cancellation removes ordinary hit cap"),Player->GetCombat()->GetIncomingHitCap(),0.f);
    if(!Cast())return false;
    Clock(State->GetWindowRemaining(UBreakerAbility_Hold::WindowKey())+.05f);
    FBreakerDamageRequest Lethal;Lethal.BaseDamage=1.e9f;Lethal.DamageFamily=EBreakerDamageFamily::TrueDamage;
    Lethal.bCanCritical=false;Lethal.bCanBeAvoided=false;Lethal.SetInstigator(Enemy);
    Player->GetCombat()->ReceiveDamage(Lethal);
    TestTrue(TEXT("Real lethal hit kills owner during tail"),Player->GetCombat()->IsDead());
    TestEqual(TEXT("Death removes generation contribution immediately"),Grit->GetGenerationMultiplier(),1.f,.001f);
    // Native revive resets vitals, not the ability/resource purchase history.
    Player->GetCombat()->RestoreVitals();
    if(!TestFalse(TEXT("Native vitals restore revives owner"),Player->GetCombat()->IsDead()))return false;
    if(!Cast())return false;
    Clock(State->GetWindowRemaining(UBreakerAbility_Hold::WindowKey())+.05f);
    TestEqual(TEXT("A new paid cast has a fresh natural tail"),Grit->GetGenerationMultiplier(),1.f+(Authored-1.f)*.5f,.001f);
    HoldSpec=ASC->FindAbilitySpecFromClass(UBreakerAbility_Hold::StaticClass());
    if(!TestNotNull(TEXT("Actual grant remains after natural expiry"),HoldSpec))return false;
    TestFalse(TEXT("Hold is inactive while its lane tail survives"),HoldSpec->IsActive());
    ASC->ClearAbility(HoldSpec->Handle);
    TestEqual(TEXT("Removing the inactive granted ability revokes its tail"),Grit->GetGenerationMultiplier(),1.f,.001f);
    return true;
}
#endif
