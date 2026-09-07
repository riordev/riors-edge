#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerStatusComponent.h"
#include "Combat/BreakerStatusRules.h"
#include "Combat/BreakerVoid.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerVoidKernelRuntimeTest, "RiorsEdge.Combat.VoidKernelRuntime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerVoidKernelRuntimeTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Init;
    Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!TestNotNull(TEXT("isolated Void world"), World)) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL());
    const uint64 InitialFrame = GFrameCounter;
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); GFrameCounter = InitialFrame; };
    AActor* Source = World->SpawnActor<AActor>();
    AActor* Target = World->SpawnActor<AActor>();
    if (!Source || !Target) return false;
    auto* Combat = NewObject<UBreakerCombatComponent>(Target);
    Target->AddInstanceComponent(Combat); Combat->RegisterComponent();
    auto* Health = NewObject<UBreakerAttributeSet>(Target);
    Health->ApplyMaxHealth(1000); Health->ApplyHealth(1000); Combat->BindAttributes(Health);
    auto* Status = NewObject<UBreakerStatusComponent>(Target);
    Target->AddInstanceComponent(Status); Status->RegisterComponent();
    Status->SetComponentTickEnabled(false);
    const FGameplayTag Erased = FGameplayTag::RequestGameplayTag(TEXT("Status.Erased"));
    const FGameplayTag Rot = FGameplayTag::RequestGameplayTag(TEXT("Status.Rot"));
    const FBreakerStatusRule* Rule = BreakerStatusRules::FindRule(Erased);
    if (!TestNotNull(TEXT("shipped Erased rule"), Rule)) return false;
    TestTrue(TEXT("Erased is expiry damage, not periodic or armour/healing debuff"),
        Rule->bDealsDamageOnExpiry && !Rule->bDealsPeriodicDamage && !Rule->IsNonDamagingDebuff());
    TestFalse(TEXT("finite earned budget cannot spread on pierce"), Rule->bSpreadsOnPierce);
    TestEqual(TEXT("O2 authored delay"), BreakerVoid::DelaySeconds(), 2.0f);
    TestEqual(TEXT("O2 applying-hit fraction"), BreakerVoid::DamageFraction(), .5f);
    auto Hit = [&](float Damage, float Fraction = 1.0f, float Proc = 1.0f)
    {
        FBreakerDamageRequest Request;
        Request.BaseDamage = Damage; Request.DamageFamily = EBreakerDamageFamily::Elemental;
        Request.Element = EBreakerElement::Void; Request.ElementalFraction = Fraction;
        Request.ProcCoefficient = Proc; Request.bCanCritical = false; Request.SetInstigator(Source);
        return Combat->ReceiveDamage(Request);
    };
    auto Reset = [&]()
    {
        Status->ConsumeAllStatuses();
        Status->AdvanceStatuses(20);
        Health->ApplyHealth(1000);
        Status->VoidResistancePercent = 0;
    };
    TestEqual(TEXT("threshold follows real chassis maximum health"), Status->GetVoidThreshold(), 100.0f);
    Health->ApplyMaxHealth(2000);
    TestEqual(TEXT("larger chassis requires more buildup"), Status->GetVoidThreshold(), 200.0f);
    Health->ApplyMaxHealth(1000);
    Hit(0); Hit(10, 0); Hit(10, 1, 0);
    TestEqual(TEXT("zero damage, conversion and generation cannot accrue"), Status->GetVoidBuildup(), 0.0f);
    Combat->DodgeChance = 1;
    TestTrue(TEXT("real dodge refuses the hit"), Hit(100).bDodged);
    Combat->DodgeChance = 0;
    TestEqual(TEXT("dodge leaves no Void buildup"), Status->GetVoidBuildup(), 0.0f);
    Status->GrantStatusImmunity(1);
    Hit(100);
    TestEqual(TEXT("immunity refuses buildup"), Status->GetVoidBuildup(), 0.0f);
    Status->AdvanceStatuses(1);
    Status->VoidResistancePercent = 50;
    TestEqual(TEXT("Void resistance does not reduce immediate damage"), Hit(100).HealthDamage, 100.0f);
    TestEqual(TEXT("resistance only halves buildup"), Status->GetVoidBuildup(), 50.0f);
    Hit(100);
    if (!TestTrue(TEXT("real accepted hit earns Erased"), Status->HasStatus(Erased))) return false;
    if (!TestEqual(TEXT("one application"), Status->GetActiveStatuses().Num(), 1)) return false;
    TestEqual(TEXT("triggering hit earns fifty damage, not accumulated buildup"), Status->GetActiveStatuses()[0].UnpaidDamageBudget, 50.0f);
    TestEqual(TEXT("no per-tick payload"), Status->GetActiveStatuses()[0].Spec.BaseDamagePerTick, 0.0f);
    TestEqual(TEXT("no legacy armour reduction"), Status->GetArmorMultiplier(), 1.0f);
    TestEqual(TEXT("no legacy healing reduction"), Status->GetHealingReceivedMultiplier(), 1.0f);
    const float BeforeWaiting = Health->GetHealth();
    Status->AdvanceStatuses(1);
    TestEqual(TEXT("no phantom tick before deadline"), Health->GetHealth(), BeforeWaiting);
    FBreakerStatusApplicationSpec Forged = Status->GetActiveStatuses()[0].Spec;
    Forged.BaseDamagePerTick = 1000; Forged.Duration = 100; Forged.InitialStacks = 10;
    Status->ApplyStatus(Forged, EBreakerDamageFamily::Elemental, Source);
    Hit(200);
    TestEqual(TEXT("stronger hits and carried statuses cannot refresh deadline"), Status->GetActiveStatuses()[0].RemainingDuration, 1.0f);
    TestEqual(TEXT("stronger hits cannot replace unpaid snapshot"), Status->GetActiveStatuses()[0].UnpaidDamageBudget, 50.0f);
    const float BeforeBurst = Health->GetHealth();
    Status->AdvanceStatuses(1);
    TestEqual(TEXT("deadline pays snapshot once"), BeforeBurst - Health->GetHealth(), 50.0f, .001f);
    TestFalse(TEXT("burst removes Erased"), Status->HasStatus(Erased));
    TestEqual(TEXT("burst does not build its own successor"), Status->GetVoidBuildup(), 0.0f);
    const float AfterBurst = Health->GetHealth();
    Status->AdvanceStatuses(10);
    TestEqual(TEXT("expired budget cannot pay again"), Health->GetHealth(), AfterBurst);

    Reset(); Hit(100);
    const float BeforeHitch = Health->GetHealth();
    Status->AdvanceStatuses(20);
    TestEqual(TEXT("hitch pays exactly the same finite budget"), BeforeHitch - Health->GetHealth(), 50.0f, .001f);
    Reset(); Hit(100); Status->AdvanceStatuses(.75f);
    bool bFound = false;
    const FBreakerActiveStatus Consumed = Status->ConsumeStatus(Erased, bFound);
    TestTrue(TEXT("real single-type consumption succeeds"), bFound);
    TestEqual(TEXT("consumption transfers all unpaid damage before burst"), Consumed.UnpaidDamageBudget, 50.0f);
    const float BeforeConsumedDeadline = Health->GetHealth();
    Status->AdvanceStatuses(10);
    TestEqual(TEXT("consumption cancels future payout"), Health->GetHealth(), BeforeConsumedDeadline);
    TestEqual(TEXT("second consume has no budget"), Status->ConsumeStatus(Erased, bFound).UnpaidDamageBudget, 0.0f);
    TestFalse(TEXT("second consume finds nothing"), bFound);
    Reset(); Hit(100);
    const auto Cleansed = Status->ConsumeAllStatuses();
    if (!TestEqual(TEXT("cleanse returns exactly one removed status"), Cleansed.Num(), 1)) return false;
    TestEqual(TEXT("cleanse exposes unpaid budget without firing it"), Cleansed[0].UnpaidDamageBudget, 50.0f);
    const float BeforeCleanseDeadline = Health->GetHealth();
    Status->AdvanceStatuses(10);
    TestEqual(TEXT("cleanse cancels delayed damage"), Health->GetHealth(), BeforeCleanseDeadline);
    Reset(); Hit(100);
    Status->ScaleRemainingDurations(.5f);
    const float BeforeShortened = Health->GetHealth();
    Status->AdvanceStatuses(1);
    TestEqual(TEXT("duration shortening moves deadline without multiplying budget"), BeforeShortened - Health->GetHealth(), 50.0f, .001f);
    Reset(); Hit(100); Status->ScaleRemainingDurations(0);
    const float BeforeZeroed = Health->GetHealth();
    Status->AdvanceStatuses(10);
    TestEqual(TEXT("zero duration consumption does not detonate"), Health->GetHealth(), BeforeZeroed);

    Reset();
    FBreakerDamageRequest SnapshotRequest;
    SnapshotRequest.BaseDamage = 25; SnapshotRequest.SourceDamageMultiplier = 4;
    SnapshotRequest.CriticalChance = 1; SnapshotRequest.CriticalMultiplier = 2;
    SnapshotRequest.Element = EBreakerElement::Void; SnapshotRequest.ElementalFraction = .5f;
    SnapshotRequest.SetInstigator(Source);
    const auto CriticalHit = Combat->ReceiveDamage(SnapshotRequest);
    TestTrue(TEXT("applying hit actually crits"), CriticalHit.bCritical);
    if (!TestTrue(TEXT("critical converted share reaches threshold"), Status->HasStatus(Erased))) return false;
    TestEqual(TEXT("snapshot folds applying source power and critical exactly once"), Status->GetActiveStatuses()[0].UnpaidDamageBudget, 50.0f);
    SnapshotRequest.SourceDamageMultiplier = 20;
    SnapshotRequest.CriticalMultiplier = 10;
    const float BeforeSnapshotPayout = Health->GetHealth();
    Status->AdvanceStatuses(2);
    TestEqual(TEXT("delayed damage neither rebuilds source pool nor rerolls crit"), BeforeSnapshotPayout - Health->GetHealth(), 50.0f, .001f);

    Reset();
    FBreakerDamageRequest Protected;
    Protected.BaseDamage = 10; Protected.bCanCritical = false;
    Protected.Element = EBreakerElement::Void; Protected.ElementalFraction = 1;
    Protected.ElementBuildupFlat = 2; Protected.ElementBuildupFadeSeconds = 4;
    Protected.ProcCoefficient = .5f; Protected.SetInstigator(Source);
    Combat->ReceiveDamage(Protected);
    TestEqual(TEXT("flat ally contribution shares proc scaling exactly once"), Status->GetVoidBuildup(), 6.0f);
    Status->AdvanceStatuses(4);
    TestEqual(TEXT("protected contribution survives ordinary timeout"), Status->GetVoidBuildup(), 6.0f);
    Status->AdvanceStatuses(2);
    TestEqual(TEXT("protected contribution fades gradually"), Status->GetVoidBuildup(), 3.0f);
    Status->AdvanceStatuses(2);
    TestEqual(TEXT("protected contribution eventually expires"), Status->GetVoidBuildup(), 0.0f);

    Reset();
    FBreakerDamageRequest Entropy;
    Entropy.BaseDamage = 100; Entropy.Element = EBreakerElement::Entropy;
    Entropy.ElementalFraction = 1; Entropy.bCanCritical = false; Entropy.SetInstigator(Source);
    Combat->ReceiveDamage(Entropy); Hit(100);
    TestTrue(TEXT("Rot and Erased can coexist without premature reaction"), Status->HasStatus(Rot) && Status->HasStatus(Erased));
    Status->ConsumeAllStatuses();
    // Deterministic real avoidance applications: capped ordinary avoidance,
    // not an invented guaranteed immunity setting.
    Status->AilmentAvoidanceChance = .75f;
    bool bObservedAvoided = false;
    for (int32 Attempt = 0; Attempt < 32 && !bObservedAvoided; ++Attempt)
    {
        Reset(); Hit(100);
        bObservedAvoided = !Status->HasStatus(Erased);
    }
    TestTrue(TEXT("ordinary ailment avoidance can refuse earned Erased"), bObservedAvoided);
    TestEqual(TEXT("refused application retains no spendable buildup"), Status->GetVoidBuildup(), 0.0f);
    Status->AilmentAvoidanceChance = 0;
    Reset(); Hit(100);
    Hit(10000);
    TestTrue(TEXT("actual lethal hit kills target"), Combat->IsDead());
    TestFalse(TEXT("death cancels unpaid Erased"), Status->HasStatus(Erased));
    TestEqual(TEXT("death clears Void buildup"), Status->GetVoidBuildup(), 0.0f);
    Combat->RestoreVitals();
    const float AfterRevive = Health->GetHealth();
    Status->AdvanceStatuses(10);
    TestEqual(TEXT("revive never resumes a cancelled burst"), Health->GetHealth(), AfterRevive);
    Hit(50);
    Hit(10000);
    TestEqual(TEXT("death also clears a partial threshold"), Status->GetVoidBuildup(), 0.0f);
    return true;
}
#endif
