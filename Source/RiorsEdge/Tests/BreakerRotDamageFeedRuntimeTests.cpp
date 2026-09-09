#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerStatusComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "UI/BreakerDamageFeed.h"
#include "UI/BreakerPlaytestHUD.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerRotDamageFeedRuntimeTest, "RiorsEdge.UI.Damage.RotRuntime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerRotDamageFeedRuntimeTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Init;
    Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!TestNotNull(TEXT("isolated damage feed world"), World)) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL());
    const uint64 InitialFrame = GFrameCounter;
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); GFrameCounter = InitialFrame; };
    AActor* Dealer = World->SpawnActor<AActor>();
    AActor* Victim = World->SpawnActor<AActor>();
    auto* HUD = World->SpawnActor<ABreakerPlaytestHUD>();
    if (!Dealer || !Victim || !HUD) return false;
    auto* DealerCombat = NewObject<UBreakerCombatComponent>(Dealer);
    Dealer->AddInstanceComponent(DealerCombat); DealerCombat->RegisterComponent();
    auto* Combat = NewObject<UBreakerCombatComponent>(Victim);
    Victim->AddInstanceComponent(Combat); Combat->RegisterComponent();
    auto* Attributes = NewObject<UBreakerAttributeSet>(Victim);
    Combat->BindAttributes(Attributes);
    auto* Status = NewObject<UBreakerStatusComponent>(Victim);
    Victim->AddInstanceComponent(Status); Status->RegisterComponent(); Status->SetComponentTickEnabled(false);
    FBreakerDamageRequest ApplyingHit;
    ApplyingHit.BaseDamage = Status->GetEntropyThreshold();
    ApplyingHit.Element = EBreakerElement::Entropy; ApplyingHit.ElementalFraction = 1;
    ApplyingHit.bCanCritical = false; ApplyingHit.SetInstigator(Dealer);
    Combat->ReceiveDamage(ApplyingHit);
    const FGameplayTag Rot = FGameplayTag::RequestGameplayTag(TEXT("Status.Rot"));
    const FGameplayTag Bleed = FGameplayTag::RequestGameplayTag(TEXT("Status.Bleed"));
    if (!TestTrue(TEXT("ordinary accepted hit actually earns Rot"), Status->HasStatus(Rot))) return false;
    const float TickDamage = Status->GetActiveStatuses()[0].Spec.BaseDamagePerTick;
    const float TickInterval = Status->GetActiveStatuses()[0].Spec.TickInterval;
    FScriptDelegate Feed;
    Feed.BindUFunction(HUD, TEXT("HandlePlayerHitDealt"));
    DealerCombat->OnHitDealt.Add(Feed);
    // The HUD stamps arrivals from world time, independently of status time.
    // Advance both clocks: back-to-back manual ticks would falsely prove any
    // number of half-second ticks merge into a single zero-time arrival.
    auto TickStatus = [&](float Seconds)
    {
        for (float Elapsed = 0; Elapsed < Seconds;)
        {
            const float Step = FMath::Min(.01f, Seconds - Elapsed);
            ++GFrameCounter; World->Tick(LEVELTICK_All, Step); Elapsed += Step;
        }
        Status->AdvanceStatuses(Seconds);
    };
    const float StartTime = World->GetTimeSeconds();
    TickStatus(TickInterval);
    if (!TestEqual(TEXT("First real tick produces one number"), HUD->GetDamageNumbers().Num(), 1)) return false;
    const double FirstBirth = HUD->GetDamageNumbers()[0].Time;
    TestEqual(TEXT("First arrival follows one authored tick interval"), float(FirstBirth - StartTime), TickInterval, .001f);
    TickStatus(TickInterval);
    if (!TestEqual(TEXT("real Rot ticks reach one aggregated HUD number"), HUD->GetDamageNumbers().Num(), 1)) return false;
    const auto& Number = HUD->GetDamageNumbers()[0];
    TestTrue(TEXT("actual tick preserves DoT delivery"), Number.bFromDoT);
    TestEqual(TEXT("actual tick preserves Entropy identity"), Number.Element, EBreakerElement::Entropy);
    TestEqual(TEXT("actual tick preserves exact Rot provenance"), Number.DamageTypeTag, Rot);
    TestEqual(TEXT("aggregation sums actual tick damage"), Number.Value, TickDamage * 2, .001f);
    TestEqual(TEXT("Rot keeps subdued existing DoT lifetime"), Number.Lifetime, BreakerDamageFeed::MinimumDoTLifetime);
    TestEqual(TEXT("Second tick does not slide number birth"), Number.Time, FirstBirth);
    TestEqual(TEXT("Two ticks advance actual world time"), World->GetTimeSeconds() - StartTime, double(TickInterval) * 2.0, .001);
    FBreakerStatusApplicationSpec Physical;
    Physical.StatusTag = Bleed; Physical.Duration = 3; Physical.TickInterval = .5f; Physical.BaseDamagePerTick = 1;
    Physical.Snapshot.SourcePower = 1; Physical.Snapshot.CriticalChance = 0;
    Status->ApplyStatus(Physical, EBreakerDamageFamily::Physical, Dealer);
    TickStatus(.5f);
    TestEqual(TEXT("Next cadence group and physical tick stay separate"), HUD->GetDamageNumbers().Num(), 3);
    const auto* RotNumber = HUD->GetDamageNumbers().FindByPredicate([&](const FBreakerHUDDamageNumber& Entry) { return Entry.DamageTypeTag == Rot; });
    const auto* BleedNumber = HUD->GetDamageNumbers().FindByPredicate([&](const FBreakerHUDDamageNumber& Entry) { return Entry.DamageTypeTag == Bleed; });
    if (TestNotNull(TEXT("First Rot group remains independently identifiable"), RotNumber))
        TestEqual(TEXT("Expired merge group remains its original two ticks"), RotNumber->Value, TickDamage * 2, .001f);
    const auto* NextRot = HUD->GetDamageNumbers().FindByPredicate([&](const FBreakerHUDDamageNumber& Entry)
        { return Entry.DamageTypeTag == Rot && Entry.Time > FirstBirth; });
    if (TestNotNull(TEXT("A tick beyond the merge window starts a new Rot group"), NextRot))
    {
        TestEqual(TEXT("New Rot group contains only its accepted tick"), NextRot->Value, TickDamage, .001f);
        TestTrue(TEXT("New group begins outside first group's merge interval"), NextRot->Time - FirstBirth > BreakerDamageFeed::DoTMergeWindow);
    }
    if (TestNotNull(TEXT("physical tick has its own number"), BleedNumber)) TestEqual(TEXT("physical sum excludes Rot tick"), BleedNumber->Value, 1.0f, .001f);
    DealerCombat->OnHitDealt.Remove(Feed);
    return true;
}
#endif
