#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Classes/BreakerMomentumComponent.h"
#include "Combat/BreakerCombatComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionLibrary.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerContactRuntimeTest, "RiorsEdge.Movement.ContactCompletedTraversal",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerContactRuntimeTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Init;
    Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!TestNotNull(TEXT("Isolated traversal world"), World)) return false;
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
    if (!TestNotNull(TEXT("Actual Swift pawn"), Player)) return false;
    UAbilitySystemComponent* ASC = Player->GetAbilitySystemComponent();
    ASC->InitAbilityActorInfo(Player, Player); ASC->AddAttributeSetSubobject(Player->GetAttributes());
    Player->GetCombat()->BindAttributes(Player->GetAttributes());
    UBreakerProgressionComponent* Progression = Player->GetProgression();
    Progression->BindAttributes(Player->GetAttributes());
    if (!TestTrue(TEXT("Real Swift selection"), Progression->ChoosePermanentClassById(EBreakerClassId::Swift))) return false;
    Progression->GrantPlaytestPoints(4, 0); // Isolated wallet matches the current campaign's earned ceiling.
    TestEqual(TEXT("Fixture has only the currently attainable four Doctrine points"), Progression->GetProgressionState().UnspentDoctrinePoints, 4);
    UBreakerMomentumComponent* Momentum = Player->FindComponentByClass<UBreakerMomentumComponent>();
    UBreakerCharacterMovementComponent* Movement = Player->GetBreakerMovement();
    if (!Momentum || !Movement) return false;
    Momentum->BeginPlay(); // Binds actual purchase/death delegates; character never reads a save.
    Momentum->SetComponentTickEnabled(false); Movement->SetComponentTickEnabled(false);
    Movement->bRunPhysicsWithNoController = true;
    const float HalfHeight = Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    auto Clock = [&](float Step) { ++GFrameCounter; World->Tick(LEVELTICK_All, Step); };
    auto Advance = [&](float Seconds, bool bPay = true)
    {
        const float Start = World->GetTimeSeconds();
        for (float Elapsed = 0; Elapsed + KINDA_SMALL_NUMBER < Seconds;)
        {
            const float Step = FMath::Min(0.05f, Seconds - Elapsed); Clock(Step);
            if (bPay) Momentum->AdvanceLoop(Step);
            Elapsed += Step;
        }
        TestTrue(TEXT("Actual world clock advanced"), World->GetTimeSeconds() >= Start + Seconds - 0.001f);
    };
    auto Prepare = [&](float Height, bool bWait = true)
    {
        Movement->SetMovementMode(MOVE_Walking); Movement->StopMovementImmediately();
        if (bWait) Advance(1.1f);
        Blocker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        TestTrue(TEXT("Actual teleport reset"), Player->TeleportTo(FVector(0, 0, HalfHeight), FRotator::ZeroRotator, false, true));
        Movement->PerformMovement(0.001f);
        const float Feet = Player->GetActorLocation().Z - HalfHeight;
        const float Top = Feet + Height;
        Obstacle->SetBoxExtent(FVector(120, 200, Top / 2));
        Obstacle->GetOwner()->SetActorLocation(FVector(180, 0, Top / 2));
        Momentum->AdvanceLoop(0.001f); // Observe relocation before the new real traversal.
        Player->GetAttributes()->ApplyClassResource(0);
    };
    auto Complete = [&](EBreakerLedgeVerb Expected)
    {
        FBreakerLedgeTraversal Found;
        if (!TestTrue(TEXT("Actual collision resolves a traversal"), Movement->ResolveLedgeTraversal(Found))) return false;
        if (!TestEqual(TEXT("Authored height chooses actual verb"), Found.Verb, Expected)) return false;
        const double Before = Movement->GetLastLedgeTraversalTime();
        if (!TestTrue(TEXT("Real movement input accepts traversal"), Movement->TryBeginLedgeTraversal())) return false;
        for (int32 Step = 0; Step < 60 && Movement->GetLastLedgeTraversalTime() <= Before; ++Step)
        {
            Clock(0.01f); Movement->PerformMovement(0.01f);
        }
        if (!TestTrue(TEXT("Swept traversal actually completes"), Movement->GetLastLedgeTraversalTime() > Before)) return false;
        TestTrue(TEXT("Actual body reaches resolved endpoint"), Player->GetActorLocation().Equals(Found.TargetLocation, 1.0f));
        Movement->SetMovementMode(MOVE_Walking); Movement->StopMovementImmediately();
        Momentum->AdvanceLoop(0.001f);
        return true;
    };
    auto Buy = [&]()
    {
        FText Failure;
        const bool bPurchased = Progression->PurchaseNode(UBreakerProgressionLibrary::GetSwiftKineticTree(), TEXT("Swift.Kinetic.Contact"), Failure);
        return TestTrue(FString::Printf(TEXT("Actual Contact purchase: %s"), *Failure.ToString()), bPurchased);
    };
    Prepare(50);
    if (!Complete(EBreakerLedgeVerb::Vault)) return false;
    Advance(0.40f);
    TestEqual(TEXT("Unowned completed vault pays only existing traversal grant"), Momentum->GetMomentum(), Momentum->LedgeTraversalGrant, 0.01f);
    // O272: Contact is a single rank at cost 1 and its one buy reads 0.70 s,
    // so the window pays 8/s x 0.70 s = 5.6 on top of the traversal grant.
    if (!Buy()) return false;
    TestEqual(TEXT("One Contact buy spends one of four points"), Progression->GetProgressionState().UnspentDoctrinePoints, 3);
    Prepare(80);
    if (!Complete(EBreakerLedgeVerb::Vault)) return false;
    // Exactly the window, so the anti-farm case below still lands inside the
    // one-second traversal gate (0.70 + 0.05 + a 0.12 s vault < 1 s).
    Advance(0.70f);
    TestEqual(TEXT("Owned vault pays six plus 8/s for exactly .70s"), Momentum->GetMomentum(), Momentum->LedgeTraversalGrant + 5.6f, 0.02f);
    const float AfterExpired = Momentum->GetMomentum(); Advance(0.05f);
    TestEqual(TEXT("Expired Contact cannot keep paying"), Momentum->GetMomentum(), AfterExpired, 0.01f);
    // A second real traversal before one second cannot re-arm either source.
    Prepare(80, false);
    if (!Complete(EBreakerLedgeVerb::Vault)) return false;
    Advance(0.40f);
    TestEqual(TEXT("Traversal anti-farm gate also gates Contact"), Momentum->GetMomentum(), 0.0f, 0.02f);
    {
        FText Refusal;
        TestFalse(TEXT("Owned Contact refuses a second purchase"), Progression->PurchaseNode(UBreakerProgressionLibrary::GetSwiftKineticTree(), TEXT("Swift.Kinetic.Contact"), Refusal));
        TestEqual(TEXT("Refused purchase leaves three of four points"), Progression->GetProgressionState().UnspentDoctrinePoints, 3);
    }
    Prepare(120);
    if (!Complete(EBreakerLedgeVerb::Mantle)) return false;
    Advance(0.75f);
    TestEqual(TEXT("Owned real mantle earns the same .70s Contact"), Momentum->GetMomentum(), Momentum->LedgeTraversalGrant + 5.6f, 0.02f);
    Prepare(120);
    if (!Complete(EBreakerLedgeVerb::Mantle)) return false;
    Advance(0.8f, false); // Actual game time passes; intentionally hitch only the resource tick.
    Momentum->AdvanceLoop(0.8f);
    TestEqual(TEXT("Hitch pays same bounded lifetime as sliced ticks"), Momentum->GetMomentum(), Momentum->LedgeTraversalGrant + 5.6f, 0.02f);
    // Three simultaneous sources must share the existing25/s budget.
    Prepare(80);
    if (!Complete(EBreakerLedgeVerb::Vault)) return false;
    Movement->SetMovementMode(MOVE_Falling);
    Player->GetCombat()->DodgeChance = 1.0f;
    FBreakerDamageRequest Dodged; Dodged.BaseDamage = 1; Dodged.bCanCritical = false;
    TestTrue(TEXT("Real dodge creates other-source grant pressure"), Player->GetCombat()->ReceiveDamage(Dodged).bDodged);
    Player->GetCombat()->DodgeChance = 0.0f;
    const float BeforeCap = Momentum->GetMomentum(); Advance(0.10f);
    TestTrue(TEXT("Contact and other income respect combined cap"), Momentum->GetMomentum() - BeforeCap <= Momentum->GlobalGenerationCap * 0.1f + 0.01f);
    // Clear queued cap-pressure through the existing inactive-class path.
    Progression->DevForceClass(EBreakerClassId::Tank);
    Movement->SetMovementMode(MOVE_Walking); Movement->StopMovementImmediately();
    const float InactiveResource = Momentum->GetMomentum(); Advance(0.2f);
    TestEqual(TEXT("Class change cancels pending Contact income"), Momentum->GetMomentum(), InactiveResource, 0.01f);
    Progression->DevForceClass(EBreakerClassId::Swift);
    const float ReturnedResource = Momentum->GetMomentum(); Advance(0.1f);
    TestEqual(TEXT("Returning to Swift cannot restore old grace"), Momentum->GetMomentum(), ReturnedResource, 0.01f);
    // Class changes can replace doctrine state, so restore only via an actual purchase when needed.
    if (Progression->GetNodeRank(TEXT("Swift.Kinetic.Contact"), EBreakerPointCurrency::DoctrinePoints) < 1 && !Buy()) return false;
    TestEqual(TEXT("Contact holds its single rank across the class round trip"), Progression->GetNodeRank(TEXT("Swift.Kinetic.Contact"), EBreakerPointCurrency::DoctrinePoints), 1);
    Prepare(40);
    FBreakerLedgeTraversal Refused;
    TestFalse(TEXT("Below authored minimum refuses"), Movement->ResolveLedgeTraversal(Refused));
    Prepare(160);
    TestFalse(TEXT("Above mantle maximum refuses"), Movement->ResolveLedgeTraversal(Refused));
    Prepare(120);
    if (!TestTrue(TEXT("Clear mantle before blocker"), Movement->ResolveLedgeTraversal(Refused))) return false;
    Blocker->GetOwner()->SetActorLocation(Refused.TargetLocation);
    Blocker->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    TestFalse(TEXT("Occupied endpoint refuses"), Movement->ResolveLedgeTraversal(Refused));
    Blocker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    const double BeforeAbort = Movement->GetLastLedgeTraversalTime();
    if (!Movement->TryBeginLedgeTraversal()) return false;
    Clock(0.01f); Movement->PerformMovement(0.01f);
    Blocker->GetOwner()->SetActorLocation(Player->GetActorLocation() + FVector(0, 0, 80));
    Blocker->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    for (int32 Step = 0; Step < 35; ++Step) { Clock(0.01f); Movement->PerformMovement(0.01f); }
    TestEqual(TEXT("Mid-traversal obstruction never records completion"), Movement->GetLastLedgeTraversalTime(), BeforeAbort);
    // Isolate post-traversal income from the normal airborne source.
    Movement->SetMovementMode(MOVE_Walking); Movement->StopMovementImmediately();
    Momentum->AdvanceLoop(0.35f); Advance(0.75f);
    TestEqual(TEXT("Aborted traversal grants no Contact"), Momentum->GetMomentum(), 0.0f, 0.01f);
    Prepare(80);
    if (!Complete(EBreakerLedgeVerb::Vault)) return false;
    Advance(0.4f); // Existing6 grant has drained; Contact still has .3s remaining.
    const uint32 BeforeTeleportSerial = Movement->GetTraversalInvalidationSerial();
    // Land on the actual floor. Keeping the ledge's height would correctly
    // enter Falling and earn the separate ordinary8/s airborne income.
    const FVector GroundDestination(Player->GetActorLocation().X + 500.0f, 0, HalfHeight + 2.0f);
    TestTrue(TEXT("Actual teleport out of Contact"), Player->TeleportTo(GroundDestination, FRotator::ZeroRotator, false, true));
    TestTrue(TEXT("Real teleport immediately invalidates traversal income"), Movement->GetTraversalInvalidationSerial() != BeforeTeleportSerial);
    TestTrue(TEXT("Grounded destination excludes ordinary airborne income"), Movement->IsMovingOnGround());
    const float BeforeTeleport = Momentum->GetMomentum(); Advance(0.20f);
    TestEqual(TEXT("Teleport cannot continue Contact even before next movement tick"), Momentum->GetMomentum(), BeforeTeleport, 0.01f);
    Prepare(80);
    if (!Complete(EBreakerLedgeVerb::Vault)) return false;
    FBreakerDamageRequest Death; Death.BaseDamage = 1000000; Death.bCanCritical = false;
    Player->GetCombat()->ReceiveDamage(Death);
    TestTrue(TEXT("Actual death"), Player->GetCombat()->IsDead());
    const float BeforeDeath = Momentum->GetMomentum(); Advance(0.2f);
    TestEqual(TEXT("Dead Swift earns no pending or Contact income"), Momentum->GetMomentum(), BeforeDeath, 0.01f);
    Player->GetCombat()->RestoreVitals();
    const float AfterRevive = Momentum->GetMomentum(); Advance(0.2f);
    TestEqual(TEXT("Revive cannot restore old Contact window"), Momentum->GetMomentum(), AfterRevive, 0.01f);
    Prepare(80);
    if (!Complete(EBreakerLedgeVerb::Vault)) return false;
    Advance(0.4f);
    FText RespecFailure;
    if (!TestTrue(TEXT("Actual doctrine respec"), Progression->RespecAtForge(EBreakerPointCurrency::DoctrinePoints, true, RespecFailure))) return false;
    const float AfterRespec = Momentum->GetMomentum(); Advance(0.2f);
    TestEqual(TEXT("Respec cancels active Contact grace"), Momentum->GetMomentum(), AfterRespec, 0.01f);
    TestEqual(TEXT("The spent point returns to the four-point wallet"), Progression->GetProgressionState().UnspentDoctrinePoints, 4);
    return true;
}
#endif
