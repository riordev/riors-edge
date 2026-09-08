#include "Tests/BreakerReactionRuntimeObserver.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "AI/BreakerEnemyMovementComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerDamageLibrary.h"
#include "Combat/BreakerElementReactions.h"
#include "Combat/BreakerElementSourceMath.h"
#include "Combat/BreakerEnemy.h"
#include "Combat/BreakerVoid.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Progression/BreakerExperience.h"
#include "Progression/BreakerCoreWheelMath.h"
#include "Items/BreakerEquipmentComponent.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionLibrary.h"
#include "Progression/BreakerProgressionNode.h"
#include "Progression/BreakerProgressionTree.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerCoreReactionExceptionsRuntimeTest,
    "RiorsEdge.Combat.Elements.CoreReactionExceptionsRuntime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerCoreReactionExceptionsRuntimeTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Init; Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!TestNotNull(TEXT("Isolated native world"), World)) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); World->InitializeActorsForPlay(FURL());
    const uint64 Frame = GFrameCounter;
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); GFrameCounter = Frame; };
    auto MakePlayer = [&]()
    {
        auto* Player = World->SpawnActor<ABreakerCharacter>();
        if (!Player) return Player;
        Player->SetActorTickEnabled(false); Player->GetBreakerMovement()->SetComponentTickEnabled(false);
        auto* ASC = Player->GetAbilitySystemComponent(); auto* Attr = Player->GetAttributes();
        ASC->InitAbilityActorInfo(Player, Player); ASC->AddAttributeSetSubobject(Attr);
        Player->GetCombat()->BindAttributes(Attr); Player->GetProgression()->BindAttributes(Attr);
        Player->GetProgression()->ChoosePermanentClassById(EBreakerClassId::Swift);
        Player->GetProgression()->AwardExperience(UBreakerExperienceLibrary::TotalXpToReachLevel(30, Player->GetProgression()->ExperienceCurve));
        return Player;
    };
    auto* Player = MakePlayer(); auto* Ally = MakePlayer();
    if (!TestNotNull(TEXT("Owner"), Player) || !TestNotNull(TEXT("Independent allied applier"), Ally)) return false;
    auto* Progression = Player->GetProgression(); auto* Attr = Player->GetAttributes();
    auto* Tree = UBreakerProgressionLibrary::GetCoreSliceTree(); FText Reason;
    auto Buy = [&](const TCHAR* Id, int32 Ranks = 1)
    {
        for (int32 Rank = 0; Rank < Ranks; ++Rank)
            if (!TestTrue(*FString::Printf(TEXT("Earned native Core purchase %s: %s"), Id, *Reason.ToString()), Progression->PurchaseNode(Tree, Id, Reason))) return false;
        return true;
    };
    // Currency is the respec subject: fund its actual wallet price, then verify debit.
    auto PaidRespec = [&]()
    {
        auto* Equipment = Player->GetEquipment();
        const auto Cost = BreakerCoreRespecCost(Progression->GetCharacterLevel());
        const int32 Before = Equipment->GetForgeWallet().Get();
        Equipment->GrantForgeCurrency(Cost.Amount + 1);
        if (!TestTrue(TEXT("Paid native Core respec"), Progression->RespecCore(Reason))) return false;
        return TestEqual(TEXT("Actual respec price debited"), Equipment->GetForgeWallet().Get(), Before + 1);
    };
    auto Enemy = [&](float X)
    {
        FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        auto* E = World->SpawnActor<ABreakerEnemy>(FVector(X, 0, 100), FRotator::ZeroRotator, Spawn);
        if (!E) return E;
        E->ConfigureCrowdProbe(); E->SetAreaLevel(100); E->DispatchBeginPlay(); E->SetActorTickEnabled(false);
        if (auto* Move = E->FindComponentByClass<UBreakerEnemyMovementComponent>()) Move->SetComponentTickEnabled(false);
        E->FindComponentByClass<UBreakerCombatComponent>()->BindAttributes(FindObject<UBreakerAttributeSet>(E, TEXT("Attributes")));
        return E;
    };
    auto Clock = [&](float Seconds) { for (int32 I = 0; I < FMath::CeilToInt(Seconds * 100); ++I) { ++GFrameCounter; World->Tick(LEVELTICK_All, .01f); } };
    const auto Rot = FGameplayTag::RequestGameplayTag(TEXT("Status.Rot"));
    const auto Erased = FGameplayTag::RequestGameplayTag(TEXT("Status.Erased"));
    const auto Wither = FGameplayTag::RequestGameplayTag(TEXT("Reaction.Wither"));
    const auto Collapse = FGameplayTag::RequestGameplayTag(TEXT("Reaction.Collapse"));
    const auto Tear = FGameplayTag::RequestGameplayTag(TEXT("Reaction.Tear"));
    auto Observe = [&](ABreakerEnemy* E)
    {
        auto* O = NewObject<UBreakerReactionRuntimeObserver>(E);
        O->Combat = E->FindComponentByClass<UBreakerCombatComponent>(); O->Status = E->FindComponentByClass<UBreakerStatusComponent>(); O->Reactor = Player;
        O->Combat->OnDamageTaken.AddDynamic(O, &UBreakerReactionRuntimeObserver::OnHit);
        O->Status->OnStatusConsumed.AddDynamic(O, &UBreakerReactionRuntimeObserver::OnConsumed);
        return O;
    };
    auto Request = [&](ABreakerCharacter* Source, float Raw, EBreakerElement Element)
    {
        FBreakerDamageRequest Hit; Hit.BaseDamage = Raw / Source->GetAttributes()->GetAbilityDamageMultiplier();
        Hit.Element = Element; Hit.ElementalFraction = 1; Hit.DamageFamily = EBreakerDamageFamily::Elemental;
        Hit.bCanCritical = false; Hit.SetInstigator(Source);
        UBreakerDamageLibrary::FillSourcePools(Source->GetAttributes(), EBreakerDamageDelivery::Ability, Hit);
        return Hit;
    };
    auto Reactions = [&](UBreakerReactionRuntimeObserver* O)
    {
        auto Hits = O->Hits;
        Hits.RemoveAll([&](const auto& H) { return H.DamageTypeTag != Wither && H.DamageTypeTag != Collapse && H.DamageTypeTag != Tear; });
        return Hits;
    };
    // Native Vestiges resist Entropy buildup. Fund the actual threshold after
    // source buildup modifiers and target resistance; do not change the chassis.
    auto ActivationRaw = [&](ABreakerCharacter* Source, UBreakerStatusComponent* Status, EBreakerElement Element)
    {
        const auto Probe = Request(Source, 1.f, Element);
        const float Threshold = Element == EBreakerElement::Entropy ? Status->GetEntropyThreshold() : Status->GetVoidThreshold();
        const float Resistance = Element == EBreakerElement::Entropy ? Status->GetEntropyResistancePercent() : Status->GetVoidResistancePercent();
        const float Factor = BreakerElementSource::BuildupMultiplier(Probe) * BreakerElementSource::ResistanceFactor(Probe, Resistance);
        TestTrue(TEXT("Native activation witness has nonzero buildup acceptance"), Factor > 0);
        return 1.1f * BreakerElementSource::Threshold(Probe, Threshold) / FMath::Max(Factor, UE_SMALL_NUMBER);
    };
    auto Split = [&](ABreakerEnemy* E)
    {
        auto* Status = E->FindComponentByClass<UBreakerStatusComponent>();
        auto Hit = Request(Player, 2.f * FMath::Max(ActivationRaw(Player, Status, EBreakerElement::Entropy), ActivationRaw(Player, Status, EBreakerElement::Void)), EBreakerElement::None);
        Hit.ElementShares = {{EBreakerElement::Entropy, .5f}, {EBreakerElement::Void, .5f}};
        return Hit;
    };
    auto* DefaultEnemy = Enemy(1000);
    if (!TestNotNull(TEXT("Default target"), DefaultEnemy)) return false;
    auto* DefaultObserver = Observe(DefaultEnemy);
    const auto DefaultResult = DefaultObserver->Combat->ReceiveDamage(Split(DefaultEnemy));
    AddInfo(FString::Printf(TEXT("Mixed hit raw %.3f; Entropy threshold %.3f resistance %.1f remaining buildup %.3f; Void threshold %.3f resistance %.1f"),
        DefaultResult.RawDamage, DefaultObserver->Status->GetEntropyThreshold(), DefaultObserver->Status->GetEntropyResistancePercent(), DefaultObserver->Status->GetEntropyBuildup(),
        DefaultObserver->Status->GetVoidThreshold(), DefaultObserver->Status->GetVoidResistancePercent()));
    TestTrue(TEXT("Default mixed hit retains new Rot"), DefaultObserver->Status->HasStatus(Rot));
    TestTrue(TEXT("Default mixed hit also creates Erased"), DefaultObserver->Status->HasStatus(Erased));
    TestTrue(TEXT("Default mixed hit cannot react with itself"), Reactions(DefaultObserver).IsEmpty());
    if (!Buy(TEXT("Core.Reaction.Catalysis")) || !Buy(TEXT("Core.Reaction.Residue")) || !Buy(TEXT("Core.Reaction.SecondOrder"))) return false;
    auto* OrderedEnemy = Enemy(3000);
    if (!TestNotNull(TEXT("Second Order target"), OrderedEnemy)) return false;
    auto* OrderedObserver = Observe(OrderedEnemy);
    OrderedObserver->Combat->ReceiveDamage(Split(OrderedEnemy));
    const auto OrderedHits = Reactions(OrderedObserver);
    if (!TestEqual(TEXT("Purchased Second Order produces exactly one same-hit reaction"), OrderedHits.Num(), 1)) return false;
    TestEqual(TEXT("Later Void share consumes earlier own-hit Rot"), OrderedHits[0].DamageTypeTag, Wither);
    TestFalse(TEXT("Reaction does not also apply fresh Erased"), OrderedObserver->Status->HasStatus(Erased));
    TestFalse(TEXT("Reaction cannot reroll critical"), OrderedHits[0].Result.bCritical);
    if (!PaidRespec()) return false;
    auto* RespecEnemy = Enemy(5000);
    if (!TestNotNull(TEXT("Respec target"), RespecEnemy)) return false;
    auto* RespecObserver = Observe(RespecEnemy);
    RespecObserver->Combat->ReceiveDamage(Split(RespecEnemy));
    TestTrue(TEXT("Respec restores ordinary same-hit refusal"), Reactions(RespecObserver).IsEmpty());

    // All 26 points are actual authored Core purchases, funded by ordinary XP.
    for (const UBreakerProgressionNode* Node : Tree->Nodes)
        if (Node->Constellation == FName(TEXT("Reaction")))
            if (!Buy(*Node->NodeId.ToString(), Node->MaxRank)) return false;
    TestEqual(TEXT("Complete real Reaction major costs26"), Progression->GetConstellationInvestment(Tree, TEXT("Reaction")), 26);
    auto* Target = Enemy(7000); auto* Child = Enemy(7200); auto* Far = Enemy(7800);
    if (!Target || !Child || !Far) return false;
    auto* O = Observe(Target); auto* C = Observe(Child); auto* F = Observe(Far);
    auto Earn = [&](EBreakerElement Element)
    {
        O->Combat->ReceiveDamage(Request(Player, ActivationRaw(Player, O->Status, Element), Element));
    };
    Earn(EBreakerElement::Void); Earn(EBreakerElement::Entropy);
    if (!TestTrue(TEXT("Real hits earn two distinct source-owned statuses"), O->Status->HasStatus(Rot) && O->Status->HasStatus(Erased))) return false;
    float Expected = 0;
    for (const auto& Active : O->Status->GetActiveStatuses())
        if (Active.Spec.StatusTag == Rot || Active.Spec.StatusTag == Erased)
        {
            const float Normal = Active.Spec.StatusTag == Rot ? BreakerElementReactions::RemainingRotBudget(Active) : Active.UnpaidDamageBudget;
            Expected += Active.InitialReactionBudget * Normal / Active.InitialDamageBudget * .7f; // Paid Residue ranks retain30%.
        }
    O->Hits.Reset(); C->Hits.Reset(); F->Hits.Reset();
    O->bReenterOnConsume = true;
    O->Combat->ReceiveDamage(Request(Ally, 1.f, EBreakerElement::Rift));
    const auto Main = Reactions(O), Children = Reactions(C);
    if (!TestEqual(TEXT("Sympathetic triggers two defined pairs"), Main.Num(), 2) || !TestEqual(TEXT("Both bounded Chain children pay"), Children.Num(), 2)) return false;
    TestEqual(TEXT("Both original credits paid only their unretained funding"), Main[0].Result.RawDamage + Main[1].Result.RawDamage, Expected, .001f);
    TestEqual(TEXT("Children spend exactly authored half multiplicity"), Children[0].Result.RawDamage + Children[1].Result.RawDamage, Expected * .5f, .001f);
    TestTrue(TEXT("Foreign trigger cannot steal credit"), Main[0].Instigator.Get() == Player && Main[1].Instigator.Get() == Player);
    TestTrue(TEXT("No third target reaction chain"), Reactions(F).IsEmpty());
    TestFalse(TEXT("Real consume callback reentered exactly once"), O->bReenterOnConsume);
    // Remove only already-paid Residue through the native consume API to
    // isolate application lockout from independent existing-status refusal.
    bool bFound = false; O->Status->ConsumeStatus(Rot, bFound); O->Status->ConsumeStatus(Erased, bFound);
    O->Combat->RestoreVitals(); C->Combat->RestoreVitals();
    const auto DeferredHit = Request(Player, O->Status->GetVoidThreshold() * .4f, EBreakerElement::Void);
    FBreakerDamageResult LastAccepted;
    for (int32 I = 0; I < 5; ++I)
    {
        LastAccepted = O->Combat->ReceiveDamage(DeferredHit);
        C->Combat->ReceiveDamage(DeferredHit);
        TestFalse(TEXT("Owner applications remain deferred throughout lockout"), O->Status->HasStatus(Erased));
        TestFalse(TEXT("Affected Chain recipient defers the same owner"), C->Status->HasStatus(Erased));
        Clock(.5f);
    }
    TestTrue(TEXT("Deferred owner buildup remains observable"), O->Status->GetVoidBuildup() >= O->Status->GetVoidThreshold());
    const float ExpectedDeferredBudget = BreakerElementSource::StatusBudget(DeferredHit,
        BreakerElementSource::RawPart(DeferredHit, LastAccepted) * BreakerVoid::DamageFraction());
    // Ally can apply immediately without stealing or clearing owner's bank.
    O->Combat->ReceiveDamage(Request(Ally, ActivationRaw(Ally, O->Status, EBreakerElement::Entropy), EBreakerElement::Entropy));
    TestTrue(TEXT("Allied source is exempt"), O->Status->HasStatus(Rot));
    TestTrue(TEXT("Allied application leaves deferred owner's bank"), O->Status->GetVoidBuildup() >= O->Status->GetVoidThreshold());
    if (!PaidRespec()) return false;
    Clock(.55f);
    const auto* Deferred = O->Status->GetActiveStatuses().FindByPredicate([&](const auto& Active) { return Active.Spec.StatusTag == Erased; });
    if (!TestNotNull(TEXT("Eligible funded status applies after3s without another hit"), Deferred)) return false;
    TestEqual(TEXT("Deferred status preserves accepted-hit funding through respec"), Deferred->InitialDamageBudget, ExpectedDeferredBudget, .001f);
    TestTrue(TEXT("Deferred attribution remains its original owner"), Deferred->Instigator.Get() == Player);
    TestTrue(TEXT("Chain recipient also resumes after three seconds"), C->Status->HasStatus(Erased));
    const int32 ReactionsBefore = Reactions(O).Num(); Clock(.1f);
    TestEqual(TEXT("Deferred application itself cannot trigger reactions"), Reactions(O).Num(), ReactionsBefore);
    // Native elapsed-time partition: a five-second update must cross the
    // three-second unlock before the four-second buildup timeout.
    for (const UBreakerProgressionNode* Node : Tree->Nodes)
        if (Node->Constellation == FName(TEXT("Reaction")))
            if (!Buy(*Node->NodeId.ToString(), Node->MaxRank)) return false;
    auto PrepareClockTarget = [&](float X)
    {
        auto* E = Enemy(X);
        if (!E) return static_cast<UBreakerReactionRuntimeObserver*>(nullptr);
        auto* Obs = Observe(E);
        Obs->Combat->ReceiveDamage(Request(Player, ActivationRaw(Player, Obs->Status, EBreakerElement::Void), EBreakerElement::Void));
        Obs->Combat->ReceiveDamage(Request(Player, ActivationRaw(Player, Obs->Status, EBreakerElement::Entropy), EBreakerElement::Entropy));
        Obs->Combat->ReceiveDamage(Request(Ally, 1.f, EBreakerElement::Rift));
        bool Found = false;
        Obs->Status->ConsumeStatus(Rot, Found); Obs->Status->ConsumeStatus(Erased, Found);
        Obs->Combat->RestoreVitals();
        Obs->Combat->ReceiveDamage(Request(Player, ActivationRaw(Player, Obs->Status, EBreakerElement::Void), EBreakerElement::Void));
        TestFalse(TEXT("Clock witness begins with deferred Erased"), Obs->Status->HasStatus(Erased));
        Obs->Hits.Reset();
        return Obs;
    };
    auto* Coarse = PrepareClockTarget(11000); auto* Fine = PrepareClockTarget(13000); auto* Mid = PrepareClockTarget(15000);
    if (!Coarse || !Fine || !Mid) return false;
    Coarse->Status->AdvanceStatuses(5.f);
    for (int32 I = 0; I < 500; ++I) Fine->Status->AdvanceStatuses(.01f);
    Mid->Status->AdvanceStatuses(3.25f);
    const auto* MidStatus = Mid->Status->GetActiveStatuses().FindByPredicate([&](const auto& Active) { return Active.Spec.StatusTag == Erased; });
    if (!TestNotNull(TEXT("Large update unlocks before consuming post-unlock time"), MidStatus)) return false;
    TestEqual(TEXT("Only post-unlock quarter second ages resumed status"), MidStatus->RemainingDuration, BreakerVoid::DelaySeconds() - .25f, .001f);
    auto BurstTotal = [&](UBreakerReactionRuntimeObserver* Obs)
    {
        float Total = 0; int32 Count = 0;
        for (const auto& Hit : Obs->Hits) if (Hit.DamageTypeTag == Erased) { Total += Hit.Result.RawDamage; ++Count; }
        TestEqual(TEXT("Exactly one deferred Erased burst across unlock and expiry"), Count, 1);
        TestFalse(TEXT("Finite resumed status expires"), Obs->Status->HasStatus(Erased));
        TestTrue(TEXT("Deferred clock cannot manufacture a reaction"), Reactions(Obs).IsEmpty());
        return Total;
    };
    const float CoarseBudget = BurstTotal(Coarse), FineBudget = BurstTotal(Fine);
    TestTrue(TEXT("Coarse update preserves earned positive payout"), CoarseBudget > 0);
    TestEqual(TEXT("One large update and five hundred partitions pay identical funding"), CoarseBudget, FineBudget, .001f);
    return true;
}
#endif
