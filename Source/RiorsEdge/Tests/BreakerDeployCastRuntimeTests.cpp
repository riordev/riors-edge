#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Abilities/BreakerAbilityComponent.h"
#include "Abilities/BreakerGunsmithAbilities.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Classes/BreakerScrapComponent.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerDeployable.h"
#include "Components/BoxComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionLibrary.h"
#include "TimerManager.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerDeployCastRuntimeTest, "RiorsEdge.Abilities.Gunsmith.DeployCastRuntime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerDeployCastRuntimeTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Init; Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!TestNotNull(TEXT("Isolated cast world"), World)) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    const uint64 InitialFrame = GFrameCounter;
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); GFrameCounter = InitialFrame; };
    ABreakerCharacter* Player = World->SpawnActor<ABreakerCharacter>(FVector(0, 0, 200), FRotator::ZeroRotator);
    APlayerController* Controller = World->SpawnActor<APlayerController>();
    if (!Player || !Controller) return false;
    // Before camera startup GetPlayerViewPoint falls back to its view target's
    // actor transform. Seed the controller too, as real player startup does.
    Controller->SetInitialLocationAndRotation(Player->GetActorLocation(), FRotator(-20, 0, 0));
    Controller->Possess(Player); Controller->SetControlRotation(FRotator(-20, 0, 0));
    UAbilitySystemComponent* ASC = Player->GetAbilitySystemComponent();
    ASC->InitAbilityActorInfo(Player, Player); ASC->AddAttributeSetSubobject(Player->GetAttributes());
    Player->GetCombat()->BindAttributes(Player->GetAttributes());
    Player->GetProgression()->BindAttributes(Player->GetAttributes());
    Player->GetProgression()->ChoosePermanentClassById(EBreakerClassId::Gunsmith);
    Player->FindComponentByClass<UBreakerScrapComponent>()->BindAttributes(Player->GetAttributes());
    // Explicit full-pool purchase fixture, not a claim about acquisition.
    Player->GetProgression()->GrantPlaytestPoints(8, 0);
    auto Equip = [&](FName Id)
    {
        Player->GetProgression()->DevForceEquipAbility(EBreakerAbilitySlot::ClassAbilityOne, Id);
        Player->GetAbilities()->RefreshGrants();
        return TestEqual(TEXT("Fixture actually equipped requested ability"), Player->GetAbilities()->GetAbilityIdForSlot(EBreakerAbilitySlot::ClassAbilityOne), Id);
    };
    if (!Equip(TEXT("Gunsmith.MineCluster"))) return false;
    AActor* Floor = World->SpawnActor<AActor>();
    UBoxComponent* Body = NewObject<UBoxComponent>(Floor); Floor->AddInstanceComponent(Body); Floor->SetRootComponent(Body);
    Body->SetBoxExtent(FVector(2000, 2000, 10)); Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    Body->SetCollisionResponseToAllChannels(ECR_Block); Body->RegisterComponent(); Floor->SetActorLocation(FVector(0, 0, -10));
    auto Live = [&]() { TArray<ABreakerDeployable*> Result; for (const auto& Entry : ABreakerDeployable::GetLiveDeployables()) if (Entry.IsValid() && Entry->GetOwningCharacter() == Player) Result.Add(Entry.Get()); return Result; };
    auto Clear = [&]() { for (ABreakerDeployable* Item : Live()) Item->Destroy(); };
    auto Press = [&]() { return TestTrue(TEXT("Actual equipped slot input"), Player->GetAbilities()->TryActivateSlot(EBreakerAbilitySlot::ClassAbilityOne)); };
    // Tick the actual timer manager directly: this world intentionally never
    // begins player play (and must not load owner saves). No world collection
    // or camera/physics startup is needed to exercise GAS cast timers.
    auto Advance = [&](float Seconds) { for (float Elapsed = 0; Elapsed < Seconds; Elapsed += 0.05f) { ++GFrameCounter; World->GetTimerManager().Tick(0.05f); } };
    FVector InitialEye, InitialPlace; FRotator InitialView;
    Controller->GetPlayerViewPoint(InitialEye, InitialView);
    if (!TestTrue(TEXT("Fixture player viewpoint starts above the real floor"), InitialEye.Z > 100)
        || !TestTrue(TEXT("Actual initial placement resolves against floor"), ABreakerDeployable::ResolvePlacement(World, Player, InitialEye, InitialView.Vector(), 800, InitialPlace))) return false;
    Player->GetAttributes()->ApplyClassResource(100);
    if (!Press()) return false;
    FGameplayAbilitySpec* MineSpec = ASC->FindAbilitySpecFromClass(UBreakerAbility_MineCluster::StaticClass());
    if (!TestTrue(TEXT("Valid initial placement leaves actual GAS cast active"), MineSpec && MineSpec->IsActive())) return false;
    Advance(0.5f); TestEqual(TEXT("Base cast is not instantaneous"), Live().Num(), 0);
    TestEqual(TEXT("Pending cast reserves no payment"), Player->GetAttributes()->GetClassResource(), 100.0f);
    Advance(0.6f); if (!TestEqual(TEXT("Base cast creates one object"), Live().Num(), 1)) return false;
    TestEqual(TEXT("Base cost paid once"), Live()[0]->GetScrapCost(), 35.0f);
    Clear();
    const UBreakerProgressionTree* Tree = UBreakerProgressionLibrary::GetGunsmithTinkererTree();
    // O272: Dead Ground is Cheap Work's impactful half; the pair is two
    // points, and Cheap Work's single rank is the 18-off Dry discount.
    for (const TCHAR* Id : { TEXT("Gunsmith.Tinkerer.CheapWork"), TEXT("Gunsmith.Tinkerer.DeadGround") })
    { FText Reason; if (!TestTrue(Id, Player->GetProgression()->PurchaseNode(Tree, Id, Reason))) return false; }
    for (float Resource : {20.0f, 50.0f})
    {
        Player->GetAttributes()->ApplyClassResource(Resource);
        if (!Press() || !TestEqual(TEXT("Dry and Stocked placements are immediate"), Live().Num(), 1)) return false;
        TestEqual(TEXT("Refund base is the exact pre-spend price"), Live()[0]->GetScrapCost(), Resource == 20 ? 17.0f : 35.0f);
        Clear();
    }
    Player->GetAttributes()->ApplyClassResource(90); if (!Press()) return false;
    Player->GetAttributes()->ApplyClassResource(50); Advance(1.1f);
    TestEqual(TEXT("Surplus delay remains doubled despite mid-cast band change"), Live().Num(), 0);
    Advance(1.0f); if (!TestEqual(TEXT("Doubled cast completes"), Live().Num(), 1)) return false;
    Clear();
    Player->GetAttributes()->ApplyClassResource(90); if (!Press() || !Press()) return false;
    Advance(2.2f); TestEqual(TEXT("Second press cancels pending cast"), Live().Num(), 0);
    TestEqual(TEXT("Cancellation costs nothing"), Player->GetAttributes()->GetClassResource(), 90.0f);
    if (!Press()) return false;
    Body->SetCollisionEnabled(ECollisionEnabled::NoCollision); Advance(2.2f);
    TestEqual(TEXT("Disappearing floor refuses the locked placement"), Live().Num(), 0);
    TestEqual(TEXT("Invalidated placement costs nothing"), Player->GetAttributes()->GetClassResource(), 90.0f);
    Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    if (!Press()) return false;
    AActor* Wall = World->SpawnActor<AActor>();
    UBoxComponent* WallBody = NewObject<UBoxComponent>(Wall); Wall->AddInstanceComponent(WallBody); Wall->SetRootComponent(WallBody);
    WallBody->SetBoxExtent(FVector(10, 300, 300)); WallBody->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    WallBody->SetCollisionResponseToAllChannels(ECR_Block); WallBody->RegisterComponent(); Wall->SetActorLocation(FVector(100, 0, 100));
    Advance(2.2f); TestEqual(TEXT("New intervening wall invalidates locked placement"), Live().Num(), 0);
    TestEqual(TEXT("Occluded placement costs nothing"), Player->GetAttributes()->GetClassResource(), 90.0f);
    Wall->Destroy();
    if (!Press()) return false;
    Player->SetActorLocation(FVector(-2000, 0, 200));
    Controller->SetInitialLocationAndRotation(Player->GetActorLocation(), FRotator(-20, 0, 0)); Advance(2.2f);
    TestEqual(TEXT("Leaving placement range cancels deployment"), Live().Num(), 0);
    TestEqual(TEXT("Out-of-range placement costs nothing"), Player->GetAttributes()->GetClassResource(), 90.0f);
    Player->SetActorLocation(FVector(0, 0, 200));
    Controller->SetInitialLocationAndRotation(Player->GetActorLocation(), FRotator(-20, 0, 0));
    // Turret is the shipped second-slot starter. Move that fixture slot to a
    // distinct class ability first; the dev writer correctly refuses duplicates.
    Player->GetProgression()->DevForceEquipAbility(EBreakerAbilitySlot::ClassAbilityTwo, TEXT("Gunsmith.Disruptor"));
    Player->GetAbilities()->RefreshGrants();
    if (!TestEqual(TEXT("Second slot no longer holds Turret"), Player->GetAbilities()->GetAbilityIdForSlot(EBreakerAbilitySlot::ClassAbilityTwo), FName(TEXT("Gunsmith.Disruptor")))) return false;
    if (!Press()) return false;
    if (!Equip(TEXT("Gunsmith.Turret"))) return false;
    Advance(2.2f);
    TestEqual(TEXT("Unequipping cancels delayed spawn"), Live().Num(), 0);
    Player->GetAttributes()->ApplyClassResource(50); if (!Press()) return false;
    TestEqual(TEXT("Dead Ground does not make Field Tech instant"), Live().Num(), 0);
    Advance(1.1f); if (!TestEqual(TEXT("Turret retains base cast duration"), Live().Num(), 1)) return false;
    Player->GetAttributes()->ApplyClassResource(100); if (!Press()) return false; Advance(1.1f);
    if (!TestEqual(TEXT("Two paid turrets reach the density cap"), Live().Num(), 2)) return false;
    const TArray<ABreakerDeployable*> BeforeRefusal = Live();
    Player->GetAttributes()->ApplyClassResource(100); if (!Press()) return false;
    Player->GetAttributes()->ApplyClassResource(0); Advance(1.1f);
    TestEqual(TEXT("Lost affordability never evicts the old field"), Live().Num(), 2);
    for (ABreakerDeployable* Old : BeforeRefusal) TestTrue(TEXT("Refused replacement preserves each original object"), Live().Contains(Old));
    Clear();
    Player->GetAttributes()->ApplyClassResource(100); if (!Press()) return false;
    FBreakerDamageRequest Kill; Kill.BaseDamage = 100000; Kill.bCanCritical = false; Kill.bBypassShield = true;
    Player->GetCombat()->ReceiveDamage(Kill); Advance(1.2f);
    TestEqual(TEXT("Death cancels placement"), Live().Num(), 0);
    return true;
}
#endif
