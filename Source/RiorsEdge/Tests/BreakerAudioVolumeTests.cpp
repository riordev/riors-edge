#include "Misc/AutomationTest.h"
#include "Audio/BreakerSoundDirector.h"
#include "Audio/BreakerSoundMath.h"
#include "Components/AudioComponent.h"
#include "Settings/BreakerGameSettings.h"
#include "Engine/World.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerAudioVolumeRoutingTest,
    "RiorsEdge.Audio.VolumeRouting", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerAudioVolumeRoutingTest::RunTest(const FString& Parameters)
{
    TestEqual(TEXT("Master and effects compose"), BreakerSound::EffectsGain(0.5f, 0.4f), 0.2f);
    TestEqual(TEXT("Master mutes"), BreakerSound::EffectsGain(0, 1), 0.0f);
    TestEqual(TEXT("Effects mutes"), BreakerSound::EffectsGain(1, 0), 0.0f);
    TestEqual(TEXT("Gain clamps at full volume"), BreakerSound::EffectsGain(2, 3), 1.0f);

    UWorld::InitializationValues Initialization;
    Initialization.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true,
        ERHIFeatureLevel::Num, &Initialization);
    if (!TestNotNull(TEXT("Volume fixture world"), World)) return false;
    ON_SCOPE_EXIT { World->DestroyWorld(false); };
    ABreakerSoundDirector* Director = World->SpawnActor<ABreakerSoundDirector>();
    if (!TestNotNull(TEXT("Sound director"), Director)) return false;
    TArray<UAudioComponent*> Voices;
    Director->GetComponents(Voices);
    // ELEVEN, DOWN FROM THIRTEEN (owner, playtest 2026-09-10). The footstep and
    // take-hit voices are gone with the cues they carried: "i dont need audio
    // of my character groaning when i take damage (its so fucking annoying same
    // thing with footsteps)". This count is not a range being widened to go
    // green — it is the roster, and the roster shrank by decision. The
    // ASSERTION it exists for is the one below: every voice that remains is
    // routed through the settings pool.
    if (!TestEqual(TEXT("All eleven player cues have voices"), Voices.Num(), 11)) return false;
    TestFalse(TEXT("Footsteps are gone, not merely silenced"), Voices.ContainsByPredicate([](const UAudioComponent* Voice) { return Voice->GetFName() == TEXT("FootstepVoice"); }));
    TestFalse(TEXT("The take-hit vocal is gone with them"), Voices.ContainsByPredicate([](const UAudioComponent* Voice) { return Voice->GetFName() == TEXT("TakeHitVoice"); }));
    TestTrue(TEXT("Void activation has its own voice"), Voices.ContainsByPredicate([](const UAudioComponent* Voice) { return Voice->GetFName() == TEXT("VoidMarkVoice"); }));
    TestTrue(TEXT("Void payout has its own voice"), Voices.ContainsByPredicate([](const UAudioComponent* Voice) { return Voice->GetFName() == TEXT("VoidBurstVoice"); }));
    TestTrue(TEXT("Rift activation has its own voice"), Voices.ContainsByPredicate([](const UAudioComponent* Voice) { return Voice->GetFName() == TEXT("RiftVoice"); }));
    TestTrue(TEXT("Reactions have a bounded shared voice"), Voices.ContainsByPredicate([](const UAudioComponent* Voice) { return Voice->GetFName() == TEXT("ReactionVoice"); }));
    // A level was gained. It routes through the same settings pool as every
    // other cue, which is the whole point of counting them here.
    TestTrue(TEXT("Level-up has its own voice"), Voices.ContainsByPredicate([](const UAudioComponent* Voice) { return Voice->GetFName() == TEXT("LevelUpVoice"); }));
    UBreakerGameSettings* Settings = NewObject<UBreakerGameSettings>();
    Settings->MasterVolume = 0.5f;
    Settings->EffectsVolume = 0.4f;
    Settings->ApplyAudioSettings();
    for (const UAudioComponent* Voice : Voices)
        TestEqual(TEXT("Every real voice receives combined gain"), Voice->VolumeMultiplier, 0.2f);
    Director->ApplyVolumeSettings(0, 1);
    for (const UAudioComponent* Voice : Voices)
        TestEqual(TEXT("Every real voice mutes"), Voice->VolumeMultiplier, 0.0f);
    Director->ApplyVolumeSettings(1, 1);
    for (const UAudioComponent* Voice : Voices)
        TestEqual(TEXT("Every real voice unmutes"), Voice->VolumeMultiplier, 1.0f);
    return true;
}
#endif
