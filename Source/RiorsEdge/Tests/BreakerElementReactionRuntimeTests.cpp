#include "Tests/BreakerReactionRuntimeObserver.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Attributes/BreakerAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerCombatComponent.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionLibrary.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

namespace BreakerReactionFixture
{
FGameplayTag Tag(const TCHAR* Name) { return FGameplayTag::RequestGameplayTag(FName(Name)); }
FBreakerDamageRequest Request(AActor* Source, EBreakerElement Element, float Damage)
{
    FBreakerDamageRequest Value;
    Value.BaseDamage = Damage;
    Value.DamageFamily = EBreakerDamageFamily::Elemental;
    Value.Element = Element; Value.ElementalFraction = 1;
    Value.bCanCritical = false; Value.SetInstigator(Source);
    return Value;
}
}

void UBreakerReactionRuntimeObserver::OnHit(const FBreakerHitContext& Hit)
{
    Hits.Add(Hit);
    if (bAdvanceOnRotTick && Hit.DamageTypeTag == BreakerReactionFixture::Tag(TEXT("Status.Rot")))
    {
        bAdvanceOnRotTick = false;
        Status->AdvanceStatuses(1);
    }
    if (bReactOnRotTick && Hit.DamageTypeTag == BreakerReactionFixture::Tag(TEXT("Status.Rot")))
    {
        bReactOnRotTick = false;
        Combat->ReceiveDamage(BreakerReactionFixture::Request(Reactor, EBreakerElement::Rift, 1));
    }
    if (bKillOnOuterHit && Status->IsElementTransactionActive() && !Hit.bFromDoT)
    {
        bKillOnOuterHit = false;
        Combat->ReceiveDamage(BreakerReactionFixture::Request(Reactor, EBreakerElement::None, 10000));
    }
}

