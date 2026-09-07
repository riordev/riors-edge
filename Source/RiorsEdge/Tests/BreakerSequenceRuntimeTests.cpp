#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Classes/BreakerManaComponent.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerStatusComponent.h"
#include "Combat/BreakerStatusRules.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionLibrary.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerSequenceRuntimeTest, "RiorsEdge.Classes.Mana.SequenceRuntime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerSequenceRuntimeTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Init;
    Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!TestNotNull(TEXT("isolated Sequence world"), World)) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL());
    const uint64 InitialFrame = GFrameCounter;
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); GFrameCounter = InitialFrame; };
    ABreakerCharacter* Player = World->SpawnActor<ABreakerCharacter>();
    if (!TestNotNull(TEXT("real Caster without save-loading BeginPlay"), Player)) return false;
    Player->GetBreakerMovement()->SetComponentTickEnabled(false);
    Player->GetAbilitySystemComponent()->InitAbilityActorInfo(Player, Player);
    Player->GetAbilitySystemComponent()->AddAttributeSetSubobject(Player->GetAttributes());
    Player->GetCombat()->BindAttributes(Player->GetAttributes());
    auto* Progression = Player->GetProgression();
    Progression->BindAttributes(Player->GetAttributes());
    FBreakerProgressionState State;
    State.PermanentClass = EBreakerClassId::Caster;
    // Explicit campaign-completed budget fixture, not earned-story proof.
    State.UnspentDoctrinePoints = 8;
    Progression->LoadProgressionState(State);
    auto* Mana = Player->GetMana();
    Mana->BindAttributes(Player->GetAttributes());
    Mana->PassiveRegenPerSecond = 0;
    Mana->SetComponentTickEnabled(false);
    auto Advance = [&](int32 Steps)
    {
        for (int32 I = 0; I < Steps; ++I)
        {
            ++GFrameCounter; World->Tick(LEVELTICK_All, 0.05f);
            Mana->AdvanceLoop(0.05f);
        }
    };
    auto ResetIncome = [&] { Advance(22); Player->GetAttributes()->ApplyClassResource(0); };
    // Ordinary applications queue into the existing per-second meter;
    // Sequence pays immediately. Flush only that real production meter.
    auto SettleOrdinaryIncome = [&] { Mana->AdvanceLoop(1.0f); };
    auto Target = [&]()
    {
        AActor* Actor = World->SpawnActor<AActor>();
        if (!Actor) return static_cast<UBreakerStatusComponent*>(nullptr);
        auto* Combat = NewObject<UBreakerCombatComponent>(Actor);
        Actor->AddInstanceComponent(Combat); Combat->RegisterComponent();
        auto* Health = NewObject<UBreakerAttributeSet>(Actor);
        Health->ApplyMaxHealth(10000); Health->ApplyHealth(10000); Combat->BindAttributes(Health);
        auto* Status = NewObject<UBreakerStatusComponent>(Actor);
        Actor->AddInstanceComponent(Status); Status->RegisterComponent();
        // Sequence tracks world application timestamps; DoT damage/lifetime
        // itself is tested independently and cannot kill these targets here.
        Status->SetComponentTickEnabled(false);
        return Status;
    };
    auto Apply = [&](UBreakerStatusComponent* Status, int32 Type, float Proc = 1.0f)
    {
        if (Type == 2)
        {
            FBreakerDamageRequest Hit;
            Hit.BaseDamage = Status->GetEntropyThreshold() / (Proc > 0.0f ? Proc : 1.0f);
            Hit.Element = EBreakerElement::Entropy; Hit.ElementalFraction = 1.0f;
            Hit.Delivery = EBreakerDamageDelivery::Ability; Hit.ProcCoefficient = Proc;
            Hit.bCanCritical = false; Hit.SetInstigator(Player);
            Status->GetOwner()->FindComponentByClass<UBreakerCombatComponent>()->ReceiveDamage(Hit);
            return;
        }
        FBreakerStatusApplicationSpec Spec;
        if (Type != 2)
        {
            Spec.StatusTag = FGameplayTag::RequestGameplayTag(Type == 0 ? TEXT("Status.Bleed") : TEXT("Status.Poison"));
            Spec.BaseDamagePerTick = 1; Spec.Duration = 20; Spec.TickInterval = 1;
        }
        Spec.ProcCoefficient = Proc;
        Status->ApplyStatus(Spec, Type == 2 ? EBreakerDamageFamily::Elemental : EBreakerDamageFamily::Physical, Player);
    };
    auto Triple = [&](UBreakerStatusComponent* Status, float Proc = 1.0f) { Apply(Status, 0, Proc); Apply(Status, 1, Proc); Apply(Status, 2, Proc); };
    auto Buy = [&](const TCHAR* Id)
    {
        FText Reason;
        return TestTrue(Id, Progression->PurchaseNode(UBreakerProgressionLibrary::GetCasterMultispellTree(), Id, Reason));
    };
    ResetIncome();
    UBreakerStatusComponent* Baseline = Target();
    if (!TestNotNull(TEXT("baseline real target"), Baseline)) return false;
    Triple(Baseline);
    SettleOrdinaryIncome();
    TestEqual(TEXT("three real types without Sequence pay only ordinary capped income"), Mana->GetMana(), 6.0f);
    if (!Buy(TEXT("Caster.Multispell.Reservoir")) || !Buy(TEXT("Caster.Multispell.Reservoir"))
        || !Buy(TEXT("Caster.Multispell.Sequence"))) return false;
    ResetIncome();
    UBreakerStatusComponent* First = Target();
    if (!TestNotNull(TEXT("rank one real target"), First)) return false;
    Apply(First, 0); Apply(First, 0);
    // The same production event also announces unrelated XP/loadout changes;
    // an unchanged Sequence rank must preserve an in-progress rotation.
    Progression->OnProgressionChanged.Broadcast();
    Apply(First, 1);
    SettleOrdinaryIncome();
    TestEqual(TEXT("repeated type cannot masquerade as third distinct application"), Mana->GetMana(), 4.0f);
    Apply(First, 2);
    TestEqual(TEXT("third real type pays rank-one lump immediately"), Mana->GetMana(), 14.0f);
    SettleOrdinaryIncome();
    TestEqual(TEXT("rank one lump sum is visible beyond ordinary six-per-second cap"), Mana->GetMana(), 16.0f);
    Triple(First);
    SettleOrdinaryIncome();
    TestEqual(TEXT("same-target immediate repeat cannot pay twice"), Mana->GetMana(), 16.0f);
    UBreakerStatusComponent* Independent = Target();
    if (!TestNotNull(TEXT("independent target"), Independent)) return false;
    Triple(Independent);
    TestEqual(TEXT("another target owns an independent immediate Sequence cooldown"), Mana->GetMana(), 26.0f);
    SettleOrdinaryIncome();
    TestEqual(TEXT("second target ordinary applications retain their own metered income"), Mana->GetMana(), 32.0f);
    Advance(202);
    First->AdvanceStatuses(4.0f); // Real Rot expires before another threshold application.
    SettleOrdinaryIncome(); // Drain real tick income before measuring the new rotation.
    Player->GetAttributes()->ApplyClassResource(0);
    Triple(First);
    SettleOrdinaryIncome();
    TestEqual(TEXT("physical refresh plus newly earned Rot pays after cooldown"), Mana->GetMana(), 12.0f);
    if (!Buy(TEXT("Caster.Multispell.Sequence"))) return false;
    ResetIncome();
    UBreakerStatusComponent* RankTwo = Target();
    if (!TestNotNull(TEXT("rank two target"), RankTwo)) return false;
    Triple(RankTwo);
    TestEqual(TEXT("rank two lump pays before ordinary queued income"), Mana->GetMana(), 15.0f);
    SettleOrdinaryIncome();
    TestEqual(TEXT("purchased rank two upgrades only lump sum to fifteen"), Mana->GetMana(), 21.0f);
    ResetIncome();
    UBreakerStatusComponent* SplitA = Target(); UBreakerStatusComponent* SplitB = Target();
    if (!TestNotNull(TEXT("split A"), SplitA) || !TestNotNull(TEXT("split B"), SplitB)) return false;
    Apply(SplitA, 0); Apply(SplitA, 1); Apply(SplitB, 2);
    SettleOrdinaryIncome();
    TestEqual(TEXT("types on different targets never complete one rotation"), Mana->GetMana(), 6.0f);
    ResetIncome();
    UBreakerStatusComponent* Slow = Target();
    if (!TestNotNull(TEXT("slow rotation target"), Slow)) return false;
    Apply(Slow, 0); Advance(122); Apply(Slow, 1); Apply(Slow, 2);
    SettleOrdinaryIncome();
    TestEqual(TEXT("world-time window expiry prevents old first type counting"), Mana->GetMana(), 6.0f);
    ResetIncome();
    UBreakerStatusComponent* Echo = Target();
    if (!TestNotNull(TEXT("secondary effect target"), Echo)) return false;
    Triple(Echo, 0);
    SettleOrdinaryIncome();
    TestEqual(TEXT("proc-zero applications cannot earn Rot or grant Sequence income"), Mana->GetMana(), 0.0f);
    ResetIncome();
    UBreakerStatusComponent* ReducedProc = Target();
    if (!TestNotNull(TEXT("partial-proc rotation target"), ReducedProc)) return false;
    Apply(ReducedProc, 0); Apply(ReducedProc, 1, 0.5f); Apply(ReducedProc, 2);
    TestEqual(TEXT("Sequence uses weakest contribution proc across the three types"), Mana->GetMana(), 7.5f);
    SettleOrdinaryIncome();
    TestEqual(TEXT("partial-proc ordinary income remains independent of lump scaling"), Mana->GetMana(), 12.5f);
    ResetIncome();
    UBreakerStatusComponent* DeadTarget = Target();
    if (!TestNotNull(TEXT("corpse application target"), DeadTarget)) return false;
    FBreakerDamageRequest Lethal;
    Lethal.BaseDamage = 100000; Lethal.bCanCritical = false; Lethal.bBypassShield = true;
    auto* DeadCombat = DeadTarget->GetOwner()->FindComponentByClass<UBreakerCombatComponent>();
    DeadCombat->ReceiveDamage(Lethal);
    if (!TestTrue(TEXT("actual lethal damage creates corpse"), DeadCombat->IsDead())) return false;
    Triple(DeadTarget); SettleOrdinaryIncome();
    TestEqual(TEXT("rejected corpse applications cannot complete Sequence"), Mana->GetMana(), 0.0f);
    ResetIncome();
    UBreakerStatusComponent* Suspended = Target();
    if (!TestNotNull(TEXT("suspended target"), Suspended)) return false;
    Mana->PushGenerationSuspension(TEXT("Sequence.Test"));
    Triple(Suspended);
    SettleOrdinaryIncome();
    TestEqual(TEXT("generation suspension blocks the uncapped node payoff too"), Mana->GetMana(), 0.0f);
    Mana->PopGenerationSuspension(TEXT("Sequence.Test"));
    FText Reason;
    if (!TestTrue(TEXT("actual Forge respec removes Sequence"), Progression->RespecAtForge(EBreakerPointCurrency::DoctrinePoints, true, Reason))) return false;
    ResetIncome();
    UBreakerStatusComponent* Respecced = Target();
    if (!TestNotNull(TEXT("respecced target"), Respecced)) return false;
    Triple(Respecced);
    SettleOrdinaryIncome();
    TestEqual(TEXT("removed node leaves baseline income only"), Mana->GetMana(), 6.0f);
    if (!Buy(TEXT("Caster.Multispell.Reservoir")) || !Buy(TEXT("Caster.Multispell.Reservoir"))
        || !Buy(TEXT("Caster.Multispell.Sequence"))) return false;
    ResetIncome();
    UBreakerStatusComponent* CorpseSource = Target();
    if (!TestNotNull(TEXT("dead applier target"), CorpseSource)) return false;
    Player->GetAttributes()->ApplyHealth(0);
    Triple(CorpseSource);
    SettleOrdinaryIncome();
    TestEqual(TEXT("dead caster receives no status or Sequence generation"), Mana->GetMana(), 0.0f);
    return true;
}
#endif
