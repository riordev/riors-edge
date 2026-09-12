#include "Tests/BreakerReactionRuntimeObserver.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "AI/BreakerEnemyMovementComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Classes/BreakerManaComponent.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerDamageLibrary.h"
#include "Combat/BreakerEnemy.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Progression/BreakerExperience.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionLibrary.h"
#include "Progression/BreakerProgressionTree.h"
#include "Save/BreakerMissionContent.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerConductorRuleRuntimeTest,
    "RiorsEdge.Combat.Elements.ConductorRuleRuntime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerConductorRuleRuntimeTest::RunTest(const FString&)
{
    UWorld::InitializationValues Init;
    Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!TestNotNull(TEXT("Runtime world"), World)) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL());
    const uint64 Frame = GFrameCounter;
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); GFrameCounter = Frame; };
    auto* Player = World->SpawnActor<ABreakerCharacter>();
    if (!TestNotNull(TEXT("Actual Caster pawn"), Player)) return false;
    Player->bRefuseSavesForPendingCharacter = true;
    Player->SetActorTickEnabled(false);
    Player->GetBreakerMovement()->SetComponentTickEnabled(false);
    auto* Attr = Player->GetAttributes();
    auto* ASC = Player->GetAbilitySystemComponent();
    ASC->InitAbilityActorInfo(Player, Player); ASC->AddAttributeSetSubobject(Attr);
    Player->GetCombat()->BindAttributes(Attr);
    auto* Progression = Player->GetProgression(); Progression->BindAttributes(Attr);
    if (!TestTrue(TEXT("Choose actual Caster kit"), Progression->ChoosePermanentClassById(EBreakerClassId::Caster))) return false;
    Progression->AwardExperience(UBreakerExperienceLibrary::TotalXpToReachLevel(40, Progression->ExperienceCurve));
    FBreakerQuestFlagSet Flags;
    // Restore the real campaign's completed benchmarks, not a synthetic wallet.
    // This fixture validates entitlement settlement; it does not replay missions.
    for (const auto& Mission : UBreakerMissionLibrary::GetMissions())
        for (const auto& Beat : Mission.Beats)
            for (FName Flag : UBreakerMissionLibrary::BeatCompletionFlags(Beat)) Flags.Add(Flag);
    Progression->SettleDoctrineEntitlement(Flags);
    TestEqual(TEXT("Campaign entitlement supplies eight Doctrine points"), Progression->GetProgressionState().UnspentDoctrinePoints, 8);
    auto* Tree = UBreakerProgressionLibrary::GetCasterMultispellTree();
    FText Reason;
    if (!TestTrue(TEXT("Commit actual Multispell branch"), Progression->CommitToBranch(Tree->TreeId, Reason))) return false;
    // O272: Conductor's Rule is the impactful half of Chain's pair; one point each.
    for (const TCHAR* Id : {TEXT("Caster.Multispell.Chain"), TEXT("Caster.Multispell.ConductorRule")})
        if (!TestTrue(FString::Printf(TEXT("Paid %s: %s"), Id, *Reason.ToString()), Progression->PurchaseNode(Tree, Id, Reason))) return false;
    TestEqual(TEXT("Chain pair spends two of eight"), Progression->GetProgressionState().UnspentDoctrinePoints, 6);
    auto* Mana = Player->GetMana(); Mana->BindAttributes(Attr); Mana->SetComponentTickEnabled(false);
    Mana->PassiveRegenPerSecond = 0; // Isolate the conditional-income cap, not tune gameplay.
    if (!TestTrue(TEXT("Native resource spend leaves positive-bank headroom"), Mana->TrySpendMana(60))) return false;
    auto SpawnTarget = [&](float X)
    {
        FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        auto* Enemy = World->SpawnActor<ABreakerEnemy>(FVector(X, 0, 100), FRotator::ZeroRotator, Spawn);
        if (!Enemy) return Enemy;
        Enemy->ConfigureCrowdProbe(); Enemy->DispatchBeginPlay(); Enemy->SetAreaLevel(40); Enemy->SetActorTickEnabled(false);
        if (auto* Movement = Enemy->FindComponentByClass<UBreakerEnemyMovementComponent>()) Movement->SetComponentTickEnabled(false);
        Enemy->FindComponentByClass<UBreakerCombatComponent>()->BindAttributes(FindObject<UBreakerAttributeSet>(Enemy, TEXT("Attributes")));
        return Enemy;
    };
    auto* A = SpawnTarget(1000); auto* B = SpawnTarget(4000);
    if (!TestNotNull(TEXT("First native target"), A) || !TestNotNull(TEXT("Independent native target"), B)) return false;
    auto* Status = A->FindComponentByClass<UBreakerStatusComponent>();
    auto* Combat = A->FindComponentByClass<UBreakerCombatComponent>();
    auto* OtherStatus = B->FindComponentByClass<UBreakerStatusComponent>();
    auto* OtherCombat = B->FindComponentByClass<UBreakerCombatComponent>();
    auto* Observer = NewObject<UBreakerReactionRuntimeObserver>(A);
    Combat->OnDamageTaken.AddDynamic(Observer, &UBreakerReactionRuntimeObserver::OnHit);
    const auto Rot = FGameplayTag::RequestGameplayTag(TEXT("Status.Rot"));
    const auto Collapse = FGameplayTag::RequestGameplayTag(TEXT("Reaction.Collapse"));
    auto Hit = [&](EBreakerElement Element, float Damage)
    {
        FBreakerDamageRequest Request; Request.BaseDamage = Damage; Request.Element = Element;
        Request.ElementalFraction = 1; Request.DamageFamily = EBreakerDamageFamily::Elemental;
        Request.bCanCritical = false; Request.SetInstigator(Player);
        UBreakerDamageLibrary::FillSourcePools(Attr, EBreakerDamageDelivery::Ability, Request);
        return Request;
    };
    const auto Trigger = Hit(EBreakerElement::Rift, 1);
    auto EarnRot = [&](UBreakerStatusComponent* TargetStatus, UBreakerCombatComponent* TargetCombat)
    {
        TargetCombat->RestoreVitals();
        const auto Request = Hit(EBreakerElement::Entropy, TargetStatus->GetEntropyThreshold() / (4 * Attr->GetAbilityDamageMultiplier()));
        for (int32 I = 0; I < 20 && !TargetStatus->HasStatus(Rot); ++I) TargetCombat->ReceiveDamage(Request);
        return TargetStatus->HasStatus(Rot);
    };
    // Existing public suspension discards earlier ordinary application income.
    // Removing it restores the native cap; no test access to the pending bank.
    auto DiscardPriorIncome = [&]() { Mana->PushGenerationSuspension(TEXT("Test.Conductor.Isolate")); Mana->PopGenerationSuspension(TEXT("Test.Conductor.Isolate")); };
    auto ReactionCount = [&]() { int32 Count = 0; for (const auto& H : Observer->Hits) if (H.DamageTypeTag == Collapse) ++Count; return Count; };
    if (!TestTrue(TEXT("Emitted Entropy hits earn finite Rot"), EarnRot(Status, Combat))) return false;
    Combat->ReceiveDamage(Trigger);
    TestEqual(TEXT("First eligible reaction pays"), ReactionCount(), 1);
    TestFalse(TEXT("Accepted reaction consumes its fuel"), Status->HasStatus(Rot));
    if (!TestTrue(TEXT("Further native hits earn new fuel"), EarnRot(Status, Combat))) return false;
    DiscardPriorIncome();
    const auto Before = Status->GetActiveStatuses()[0]; const float Bank = Mana->GetMana();
    const float RiftBeforeRefusal = Status->GetRiftBuildup();
    Combat->ReceiveDamage(Trigger);
    TestEqual(TEXT("Same-source same-target immediate reaction is refused"), ReactionCount(), 1);
    TestEqual(TEXT("Refused reaction does not add ordinary Rift buildup"), Status->GetRiftBuildup(), RiftBeforeRefusal);
    TestFalse(TEXT("Refused reaction releases its claimed empty transaction"), Status->IsElementTransactionActive());
    if (!TestTrue(TEXT("Refusal preserves fuel"), Status->HasStatus(Rot))) return false;
    const auto After = Status->GetActiveStatuses()[0];
    TestEqual(TEXT("Refusal preserves application identity"), After.ApplicationSerial, Before.ApplicationSerial);
    TestEqual(TEXT("Refusal preserves finite unpaid funding"), After.UnpaidDamageBudget, Before.UnpaidDamageBudget);
    TestEqual(TEXT("Compensation is queued, not immediate"), Mana->GetMana(), Bank);
    const float Slice = 2 / Mana->GlobalGenerationCap;
    Mana->AdvanceLoop(Slice);
    TestEqual(TEXT("One small cap slice pays only two Mana"), Mana->GetMana(), Bank + 2, .001f);
    Mana->AdvanceLoop(20 / Mana->GlobalGenerationCap);
    TestEqual(TEXT("One refused reaction owes exactly ten Mana"), Mana->GetMana(), Bank + 10, .001f);
    FBreakerDamageRequest Ordinary; Ordinary.BaseDamage = 1; Ordinary.bCanCritical = false; Ordinary.SetInstigator(Player);
    Combat->ReceiveDamage(Ordinary); Mana->AdvanceLoop(1);
    TestEqual(TEXT("Unrelated physical hit earns no refusal compensation"), Mana->GetMana(), Bank + 10, .001f);
    if (!TestTrue(TEXT("Other target earns native fuel"), EarnRot(OtherStatus, OtherCombat))) return false;
    OtherCombat->ReceiveDamage(Trigger);
    TestFalse(TEXT("Separate target can react inside first target's throttle"), OtherStatus->HasStatus(Rot));
    DiscardPriorIncome();
    for (int32 I = 0; I < 50; ++I) { ++GFrameCounter; World->Tick(LEVELTICK_All, .01f); }
    Combat->ReceiveDamage(Trigger);
    TestEqual(TEXT("Exactly fifty native .01-second ticks release preserved fuel"), ReactionCount(), 2);
    TestFalse(TEXT("Later accepted reaction consumes preserved fuel"), Status->HasStatus(Rot));
    // Invoke real nested combat from the existing consumption callback observer.
    if (!TestTrue(TEXT("Reentry case earns fuel"), EarnRot(Status, Combat))) return false;
    for (int32 I = 0; I < 50; ++I) { ++GFrameCounter; World->Tick(LEVELTICK_All, .01f); }
    Observer->Combat = Combat; Observer->Status = Status; Observer->Reactor = Player;
    Status->OnStatusConsumed.AddDynamic(Observer, &UBreakerReactionRuntimeObserver::OnConsumed);
    Observer->bReenterOnConsume = true; DiscardPriorIncome();
    Combat->RestoreVitals(); Combat->ReceiveDamage(Trigger);
    TestFalse(TEXT("Nested callback actually ran"), Observer->bReenterOnConsume);
    TestEqual(TEXT("Nested hit cannot duplicate the paid reaction"), ReactionCount(), 3);
    const float BeforeDrain = Mana->GetMana(); Mana->AdvanceLoop(1);
    TestEqual(TEXT("Transaction-blocked nested hit is not a Conductor refusal payout"), Mana->GetMana(), BeforeDrain, .001f);
    if (!TestTrue(TEXT("Respec case earns fuel"), EarnRot(Status, Combat))) return false;
    if (!TestTrue(TEXT("Actual Forge Doctrine respec succeeds"), Progression->RespecAtForge(EBreakerPointCurrency::DoctrinePoints, true, Reason))) return false;
    Combat->ReceiveDamage(Trigger);
    TestEqual(TEXT("Removed Conductor does not retain its throttle"), ReactionCount(), 4);
    return true;
}
#endif
