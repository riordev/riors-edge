#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Abilities/BreakerAbilityComponent.h"
#include "Abilities/BreakerBreachCharge.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerEnemy.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionLibrary.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerOverpressureRuntimeTest,
    "RiorsEdge.Abilities.Tank.OverpressurePurchasedRuntime", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerOverpressureRuntimeTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Init;
    Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!TestNotNull(TEXT("Isolated runtime world"), World)) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
    // Native actor UFUNCTION delegates also pass through AActor::ProcessEvent;
    // an uninitialized world suppresses that path even for native callbacks.
    // Initialize actors without Character BeginPlay/save loading.
    World->InitializeActorsForPlay(FURL());
    if (!TestTrue(TEXT("World permits actual actor death delegates"), World->AreActorsInitialized())) return false;
    ABreakerCharacter* Player = World->SpawnActor<ABreakerCharacter>();
    Player->SetActorTickEnabled(false);
    Player->GetBreakerMovement()->SetComponentTickEnabled(false);
    UAbilitySystemComponent* ASC = Player->GetAbilitySystemComponent();
    ASC->InitAbilityActorInfo(Player, Player);
    ASC->AddAttributeSetSubobject(Player->GetAttributes());
    Player->GetCombat()->BindAttributes(Player->GetAttributes());
    Player->GetProgression()->BindAttributes(Player->GetAttributes());
    FBreakerProgressionState State;
    State.PermanentClass = EBreakerClassId::Tank;
    // Purchased-node wiring fixture at the authored full campaign budget;
    // this does not claim that all campaign benchmarks are currently reachable.
    State.UnspentDoctrinePoints = 8;
    State.UnspentAbilityTokens = 1;
    Player->GetProgression()->LoadProgressionState(State);
    FText Error;
    if (!TestTrue(TEXT("Actual Breach Charge token purchase"), Player->GetProgression()->SpendAbilityToken(TEXT("Tank.BreachCharge"), Error))) return false;
    UBreakerAbilityComponent* Abilities = Player->GetAbilities();
    if (!TestTrue(TEXT("Actual Breach Charge equip"), Abilities->TryEquipAbility(EBreakerAbilitySlot::ClassAbilityTwo, TEXT("Tank.BreachCharge"), Error))) return false;
    Abilities->RefreshGrants();
    ABreakerEnemy* Enemy = World->SpawnActor<ABreakerEnemy>(FVector(650, 0, 0), FRotator::ZeroRotator);
    auto* EnemyAttributes = Cast<UBreakerAttributeSet>(Enemy->GetDefaultSubobjectByName(TEXT("Attributes")));
    if (!TestNotNull(TEXT("Enemy attributes"), EnemyAttributes)) return false;
    Enemy->GetAbilitySystemComponent()->AddAttributeSetSubobject(EnemyAttributes);
    Enemy->DispatchBeginPlay();
    Enemy->SetActorTickEnabled(false);
    EnemyAttributes->ApplyMaxHealth(100000);
    EnemyAttributes->ApplyHealth(100000);
    auto Fund = [&]() { ASC->SetNumericAttributeBase(UBreakerAttributeSet::GetClassResourceAttribute(), 100.0f); };
    auto CastSlot = [&]() { return Abilities->TryActivateSlot(EBreakerAbilitySlot::ClassAbilityTwo); };
    auto Charges = [&]()
    {
        TArray<ABreakerBreachCharge*> Result;
        for (TActorIterator<ABreakerBreachCharge> It(World); It; ++It)
            if (!It->IsActorBeingDestroyed()) Result.Add(*It);
        return Result;
    };
    // This synchronous isolated test owns its temporary frame counter span;
    // TimerManager otherwise refuses a second tick in the same engine frame.
    TGuardValue<uint64> FrameCounterScope(GFrameCounter, GFrameCounter);
    auto Advance = [&](float Seconds)
    {
        // Large synthetic frames are clamped by AWorldSettings, so ten 0.8s
        // calls did not advance the eight-second cooldown. Use real-sized
        // frames and verify elapsed world time rather than removing effects.
        const double Before = World->GetTimeSeconds();
        const int32 Steps = FMath::Max(1, FMath::CeilToInt(Seconds / 0.05f));
        for (int32 Step = 0; Step < Steps; ++Step) { ++GFrameCounter; World->Tick(LEVELTICK_All, Seconds / Steps); }
        TestTrue(TEXT("Fixture advances the requested game time"), World->GetTimeSeconds() - Before >= Seconds - 0.01f);
    };
    Fund();
    if (!TestTrue(TEXT("Baseline cast uses actual slot input"), CastSlot())) return false;
    TestEqual(TEXT("Placement pays authored thirty resource"), Player->GetAttributes()->GetClassResource(), 70.0f);
    if (!TestEqual(TEXT("One persistent charge exists"), Charges().Num(), 1)) return false;
    TestFalse(TEXT("No Overpressure cannot re-press through cooldown"), CastSlot());
    Advance(0.5f);
    TestEqual(TEXT("Baseline fuse does not detonate early"), Charges().Num(), 1);
    Advance(1.0f);
    TestEqual(TEXT("Baseline fuse detonates and removes body"), Charges().Num(), 0);
    TestTrue(TEXT("Actual baseline blast damages enemy"), EnemyAttributes->GetHealth() < 100000);
    Advance(8.0f);
    TestEqual(TEXT("Baseline cooldown genuinely expired"), Abilities->GetCooldownRemaining(EBreakerAbilitySlot::ClassAbilityTwo), 0.0f);
    const UBreakerProgressionTree* Tree = UBreakerProgressionLibrary::GetTankDemolitionistTree();
    for (int32 Rank = 0; Rank < 2; ++Rank)
        if (!TestTrue(TEXT("Actual Shaped Charge prerequisite purchase"), Player->GetProgression()->PurchaseNode(Tree, TEXT("Tank.Demolitionist.ShapedCharge"), Error))) return false;
    if (!TestTrue(TEXT("Actual rank one Overpressure purchase"), Player->GetProgression()->PurchaseNode(Tree, TEXT("Tank.Demolitionist.Overpressure"), Error))) return false;
    Fund();
    if (!TestTrue(TEXT("Rank one places charge"), CastSlot())) return false;
    if (!TestEqual(TEXT("Rank one has one live charge"), Charges().Num(), 1)) return false;
    const FVector Fixed = Charges()[0]->GetActorLocation();
    Enemy->SetActorLocation(Enemy->GetActorLocation() + FVector(0, 100, 0));
    Charges()[0]->Tick(0);
    TestTrue(TEXT("Rank one charge stays at its fixed impact"), Charges()[0]->GetActorLocation().Equals(Fixed));
    ASC->SetNumericAttributeBase(UBreakerAttributeSet::GetClassResourceAttribute(), 0);
    const float Cooldown = Abilities->GetCooldownRemaining(EBreakerAbilitySlot::ClassAbilityTwo);
    const float BeforeBlast = EnemyAttributes->GetHealth();
    TestTrue(TEXT("Re-press can detonate with no remaining resource"), CastSlot());
    TestEqual(TEXT("Re-press removes charge immediately"), Charges().Num(), 0);
    TestEqual(TEXT("Re-press costs no additional resource"), Player->GetAttributes()->GetClassResource(), 0.0f);
    TestEqual(TEXT("Re-press does not restart cooldown"), Abilities->GetCooldownRemaining(EBreakerAbilitySlot::ClassAbilityTwo), Cooldown);
    TestTrue(TEXT("Re-press causes actual blast damage"), EnemyAttributes->GetHealth() < BeforeBlast);
    Advance(8.0f);
    if (!TestTrue(TEXT("Actual rank two Overpressure purchase"), Player->GetProgression()->PurchaseNode(Tree, TEXT("Tank.Demolitionist.Overpressure"), Error))) return false;
    if (!TestTrue(TEXT("Actual Demolition purchase"), Player->GetProgression()->PurchaseNode(Tree, TEXT("Tank.Demolitionist.Demolition"), Error))) return false;
    Enemy->SetActorLocation(FVector(650, 0, 0));
    Fund();
    if (!TestTrue(TEXT("Sticky first placement"), CastSlot()) || !TestEqual(TEXT("Sticky body exists"), Charges().Num(), 1)) return false;
    const FVector BeforeMove = Charges()[0]->GetActorLocation();
    const FVector Shift(0, 800, 0);
    Enemy->SetActorLocation(Enemy->GetActorLocation() + Shift);
    Charges()[0]->Tick(0);
    TestTrue(TEXT("Rank two follows actual enemy impact offset"), Charges()[0]->GetActorLocation().Equals(BeforeMove + Shift, 0.1));
    const float StickyBefore = EnemyAttributes->GetHealth();
    const float SharedCooldown = Abilities->GetCooldownRemaining(EBreakerAbilitySlot::ClassAbilityTwo);
    TestTrue(TEXT("First sticky detonation"), CastSlot());
    TestTrue(TEXT("Blast resolves at moved enemy"), EnemyAttributes->GetHealth() < StickyBefore);
    Enemy->SetActorLocation(FVector(650, 0, 0));
    TestTrue(TEXT("Demolition allows second paid placement in same cooldown"), CastSlot());
    TestEqual(TEXT("Two placements charge twice, not four times"), Player->GetAttributes()->GetClassResource(), 40.0f);
    TestEqual(TEXT("Second placement preserves shared cooldown"), Abilities->GetCooldownRemaining(EBreakerAbilitySlot::ClassAbilityTwo), SharedCooldown);
    TestTrue(TEXT("Second detonation"), CastSlot());
    TestFalse(TEXT("Third placement refused while shared cooldown runs"), CastSlot());
    Advance(8.0f);
    Player->SetActorLocation(FVector(350, 0, 0));
    Fund();
    const float BeforeSelfBlast = Player->GetAttributes()->GetHealth();
    TestTrue(TEXT("Nearby charge placement"), CastSlot());
    TestTrue(TEXT("Nearby charge re-press"), CastSlot());
    TestTrue(TEXT("Early detonation retains positive takeoff self-damage"), Player->GetAttributes()->GetHealth() < BeforeSelfBlast);
    Player->SetActorLocation(FVector::ZeroVector);
    Advance(8.0f);
    Fund();
    TestTrue(TEXT("Place before unequip"), CastSlot());
    if (!TestTrue(TEXT("Actual replacement equip"), Abilities->TryEquipAbility(EBreakerAbilitySlot::ClassAbilityTwo, TEXT("Tank.Rend"), Error))) return false;
    TestEqual(TEXT("Unequip cancels pending charge"), Charges().Num(), 0);
    const float UnequipHealth = EnemyAttributes->GetHealth();
    Advance(2.0f);
    TestEqual(TEXT("Unequipped fuse never damages later"), EnemyAttributes->GetHealth(), UnequipHealth);
    if (!TestTrue(TEXT("Re-equip charge"), Abilities->TryEquipAbility(EBreakerAbilitySlot::ClassAbilityTwo, TEXT("Tank.BreachCharge"), Error))) return false;
    Advance(8.0f); Fund();
    TestTrue(TEXT("Place before owner death"), CastSlot());
    FBreakerDamageRequest Kill;
    Kill.BaseDamage = 1000000; Kill.bCanCritical = false; Kill.bBypassShield = true;
    TestTrue(TEXT("Owner actually dies through combat"), Player->GetCombat()->ReceiveDamage(Kill).bKilled);
    TestTrue(TEXT("Owner death state is authoritative"), Player->GetCombat()->IsDead());
    TestEqual(TEXT("Owner death cancels pending charge"), Charges().Num(), 0);
    const float DeathHealth = EnemyAttributes->GetHealth();
    Advance(2.0f);
    TestEqual(TEXT("Dead owner leaves no delayed blast"), EnemyAttributes->GetHealth(), DeathHealth);
    return true;
}
#endif
