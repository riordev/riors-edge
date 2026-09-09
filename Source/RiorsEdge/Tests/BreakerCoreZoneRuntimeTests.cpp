#include "Misc/AutomationTest.h"
#include "Tests/BreakerCastTestHelpers.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Abilities/BreakerAbility_Rot.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Classes/BreakerManaComponent.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerDamageLibrary.h"
#include "Combat/BreakerZoneActor.h"
#include "Combat/BreakerZoneMath.h"
#include "Components/SphereComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Progression/BreakerClassDefinition.h"
#include "Progression/BreakerExperience.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionNode.h"
#include "Progression/BreakerProgressionTree.h"
#include "Tests/BreakerReactionRuntimeObserver.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerCoreZoneBudgetTest, "RiorsEdge.Combat.Zone.CoreFiniteBudget",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerCoreZoneBudgetTest::RunTest(const FString& Parameters)
{
    TestEqual(TEXT("Partial interval is not funded"), UBreakerZoneMath::FundedTicks(2.9f, 1), 2);
    TestEqual(TEXT("Full schedule exceeds live hitch guard"), UBreakerZoneMath::FundedTicks(6, .1f), 60);
    TestEqual(TEXT("Invalid duration funds nothing"), UBreakerZoneMath::FundedTicks(-1, 1), 0);
    TestEqual(TEXT("Standing matures at third scheduled tick"), UBreakerZoneMath::StandingIncreased(4, 1, 0), 5.0f);
    TestEqual(TEXT("Paid refresh preserves mature age"), UBreakerZoneMath::StandingIncreased(4, 1, 3), 10.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerCoreZoneRuntimeTest, "RiorsEdge.Combat.Zone.CoreStandingDetonationRuntime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerCoreZoneRuntimeTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Init; Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!World) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); World->InitializeActorsForPlay(FURL());
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
    auto* Player = World->SpawnActor<ABreakerCharacter>(); if (!Player) return false;
    Player->SetActorTickEnabled(false); Player->GetBreakerMovement()->SetComponentTickEnabled(false);
    auto* ASC = Player->GetAbilitySystemComponent(); auto* Attr = Player->GetAttributes();
    ASC->InitAbilityActorInfo(Player, Player); ASC->AddAttributeSetSubobject(Attr);
    Player->GetCombat()->BindAttributes(Attr); auto* Progression = Player->GetProgression(); Progression->BindAttributes(Attr);
    if (!TestTrue(TEXT("Real Caster selection"), Progression->ChoosePermanentClassById(EBreakerClassId::Caster))) return false;
    Player->GetMana()->BindAttributes(Attr); Player->GetMana()->AdvanceLoop(20);
    auto* Definition = DuplicateObject<UBreakerClassDefinition>(Progression->ClassDefinition, Player);
    auto* Tree = NewObject<UBreakerProgressionTree>(Definition); Tree->TreeId = TEXT("Test.Core.Zone"); Tree->Currency = EBreakerPointCurrency::CorePoints;
    for (const TCHAR* Id : { TEXT("Standing"), TEXT("Detonation") })
    {
        auto* Node = NewObject<UBreakerProgressionNode>(Tree); Node->NodeId = FName(Id); Node->Currency = Tree->Currency;
        Node->GrantedTags.AddTag(FGameplayTag::RequestGameplayTag(FName(FString(TEXT("Progression.Node.Core.")) + Id)));
        Tree->Nodes.Add(Node);
    }
    Definition->BranchTrees.Add(Tree); Progression->ClassDefinition = Definition;
    Progression->AwardExperience(UBreakerExperienceLibrary::TotalXpToReachLevel(5, Progression->ExperienceCurve));
    FText Reason;
    if (!TestTrue(TEXT("Standing purchased with earned point"), Progression->PurchaseNode(Tree, TEXT("Standing"), Reason))) return false;
    auto* Victim = World->SpawnActor<AActor>(); auto* Body = NewObject<USphereComponent>(Victim);
    Victim->AddInstanceComponent(Body); Victim->SetRootComponent(Body); Body->SetSphereRadius(35);
    Body->SetCollisionEnabled(ECollisionEnabled::QueryOnly); Body->SetCollisionResponseToAllChannels(ECR_Ignore);
    Body->SetCollisionResponseToChannel(ECC_GameTraceChannel2, ECR_Block); Body->RegisterComponent(); Victim->SetActorLocation(FVector(300, 0, 0));
    auto* Combat = NewObject<UBreakerCombatComponent>(Victim); Victim->AddInstanceComponent(Combat); Combat->RegisterComponent();
    auto* Health = NewObject<UBreakerAttributeSet>(Victim); Health->ApplyMaxHealth(1000000); Health->ApplyHealth(1000000); Combat->BindAttributes(Health);
    auto* Observer = NewObject<UBreakerReactionRuntimeObserver>(World); Combat->OnDamageTaken.AddDynamic(Observer, &UBreakerReactionRuntimeObserver::OnHit);
    FBreakerZoneSpec Spec; Spec.Duration = 4; Spec.TickInterval = 1; Spec.RadiusCm = 500;
    Spec.TickDamage.BaseDamage = 100; Spec.TickDamage.bCanCritical = false;
    UBreakerDamageLibrary::AddSourceIncreased(Spec.TickDamage, 100);
    auto SpawnZone = [&](const FBreakerZoneSpec& Payload)
    {
        auto* Zone = World->SpawnActor<ABreakerZoneActor>(); Zone->SetActorTickEnabled(false);
        Zone->ConfigureZone(Payload, Player); return Zone;
    };
    auto* Ordinary = SpawnZone(Spec); Ordinary->AdvanceZone(2);
    if (!TestEqual(TEXT("First two scheduled ticks land"), Observer->Hits.Num(), 2)) return false;
    TestEqual(TEXT("Early tick uses existing additive bucket"), Observer->Hits[0].Result.RawDamage, 200.0f, .01f);
    Ordinary->SetExpiryPaused(true); Ordinary->AdvanceZone(1);
    TestEqual(TEXT("Paused lifetime still matures Standing at scheduled age three"), Observer->Hits.Last().Result.RawDamage, 210.0f, .01f);
    Ordinary->RefreshPaidPayload(Spec); Ordinary->AdvanceZone(1);
    TestEqual(TEXT("Refresh retains Standing age"), Observer->Hits.Last().Result.RawDamage, 210.0f, .01f);
    Ordinary->ReleaseAllOccupants(); Ordinary->Destroy(); Observer->Hits.Reset();
    if (!TestTrue(TEXT("Detonation purchased with earned point"), Progression->PurchaseNode(Tree, TEXT("Detonation"), Reason))) return false;
    auto* Deferred = SpawnZone(Spec); Deferred->AdvanceZone(2);
    TestEqual(TEXT("Detonation suppresses periodic damage"), Observer->Hits.Num(), 0);
    Deferred->SetExpiryPaused(true); Deferred->AdvanceZone(100);
    TestEqual(TEXT("Pause does not settle damage"), Observer->Hits.Num(), 0);
    Deferred->SetExpiryPaused(false); Deferred->AdvanceZone(100);
    if (!TestEqual(TEXT("Natural expiry settles exactly once"), Observer->Hits.Num(), 1)) return false;
    TestEqual(TEXT("Pause and hitch cannot grow original funded schedule; Standing is slice weighted"), Observer->Hits[0].Result.RawDamage, 820.0f, .02f);
    Deferred->AdvanceZone(100); TestEqual(TEXT("Repeated expiry cannot repay"), Observer->Hits.Num(), 1);
    Observer->Hits.Reset();
    auto* Recast = SpawnZone(Spec); Recast->AdvanceZone(3);
    auto Stronger = Spec; Stronger.TickDamage.BaseDamage = 150;
    Recast->RefreshPaidPayload(Stronger); Recast->AdvanceZone(4);
    if (!TestEqual(TEXT("Refresh settles one replacement budget"), Observer->Hits.Num(), 1)) return false;
    TestEqual(TEXT("Refresh replaces payload without banking previous budget"), Observer->Hits[0].Result.RawDamage, 1260.0f, .02f);
    Observer->Hits.Reset();
    // Native GAS grant isolates paid Rot delivery; the Core rules were earned above.
    const auto Handle = ASC->GiveAbility(FGameplayAbilitySpec(UBreakerAbility_Rot::StaticClass(), 1));
    Player->GetCombat()->PushOutgoingModifier(TEXT("Test.Zone.Placement"), 3, 1.15f, 100);
    const float BeforeMana = Player->GetMana()->GetMana();
    if (!TestTrue(TEXT("Paid Rot activates"), ASC->TryActivateAbility(Handle))) return false;
    BreakerResolvePendingCast(World, Player);
    TestTrue(TEXT("Native Rot resource debit occurs"), Player->GetMana()->GetMana() < BeforeMana);
    ABreakerZoneActor* Paid = nullptr;
    for (const auto& Held : ABreakerZoneActor::GetLiveZones())
        if (auto* Zone = Held.Get()) if (Zone->GetZoneInstigator() == Player && !Zone->IsReleased()) Paid = Zone;
    if (!TestNotNull(TEXT("Paid cast creates real zone"), Paid)) return false;
    Paid->SetActorTickEnabled(false);
    const auto PaidSpec = Paid->GetSpec();
    FBreakerDamageRequest Expected = PaidSpec.TickDamage;
    UBreakerDamageLibrary::AddSourceIncreased(Expected, UBreakerZoneMath::StandingIncreased(PaidSpec.Duration, PaidSpec.TickInterval, 0));
    Player->GetCombat()->ApplyOutgoingModifiers(Expected);
    Expected.BaseDamage *= UBreakerZoneMath::FundedTicks(PaidSpec.Duration, PaidSpec.TickInterval);
    Player->GetCombat()->RemoveOutgoingModifier(TEXT("Test.Zone.Placement"));
    // Snapshot survives a real respec while the finite zone is pending.
    if (!TestTrue(TEXT("Real Core respec while pending"), Progression->RespecCore(Reason))) return false;
    Paid->AdvanceZone(PaidSpec.Duration + 1);
    if (!TestEqual(TEXT("Paid Rot delivers one expiry hit after respec"), Observer->Hits.Num(), 1)) return false;
    const auto& Hit = Observer->Hits[0];
    Expected.bUseSnapshotCritical = true; Expected.bSnapshotCriticalResult = Hit.Result.bCritical;
    TestEqual(TEXT("Paid Rot preserves placement outgoing snapshot"), Hit.Result.RawDamage,
        UBreakerDamageLibrary::ResolveDamage(Expected, {}).RawDamage, .1f);
    Observer->Hits.Reset();
    if (!TestTrue(TEXT("Rule can be repurchased through same earned pool"), Progression->PurchaseNode(Tree, TEXT("Detonation"), Reason))) return false;
    auto* Cancelled = SpawnZone(Spec); Cancelled->ReleaseAllOccupants(); Cancelled->AdvanceZone(10);
    TestEqual(TEXT("Explicit release never pays"), Observer->Hits.Num(), 0);
    auto* Orphan = SpawnZone(Spec);
    FBreakerDamageRequest Kill; Kill.BaseDamage = 1000000; Kill.bCanCritical = false;
    Player->GetCombat()->ReceiveDamage(Kill); Orphan->AdvanceZone(10);
    TestEqual(TEXT("Owner death cancels pending damage"), Observer->Hits.Num(), 0);
    return true;
}
#endif
