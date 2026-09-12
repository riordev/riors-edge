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
#include "Camera/PlayerCameraManager.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "Abilities/BreakerAbilityStateComponent.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerTellRuntimeTest, "RiorsEdge.Abilities.TellPaidWindup",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerTellRuntimeTest::RunTest(const FString& Parameters)
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
    // Tell is a single-rank travel root (O272): no prerequisite, one buy below.
    auto* Tree = UBreakerProgressionLibrary::GetSupportWardenTree();
    auto* Charge = Player->GetCharge(); Charge->BindAttributes(Attr); Charge->BeginPlay(); Charge->SetComponentTickEnabled(false); Charge->SetInCombat(true);
    for (int32 I=0; I<12; ++I)
    {
        Charge->AdvanceLoop(1);
        FBreakerDamageRequest Hurt; Hurt.BaseDamage=Attr->GetMaxHealth()*.25f; Hurt.DamageFamily=EBreakerDamageFamily::TrueDamage; Hurt.bCanCritical=false; Hurt.bCanBeAvoided=false;
        Combat->ReceiveDamage(Hurt); Combat->ApplyHealingAmount(Hurt.BaseDamage,Player,FGameplayTag()); Charge->AdvanceLoop(1);
    }
    auto* Target = World->SpawnActor<ABreakerRangedEnemy>(FVector(1500,0,100),FRotator(0,180,0));
    if (!Target) return false;
    Target->ConfigureCrowdProbe(); Target->DispatchBeginPlay(); Target->SetActorTickEnabled(false);
    ++GFrameCounter; World->Tick(LEVELTICK_All,.05f);
    FVector Eye; FRotator Facing; Controller->GetPlayerViewPoint(Eye,Facing);
    Controller->SetControlRotation((Target->GetActorLocation()-Eye).Rotation());
    if (Controller->PlayerCameraManager) Controller->PlayerCameraManager->UpdateCamera(.05f);
    if (!Progression->IsAbilityUnlocked(TEXT("Support.Mark")))
        if (!Progression->SpendAbilityToken(TEXT("Support.Mark"),Reason)) return false;
    const float Before = Charge->GetCharge();
    const auto Handle = ASC->GiveAbility(FGameplayAbilitySpec(UBreakerAbility_Mark::StaticClass(),1));
    if (!TestTrue(TEXT("actual paid aimed Mark"),ASC->TryActivateAbility(Handle))) return false;
    TestTrue(TEXT("Mark pays resource"),Charge->GetCharge()<Before);
    if (!TestTrue(TEXT("real cast owns target mark"),UBreakerAbilityStateComponent::FindOrAdd(Player)->IsMarked(Target))) return false;
    TestFalse(TEXT("unbought Tell does not advertise attack"),UBreakerAbility_Mark::ShouldShowTell(Player,Target));
    if (!TestTrue(TEXT("Tell buys with one point"),Progression->PurchaseNode(Tree,TEXT("Support.Warden.Tell"),Reason))) return false;
    TestFalse(TEXT("idle marked enemy is not a danger flash"),UBreakerAbility_Mark::ShouldShowTell(Player,Target));
    bool SawWindup=false;
    for (int32 I=0; I<80; ++I)
    {
        ++GFrameCounter; World->Tick(LEVELTICK_All,.05f); Target->Tick(.05f);
        if (Target->IsWindingUp()) { SawWindup=true; break; }
    }
    if (!TestTrue(TEXT("native Lattice decision actually begins an attack"),SawWindup)) return false;
    TestTrue(TEXT("owned Tell shows actual attack windup"),UBreakerAbility_Mark::ShouldShowTell(Player,Target));
    auto* Other = World->SpawnActor<ABreakerCharacter>();
    TestFalse(TEXT("another viewer never inherits owner telegraph"),UBreakerAbility_Mark::ShouldShowTell(Other,Target));
    if (!TestTrue(TEXT("actual respec removes Tell"),Progression->RespecAtForge(EBreakerPointCurrency::DoctrinePoints,true,Reason))) return false;
    TestFalse(TEXT("respec immediately hides still-winding target"),UBreakerAbility_Mark::ShouldShowTell(Player,Target));
    ASC->CancelAbilityHandle(Handle);
    TestFalse(TEXT("cancellation remains hidden"),UBreakerAbility_Mark::ShouldShowTell(Player,Target));
    return true;
}
#endif
