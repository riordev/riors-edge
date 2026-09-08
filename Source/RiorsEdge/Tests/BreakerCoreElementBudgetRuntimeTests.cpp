#include "Tests/BreakerReactionRuntimeObserver.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Attributes/BreakerAttributeAggregation.h"
#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerDamageLibrary.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerClassDefinition.h"
#include "Progression/BreakerProgressionTree.h"
#include "Progression/BreakerProgressionNode.h"
#include "Progression/BreakerExperience.h"
#include "Progression/BreakerProgressionLibrary.h"
#include "Save/BreakerMissionContent.h"
#include "Components/SphereComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerCoreElementBudgetRuntimeTest, "RiorsEdge.Combat.Elements.CoreBudgetRuntime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerCoreElementBudgetRuntimeTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Init;
    Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!TestNotNull(TEXT("isolated budget world"), World)) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL());
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
    auto* Player = World->SpawnActor<ABreakerCharacter>();
    auto* Target = World->SpawnActor<AActor>();
    auto* Reactor = World->SpawnActor<AActor>();
    if (!Player || !Target || !Reactor) return false;
    auto* ASC = Player->GetAbilitySystemComponent();
    ASC->InitAbilityActorInfo(Player, Player); ASC->AddAttributeSetSubobject(Player->GetAttributes());
    Player->GetProgression()->BindAttributes(Player->GetAttributes());
    Player->GetCombat()->BindAttributes(Player->GetAttributes());
    if (!TestTrue(TEXT("real Caster selection"), Player->GetProgression()->ChoosePermanentClassById(EBreakerClassId::Caster))) return false;
    auto* Definition = DuplicateObject<UBreakerClassDefinition>(Player->GetProgression()->ClassDefinition, Player);
    auto* Tree = NewObject<UBreakerProgressionTree>(Definition);
    Tree->TreeId = TEXT("Test.Core.ElementBudgets"); Tree->Currency = EBreakerPointCurrency::CorePoints;
    auto* Node = NewObject<UBreakerProgressionNode>(Tree); Node->NodeId = TEXT("Test.Core.ElementBudgets.Lanes");
    const TPair<EBreakerNodeStatTarget, float> Lines[] = {
        { EBreakerNodeStatTarget::Damage, 100 }, { EBreakerNodeStatTarget::ElementalDamage, 50 },
        { EBreakerNodeStatTarget::RotDamage, 30 }, { EBreakerNodeStatTarget::VoidBurstDamage, 40 },
        { EBreakerNodeStatTarget::RiftBurstDamage, 60 }, { EBreakerNodeStatTarget::ReactionDamage, 20 } };
    for (const auto& Line : Lines)
    {
        FBreakerNodeEffect Effect; Effect.StatTarget = Line.Key;
        Effect.StatBucket = EBreakerNodeStatBucket::IncreasedPercent; Effect.ValuePerRank = Line.Value;
        Node->Effects.Add(Effect);
    }
    Tree->Nodes.Add(Node); Definition->BranchTrees.Add(Tree); Player->GetProgression()->ClassDefinition = Definition;
    Player->GetProgression()->AwardExperience(UBreakerExperienceLibrary::TotalXpToReachLevel(2, Player->GetProgression()->ExperienceCurve));
    FText Reason;
    if (!TestTrue(TEXT("actual level-earned schema purchase"), Player->GetProgression()->PurchaseNode(Tree, Node->NodeId, Reason))) return false;
    auto* Combat = NewObject<UBreakerCombatComponent>(Target); Target->AddInstanceComponent(Combat); Combat->RegisterComponent();
    auto* Health = NewObject<UBreakerAttributeSet>(Target); Health->ApplyMaxHealth(1000); Health->ApplyHealth(1000); Combat->BindAttributes(Health);
    auto* Status = NewObject<UBreakerStatusComponent>(Target); Target->AddInstanceComponent(Status); Status->RegisterComponent();
    Status->SetComponentTickEnabled(false);
    auto* Observer = NewObject<UBreakerReactionRuntimeObserver>(Target); Observer->Combat = Combat; Observer->Status = Status; Observer->Reactor = Reactor;
    Combat->OnDamageTaken.AddDynamic(Observer, &UBreakerReactionRuntimeObserver::OnHit);
    Status->OnStatusConsumed.AddDynamic(Observer, &UBreakerReactionRuntimeObserver::OnConsumed);
    auto Request = [&](EBreakerElement Element)
    {
        FBreakerDamageRequest Hit; Hit.BaseDamage = 100; Hit.Element = Element; Hit.ElementalFraction = 1;
        Hit.DamageFamily = EBreakerDamageFamily::Elemental; Hit.bCanCritical = false; Hit.SetInstigator(Player);
        UBreakerDamageLibrary::FillSourcePools(Player->GetAttributes(), EBreakerDamageDelivery::Ability, Hit);
        return Hit;
    };
    auto Reset = [&]() { Status->ConsumeAllStatuses(); Status->AdvanceStatuses(20); Combat->RestoreVitals(); Observer->Hits.Reset(); };
    auto Trigger = [&](EBreakerElement Element)
    {
        FBreakerDamageRequest Hit; Hit.BaseDamage = 1; Hit.Element = Element; Hit.ElementalFraction = 1;
        Hit.DamageFamily = EBreakerDamageFamily::Elemental; Hit.bCanCritical = false; Hit.SetInstigator(Reactor);
        return Combat->ReceiveDamage(Hit);
    };
    const FGameplayTag Rot = FGameplayTag::RequestGameplayTag(TEXT("Status.Rot"));
    const FGameplayTag Erased = FGameplayTag::RequestGameplayTag(TEXT("Status.Erased"));
    // One legally spent Core point also earns the existing +0.25% baseline.
    for (const auto Element : { EBreakerElement::Entropy, EBreakerElement::Void, EBreakerElement::Rift })
    {
        Reset();
        const auto Result = Combat->ReceiveDamage(Request(Element));
        TestEqual(TEXT("direct elemental damage shares the delivery Increased bucket"), Result.RawDamage, 250.25f, .001f);
        const float ExpectedBudget = Element == EBreakerElement::Entropy ? 140.125f : Element == EBreakerElement::Void ? 145.125f : 155.125f;
        if (Element == EBreakerElement::Rift)
        {
            TestEqual(TEXT("Rift pays only its specifically composed burst"), Health->GetHealth(), 1000 - 250.25f - ExpectedBudget, .001f);
            continue;
        }
        if (!TestEqual(TEXT("one status earned from actual damage"), Status->GetActiveStatuses().Num(), 1)) return false;
        const auto Active = Status->GetActiveStatuses()[0];
        TestEqual(TEXT("specific status Increased joins rather than multiplies"), Active.UnpaidDamageBudget, ExpectedBudget, .001f);
        TestTrue(TEXT("reaction credit funded at application"), Active.bHasReactionCreditSnapshot);
        TestEqual(TEXT("reaction Increased joins same original bucket"), Active.InitialReactionBudget, ExpectedBudget + 10, .001f);
        Status->AdvanceStatuses(Element == EBreakerElement::Entropy ? 4.0f : 2.0f);
        TestEqual(TEXT("native periodic or delayed delivery consumes its own budget once"), Health->GetHealth(), 1000 - 250.25f - ExpectedBudget, .001f);
    }
    Reset();
    const FBreakerDamageRequest Emitted = Request(EBreakerElement::Entropy);
    Combat->ReceiveDamage(Emitted);
    Status->AdvanceStatuses(1); // Two of eight normal payments have been claimed.
    if (!TestTrue(TEXT("real low-level respec after application"), Player->GetProgression()->RespecCore(Reason))) return false;
    Observer->Hits.Reset();
    Observer->bReenterOnConsume = true;
    Trigger(EBreakerElement::Rift);
    const auto* Reaction = Observer->Hits.FindByPredicate([](const FBreakerHitContext& Hit)
    { return Hit.DamageTypeTag == FGameplayTag::RequestGameplayTag(TEXT("Reaction.Collapse")); });
    if (!TestNotNull(TEXT("foreign source triggers one Collapse"), Reaction)) return false;
    TestEqual(TEXT("partial reaction inherits original credit across respec"), Reaction->Result.RawDamage, 112.59375f, .001f);
    TestEqual(TEXT("reaction retains original applier"), Reaction->Instigator.Get(), static_cast<AActor*>(Player));
    TestFalse(TEXT("consumed Rot absent"), Status->HasStatus(Rot));
    const float Settled = Health->GetHealth(); Status->AdvanceStatuses(10);
    TestEqual(TEXT("consumption cannot pay either ledger twice"), Health->GetHealth(), Settled);
    Reset(); Combat->ReceiveDamage(Emitted); // Already-emitted source survives respec too.
    Status->ScaleRemainingDurations(.5f);
    Observer->Hits.Reset(); Trigger(EBreakerElement::Void);
    const auto* Wither = Observer->Hits.FindByPredicate([](const FBreakerHitContext& Hit)
    { return Hit.DamageTypeTag == FGameplayTag::RequestGameplayTag(TEXT("Reaction.Wither")); });
    if (!TestNotNull(TEXT("shortened Rot produces Wither"), Wither)) return false;
    TestEqual(TEXT("shortened lifetime reduces reaction credit to scheduled half"), Wither->Result.RawDamage, 75.0625f, .001f);
    Reset();
    const auto Fresh = Request(EBreakerElement::Void); Combat->ReceiveDamage(Fresh);
    if (!TestTrue(TEXT("fresh baseline Void earns Erased"), Status->HasStatus(Erased))) return false;
    Observer->Hits.Reset(); Trigger(EBreakerElement::Rift);
    const auto* Tear = Observer->Hits.FindByPredicate([](const FBreakerHitContext& Hit)
    { return Hit.DamageTypeTag == FGameplayTag::RequestGameplayTag(TEXT("Reaction.Tear")); });
    if (!TestNotNull(TEXT("fresh baseline Tear"), Tear)) return false;
    TestEqual(TEXT("removed bonuses do not enter new credit"), Tear->Result.RawDamage, 50.0f, .001f);
    Reset();
    // Boundary diagnostic for an existing temporary delivery window which has
    // already spent the ceiling; this is not a newly granted player power.
    FBreakerDamageRequest Window = Emitted;
    const float Ceiling = FBreakerAttributeAggregator::ComposedMoreCeiling();
    Window.SourceMoreProduct = Ceiling;
    Window.SourceDamageMultiplier = Window.SourceFlatFactor * (1 + Window.SourceIncreasedPercent / 100) * Ceiling;
    Window.ElementSource.ElementalMoreProduct = 1.22f;
    Window.ElementSource.ReactionMoreProduct = 1.20f;
    Combat->ReceiveDamage(Window);
    if (!TestEqual(TEXT("ceiling diagnostic earns one Rot"), Status->GetActiveStatuses().Num(), 1)) return false;
    TestEqual(TEXT("element scope cannot exceed already spent delivery ceiling"), Status->GetActiveStatuses()[0].UnpaidDamageBudget, 140.125f * Ceiling, .001f);
    TestEqual(TEXT("reaction scope also cannot exceed shared ceiling"), Status->GetActiveStatuses()[0].InitialReactionBudget, 150.125f * Ceiling, .001f);
    Observer->Hits.Reset(); Trigger(EBreakerElement::Rift);
    const auto* Capped = Observer->Hits.FindByPredicate([](const FBreakerHitContext& Hit)
    { return Hit.DamageTypeTag == FGameplayTag::RequestGameplayTag(TEXT("Reaction.Collapse")); });
    if (!TestNotNull(TEXT("capped diagnostic reaction settles"), Capped)) return false;
    TestEqual(TEXT("native reaction pays bounded funded amount"), Capped->Result.RawDamage, 150.125f * Ceiling, .001f);
    Reset();
    // Settle the actual authored campaign entitlement, then buy the real
    // Variance prerequisite and Chain. This isolates propagation, not quest play.
    FBreakerQuestFlagSet Completed;
    for (const auto& Mission : UBreakerMissionLibrary::GetMissions())
        for (const auto& Beat : Mission.Beats)
            for (FName Flag : UBreakerMissionLibrary::BeatCompletionFlags(Beat)) Completed.Add(Flag);
    Player->GetProgression()->SettleDoctrineEntitlement(Completed);
    const auto* Multispell = UBreakerProgressionLibrary::GetCasterMultispellTree();
    for (const TCHAR* Id : { TEXT("Caster.Multispell.Variance"), TEXT("Caster.Multispell.Variance"), TEXT("Caster.Multispell.Chain") })
        if (!TestTrue(TEXT("actual doctrine path to Chain"), Player->GetProgression()->PurchaseNode(Multispell, Id, Reason))) return false;
    AActor* Recipient = World->SpawnActor<AActor>();
    if (!TestNotNull(TEXT("nearby Chain recipient"), Recipient)) return false;
    auto* Body = NewObject<USphereComponent>(Recipient); Recipient->AddInstanceComponent(Body); Recipient->SetRootComponent(Body);
    Body->SetSphereRadius(40); Body->SetCollisionEnabled(ECollisionEnabled::NoCollision); Body->RegisterComponent();
    Recipient->SetActorLocation(FVector(0, 300, 0));
    auto* RecipientCombat = NewObject<UBreakerCombatComponent>(Recipient); Recipient->AddInstanceComponent(RecipientCombat); RecipientCombat->RegisterComponent();
    auto* RecipientHealth = NewObject<UBreakerAttributeSet>(Recipient); RecipientHealth->ApplyMaxHealth(1000); RecipientHealth->ApplyHealth(1000); RecipientCombat->BindAttributes(RecipientHealth);
    auto* RecipientStatus = NewObject<UBreakerStatusComponent>(Recipient); Recipient->AddInstanceComponent(RecipientStatus); RecipientStatus->RegisterComponent();
    RecipientStatus->SetComponentTickEnabled(false);
    auto* RecipientObserver = NewObject<UBreakerReactionRuntimeObserver>(Recipient); RecipientObserver->Combat = RecipientCombat; RecipientObserver->Status = RecipientStatus;
    RecipientCombat->OnDamageTaken.AddDynamic(RecipientObserver, &UBreakerReactionRuntimeObserver::OnHit);
    FBreakerStatusApplicationSpec Poison; Poison.StatusTag = FGameplayTag::RequestGameplayTag(TEXT("Status.Poison"));
    Poison.BaseDamagePerTick = 1; Poison.Duration = 10; Poison.TickInterval = 1;
    Status->ApplyStatus(Poison, EBreakerDamageFamily::Physical, Player);
    Combat->ReceiveDamage(Emitted);
    if (!TestEqual(TEXT("purchased Chain propagates the earned Rot"), RecipientStatus->GetActiveStatuses().Num(), 1)) return false;
    const FBreakerActiveStatus& Copy = RecipientStatus->GetActiveStatuses()[0];
    TestEqual(TEXT("Chain carries only the funded normal budget"), Copy.UnpaidDamageBudget, 140.125f, .001f);
    TestEqual(TEXT("Chain carries original reaction credit without amplification"), Copy.InitialReactionBudget, 150.125f, .001f);
    TestEqual(TEXT("Chain copy cannot spread again"), Copy.Spec.ProcCoefficient, 0.0f);
    RecipientStatus->AdvanceStatuses(1);
    FBreakerDamageRequest CopyTrigger; CopyTrigger.BaseDamage = 1; CopyTrigger.Element = EBreakerElement::Void;
    CopyTrigger.ElementalFraction = 1; CopyTrigger.DamageFamily = EBreakerDamageFamily::Elemental; CopyTrigger.bCanCritical = false; CopyTrigger.SetInstigator(Reactor);
    RecipientCombat->ReceiveDamage(CopyTrigger);
    const auto* CopyReaction = RecipientObserver->Hits.FindByPredicate([](const FBreakerHitContext& Hit)
    { return Hit.DamageTypeTag == FGameplayTag::RequestGameplayTag(TEXT("Reaction.Wither")); });
    if (!TestNotNull(TEXT("actual Chain copy settles its remaining credit"), CopyReaction)) return false;
    TestEqual(TEXT("Chain copy pays only its six unpaid ticks"), CopyReaction->Result.RawDamage, 112.59375f, .001f);
    return true;
}
#endif
