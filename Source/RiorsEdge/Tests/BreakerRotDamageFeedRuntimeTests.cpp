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
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
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
    Status->AdvanceStatuses(TickInterval);
    Status->AdvanceStatuses(TickInterval);
    if (!TestEqual(TEXT("real Rot ticks reach one aggregated HUD number"), HUD->GetDamageNumbers().Num(), 1)) return false;
    const auto& Number = HUD->GetDamageNumbers()[0];
    TestTrue(TEXT("actual tick preserves DoT delivery"), Number.bFromDoT);
    TestEqual(TEXT("actual tick preserves Entropy identity"), Number.Element, EBreakerElement::Entropy);
    TestEqual(TEXT("actual tick preserves exact Rot provenance"), Number.DamageTypeTag, Rot);
    TestEqual(TEXT("aggregation sums actual tick damage"), Number.Value, TickDamage * 2, .001f);
    TestEqual(TEXT("Rot keeps subdued existing DoT lifetime"), Number.Lifetime, BreakerDamageFeed::MinimumDoTLifetime);
    FBreakerStatusApplicationSpec Physical;
    Physical.StatusTag = Bleed; Physical.Duration = 3; Physical.TickInterval = .5f; Physical.BaseDamagePerTick = 1;
    Physical.Snapshot.SourcePower = 1; Physical.Snapshot.CriticalChance = 0;
    Status->ApplyStatus(Physical, EBreakerDamageFamily::Physical, Dealer);
    Status->AdvanceStatuses(.5f);
    TestEqual(TEXT("simultaneous physical and Rot ticks stay separate in the actual HUD"), HUD->GetDamageNumbers().Num(), 2);
    const auto* RotNumber = HUD->GetDamageNumbers().FindByPredicate([&](const FBreakerHUDDamageNumber& Entry) { return Entry.DamageTypeTag == Rot; });
    const auto* BleedNumber = HUD->GetDamageNumbers().FindByPredicate([&](const FBreakerHUDDamageNumber& Entry) { return Entry.DamageTypeTag == Bleed; });
    if (TestNotNull(TEXT("Rot number remains independently identifiable"), RotNumber)) TestEqual(TEXT("Rot sum excludes physical tick"), RotNumber->Value, TickDamage * 3, .001f);
    if (TestNotNull(TEXT("physical tick has its own number"), BleedNumber)) TestEqual(TEXT("physical sum excludes Rot tick"), BleedNumber->Value, 1.0f, .001f);
    DealerCombat->OnHitDealt.Remove(Feed);
    return true;
}
#endif
