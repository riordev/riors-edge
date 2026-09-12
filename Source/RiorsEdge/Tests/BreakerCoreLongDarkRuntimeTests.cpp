#include "Tests/BreakerLongDarkRuntimeObserver.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerDamageLibrary.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerClassDefinition.h"
#include "Progression/BreakerProgressionTree.h"
#include "Progression/BreakerProgressionNode.h"
#include "Progression/BreakerProgressionLibrary.h"
#include "Progression/BreakerExperience.h"
#include "Save/BreakerMissionContent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Components/SceneComponent.h"

void UBreakerLongDarkRuntimeObserver::OnConsumed(const FBreakerActiveStatus& Status)
{
    ++Consumed;
    if (KillTarget)
    {
        auto* Victim = KillTarget.Get(); KillTarget = nullptr;
        FBreakerDamageRequest Kill; Kill.BaseDamage = 10000; Kill.DamageFamily = EBreakerDamageFamily::TrueDamage; Kill.bCanCritical = false;
        Victim->ReceiveDamage(Kill);
    }
    if (DestroyTarget) { auto* Victim = DestroyTarget.Get(); DestroyTarget = nullptr; Victim->Destroy(); }
    if (bReenter && ReentryTarget)
    {
        bReenter = false;
        ReentryTarget->ReceiveDamage(ReentryHit);
    }
}

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerCoreLongDarkRuntimeTest, "RiorsEdge.Combat.Entropy.LongDarkLeaseRuntime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerCoreLongDarkRuntimeTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Init;
    Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!TestNotNull(TEXT("isolated Long Dark world"), World)) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL());
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
    auto* Player = World->SpawnActor<ABreakerCharacter>();
    if (!Player) return false;
    auto* ASC = Player->GetAbilitySystemComponent(); ASC->InitAbilityActorInfo(Player, Player); ASC->AddAttributeSetSubobject(Player->GetAttributes());
    auto* Progression = Player->GetProgression(); Progression->BindAttributes(Player->GetAttributes()); Player->GetCombat()->BindAttributes(Player->GetAttributes());
    if (!TestTrue(TEXT("real Caster selection"), Progression->ChoosePermanentClassById(EBreakerClassId::Caster))) return false;
    auto* Definition = DuplicateObject<UBreakerClassDefinition>(Progression->ClassDefinition, Player);
    auto* Tree = NewObject<UBreakerProgressionTree>(Definition); Tree->TreeId = TEXT("Test.Core.LongDark"); Tree->Currency = EBreakerPointCurrency::CorePoints;
    auto* Node = NewObject<UBreakerProgressionNode>(Tree); Node->NodeId = TEXT("Test.Core.LongDark.Rule");
    Node->GrantedTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Progression.Node.Core.LongDark")));
    Tree->Nodes.Add(Node); Definition->BranchTrees.Add(Tree); Progression->ClassDefinition = Definition;
    Progression->AwardExperience(UBreakerExperienceLibrary::TotalXpToReachLevel(2, Progression->ExperienceCurve));
    struct FTarget
    {
        AActor* Actor; UBreakerCombatComponent* Combat; UBreakerStatusComponent* Status; UBreakerAttributeSet* Health;
    };
    int32 TargetIndex = 0;
    auto MakeTarget = [&]()
    {
        FTarget Target; Target.Actor = World->SpawnActor<AActor>();
        auto* Root = NewObject<USceneComponent>(Target.Actor); Target.Actor->AddInstanceComponent(Root); Target.Actor->SetRootComponent(Root); Root->RegisterComponent();
        Target.Actor->SetActorLocation(FVector(500 + 100 * TargetIndex++, 0, 0));
        Target.Combat = NewObject<UBreakerCombatComponent>(Target.Actor); Target.Actor->AddInstanceComponent(Target.Combat); Target.Combat->RegisterComponent();
        Target.Health = NewObject<UBreakerAttributeSet>(Target.Actor); Target.Health->ApplyMaxHealth(1000); Target.Health->ApplyHealth(1000); Target.Combat->BindAttributes(Target.Health);
        Target.Status = NewObject<UBreakerStatusComponent>(Target.Actor); Target.Actor->AddInstanceComponent(Target.Status); Target.Status->RegisterComponent(); Target.Status->SetComponentTickEnabled(false);
        return Target;
    };
    const FTarget A = MakeTarget(), B = MakeTarget(), C = MakeTarget();
    const auto Rot = FGameplayTag::RequestGameplayTag(TEXT("Status.Rot"));
    auto Emit = [&]()
    {
        FBreakerDamageRequest Hit; Hit.BaseDamage = 100; Hit.Element = EBreakerElement::Entropy; Hit.ElementalFraction = 1;
        Hit.DamageFamily = EBreakerDamageFamily::Elemental; Hit.bCanCritical = false; Hit.SetInstigator(Player);
        UBreakerDamageLibrary::FillSourcePools(Player->GetAttributes(), EBreakerDamageDelivery::Ability, Hit); return Hit;
    };
    A.Combat->ReceiveDamage(Emit()); A.Status->AdvanceStatuses(8);
    TestFalse(TEXT("ordinary Rot still expires at its finite duration"), A.Status->HasStatus(Rot));
    TestEqual(TEXT("ordinary Rot pays only eight baseline ticks"), A.Health->GetHealth(), 850.0f, .001f);
    A.Combat->RestoreVitals();
    FText Reason;
    if (!TestTrue(TEXT("level-earned Long Dark schema purchase"), Progression->PurchaseNode(Tree, Node->NodeId, Reason))) return false;
    const FBreakerDamageRequest Hit = Emit();
    A.Combat->ReceiveDamage(Hit);
    if (!TestEqual(TEXT("one persistent Rot earned"), A.Status->GetActiveStatuses().Num(), 1)) return false;
    TestTrue(TEXT("owned application has persistent lease"), A.Status->GetActiveStatuses()[0].bPersistentRot);
    A.Status->AdvanceStatuses(8);
    if (!TestEqual(TEXT("lease remains after twice ordinary duration"), A.Status->GetActiveStatuses().Num(), 1)) return false;
    TestEqual(TEXT("persistent snapshot stops after its eight funded ticks including point floor"), A.Health->GetHealth(), 849.625f, .001f);
    TestEqual(TEXT("funding clock reaches zero without a sentinel"), A.Status->GetActiveStatuses()[0].RemainingDuration, 0.0f);
    TestEqual(TEXT("normal budget never replenished by persistence"), A.Status->GetActiveStatuses()[0].UnpaidDamageBudget, 0.0f, .001f);
    FBreakerDamageRequest Trigger; Trigger.BaseDamage = 1; Trigger.Element = EBreakerElement::Rift; Trigger.ElementalFraction = 1;
    Trigger.DamageFamily = EBreakerDamageFamily::Elemental; Trigger.bCanCritical = false;
    const float BeforeReaction = A.Health->GetHealth(); A.Combat->ReceiveDamage(Trigger);
    TestFalse(TEXT("late reaction still consumes zero-credit persistent Rot"), A.Status->HasStatus(Rot));
    TestEqual(TEXT("late reaction cannot invent more credit"), A.Health->GetHealth(), BeforeReaction - 1, .001f);
    A.Status->AdvanceStatuses(4); TestEqual(TEXT("consumed persistent ticks stay stopped"), A.Health->GetHealth(), BeforeReaction - 1, .001f);

    A.Combat->RestoreVitals(); A.Combat->ReceiveDamage(Hit); A.Status->AdvanceStatuses(1);
    auto* Observer = NewObject<UBreakerLongDarkRuntimeObserver>(Player); Observer->ReentryTarget = C.Combat; Observer->ReentryHit = Hit; Observer->bReenter = true;
    A.Status->OnStatusConsumed.AddDynamic(Observer, &UBreakerLongDarkRuntimeObserver::OnConsumed);
    B.Combat->ReceiveDamage(Hit);
    TestFalse(TEXT("new lease removes prior target before callbacks"), A.Status->HasStatus(Rot));
    TestTrue(TEXT("new target owns lease"), B.Status->HasStatus(Rot));
    TestFalse(TEXT("old removal callback cannot acquire a third lease"), C.Status->HasStatus(Rot));
    TestEqual(TEXT("old application consumed once"), Observer->Consumed, 1);
    A.Combat->RestoreVitals(); A.Combat->ReceiveDamage(Hit);
    const FTarget KilledIncoming = MakeTarget(); Observer->KillTarget = KilledIncoming.Combat;
    KilledIncoming.Combat->ReceiveDamage(Hit);
    TestTrue(TEXT("old removal callback actually kills incoming target"), KilledIncoming.Combat->IsDead());
    TestFalse(TEXT("dead incoming target receives no reserved status"), KilledIncoming.Status->HasStatus(Rot));
    A.Combat->RestoreVitals(); A.Combat->ReceiveDamage(Hit);
    const FTarget DestroyedIncoming = MakeTarget(); Observer->DestroyTarget = DestroyedIncoming.Actor;
    DestroyedIncoming.Combat->ReceiveDamage(Hit);
    TestFalse(TEXT("destroyed incoming target receives no reserved status"), DestroyedIncoming.Status->HasStatus(Rot));
    B.Combat->RestoreVitals(); B.Combat->ReceiveDamage(Hit);
    TestTrue(TEXT("failed callback reservations do not strand the source lease"), B.Status->HasStatus(Rot));
    if (!TestTrue(TEXT("real respec revokes current lease"), Progression->RespecCore(Reason))) return false;
    TestFalse(TEXT("respec removes persistent application immediately"), B.Status->HasStatus(Rot));
    C.Combat->RestoreVitals(); C.Combat->ReceiveDamage(Hit);
    if (!TestEqual(TEXT("old emitted hit retains finite application after respec"), C.Status->GetActiveStatuses().Num(), 1)) return false;
    TestFalse(TEXT("old emitted hit cannot recreate revoked persistence"), C.Status->GetActiveStatuses()[0].bPersistentRot);
    C.Status->AdvanceStatuses(4); TestFalse(TEXT("old emitted finite fallback expires"), C.Status->HasStatus(Rot));
    if (!TestTrue(TEXT("actual rebuy with refunded point"), Progression->PurchaseNode(Tree, Node->NodeId, Reason))) return false;
    B.Combat->RestoreVitals(); B.Combat->ReceiveDamage(Hit); B.Status->AdvanceStatuses(2); B.Status->ScaleRemainingDurations(.5f);
    TestEqual(TEXT("shortening retains only scheduled finite credit"), B.Status->GetActiveStatuses()[0].UnpaidDamageBudget, 12.53125f, .001f);
    B.Status->AdvanceStatuses(3);
    TestTrue(TEXT("positive shortening does not cancel permanent ownership"), B.Status->HasStatus(Rot));
    TestEqual(TEXT("shortened funding still reaches zero"), B.Status->GetActiveStatuses()[0].UnpaidDamageBudget, 0.0f, .001f);
    B.Status->ScaleRemainingDurations(0); TestFalse(TEXT("explicit zero-duration cleanse removes permanent Rot"), B.Status->HasStatus(Rot));
    A.Combat->RestoreVitals(); A.Combat->ReceiveDamage(Hit); A.Status->ConsumeAllStatuses();
    B.Combat->RestoreVitals(); B.Combat->ReceiveDamage(Hit); bool bFound = false; A.Status->ConsumeStatus(Rot, bFound);
    B.Status->AdvanceStatuses(5); TestTrue(TEXT("stale target cleanup cannot revoke another serial"), B.Status->HasStatus(Rot));
    FBreakerDamageRequest Kill; Kill.BaseDamage = 10000; Kill.DamageFamily = EBreakerDamageFamily::TrueDamage; Kill.bCanCritical = false;
    B.Combat->ReceiveDamage(Kill); TestFalse(TEXT("actual victim death clears lease"), B.Status->HasStatus(Rot));
    C.Combat->RestoreVitals(); C.Combat->ReceiveDamage(Hit); C.Status->AdvanceStatuses(5); TestTrue(TEXT("victim death frees next target lease"), C.Status->HasStatus(Rot));
    C.Actor->Destroy(); A.Combat->RestoreVitals(); A.Combat->ReceiveDamage(Hit);
    TestTrue(TEXT("target destruction frees next lease"), A.Status->HasStatus(Rot));
    Player->GetCombat()->ReceiveDamage(Kill); TestFalse(TEXT("actual source death revokes target lease immediately"), A.Status->HasStatus(Rot));
    Player->GetCombat()->RestoreVitals(); A.Status->AdvanceStatuses(4); TestFalse(TEXT("revival does not restart old lease"), A.Status->HasStatus(Rot));

    // Purchase the actual single-rank Chain (O272: no prerequisite) from
    // authored campaign entitlement. Its physical propagation remains allowed,
    // but persistent Rot cannot spread.
    FBreakerQuestFlagSet Completed;
    for (const auto& Mission : UBreakerMissionLibrary::GetMissions())
        for (const auto& Beat : Mission.Beats)
            for (FName Flag : UBreakerMissionLibrary::BeatCompletionFlags(Beat)) Completed.Add(Flag);
    Progression->SettleDoctrineEntitlement(Completed);
    const auto* Multispell = UBreakerProgressionLibrary::GetCasterMultispellTree();
    if (!TestTrue(TEXT("actual Chain doctrine purchase"), Progression->PurchaseNode(Multispell, TEXT("Caster.Multispell.Chain"), Reason))) return false;
    B.Combat->RestoreVitals(); A.Combat->RestoreVitals();
    FBreakerStatusApplicationSpec Poison; Poison.StatusTag = FGameplayTag::RequestGameplayTag(TEXT("Status.Poison")); Poison.BaseDamagePerTick = 1; Poison.Duration = 10; Poison.TickInterval = 1;
    A.Status->ApplyStatus(Poison, EBreakerDamageFamily::Physical, Player); A.Combat->ReceiveDamage(Hit);
    TestTrue(TEXT("original owns Rot with two status types"), A.Status->HasStatus(Rot));
    TestFalse(TEXT("Chain cannot propagate persistent Rot"), B.Status->HasStatus(Rot));
    FBreakerStatusApplicationSpec Bleed = Poison; Bleed.StatusTag = FGameplayTag::RequestGameplayTag(TEXT("Status.Bleed"));
    A.Status->ApplyStatus(Bleed, EBreakerDamageFamily::Physical, Player);
    TestTrue(TEXT("ordinary physical Chain still propagates"), B.Status->HasStatus(Bleed.StatusTag));
    Player->Destroy(); TestFalse(TEXT("source destruction revokes maintained Rot"), A.Status->HasStatus(Rot));
    return true;
}
#endif
