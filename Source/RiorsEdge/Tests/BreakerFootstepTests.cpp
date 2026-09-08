#include "Misc/AutomationTest.h"
#include "Audio/BreakerFootstepComponent.h"
#include "Audio/BreakerWaveFile.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Audio/BreakerSoundDirector.h"
#include "AbilitySystemComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerCombatComponent.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundWaveProcedural.h"
#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerFootstepTest, "RiorsEdge.Audio.GroundedFootsteps", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerFootstepTest::RunTest(const FString& Parameters)
{
    for (int32 Index = 0; Index < 5; ++Index)
    {
        TArray<uint8> Bytes;
        const FString Path = FPaths::ProjectContentDir() / FString::Printf(TEXT("Breaker/Audio/kenney/impact-sounds/footstep_concrete_%03d.wav"), Index);
        TestTrue(TEXT("shipped footstep sample readable"), FFileHelper::LoadFileToArray(Bytes, *Path));
        const auto Wave = BreakerWave::ParseWav(Bytes);
        TestTrue(TEXT("existing PCM playback parser decodes recorded sample"), Wave.IsValid());
        TestTrue(TEXT("sample contains actual nonzero sound"), Wave.Samples.ContainsByPredicate([](int16 Sample) { return Sample != 0; }));
    }
    for (float Speed : { 400.0f, 1000.0f })
    {
        FBreakerFootstepCadence Cadence;
        FVector Position = FVector::ZeroVector;
        Cadence.Advance(Position, Speed, .05f, true);
        int32 Steps = 0; float SinceStep = 100;
        for (int32 Frame = 0; Frame < 200; ++Frame)
        {
            Position.X += Speed * .05f; SinceStep += .05f;
            if (Cadence.Advance(Position, Speed, .05f, true))
            {
                TestTrue(TEXT("sprint cadence never exceeds authored cap"), SinceStep >= .28f);
                SinceStep = 0; ++Steps;
            }
        }
        TestTrue(TEXT("actual grounded distance produces footsteps"), Steps > 15);
        TestTrue(TEXT("ten seconds cannot produce footstep spam"), Steps <= 35);
        for (int32 Frame = 0; Frame < 20; ++Frame)
            TestFalse(TEXT("stationary body never steps even with stale velocity"), Cadence.Advance(Position, Speed, .05f, true));
        for (int32 Frame = 0; Frame < 20; ++Frame)
        {
            Position.X += Speed * .05f;
            TestFalse(TEXT("airborne/menu/traversal qualification discards distance"), Cadence.Advance(Position, Speed, .05f, false));
        }
        Position.X += 10000;
        TestFalse(TEXT("teleport cannot sound a step"), Cadence.Advance(Position, Speed, .05f, true));
        TestFalse(TEXT("stationary arrival remains quiet"), Cadence.Advance(Position, 0, .05f, true));
        Position.X += 500;
        TestFalse(TEXT("hitch cannot replay accumulated distance"), Cadence.Advance(Position, Speed, 1, true));
    }
    UWorld::InitializationValues Init;
    Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!TestNotNull(TEXT("native footsteps world"), World)) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL());
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
    auto* Player = World->SpawnActor<ABreakerCharacter>();
    auto* Controller = World->SpawnActor<APlayerController>();
    if (!Player || !Controller) return false;
    Controller->Player = NewObject<ULocalPlayer>(GEngine);
    Controller->SetAsLocalPlayerController(); Controller->Possess(Player);
    TestTrue(TEXT("fixture controller is actually local"), Controller->IsLocalController());
    Player->SetActorTickEnabled(false);
    auto* Movement = Player->GetBreakerMovement(); Movement->SetComponentTickEnabled(false);
    Player->GetAbilitySystemComponent()->InitAbilityActorInfo(Player, Player);
    Player->GetAbilitySystemComponent()->AddAttributeSetSubobject(Player->GetAttributes());
    Player->GetCombat()->BindAttributes(Player->GetAttributes());
    auto* Footsteps = Player->FindComponentByClass<UBreakerFootstepComponent>();
    if (!TestNotNull(TEXT("shipping Character attaches real footstep component"), Footsteps)) return false;
    // Existing owner stays unbegun (no owner saves). New sound director begins
    // normally, loading its actual samples and wiring its playback voice.
    World->SetBegunPlay(true);
    Movement->SetMovementMode(MOVE_Falling);
    Movement->Velocity = FVector(400, 0, 0);
    for (int32 Frame = 0; Frame < 12; ++Frame)
    {
        Player->SetActorLocation(Player->GetActorLocation() + FVector(20, 0, 0));
        Footsteps->TickComponent(.05f, LEVELTICK_All, &Footsteps->PrimaryComponentTick);
    }
    TestFalse(TEXT("actual airborne movement gate creates no audio director"), TActorIterator<ABreakerSoundDirector>(World) ? true : false);
    Movement->SetMovementMode(MOVE_Walking);
    for (int32 Frame = 0; Frame < 12; ++Frame)
    {
        Player->SetActorLocation(Player->GetActorLocation() + FVector(20, 0, 0));
        Footsteps->TickComponent(.05f, LEVELTICK_All, &Footsteps->PrimaryComponentTick);
    }
    ABreakerSoundDirector* Director = nullptr;
    int32 Directors = 0;
    for (TActorIterator<ABreakerSoundDirector> It(World); It; ++It) { Director = *It; ++Directors; }
    if (!TestEqual(TEXT("grounded component reuses one shared audio director"), Directors, 1)) return false;
    TArray<UAudioComponent*> Voices; Director->GetComponents(Voices);
    UAudioComponent* Voice = nullptr;
    for (auto* Candidate : Voices) if (Candidate->GetFName() == TEXT("FootstepVoice")) Voice = Candidate;
    if (!TestNotNull(TEXT("actual step reaches dedicated voice"), Voice)) return false;
    auto* Wave = Cast<USoundWaveProcedural>(Voice->Sound);
    TestTrue(TEXT("grounded distance queues shipped PCM through actual playback path"), Wave && Wave->GetAvailableAudioByteCount() > 0);
    return true;
}
#endif
