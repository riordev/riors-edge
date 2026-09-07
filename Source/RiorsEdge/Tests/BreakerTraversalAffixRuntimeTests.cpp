#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerCombatComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Items/BreakerAffixLibrary.h"
#include "Items/BreakerEquipmentComponent.h"
#include "Items/BreakerLootLibrary.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Progression/BreakerBuildConditions.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerTraversalAffixRuntimeTest, "RiorsEdge.Items.TraversalAffixRuntime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerTraversalAffixRuntimeTest::RunTest(const FString& Parameters)
{
    const FName LegacyId(TEXT("Offense.WallRideDamage"));
    const auto* Definition = UBreakerAffixLibrary::FindAffix(UBreakerAffixLibrary::GetSliceAffixPool(), LegacyId);
    if (!TestNotNull(TEXT("saved legacy affix ID still resolves"), Definition)) return false;
    TestEqual(TEXT("serialized target remains compatible"), Definition->StatTarget, EBreakerStatTarget::WallRideDamage);
    TestEqual(TEXT("old ID uses actual replacement verb"), Definition->Condition, EBreakerBuildCondition::RecentlyLedgeTraversed);
    FBreakerItemInstance Item;
    int32 Line = INDEX_NONE;
    for (int32 Seed = 1; Seed <= 8192; ++Seed)
    {
        Item = UBreakerLootLibrary::RollItem(TEXT("Traversal.Legacy"), EBreakerEquipSlot::Boots, EBreakerItemRarity::Standard, 1, Seed);
        Line = Item.Affixes.IndexOfByPredicate([&](const auto& Affix) { return Affix.AffixId == LegacyId; });
        if (Line != INDEX_NONE) break;
    }
    if (!TestTrue(TEXT("legacy ID remains reachable through actual loot"), Line != INDEX_NONE)) return false;
    const float Value = Item.Affixes[Line].Value;
    // Isolate this consumer from unrelated rolled conditions; preserve its real ID/tier/value.
    for (auto& Affix : Item.Affixes) if (Affix.AffixId != LegacyId) Affix.Value = 0;
    UWorld::InitializationValues Init;
    Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!TestNotNull(TEXT("isolated traversal world"), World)) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL());
    const uint64 InitialFrame = GFrameCounter;
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); GFrameCounter = InitialFrame; };
    auto Box = [&](FVector Location, FVector Extent)
    {
        AActor* Actor = World->SpawnActor<AActor>();
        UBoxComponent* Shape = NewObject<UBoxComponent>(Actor);
        Actor->AddInstanceComponent(Shape); Actor->SetRootComponent(Shape);
        Shape->SetBoxExtent(Extent); Shape->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        Shape->SetCollisionResponseToAllChannels(ECR_Block); Shape->RegisterComponent(); Actor->SetActorLocation(Location);
        return Shape;
    };
    Box(FVector(0, 0, -10), FVector(3000, 3000, 10));
    UBoxComponent* Obstacle = Box(FVector(180, 0, 40), FVector(120, 200, 40));
    UBoxComponent* Blocker = Box(FVector(1000, 0, 1000), FVector(100, 100, 100));
    Blocker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    ABreakerCharacter* Player = World->SpawnActor<ABreakerCharacter>(FVector(-500, 0, 200), FRotator::ZeroRotator);
    if (!TestNotNull(TEXT("real pawn without save-loading BeginPlay"), Player)) return false;
    Player->GetAbilitySystemComponent()->InitAbilityActorInfo(Player, Player);
    Player->GetAbilitySystemComponent()->AddAttributeSetSubobject(Player->GetAttributes());
    Player->GetCombat()->BindAttributes(Player->GetAttributes());
    auto* Equipment = Player->GetEquipment(); Equipment->BindAttributes(Player->GetAttributes());
    if (!TestTrue(TEXT("old-ID item actually equips"), Equipment->EquipItem(Item))) return false;
    auto* Movement = Player->GetBreakerMovement();
    Movement->SetComponentTickEnabled(false); Movement->bRunPhysicsWithNoController = true;
    const float HalfHeight = Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    auto Clock = [&](float Step) { ++GFrameCounter; World->Tick(LEVELTICK_All, Step); };
    auto Refresh = [&]() { Equipment->TickComponent(0.0f, LEVELTICK_All, nullptr); return Player->GetAttributes()->GetDamageMultiplier(); };
    const float Baseline = Refresh();
    auto Expire = [&]() { for (int32 I = 0; I < 64; ++I) Clock(0.05f); TestEqual(TEXT("expired traversal returns weapon damage to baseline"), Refresh(), Baseline, 0.0001f); };
    auto Prepare = [&](float Height)
    {
        Blocker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Movement->SetMovementMode(MOVE_Walking); Movement->StopMovementImmediately();
        TestTrue(TEXT("real relocation to traversal start"), Player->TeleportTo(FVector(0, 0, HalfHeight), FRotator::ZeroRotator, false, true));
        Movement->PerformMovement(0.001f);
        const float Top = Player->GetActorLocation().Z - HalfHeight + Height;
        Obstacle->SetBoxExtent(FVector(120, 200, Top / 2));
        Obstacle->GetOwner()->SetActorLocation(FVector(180, 0, Top / 2));
    };
    for (const float Height : {80.0f, 120.0f})
    {
        Prepare(Height);
        TestEqual(TEXT("teleport alone cannot create traversal damage"), Refresh(), Baseline, 0.0001f);
        FBreakerLedgeTraversal Found;
        if (!TestTrue(TEXT("actual geometry resolves"), Movement->ResolveLedgeTraversal(Found))) return false;
        TestEqual(TEXT("real geometry chooses vault or mantle"), Found.Verb, Height < 100 ? EBreakerLedgeVerb::Vault : EBreakerLedgeVerb::Mantle);
        const double Before = Movement->GetLastLedgeTraversalTime();
        if (!TestTrue(TEXT("real traversal input accepted"), Movement->TryBeginLedgeTraversal())) return false;
        for (int32 I = 0; I < 60 && Movement->GetLastLedgeTraversalTime() <= Before; ++I) { Clock(0.01f); Movement->PerformMovement(0.01f); }
        if (!TestTrue(TEXT("swept traversal records successful completion"), Movement->GetLastLedgeTraversalTime() > Before)) return false;
        TestTrue(TEXT("actual pawn arrives at endpoint"), Player->GetActorLocation().Equals(Found.TargetLocation, 1.0f));
        TestEqual(TEXT("legacy affix feeds live additive weapon damage"), Refresh(), Baseline + Value / 100.0f, 0.0001f);
        TestFalse(TEXT("retired wall ride never becomes active"), FBreakerBuildConditionState::EvaluateForActor(Player).IsActive(EBreakerBuildCondition::WallRiding));
        Expire();
    }
    Prepare(120);
    const double BeforeAbort = Movement->GetLastLedgeTraversalTime();
    if (!TestTrue(TEXT("abort fixture begins actual traversal"), Movement->TryBeginLedgeTraversal())) return false;
    Clock(0.01f); Movement->PerformMovement(0.01f);
    Blocker->GetOwner()->SetActorLocation(Player->GetActorLocation() + FVector(0, 0, 80));
    Blocker->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    for (int32 I = 0; I < 35; ++I) { Clock(0.01f); Movement->PerformMovement(0.01f); }
    TestEqual(TEXT("obstruction does not record completion"), Movement->GetLastLedgeTraversalTime(), BeforeAbort);
    TestEqual(TEXT("aborted traversal cannot pay affix"), Refresh(), Baseline, 0.0001f);
    Prepare(80);
    if (!TestTrue(TEXT("teleport fixture starts actual traversal"), Movement->TryBeginLedgeTraversal())) return false;
    Clock(0.01f); Movement->PerformMovement(0.01f);
    TestTrue(TEXT("real mid-traversal teleport"), Player->TeleportTo(FVector(700, 0, HalfHeight + 2), FRotator::ZeroRotator, false, true));
    for (int32 I = 0; I < 40; ++I) { Clock(0.01f); Movement->PerformMovement(0.01f); }
    TestEqual(TEXT("teleport interruption cannot manufacture completion"), Movement->GetLastLedgeTraversalTime(), BeforeAbort);
    TestEqual(TEXT("teleport interruption pays no traversal affix"), Refresh(), Baseline, 0.0001f);
    return true;
}
#endif