void UBreakerReactionRuntimeObserver::OnConsumed(const FBreakerActiveStatus& Active)
{
    ++Consumed;
    if (!bReenterOnConsume) return;
    bReenterOnConsume = false;
    // Real nested hits must not create a fresh Rot or react with a second mark.
    Combat->ReceiveDamage(BreakerReactionFixture::Request(Reactor, EBreakerElement::Entropy, 100));
    Combat->ReceiveDamage(BreakerReactionFixture::Request(Reactor, EBreakerElement::Rift, 1));
}

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerElementReactionRuntimeTest, "RiorsEdge.Combat.Elements.ReactionMatrix",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerElementReactionRuntimeTest::RunTest(const FString& Parameters)
{
    using namespace BreakerReactionFixture;
    UWorld::InitializationValues Init;
    Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!World) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL());
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
    auto* Applier = World->SpawnActor<AActor>();
    auto* Reactor = World->SpawnActor<AActor>();
    auto* Target = World->SpawnActor<AActor>();
    if (!Applier || !Reactor || !Target) return false;
    auto* Combat = NewObject<UBreakerCombatComponent>(Target);
    Target->AddInstanceComponent(Combat); Combat->RegisterComponent();
    auto* Health = NewObject<UBreakerAttributeSet>(Target);
    Health->ApplyMaxHealth(1000); Health->ApplyHealth(1000); Combat->BindAttributes(Health);
    auto* Status = NewObject<UBreakerStatusComponent>(Target);
    Target->AddInstanceComponent(Status); Status->RegisterComponent(); Status->SetComponentTickEnabled(false);
    auto* Observer = NewObject<UBreakerReactionRuntimeObserver>(Target);
    Observer->Combat = Combat; Observer->Status = Status; Observer->Reactor = Reactor;
    Combat->OnDamageTaken.AddDynamic(Observer, &UBreakerReactionRuntimeObserver::OnHit);
    Status->OnStatusConsumed.AddDynamic(Observer, &UBreakerReactionRuntimeObserver::OnConsumed);
    const auto Rot = Tag(TEXT("Status.Rot")), Erased = Tag(TEXT("Status.Erased"));
    const auto Collapse = Tag(TEXT("Reaction.Collapse")), Wither = Tag(TEXT("Reaction.Wither")), Tear = Tag(TEXT("Reaction.Tear"));
    auto Reset = [&]()
    {
        Observer->bAdvanceOnRotTick = Observer->bReactOnRotTick = Observer->bReenterOnConsume = Observer->bKillOnOuterHit = false;
        Status->ConsumeAllStatuses(); Status->AdvanceStatuses(20); Combat->RestoreVitals();
        Combat->DodgeChance = 0; Observer->Hits.Reset(); Observer->Consumed = 0;
    };
    auto Earn = [&](EBreakerElement Element)
    {
        Combat->ReceiveDamage(Request(Applier, Element, 100));
        TestTrue(TEXT("actual accepted hit earns requested status"), Status->HasStatus(Element == EBreakerElement::Entropy ? Rot : Erased));
        Observer->Hits.Reset(); Observer->Consumed = 0;
    };
    auto Reactions = [&]()
    {
        TArray<FBreakerHitContext> Found;
        for (const auto& Hit : Observer->Hits)
            if (Hit.DamageTypeTag == Collapse || Hit.DamageTypeTag == Wither || Hit.DamageTypeTag == Tear) Found.Add(Hit);
        return Found;
    };
    for (const auto Incoming : { EBreakerElement::Rift, EBreakerElement::Void })
        for (const int32 PaidTicks : { 0, 1, 7 })
        {
            Reset(); Earn(EBreakerElement::Entropy);
            if (PaidTicks > 0) Status->AdvanceStatuses(PaidTicks * .5f);
            Observer->Hits.Reset();
            const float Before = Health->GetHealth();
            Combat->ReceiveDamage(Request(Reactor, Incoming, 1));
            const auto Found = Reactions();
            if (!TestEqual(TEXT("one ordered Rot reaction"), Found.Num(), 1)) return false;
            TestEqual(TEXT("correct reaction pair"), Found[0].DamageTypeTag, Incoming == EBreakerElement::Rift ? Collapse : Wither);
            TestEqual(TEXT("only unpaid ticks are paid"), Found[0].Result.HealthDamage, (8 - PaidTicks) * 6.25f, .001f);
            TestEqual(TEXT("consumed snapshot retains original applier"), Found[0].Instigator.Get(), Applier);
            TestEqual(TEXT("reaction procs nothing"), Found[0].ProcCoefficient, 0.0f);
            TestFalse(TEXT("reaction cannot crit"), Found[0].Result.bCritical);
            TestEqual(TEXT("outer event precedes reaction"), Observer->Hits[0].Result.RemainingHealth, Before - 1, .001f);
            TestFalse(TEXT("Rot is consumed"), Status->HasStatus(Rot));
            TestEqual(TEXT("trigger adds no Rift buildup"), Status->GetRiftBuildup(), 0.0f);
            TestEqual(TEXT("trigger adds no Void buildup"), Status->GetVoidBuildup(), 0.0f);
            const float Settled = Health->GetHealth(); Status->AdvanceStatuses(10);
            TestEqual(TEXT("consumed ticks never pay later"), Health->GetHealth(), Settled);
        }
    Reset(); Earn(EBreakerElement::Void);
    Combat->ReceiveDamage(Request(Reactor, EBreakerElement::Rift, 1));
    if (!TestEqual(TEXT("Erased earns one Tear"), Reactions().Num(), 1)) return false;
    TestEqual(TEXT("Tear spends delayed budget"), Reactions()[0].Result.HealthDamage, 50.0f);
    TestEqual(TEXT("Tear provenance"), Reactions()[0].DamageTypeTag, Tear);
    const float Torn = Health->GetHealth(); Status->AdvanceStatuses(10);
    TestEqual(TEXT("consumed Erased never pays expiry"), Health->GetHealth(), Torn);
    Reset(); Earn(EBreakerElement::Void); Status->AdvanceStatuses(2); Observer->Hits.Reset();
    Combat->ReceiveDamage(Request(Reactor, EBreakerElement::Rift, 1));
    TestEqual(TEXT("already paid Erased cannot react"), Reactions().Num(), 0);
    Reset(); Earn(EBreakerElement::Entropy); Status->AdvanceStatuses(4); Observer->Hits.Reset();
    Combat->ReceiveDamage(Request(Reactor, EBreakerElement::Void, 1));
    TestEqual(TEXT("already paid Rot cannot react"), Reactions().Num(), 0);
    for (const auto Incoming : { EBreakerElement::Entropy, EBreakerElement::Void, EBreakerElement::None })
    {
        Reset(); Earn(EBreakerElement::Void);
        Combat->ReceiveDamage(Request(Reactor, Incoming, 1));
        TestEqual(TEXT("reverse and unpaired hits do not react"), Reactions().Num(), 0);
        TestTrue(TEXT("unpaired hit preserves Erased"), Status->HasStatus(Erased));
    }
    Reset(); Earn(EBreakerElement::Void); Earn(EBreakerElement::Entropy);
    TestTrue(TEXT("opposite order can hold both earned statuses"), Status->HasStatus(Rot) && Status->HasStatus(Erased));
    Observer->bReenterOnConsume = true;
    Combat->ReceiveDamage(Request(Reactor, EBreakerElement::Rift, 1));
    if (!TestEqual(TEXT("overlap and consume callback create only one reaction"), Reactions().Num(), 1)) return false;
    TestEqual(TEXT("Rot wins authored priority"), Reactions()[0].DamageTypeTag, Collapse);
    TestTrue(TEXT("second status remains unpaid"), Status->HasStatus(Erased));
    TestFalse(TEXT("consume callback cannot reapply Rot"), Status->HasStatus(Rot));
    TestEqual(TEXT("nested hits cannot build fresh Entropy"), Status->GetEntropyBuildup(), 0.0f);
    Reset(); Earn(EBreakerElement::Entropy); Observer->bReactOnRotTick = true;
    Status->AdvanceStatuses(4);
    if (!TestEqual(TEXT("hitch tick callback reacts exactly once"), Reactions().Num(), 1)) return false;
    TestEqual(TEXT("first paid tick leaves all seven earned ticks"), Reactions()[0].Result.HealthDamage, 43.75f);
    TestEqual(TEXT("hitch total includes tick, trigger and remaining budget"), Health->GetHealth(), 849.0f);
    Reset(); Earn(EBreakerElement::Entropy); Status->AdvanceStatuses(.25f); Status->ScaleRemainingDurations(.5f);
    Combat->ReceiveDamage(Request(Reactor, EBreakerElement::Rift, 1));
    if (!TestEqual(TEXT("shortened off-cadence Rot reacts"), Reactions().Num(), 1)) return false;
    TestEqual(TEXT("half deadline retains four scheduled ticks"), Reactions()[0].Result.HealthDamage, 25.0f);
    Reset(); Earn(EBreakerElement::Entropy); Status->AdvanceStatuses(.25f);
    Status->ScaleRemainingDurations(.5f); Status->ScaleRemainingDurations(2);
    Combat->ReceiveDamage(Request(Reactor, EBreakerElement::Rift, 1));
    if (!TestEqual(TEXT("extended shortened Rot still reacts once"), Reactions().Num(), 1)) return false;
    TestEqual(TEXT("extension cannot restore canceled damage"), Reactions()[0].Result.HealthDamage, 25.0f);
    Reset(); Earn(EBreakerElement::Entropy); Observer->bAdvanceOnRotTick = true;
    Status->AdvanceStatuses(.5f);
    TestEqual(TEXT("nested clock advance cannot pay extra ticks"), Health->GetHealth(), 893.75f);
    if (!TestEqual(TEXT("Rot survives first tick"), Status->GetActiveStatuses().Num(), 1)) return false;
    TestEqual(TEXT("nested advance cannot double-advance lifetime"), Status->GetActiveStatuses()[0].RemainingDuration, 3.5f);
    for (int32 Guard = 0; Guard < 7; ++Guard)
    {
        Reset(); Earn(EBreakerElement::Entropy);
        auto Trigger = Request(Reactor, EBreakerElement::Rift, 1);
        if (Guard == 0) Trigger.BaseDamage = 0;
        if (Guard == 1) Trigger.ProcCoefficient = 0;
        if (Guard == 2) Trigger.bIsDamageOverTime = true;
        if (Guard == 3) Trigger.ElementalFraction = 0;
        if (Guard == 4) Combat->DodgeChance = 1;
        if (Guard == 5) Status->GrantStatusImmunity(1);
        if (Guard == 6) Trigger.BaseDamage = 10000;
        Combat->ReceiveDamage(Trigger);
        TestEqual(TEXT("invalid/avoided/lethal trigger cannot react"), Reactions().Num(), 0);
        if (Guard != 6) TestTrue(TEXT("refused trigger preserves earned Rot"), Status->HasStatus(Rot));
    }
    Reset(); Earn(EBreakerElement::Entropy);
    Combat->PushIncomingDamageModifier(TEXT("ReactionFixture.Zero"), 0);
    TestEqual(TEXT("actual zero mitigation guard"), Combat->ReceiveDamage(Request(Reactor, EBreakerElement::Rift, 1)).HealthDamage, 0.0f);
    TestEqual(TEXT("fully mitigated hit cannot consume budget"), Reactions().Num(), 0);
    TestTrue(TEXT("fully mitigated hit preserves Rot"), Status->HasStatus(Rot));
    Combat->RemoveIncomingDamageModifier(TEXT("ReactionFixture.Zero"));
    Reset(); Earn(EBreakerElement::Entropy); Observer->bKillOnOuterHit = true;
    Combat->ReceiveDamage(Request(Reactor, EBreakerElement::Rift, 1));
    TestTrue(TEXT("outer callback actually kills"), Combat->IsDead());
    TestEqual(TEXT("callback death cancels pending reaction"), Reactions().Num(), 0);

    // Separate native player fixture pays the real prerequisite path for parry.
    // Only components begin; Character BeginPlay would load owner saves.
    auto* Player = World->SpawnActor<ABreakerCharacter>(FVector(500, 0, 0), FRotator::ZeroRotator);
    if (!Player) return false;
    auto* ASC = Player->GetAbilitySystemComponent();
    ASC->InitAbilityActorInfo(Player, Player); ASC->AddAttributeSetSubobject(Player->GetAttributes());
    Player->GetCombat()->BindAttributes(Player->GetAttributes());
    Player->GetProgression()->BindAttributes(Player->GetAttributes());
    Player->GetCombat()->BeginPlay();
    auto* PlayerStatus = Player->FindComponentByClass<UBreakerStatusComponent>();
    if (!PlayerStatus) return false;
    auto* Progression = Player->GetProgression();
    Progression->AwardExperience(UBreakerExperienceLibrary::TotalXpToReachLevel(4, Progression->ExperienceCurve));
    FText Failure;
    for (const TCHAR* Node : {TEXT("Core.Bulwark.Read"), TEXT("Core.Bulwark.Guard"), TEXT("Core.Bulwark.Parry")})
        if (!TestTrue(TEXT("purchases ordinary parry prerequisites"), Progression->PurchaseNode(UBreakerProgressionLibrary::GetCoreSliceTree(), Node, Failure))) return false;
    Player->GetCombat()->ReceiveDamage(Request(Applier, EBreakerElement::Entropy, PlayerStatus->GetEntropyThreshold()));
    if (!TestTrue(TEXT("parry target owns actually earned Rot"), PlayerStatus->HasStatus(Rot))) return false;
    if (!TestTrue(TEXT("purchased parry opens"), Player->GetCombat()->TryParry())) return false;
    auto Frontal = Request(Reactor, EBreakerElement::Rift, 1);
    Frontal.bHasSourceLocation = true; Frontal.SourceLocation = Player->GetActorLocation() + FVector(100, 0, 0);
    TestTrue(TEXT("actual frontal reaction trigger is parried"), Player->GetCombat()->ReceiveDamage(Frontal).bParried);
    TestTrue(TEXT("parry preserves unpaid Rot"), PlayerStatus->HasStatus(Rot));
    return true;
}
#endif
