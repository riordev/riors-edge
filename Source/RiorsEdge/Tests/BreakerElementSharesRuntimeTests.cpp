#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerElementSharesMath.h"
#include "Combat/BreakerStatusComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Tests/BreakerReactionRuntimeObserver.h"

void UBreakerReactionRuntimeObserver::OnApplied(const FBreakerActiveStatus& Active)
{
    if (!bReenterOnApply || !Combat) return;
    bReenterOnApply = false;
    FBreakerDamageRequest Nested;
    Nested.BaseDamage = 10;
    Nested.Element = EBreakerElement::Void;
    Nested.ElementalFraction = 1;
    Nested.bCanCritical = false;
    Nested.SetInstigator(Reactor);
    Combat->ReceiveDamage(Nested);
}

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerElementSharesRuntimeTest,
    "RiorsEdge.Combat.Elements.OrderedSharesRuntime", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerElementSharesRuntimeTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Init;
    Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!TestNotNull(TEXT("isolated split request world"), World)) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL());
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
    auto* Source = World->SpawnActor<AActor>();
    auto* Target = World->SpawnActor<AActor>();
    if (!Source || !Target) return false;
    auto* Combat = NewObject<UBreakerCombatComponent>(Target);
    Target->AddInstanceComponent(Combat); Combat->RegisterComponent();
    auto* Health = NewObject<UBreakerAttributeSet>(Target);
    Health->ApplyMaxHealth(1000); Health->ApplyHealth(1000); Combat->BindAttributes(Health);
    auto* Status = NewObject<UBreakerStatusComponent>(Target);
    Target->AddInstanceComponent(Status); Status->RegisterComponent(); Status->SetComponentTickEnabled(false);
    auto* Observer = NewObject<UBreakerReactionRuntimeObserver>(Target);
    Observer->Combat = Combat; Observer->Status = Status; Observer->Reactor = Source;
    Combat->OnDamageTaken.AddDynamic(Observer, &UBreakerReactionRuntimeObserver::OnHit);
    Status->OnStatusConsumed.AddDynamic(Observer, &UBreakerReactionRuntimeObserver::OnConsumed);
    Status->OnStatusApplied.AddDynamic(Observer, &UBreakerReactionRuntimeObserver::OnApplied);
    const auto Rot = FGameplayTag::RequestGameplayTag(TEXT("Status.Rot"));
    const auto Erased = FGameplayTag::RequestGameplayTag(TEXT("Status.Erased"));
    const auto Wither = FGameplayTag::RequestGameplayTag(TEXT("Reaction.Wither"));
    auto Request = [&](float Damage, TArray<FBreakerElementShare> Shares)
    {
        FBreakerDamageRequest Hit;
        Hit.BaseDamage = Damage; Hit.DamageFamily = EBreakerDamageFamily::Elemental;
        Hit.bCanCritical = false; Hit.SetInstigator(Source); Hit.ElementShares = MoveTemp(Shares);
        return Hit;
    };
    auto Reset = [&]()
    {
        Observer->bReenterOnConsume = false;
        Status->ConsumeAllStatuses(); Status->AdvanceStatuses(20); Combat->RestoreVitals();
        Observer->Hits.Reset(); Observer->Consumed = 0;
    };
    auto Budget = [&](FGameplayTag Tag)
    {
        const auto* Active = Status->GetActiveStatuses().FindByPredicate([&](const auto& Entry) { return Entry.Spec.StatusTag == Tag; });
        return Active ? Active->UnpaidDamageBudget : -1.0f;
    };
    auto Split = Request(200, {{EBreakerElement::Entropy, .5f}, {EBreakerElement::Void, .5f}});
    const auto Result = Combat->ReceiveDamage(Split);
    TestEqual(TEXT("split retains one raw damage result"), Result.RawDamage, 200.0f);
    TestEqual(TEXT("split lands direct damage only once"), Health->GetHealth(), 800.0f);
    TestEqual(TEXT("one direct hit callback"), Observer->Hits.Num(), 1);
    TestTrue(TEXT("newly earned Rot survives later share"), Status->HasStatus(Rot));
    TestTrue(TEXT("same hit also earns Erased without self-reaction"), Status->HasStatus(Erased));
    TestEqual(TEXT("equal Entropy share earns finite fifty budget"), Budget(Rot), 50.0f);
    TestEqual(TEXT("equal Void share earns same finite budget"), Budget(Erased), 50.0f);
    TestEqual(TEXT("no newly earned status was consumed"), Observer->Consumed, 0);

    Reset();
    Combat->ReceiveDamage(Request(100, {{EBreakerElement::Entropy, 1}}));
    if (!TestTrue(TEXT("preexisting Rot is genuinely earned"), Status->HasStatus(Rot))) return false;
    Observer->Hits.Reset();
    Combat->ReceiveDamage(Request(10, {{EBreakerElement::Void, .5f}, {EBreakerElement::Rift, .5f}}));
    TestEqual(TEXT("ordered reaction produces outer hit and one child"), Observer->Hits.Num(), 2);
    if (Observer->Hits.Num() == 2)
    {
        TestEqual(TEXT("first eligible share chooses Wither"), Observer->Hits[1].DamageTypeTag, Wither);
        TestEqual(TEXT("reaction spends only previous unpaid budget"), Observer->Hits[1].Result.HealthDamage, 50.0f);
    }
    TestEqual(TEXT("exactly one status consumed"), Observer->Consumed, 1);
    TestEqual(TEXT("reacting hit adds no Void buildup"), Status->GetVoidBuildup(), 0.0f);
    TestEqual(TEXT("later Rift share adds no buildup"), Status->GetRiftBuildup(), 0.0f);
    TestEqual(TEXT("one direct hit plus prior budget"), Health->GetHealth(), 840.0f);

    Reset();
    auto Duplicate = Request(20, {{EBreakerElement::Entropy, .25f}, {EBreakerElement::Entropy, .25f}, {EBreakerElement::Void, .5f}});
    Duplicate.ElementBuildupFlat = 12;
    const auto Normalized = BreakerElementShares::Resolve(Duplicate);
    TestEqual(TEXT("duplicate element merges before dispatch"), Normalized.Num(), 2);
    Combat->ReceiveDamage(Duplicate);
    TestEqual(TEXT("Entropy flat bonus is paid once per hit"), Status->GetEntropyBuildup(), 22.0f);
    TestEqual(TEXT("Entropy-specific bonus never spills into Void"), Status->GetVoidBuildup(), 10.0f);
    TestEqual(TEXT("duplicates never duplicate direct damage"), Health->GetHealth(), 980.0f);
    Reset();
    Split.ProcCoefficient = 0;
    Combat->ReceiveDamage(Split);
    TestEqual(TEXT("zero proc still lands one ordinary direct hit"), Health->GetHealth(), 800.0f);
    TestEqual(TEXT("zero proc earns no statuses"), Status->GetActiveStatuses().Num(), 0);
    TestEqual(TEXT("zero proc earns no Entropy"), Status->GetEntropyBuildup(), 0.0f);
    TestEqual(TEXT("zero proc earns no Void"), Status->GetVoidBuildup(), 0.0f);

    Reset();
    Combat->ReceiveDamage(Request(100, {{EBreakerElement::Entropy, 1}}));
    Observer->bReenterOnConsume = true;
    Combat->ReceiveDamage(Request(10, {{EBreakerElement::Void, .5f}, {EBreakerElement::Rift, .5f}}));
    TestEqual(TEXT("real recursive consume callback cannot consume twice"), Observer->Consumed, 1);
    TestFalse(TEXT("nested real Entropy hit cannot recreate Rot during transaction"), Status->HasStatus(Rot));
    TestFalse(TEXT("nested hit cannot create Erased"), Status->HasStatus(Erased));
    TestEqual(TEXT("recursive transaction leaves no new buildup"), Status->GetEntropyBuildup(), 0.0f);
    TestFalse(TEXT("outer dispatch releases its guard"), Status->IsElementTransactionActive());
    Reset();
    Observer->bReenterOnApply = true;
    Split.ProcCoefficient = 1;
    Combat->ReceiveDamage(Split);
    TestTrue(TEXT("nested application callback cannot consume newly earned Rot"), Status->HasStatus(Rot));
    TestTrue(TEXT("outer sibling still earns Erased"), Status->HasStatus(Erased));
    TestEqual(TEXT("nested application callback creates no reaction"), Observer->Consumed, 0);
    TestEqual(TEXT("nested direct hit remains payable without elemental recursion"), Health->GetHealth(), 790.0f);
    return true;
}
#endif
