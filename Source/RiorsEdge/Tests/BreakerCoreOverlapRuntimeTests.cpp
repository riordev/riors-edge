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
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerCoreOverlapRuntimeTest,
    "RiorsEdge.Combat.Elements.CoreOverlapRuntime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerCoreOverlapRuntimeTest::RunTest(const FString& Parameters)
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
    if (!Buy(TEXT("Core.Reaction.Catalysis")) || !Buy(TEXT("Core.Reaction.Feedback")) || !Buy(TEXT("Core.Reaction.Overlap"))) return false;
    TestEqual(TEXT("Actual Overlap branch purchase costs four points"), Progression->GetConstellationInvestment(Tree, TEXT("Reaction")), 4);
    const float Fraction = BreakerElementReactions::OverlapThresholdFraction();
    TestEqual(TEXT("Editable authored Overlap threshold fraction"), Fraction, .25f);
    auto Earn = [&](UBreakerReactionRuntimeObserver* O, EBreakerElement Element)
    {
        O->Combat->RestoreVitals();
        O->Combat->ReceiveDamage(Request(Player, ActivationRaw(Player, O->Status, Element), Element));
    };
    auto* E = Enemy(1000); if (!E) return false; auto* O = Observe(E);
    for (int32 I = 0; I < 5; ++I)
    {
        Earn(O, EBreakerElement::Entropy);
        if (!TestTrue(TEXT("Native resisted hit earns Rot"), O->Status->HasStatus(Rot))) return false;
        O->Combat->ReceiveDamage(Request(Ally, 1, EBreakerElement::Rift));
        TestEqual(TEXT("Collapse contributes exactly one unfunded Void seed"), O->Status->GetVoidBuildup(), (I + 1) * Fraction * O->Status->GetVoidThreshold(), .01f);
        TestFalse(TEXT("Seed does not apply Erased, even above threshold"), O->Status->HasStatus(Erased));
    }
    TestEqual(TEXT("Only five earned original reactions were paid"), Reactions(O).Num(), 5);
    O->Status->AdvanceStatuses(.1f);
    TestFalse(TEXT("Time alone cannot turn seed into an application"), O->Status->HasStatus(Erased));
    O->Combat->RestoreVitals();
    // Removing the node preserves earned buildup, but the later direct hit
    // supplies all status funding and receives no reaction-budget transfer.
    if (!PaidRespec()) return false;
    const auto Crossing = Request(Ally, O->Status->GetVoidThreshold() * .01f, EBreakerElement::Void);
    const auto Result = O->Combat->ReceiveDamage(Crossing);
    const auto* Applied = O->Status->GetActiveStatuses().FindByPredicate([&](const auto& A) { return A.Spec.StatusTag == Erased; });
    if (!TestNotNull(TEXT("Later native hit funds ordinary application"), Applied)) return false;
    const float Budget = BreakerElementSource::StatusBudget(Crossing, BreakerElementSource::RawPart(Crossing, Result) * BreakerVoid::DamageFraction());
    TestEqual(TEXT("Seed adds zero damage funding"), Applied->InitialDamageBudget, Budget, .001f);
    TestTrue(TEXT("Allied crossing hit owns the resulting status, not the seed author"), Applied->Instigator.Get() == Ally);
    TestEqual(TEXT("Crossing hit creates no reaction chain"), Reactions(O).Num(), 5);
    auto* Plain = Enemy(3000); if (!Plain) return false; auto* P = Observe(Plain);
    Earn(P, EBreakerElement::Entropy); P->Combat->ReceiveDamage(Request(Ally, 1, EBreakerElement::Rift));
    TestEqual(TEXT("Respec removes future seed production"), P->Status->GetVoidBuildup(), 0.f);
    if (!Buy(TEXT("Core.Reaction.Catalysis")) || !Buy(TEXT("Core.Reaction.Feedback")) || !Buy(TEXT("Core.Reaction.Overlap"))) return false;
    auto* WitherTarget = Enemy(5000); auto* TearTarget = Enemy(7000);
    if (!WitherTarget || !TearTarget) return false;
    auto* W = Observe(WitherTarget); auto* T = Observe(TearTarget);
    Earn(W, EBreakerElement::Entropy); W->Combat->ReceiveDamage(Request(Ally, 1, EBreakerElement::Void));
    TestEqual(TEXT("Wither seeds Rift"), W->Status->GetRiftBuildup(), Fraction * W->Status->GetRiftThreshold(), .001f);
    Earn(T, EBreakerElement::Void); T->Combat->ReceiveDamage(Request(Ally, 1, EBreakerElement::Rift));
    TestEqual(TEXT("Tear seeds Entropy without resistance scaling"), T->Status->GetEntropyBuildup(), Fraction * T->Status->GetEntropyThreshold(), .001f);
    // The real full major enables O231. Seed alone never enters its funded
    // auto-replay bank, while the next accepted hit can fund that bank.
    if (!PaidRespec()) return false;
    for (const UBreakerProgressionNode* Node : Tree->Nodes)
        if (Node->Constellation == FName(TEXT("Reaction")))
            if (!Buy(*Node->NodeId.ToString(), Node->MaxRank)) return false;
    auto* LockedTarget = Enemy(9000); if (!LockedTarget) return false; auto* L = Observe(LockedTarget);
    Earn(L, EBreakerElement::Entropy); L->Combat->ReceiveDamage(Request(Ally, 1, EBreakerElement::Rift));
    bool Found = false; L->Status->ConsumeStatus(Rot, Found);
    L->Status->AdvanceStatuses(3.f);
    TestFalse(TEXT("Sympathetic unlock cannot auto-apply seed-only buildup"), L->Status->HasStatus(Erased));
    TestEqual(TEXT("Unfunded seed survives unlock under its original timeout"), L->Status->GetVoidBuildup(), Fraction * L->Status->GetVoidThreshold(), .001f);
    L->Status->AdvanceStatuses(1.01f);
    TestEqual(TEXT("Unclaimed seed expires through ordinary buildup timeout"), L->Status->GetVoidBuildup(), 0.f);
    auto* FundedTarget = Enemy(11000); if (!FundedTarget) return false; auto* F = Observe(FundedTarget);
    Earn(F, EBreakerElement::Entropy); F->Combat->ReceiveDamage(Request(Ally, 1, EBreakerElement::Rift));
    F->Status->ConsumeStatus(Rot, Found); F->Combat->RestoreVitals();
    const auto FundedHit = Request(Player, ActivationRaw(Player, F->Status, EBreakerElement::Void) * .8f, EBreakerElement::Void);
    const auto FundedResult = F->Combat->ReceiveDamage(FundedHit);
    TestFalse(TEXT("Funded crossing obeys active Sympathetic lock"), F->Status->HasStatus(Erased));
    F->Status->AdvanceStatuses(3.f);
    const auto* FundedStatus = F->Status->GetActiveStatuses().FindByPredicate([&](const auto& A) { return A.Spec.StatusTag == Erased; });
    if (!TestNotNull(TEXT("Later direct hit plus seed funds deferred ordinary application"), FundedStatus)) return false;
    TestEqual(TEXT("Deferred seed transfers no reaction damage"), FundedStatus->InitialDamageBudget,
        BreakerElementSource::StatusBudget(FundedHit, BreakerElementSource::RawPart(FundedHit, FundedResult) * BreakerVoid::DamageFraction()), .001f);
    return true;
}
#endif
