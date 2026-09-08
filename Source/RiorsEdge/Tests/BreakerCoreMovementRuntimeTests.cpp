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
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Progression/BreakerClassDefinition.h"
#include "Progression/BreakerExperience.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionNode.h"
#include "Progression/BreakerProgressionTree.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerCoreMovementRuntimeTest, "RiorsEdge.Movement.CoreRuntime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerCoreMovementRuntimeTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Init; Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!World) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL());
    const uint64 InitialFrame = GFrameCounter;
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); GFrameCounter = InitialFrame; };
    auto Box = [&](FVector Location, FVector Extent)
    {
        auto* Actor = World->SpawnActor<AActor>(); auto* Shape = NewObject<UBoxComponent>(Actor);
        Actor->AddInstanceComponent(Shape); Actor->SetRootComponent(Shape); Shape->SetBoxExtent(Extent);
        Shape->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics); Shape->SetCollisionResponseToAllChannels(ECR_Block);
        Shape->RegisterComponent(); Actor->SetActorLocation(Location); return Shape;
    };
    Box(FVector(0, 0, -10), FVector(4000, 4000, 10));
    auto* Obstacle = Box(FVector(180, 0, 40), FVector(120, 200, 40));
    auto* Player = World->SpawnActor<ABreakerCharacter>(FVector(-500, 0, 200), FRotator::ZeroRotator);
    if (!Player) return false;
    Player->SetActorTickEnabled(false);
    auto* Attr = Player->GetAttributes(); auto* ASC = Player->GetAbilitySystemComponent();
    ASC->InitAbilityActorInfo(Player, Player); ASC->AddAttributeSetSubobject(Attr);
    Player->GetCombat()->BindAttributes(Attr); Player->GetCombat()->SetComponentTickEnabled(false);
    auto* Progression = Player->GetProgression(); Progression->BindAttributes(Attr);
    auto* Class = NewObject<UBreakerClassDefinition>(); Class->ClassId = EBreakerClassId::Caster;
    auto* Tree = NewObject<UBreakerProgressionTree>(Class); Tree->TreeId = TEXT("Test.Core.Movement");
    Tree->Currency = EBreakerPointCurrency::CorePoints; Class->BranchTrees.Add(Tree);
    auto* Node = NewObject<UBreakerProgressionNode>(Tree); Node->NodeId = TEXT("Test.Core.Movement.Lanes");
    Node->Currency = Tree->Currency; Tree->Nodes.Add(Node);
    // Separate primitive schema with explicit test magnitudes, paid by earned XP.
    auto Effect = [&](EBreakerNodeStatTarget Target, float Value, EBreakerNodeStatBucket Bucket)
    { FBreakerNodeEffect E; E.StatTarget = Target; E.ValuePerRank = Value; E.StatBucket = Bucket; Node->Effects.Add(E); };
    Effect(EBreakerNodeStatTarget::SprintSpeed, 15, EBreakerNodeStatBucket::IncreasedPercent);
    Effect(EBreakerNodeStatTarget::Acceleration, 8, EBreakerNodeStatBucket::IncreasedPercent);
    Effect(EBreakerNodeStatTarget::JumpHeight, 18, EBreakerNodeStatBucket::IncreasedPercent);
    Effect(EBreakerNodeStatTarget::LedgeSpeed, 100, EBreakerNodeStatBucket::IncreasedPercent);
    Effect(EBreakerNodeStatTarget::SafeFallDistance, 6, EBreakerNodeStatBucket::Flat);
    Effect(EBreakerNodeStatTarget::AirJumpCount, 1, EBreakerNodeStatBucket::Flat);
    if (!Progression->ChoosePermanentClass(Class)) return false;
    Progression->AwardExperience(UBreakerExperienceLibrary::TotalXpToReachLevel(2, Progression->ExperienceCurve));
    auto* Move = Player->GetBreakerMovement(); Move->SetComponentTickEnabled(false); Move->bRunPhysicsWithNoController = true;
    Move->RefreshJumpGrant();
    const float HalfHeight = Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    auto Clock = [&](float Step) { ++GFrameCounter; World->Tick(LEVELTICK_All, Step); Move->PerformMovement(Step); };
    auto Ground = [&](float X)
    {
        Player->StopJumping(); Move->SetMovementMode(MOVE_Walking); Move->StopMovementImmediately();
        Player->TeleportTo(FVector(X, 0, HalfHeight), FRotator::ZeroRotator, false, true); Move->PerformMovement(.001f);
    };
    auto Jump = [&](int32 ExpectedCount)
    {
        Ground(-500); Player->Jump(); Player->CheckJumpInput(.01f);
        const float Impulse = Move->Velocity.Z;
        TestTrue(TEXT("Actual jump input enters falling"), Move->IsFalling());
        for (int32 I = 1; I <= ExpectedCount; ++I)
        {
            Player->StopJumping(); Player->CheckJumpInput(.01f);
            Player->Jump(); Player->CheckJumpInput(.01f);
        }
        TestEqual(TEXT("Real repeated jump input stops at purchased budget"), Player->JumpCurrentCount, ExpectedCount);
        Player->StopJumping(); return Impulse;
    };
    auto Fall = [&]()
    {
        Ground(-500); Attr->ApplyHealth(Attr->GetMaxHealth()); Attr->ApplyShield(0);
        Player->TeleportTo(FVector(-500, 0, HalfHeight + 1200), FRotator::ZeroRotator, false, true);
        Move->SetMovementMode(MOVE_Falling); Move->Velocity = FVector::ZeroVector;
        const float Before = Attr->GetHealth();
        for (int32 I = 0; I < 300 && !Move->IsMovingOnGround(); ++I) Clock(.02f);
        TestTrue(TEXT("Native fall lands on real floor"), Move->IsMovingOnGround());
        return (Before - Attr->GetHealth()) / Attr->GetMaxHealth();
    };
    auto Traverse = [&](float Height, bool bRespec)
    {
        Ground(0); const float Top = Player->GetActorLocation().Z - HalfHeight + Height;
        Obstacle->SetBoxExtent(FVector(120, 200, Top / 2)); Obstacle->GetOwner()->SetActorLocation(FVector(180, 0, Top / 2));
        FBreakerLedgeTraversal Found;
        if (!TestTrue(TEXT("Actual obstacle resolves traversal"), Move->ResolveLedgeTraversal(Found))) return -1.0f;
        if (!TestTrue(TEXT("Actual input starts traversal"), Move->TryBeginLedgeTraversal())) return -1.0f;
        const double Previous = Move->GetLastLedgeTraversalTime();
        float Elapsed = 0;
        if (bRespec)
        {
            // Input queues a movement request; the first physics step actually
            // enters the traversal and snapshots its clock.
            Clock(.005f); Elapsed += .005f;
            TestTrue(TEXT("Traversal has entered before respec"), Move->IsTraversingLedge());
            FText Failure; TestTrue(TEXT("Respec during traversal"), Progression->RespecCore(Failure));
        }
        while (Elapsed < 1 && Move->GetLastLedgeTraversalTime() <= Previous) { Clock(.005f); Elapsed += .005f; }
        TestTrue(TEXT("Swept traversal reaches real endpoint"), Player->GetActorLocation().Equals(Found.TargetLocation, 1));
        TestTrue(TEXT("Swept traversal completes"), Move->GetLastLedgeTraversalTime() > Previous);
        return Elapsed;
    };
    const float Walk = Move->GetWalkSpeedCap(), Sprint = Move->GetSprintSpeedCap(), Accel = Move->GetMaxAcceleration();
    const float BaseImpulse = Jump(2);
    TestEqual(TEXT("Ordinary twelve-metre fall loses thirty percent"), Fall(), .30f, .003f);
    const float Vault = Traverse(80, false), Mantle = Traverse(120, false);
    FText Reason;
    if (!TestTrue(TEXT("Earned Core point purchases movement schema"), Progression->PurchaseNode(Tree, Node->NodeId, Reason))) return false;
    TestEqual(TEXT("Sprint-only bonus preserves walking"), Move->GetWalkSpeedCap(), Walk, .01f);
    TestEqual(TEXT("Actual sprint cap includes distinct lane"), Move->GetSprintSpeedCap(), Sprint * 1.15f, .01f);
    TestEqual(TEXT("Engine acceleration consumer includes bonus"), Move->GetMaxAcceleration(), Accel * 1.08f, .01f);
    TestEqual(TEXT("Generic air jump works on Caster"), Move->GetGrantedJumpCount(), 3);
    TestEqual(TEXT("Actual jump scales height rather than squaring bonus"), Jump(3), BaseImpulse * FMath::Sqrt(1.18f), .01f);
    TestEqual(TEXT("Editable impulse does not ratchet"), Move->JumpZVelocity, BaseImpulse, .01f);
    TestEqual(TEXT("Six extra safe metres prevent twelve-metre fall harm"), Fall(), 0.0f, .003f);
    TestEqual(TEXT("Vault clock halves"), Traverse(80, false), Vault / 2, .011f);
    TestEqual(TEXT("Mantle snapshots faster clock across respec"), Traverse(120, true), Mantle / 2, .011f);
    TestEqual(TEXT("Respec removes extra jump"), Move->GetGrantedJumpCount(), 2);
    TestEqual(TEXT("Respec restores sprint"), Move->GetSprintSpeedCap(), Sprint, .01f);
    TestEqual(TEXT("Respec restores acceleration"), Move->GetMaxAcceleration(), Accel, .01f);
    TestEqual(TEXT("Respec restores actual impulse"), Jump(2), BaseImpulse, .01f);
    TestEqual(TEXT("Respec restores fall harm"), Fall(), .30f, .003f);
    TestEqual(TEXT("Next traversal uses restored clock"), Traverse(80, false), Vault, .011f);
    return true;
}
#endif
