#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Abilities/BreakerSupportAbilities.h"
#include "Classes/BreakerChargeComponent.h"
#include "Combat/BreakerStatusComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerCombatComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Progression/BreakerExperience.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionLibrary.h"
#include "Progression/BreakerProgressionTree.h"
#include "Save/BreakerMissionContent.h"

#include "Combat/BreakerRangedEnemy.h"
#include "Combat/BreakerZoneActor.h"
#include "EngineUtils.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "Abilities/BreakerAbilityStateComponent.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerPressureLifecycleTest, "RiorsEdge.Abilities.PressureLifecycle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerPressureLifecycleTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Init; Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    auto* World = UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
    if (!World) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); World->InitializeActorsForPlay(FURL());
    const uint64 Frame = GFrameCounter;
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); GFrameCounter = Frame; };
    auto* Floor = World->SpawnActor<AActor>(); auto* Box = NewObject<UBoxComponent>(Floor);
    Floor->AddInstanceComponent(Box); Floor->SetRootComponent(Box); Box->SetBoxExtent(FVector(4000,4000,20));
    Box->SetCollisionResponseToAllChannels(ECR_Block); Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    Box->RegisterComponent(); Floor->SetActorLocation(FVector(0,0,-20));
    auto* Player = World->SpawnActor<ABreakerCharacter>(FVector(0,0,100),FRotator::ZeroRotator);
    auto* Controller = World->SpawnActor<APlayerController>();
    if (!Player || !Controller) return false;
    Controller->Player = NewObject<ULocalPlayer>(GEngine); Controller->SetAsLocalPlayerController();
    Controller->Possess(Player); Controller->SetViewTarget(Player);
    Player->SetActorTickEnabled(false); Player->GetBreakerMovement()->SetComponentTickEnabled(false);
    auto* ASC = Player->GetAbilitySystemComponent(); auto* Attr = Player->GetAttributes();
    ASC->InitAbilityActorInfo(Player,Player); ASC->AddAttributeSetSubobject(Attr);
    auto* Combat = Player->GetCombat(); Combat->BindAttributes(Attr);
    auto* Progression = Player->GetProgression(); Progression->BindAttributes(Attr);
    if (!Progression->ChoosePermanentClassById(EBreakerClassId::Support)) return false;
    Progression->AwardExperience(UBreakerExperienceLibrary::TotalXpToReachLevel(50,Progression->ExperienceCurve));
    FBreakerQuestFlagSet Flags;
    for (const auto& Mission : UBreakerMissionLibrary::GetMissions())
        for (const auto& Beat : Mission.Beats)
            for (FName Flag : UBreakerMissionLibrary::BeatCompletionFlags(Beat)) Flags.Add(Flag);
    Flags.Add(TEXT("Quest.Finale.Seal")); Progression->SettleDoctrineEntitlement(Flags);
    FText Reason;
    if (!TestTrue(TEXT("paid Field Dressing supports ordinary self-heal income"),Progression->PurchaseNode(UBreakerProgressionLibrary::GetSupportMedicTree(),TEXT("Support.Medic.FieldDressing"),Reason))) return false;
    auto* Tree = UBreakerProgressionLibrary::GetSupportWardenTree();
    for (int32 I=0; I<2; ++I)
        if (!TestTrue(TEXT("paid Field of View prerequisite"),Progression->PurchaseNode(Tree,TEXT("Support.Warden.FieldOfView"),Reason))) return false;
    auto* Charge = Player->GetCharge(); Charge->BindAttributes(Attr); Charge->BeginPlay(); Charge->SetComponentTickEnabled(false); Charge->SetInCombat(true);
    // Fund both forty-Charge casts through ordinary capped self-heal income.
    // Twelve heals fund only the first cast; they do not guarantee eighty.
    for (int32 I=0; I<24 && Charge->GetCharge()<90.0f; ++I)
    {
        Charge->AdvanceLoop(1);
        FBreakerDamageRequest Hurt; Hurt.BaseDamage=Attr->GetMaxHealth()*.25f; Hurt.DamageFamily=EBreakerDamageFamily::TrueDamage; Hurt.bCanCritical=false; Hurt.bCanBeAvoided=false;
        Combat->ReceiveDamage(Hurt); Combat->ApplyHealingAmount(Hurt.BaseDamage,Player,FGameplayTag()); Charge->AdvanceLoop(1);
    }
    if (!TestTrue(TEXT("ordinary healing funds both paid casts with headroom"),Charge->GetCharge()>=90.0f)) return false;
    if (!TestTrue(TEXT("legal Pressure rank two within five total points"),Progression->PurchaseNode(Tree,TEXT("Support.Warden.Pressure"),Reason)
        && Progression->PurchaseNode(Tree,TEXT("Support.Warden.Pressure"),Reason))) return false;
    auto* Target=World->SpawnActor<ABreakerRangedEnemy>(FVector(1500,0,100),FRotator::ZeroRotator);
    if (!Target) return false;
    Target->ConfigureCrowdProbe(); Target->DispatchBeginPlay(); Target->SetActorTickEnabled(false);
    auto Advance=[&](float Seconds)
    {
        for (float Left=Seconds;Left>UE_SMALL_NUMBER;)
        {
            const float Step=FMath::Min(.05f,Left); ++GFrameCounter; World->Tick(LEVELTICK_All,Step);
            // Native World stays unbegun to avoid Character saves; explicitly
            // advance actual zone simulation while real timers drive Pressure.
            for (TActorIterator<ABreakerZoneActor> It(World);It;++It)
                if (!It->IsActorBeingDestroyed()) It->AdvanceZone(Step);
            Left-=Step;
        }
    };
    Advance(.1f);
    FVector Eye; FRotator Facing; Controller->GetPlayerViewPoint(Eye,Facing);
    Controller->SetControlRotation((FVector(1000,0,0)-Eye).Rotation());
    if (Controller->PlayerCameraManager) Controller->PlayerCameraManager->UpdateCamera(.05f);
    if (!Progression->IsAbilityUnlocked(TEXT("Support.Suppress")))
        if (!TestTrue(TEXT("actual earned token unlocks Suppress"),Progression->SpendAbilityToken(TEXT("Support.Suppress"),Reason))) return false;
    const float Before=Charge->GetCharge();
    const auto Handle=ASC->GiveAbility(FGameplayAbilitySpec(UBreakerAbility_Suppress::StaticClass(),1));
    if (!TestTrue(TEXT("actual paid Suppress"),ASC->TryActivateAbility(Handle))) return false;
    TestTrue(TEXT("Suppress spends normal healing-funded Charge"),Charge->GetCharge()<Before);
    ABreakerZoneActor* Zone=nullptr;
    for (TActorIterator<ABreakerZoneActor> It(World);It;++It) if (It->GetZoneInstigator()==Player) Zone=*It;
    if (!TestNotNull(TEXT("paid zone exists"),Zone)) return false;
    Target->SetActorLocation(Zone->GetActorLocation()+FVector(200,0,100));
    Zone->AdvanceZone(.01f);
    TestTrue(TEXT("actual enemy occupies zone"),Zone->GetOccupantCount()>0);
    const float Paid=Charge->GetCharge(); Advance(1.05f);
    const float Rate=GetDefault<UBreakerAbility_Suppress>()->PressureChargePerSecondRank2;
    TestEqual(TEXT("one live occupant pays authored rank-two rate"),Charge->GetCharge()-Paid,Rate,.001f);
    auto* Second=World->SpawnActor<ABreakerRangedEnemy>(Zone->GetActorLocation()+FVector(-200,0,100),FRotator::ZeroRotator);
    if (!Second) return false;
    Second->ConfigureCrowdProbe(); Second->DispatchBeginPlay(); Second->SetActorTickEnabled(false); Zone->AdvanceZone(.01f);
    const float WithTwo=Charge->GetCharge(); Advance(1);
    TestEqual(TEXT("two occupants still pay once"),Charge->GetCharge()-WithTwo,Rate,.001f);
    // Lose the node and buy it back before another timer tick: old zone cannot rearm.
    if (!TestTrue(TEXT("actual respec"),Progression->RespecAtForge(EBreakerPointCurrency::DoctrinePoints,true,Reason))) return false;
    for (int32 I=0;I<2;++I)
        if (!Progression->PurchaseNode(Tree,TEXT("Support.Warden.FieldOfView"),Reason)) return false;
    if (!Progression->PurchaseNode(Tree,TEXT("Support.Warden.Pressure"),Reason)) return false;
    const float AfterRespec=Charge->GetCharge(); Advance(1);
    TestEqual(TEXT("rebuy rank one cannot revive old rank-two lease"),Charge->GetCharge(),AfterRespec,.001f);
    // Start a new paid cast after its normal cooldown, with remaining real resources.
    Advance(10);
    if (!TestTrue(TEXT("second paid Suppress after ordinary cooldown"),ASC->TryActivateAbility(Handle))) return false;
    Zone=nullptr;
    for (TActorIterator<ABreakerZoneActor> It(World);It;++It) if (It->GetZoneInstigator()==Player && !It->IsActorBeingDestroyed()) Zone=*It;
    if (!Zone) return false;
    Target->SetActorLocation(Zone->GetActorLocation()+FVector(200,0,100)); Zone->AdvanceZone(.01f);
    const float RankOne=Charge->GetCharge(); Advance(1.05f);
    TestEqual(TEXT("new rank-one cast pays only current authored rank"),Charge->GetCharge()-RankOne,GetDefault<UBreakerAbility_Suppress>()->PressureChargePerSecond,.001f);
    FBreakerDamageRequest Lethal; Lethal.BaseDamage=Attr->GetHealth()+Attr->GetShield()+1;
    Lethal.DamageFamily=EBreakerDamageFamily::TrueDamage; Lethal.bCanCritical=false; Lethal.bCanBeAvoided=false;
    Combat->ReceiveDamage(Lethal);
    if (!TestTrue(TEXT("actual owner death"),Combat->IsDead())) return false;
    Combat->RestoreVitals();
    const float Revived=Charge->GetCharge(); Advance(1);
    TestEqual(TEXT("death then revive between ticks cannot resume old pressure"),Charge->GetCharge(),Revived,.001f);
    return true;
}
#endif
