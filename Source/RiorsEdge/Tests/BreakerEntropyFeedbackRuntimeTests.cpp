#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerStatusComponent.h"
#include "Audio/BreakerSoundDirector.h"
#include "Audio/BreakerSoundMath.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundWaveProcedural.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "Movement/BreakerCharacterMovementComponent.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerEntropyFeedbackRuntimeTest, "RiorsEdge.Audio.EntropyActivationRuntime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerEntropyFeedbackRuntimeTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Init; Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!TestNotNull(TEXT("isolated cue world"), World)) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); World->InitializeActorsForPlay(FURL());
    const uint64 InitialFrame = GFrameCounter;
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); GFrameCounter = InitialFrame; };
    auto* Player = World->SpawnActor<ABreakerCharacter>();
    auto* PC = World->SpawnActor<APlayerController>();
    if (!Player || !PC) return false;
    PC->Player = NewObject<ULocalPlayer>(GEngine); PC->SetAsLocalPlayerController(); PC->Possess(Player);
    Player->GetBreakerMovement()->SetComponentTickEnabled(false);
    auto* ASC = Player->GetAbilitySystemComponent(); ASC->InitAbilityActorInfo(Player, Player);
    ASC->AddAttributeSetSubobject(Player->GetAttributes()); Player->GetCombat()->BindAttributes(Player->GetAttributes());
    auto* Director = World->SpawnActor<ABreakerSoundDirector>();
    if (!TestNotNull(TEXT("real cue director"), Director)) return false;
    Director->DispatchBeginPlay();
    auto* Target = World->SpawnActor<AActor>();
    auto* Combat = NewObject<UBreakerCombatComponent>(Target); Target->AddInstanceComponent(Combat); Combat->RegisterComponent();
    auto* Health = NewObject<UBreakerAttributeSet>(Target); Health->ApplyMaxHealth(1000); Health->ApplyHealth(1000); Combat->BindAttributes(Health);
    auto* Status = NewObject<UBreakerStatusComponent>(Target); Target->AddInstanceComponent(Status); Status->RegisterComponent(); Status->SetComponentTickEnabled(false);
    FBreakerDamageRequest Hit; Hit.BaseDamage = 101; Hit.Element = EBreakerElement::Entropy; Hit.ElementalFraction = 1;
    Hit.bCanCritical = false; Hit.SetInstigator(Player);
    Status->GrantStatusImmunity(1); Combat->ReceiveDamage(Hit);
    TestEqual(TEXT("immune hit emits no activation cue"), Director->GetEntropyCueCount(), 0);
    Status->AdvanceStatuses(1); Combat->DodgeChance = 1; Combat->ReceiveDamage(Hit); Combat->DodgeChance = 0;
    TestEqual(TEXT("dodged hit emits no activation cue"), Director->GetEntropyCueCount(), 0);
    Combat->ReceiveDamage(Hit);
    TestTrue(TEXT("actual hit earns Rot"), Status->HasStatus(FGameplayTag::RequestGameplayTag(TEXT("Status.Rot"))));
    TestEqual(TEXT("new Rot emits exactly one cue"), Director->GetEntropyCueCount(), 1);
    TArray<UAudioComponent*> Voices; Director->GetComponents(Voices);
    UAudioComponent* EntropyVoice = nullptr;
    for (auto* Voice : Voices) if (Voice->GetFName() == TEXT("EntropyVoice")) EntropyVoice = Voice;
    if (!TestNotNull(TEXT("activation uses its own real voice"), EntropyVoice)) return false;
    auto* Wave = Cast<USoundWaveProcedural>(EntropyVoice->Sound);
    TestTrue(TEXT("actual activation queues nonempty PCM"), Wave && Wave->GetAvailableAudioByteCount() > 0);
    TestFalse(TEXT("same-time crowd activation is throttled"), Director->PlayEntropyActivation());
    Combat->ReceiveDamage(Hit); Status->AdvanceStatuses(4);
    TestEqual(TEXT("repeated hits and all Rot ticks add no activation cue"), Director->GetEntropyCueCount(), 1);
    for (int32 I = 0; I < 5; ++I) { ++GFrameCounter; World->Tick(LEVELTICK_All, .05f); }
    Combat->ReceiveDamage(Hit);
    TestEqual(TEXT("a later newly earned Rot can announce itself again"), Director->GetEntropyCueCount(), 2);
    return true;
}
#endif
