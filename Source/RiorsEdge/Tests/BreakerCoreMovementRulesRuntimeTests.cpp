#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerRiftDisplacement.h"
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
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerCoreMovementRulesRuntimeTest, "RiorsEdge.Movement.CoreRulesRuntime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerCoreMovementRulesRuntimeTest::RunTest(const FString& Parameters)
{
    const FVector EndpointStart(0, 0, 90.448f), EndpointTarget(124, 0, 171);
    TestTrue(TEXT("Nonintegral ascending path has exact terminal endpoint"),
        UBreakerCharacterMovementComponent::LedgeTraversalLocation(EndpointStart, EndpointTarget, 1.0f).Equals(EndpointTarget, 0.0f));
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
    auto* Class = NewObject<UBreakerClassDefinition>(); Class->ClassId = EBreakerClassId::Swift;
    auto* Tree = NewObject<UBreakerProgressionTree>(Class); Tree->TreeId = TEXT("Test.Core.Movement");
    Tree->Currency = EBreakerPointCurrency::CorePoints; Class->BranchTrees.Add(Tree);
    auto* Node = NewObject<UBreakerProgressionNode>(Tree); Node->NodeId = TEXT("Test.Core.Movement.Lanes");
    Node->Currency = Tree->Currency; Tree->Nodes.Add(Node);
    Node->GrantedTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Progression.Node.Core.PhantomStep")));
    auto* Immovable=NewObject<UBreakerProgressionNode>(Tree); Immovable->NodeId=TEXT("Test.Core.Immovable"); Immovable->Currency=Tree->Currency;
    Immovable->GrantedTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Progression.Node.Core.Immovable"))); Tree->Nodes.Add(Immovable);
    if (!Progression->ChoosePermanentClass(Class)) return false;
    Progression->AwardExperience(UBreakerExperienceLibrary::TotalXpToReachLevel(3, Progression->ExperienceCurve));
    auto* Move = Player->GetBreakerMovement(); Move->SetComponentTickEnabled(false); Move->bRunPhysicsWithNoController = true;
    Move->RefreshJumpGrant();
    const float HalfHeight = Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    auto Clock = [&](float Step) { ++GFrameCounter; World->Tick(LEVELTICK_All, Step); Move->PerformMovement(Step); };
    auto Ground = [&](float X)
    {
        Player->StopJumping(); Move->SetMovementMode(MOVE_Walking); Move->StopMovementImmediately();
        Player->TeleportTo(FVector(X, 0, HalfHeight), FRotator::ZeroRotator, false, true); Move->PerformMovement(.001f);
    };
    FText Reason;
    if(!Progression->PurchaseNode(Tree,Node->NodeId,Reason))return false;
    int32 Completions=0; Move->OnLedgeTraversalCompleted.AddLambda([&](EBreakerLedgeVerb){++Completions;});
    Ground(0); FBreakerLedgeTraversal Found;
    if(!TestTrue(TEXT("Actual obstacle resolves"),Move->ResolveLedgeTraversal(Found)))return false;
    if(!TestTrue(TEXT("Real input queues PhantomStep"),Move->TryBeginLedgeTraversal()))return false;
    Clock(.001f);
    TestEqual(TEXT("One native update completes exactly once"),Completions,1);
    TestFalse(TEXT("Instant traversal has no remaining custom movement"),Move->IsTraversingLedge());
    TestTrue(TEXT("Swept instant route reaches actual endpoint"),Player->GetActorLocation().Equals(Found.TargetLocation,1));
    Ground(-500); auto* Combat=Player->GetCombat();
    TestTrue(TEXT("Actual stagger applied before purchase"),Combat->ApplyStagger(2));
    if(!Progression->PurchaseNode(Tree,Immovable->NodeId,Reason))return false;
    TestTrue(TEXT("Immovable grants live stagger immunity"),Combat->IsStaggerImmune());
    TestFalse(TEXT("Owned immunity immediately hides existing stagger"),Combat->IsStaggered());
    Move->SetSprinting(true); Move->SetSlideRequested(true);
    TestFalse(TEXT("Sprint request refused"),Move->IsSprinting());
    TestFalse(TEXT("Swift dash entitlement forfeited"),Move->CanUseDash());
    TestEqual(TEXT("Speed capped to composed walking"),Move->GetMaxSpeed(),Move->GetWalkSpeedCap(),.001f);
    Move->Launch(FVector(2000,0,1000)); Move->AddImpulse(FVector(2000,0,1000),true); Clock(.01f);
    TestTrue(TEXT("Forced launch and impulse cannot accelerate owner"),Move->Velocity.Size2D()<=Move->GetWalkSpeedCap()+.01f);
    TestFalse(TEXT("Force cannot launch owner airborne"),Move->IsFalling());
    TestEqual(TEXT("Direct displacement entry respects owned immunity"),BreakerRiftDisplacement::Apply(Player,Player->GetActorLocation()-FVector(100,0,0),200),0.f);
    Player->Jump(); Player->CheckJumpInput(.01f); TestTrue(TEXT("Ordinary jump retained"),Move->Velocity.Z>0); Player->StopJumping();
    Ground(0); if(!Move->ResolveLedgeTraversal(Found) || !Move->TryBeginLedgeTraversal())return false;
    const int32 Before=Completions; Clock(.001f);
    TestTrue(TEXT("Immovable takes precedence over instantaneous traverse"),Move->IsTraversingLedge());
    for(int32 I=0;I<1000 && Move->IsTraversingLedge();++I)
    {
        const FVector Previous=Player->GetActorLocation(); Clock(.005f);
        TestTrue(TEXT("Every traversal step remains at walking pace"),FVector::Dist(Previous,Player->GetActorLocation())<=Move->GetWalkSpeedCap()*.005f+.2f);
    }
    if (Completions != Before+1) AddError(FString::Printf(TEXT("Slow traversal stopped at %s, target %s, mode %d, speed %.3f"), *Player->GetActorLocation().ToString(), *Found.TargetLocation.ToString(), int32(Move->MovementMode), Move->GetWalkSpeedCap()));
    TestEqual(TEXT("Forfeited instant traversal completes normally once"),Completions,Before+1);
    TestTrue(TEXT("Slower swept route still reaches endpoint"),Player->GetActorLocation().Equals(Found.TargetLocation,1));
    for(int32 I=0;I<50;++I) Clock(.05f); // Let the original pre-purchase stagger expire naturally.
    if(!Progression->RespecCore(Reason))return false;
    Ground(-500); Move->SetSprinting(true); TestTrue(TEXT("Respec restores sprint"),Move->IsSprinting());
    TestTrue(TEXT("Respec restores innate Swift dash"),Move->CanUseDash());
    return true;
}

// ---------------------------------------------------------------------------
// O283 on a live body: a speed the rules take away BLEEDS, and a refused
// crouch press is CONSUMED. The rig above, cut to what a grounded body needs
// — a floor, a pawn, the attribute set bound — with one difference that is
// the whole point: the movement component is driven through its OWN
// TickComponent, because the bleed step and the latent-slide check both live
// after Super::TickComponent, and PerformMovement alone never reaches them.
// Pure-maths tests (RiorsEdge.Movement.MomentumSentence) prove the bleed's
// arithmetic; these prove the two O283 latch sites actually arm it.
// ---------------------------------------------------------------------------
namespace
{
    struct FBreakerO283GroundedRig
    {
        UWorld* World = nullptr;
        ABreakerCharacter* Player = nullptr;
        UBreakerCharacterMovementComponent* Move = nullptr;
        uint64 InitialFrame = 0;
        float HalfHeight = 0.0f;

        bool Build()
        {
            UWorld::InitializationValues Init; Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
            World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
            if (!World) return false;
            GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
            World->InitializeActorsForPlay(FURL());
            InitialFrame = GFrameCounter;
            auto* Floor = World->SpawnActor<AActor>(); auto* Shape = NewObject<UBoxComponent>(Floor);
            Floor->AddInstanceComponent(Shape); Floor->SetRootComponent(Shape); Shape->SetBoxExtent(FVector(4000, 4000, 10));
            Shape->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics); Shape->SetCollisionResponseToAllChannels(ECR_Block);
            Shape->RegisterComponent(); Floor->SetActorLocation(FVector(0, 0, -10));
            Player = World->SpawnActor<ABreakerCharacter>(FVector(0, 0, 200), FRotator::ZeroRotator);
            if (!Player) return false;
            Player->SetActorTickEnabled(false);
            auto* Attr = Player->GetAttributes(); auto* ASC = Player->GetAbilitySystemComponent();
            ASC->InitAbilityActorInfo(Player, Player); ASC->AddAttributeSetSubobject(Attr);
            Player->GetCombat()->BindAttributes(Attr); Player->GetCombat()->SetComponentTickEnabled(false);
            Player->GetProgression()->BindAttributes(Attr);
            Move = Player->GetBreakerMovement(); Move->SetComponentTickEnabled(false); Move->bRunPhysicsWithNoController = true;
            HalfHeight = Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
            return true;
        }
        void Teardown()
        {
            if (!World) return;
            World->DestroyWorld(false); GEngine->DestroyWorldContext(World); GFrameCounter = InitialFrame; World = nullptr;
        }
        // Grounded on the floor, walking mode resolved against it, carrying
        // exactly PlanarSpeed forward. Written after the floor is found so the
        // resolve's own StopMovementImmediately cannot eat it.
        void Ground(float PlanarSpeed)
        {
            Player->StopJumping(); Move->SetMovementMode(MOVE_Walking); Move->StopMovementImmediately();
            Player->TeleportTo(FVector(0, 0, HalfHeight), FRotator::ZeroRotator, false, true); Move->PerformMovement(.001f);
            Move->Velocity = FVector(PlanarSpeed, 0, 0);
        }
        // One movement frame through the component's own tick, no input held.
        void Frame(float Step)
        {
            ++GFrameCounter; World->Tick(LEVELTICK_All, Step); Move->TickComponent(Step, LEVELTICK_All, nullptr);
        }
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerSprintExitBleedsRuntimeTest, "RiorsEdge.Movement.SprintExitBleeds",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerSprintExitBleedsRuntimeTest::RunTest(const FString& Parameters)
{
    FBreakerO283GroundedRig Rig;
    if (!Rig.Build()) { Rig.Teardown(); return false; }
    ON_SCOPE_EXIT { Rig.Teardown(); };
    auto* Move = Rig.Move;
    const float Sprint = Move->SprintSpeed;

    Rig.Ground(Sprint);
    Move->SetSprinting(true);
    if (!TestTrue(TEXT("The body is on the ground"), Move->IsMovingOnGround())) return false;
    TestEqual(TEXT("A plain sprint arms no ceiling"), Move->GetBoostedSpeedCeiling(), 0.0f);
    TestEqual(TEXT("Sprinting, the cap is the sprint cap"), Move->GetMaxSpeed(), Move->GetSprintSpeedCap(), .01f);

    // THE LEAVING-SPRINT EDGE. The cap has just dropped from the sprint cap to
    // the walk cap under a body carrying sprint speed; before O283 the engine
    // braked the whole gap off in about two frames. Now the speed the body has
    // becomes the ceiling THIS frame, and the bleed walks it down.
    Move->SetSprinting(false);
    TestEqual(TEXT("Releasing sprint latches the body's speed as the ceiling, the same frame"),
        Move->GetBoostedSpeedCeiling(), Sprint, .01f);
    TestEqual(TEXT("and the cap holds at that speed, the same frame"), Move->GetMaxSpeed(), Sprint, .01f);

    // Halfway through the window the ceiling is halfway down the gap: 1039
    // toward 672 over 0.5 s reads ~855 at 0.25 s. Five percent absorbs the
    // frame quantisation, never a different rule.
    float Elapsed = 0.0f;
    while (Elapsed < 0.25f - .0001f) { Rig.Frame(.01f); Elapsed += .01f; }
    TestEqual(TEXT("A quarter second in, the ceiling has bled half the gap"),
        Move->GetBoostedSpeedCeiling(), (Move->SprintSpeed + Move->WalkSpeed) * .5f, (Move->SprintSpeed + Move->WalkSpeed) * .025f);
    TestTrue(TEXT("and the cap is still above the walk cap"), Move->GetMaxSpeed() > Move->GetWalkSpeedCap() + 1.0f);

    // Past the window the ceiling is spent and the walk cap owns the answer.
    while (Elapsed < 0.6f - .0001f) { Rig.Frame(.01f); Elapsed += .01f; }
    TestEqual(TEXT("Past the window the ceiling is the no-boost sentinel"), Move->GetBoostedSpeedCeiling(), 0.0f);
    TestEqual(TEXT("and the cap is exactly the walk cap"), Move->GetMaxSpeed(), Move->GetWalkSpeedCap(), .01f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerRefusedSlideIsConsumedRuntimeTest, "RiorsEdge.Movement.RefusedSlideIsConsumed",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerRefusedSlideIsConsumedRuntimeTest::RunTest(const FString& Parameters)
{
    FBreakerO283GroundedRig Rig;
    if (!Rig.Build()) { Rig.Teardown(); return false; }
    ON_SCOPE_EXIT { Rig.Teardown(); };
    auto* Move = Rig.Move;
    if (!TestTrue(TEXT("The shipped entry is above the speed this test walks at"), Move->SlideEntrySpeed > 600.0f)) return false;

    // A crouch pressed at walking pace: refused, and the press is SPENT. It
    // used to stay live, so the slide fired whenever a later frame's speed
    // crossed the entry — a slide the player never asked for at that moment.
    Rig.Ground(600.0f);
    if (!TestTrue(TEXT("The body is on the ground"), Move->IsMovingOnGround())) return false;
    Move->SetSlideRequested(true);
    TestTrue(TEXT("The press arms before the rules answer it"), Move->IsSlideRequestArmed());
    TestFalse(TEXT("A crouch under the entry speed is refused"), Move->BeginSlide());
    TestFalse(TEXT("The refused press is consumed, not left armed"), Move->IsSlideRequestArmed());
    TestTrue(TEXT("The key is still held"), Move->IsSlideRequested());
    TestFalse(TEXT("Nothing slid"), Move->IsSliding());

    // The speed later crosses the entry with the key still held: the spent
    // press must not lie in wait for it.
    Move->Velocity = FVector(1000.0f, 0, 0);
    Rig.Frame(.01f);
    TestFalse(TEXT("A later frame over the entry does not fire the spent press"), Move->IsSliding());
    TestFalse(TEXT("and the press stays spent"), Move->IsSlideRequestArmed());

    // The next slide needs the next press: released and pressed again over
    // the entry, the same key slides — and that slide spends its own press.
    Move->SetSlideRequested(false);
    Move->SetSlideRequested(true);
    Move->Velocity = FVector(1000.0f, 0, 0);
    TestTrue(TEXT("A fresh press over the entry slides"), Move->BeginSlide());
    TestTrue(TEXT("The slide is live"), Move->IsSliding());
    TestFalse(TEXT("The slide it produced spent the press"), Move->IsSlideRequestArmed());
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerGroundStopRuntimeTest, "RiorsEdge.Movement.GroundStopDistance",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerGroundStopRuntimeTest::RunTest(const FString& Parameters)
{
    FBreakerO283GroundedRig Rig;
    if (!Rig.Build()) { Rig.Teardown(); return false; }
    ON_SCOPE_EXIT { Rig.Teardown(); };
    auto* Move = Rig.Move;
    const float AuthoredBrake = Move->BrakingDecelerationWalking;
    TestEqual(TEXT("Ground stop ships with firmer braking"), AuthoredBrake,3200.f,.001f);
    auto Stop = [&](float Speed,float Brake)
    {
        Move->BrakingDecelerationWalking = Brake;
        Rig.Ground(Speed);
        const FVector Start = Rig.Player->GetActorLocation();
        float Elapsed = 0;
        while (Move->Velocity.Size2D() > 1.f && Elapsed < 1.f) { Rig.Frame(1.f/120.f); Elapsed += 1.f/120.f; }
        TestTrue(TEXT("Grounded release stops within a second"), Move->Velocity.Size2D() <= 1.f);
        const float Distance = FVector::Dist2D(Start,Rig.Player->GetActorLocation());
        AddInfo(FString::Printf(TEXT("speed=%.1f brake=%.1f stopSeconds=%.3f stopCm=%.2f"),Speed,Brake,Elapsed,Distance));
        return Distance;
    };
    // Previous shipped settings are a diagnostic control, with identical live
    // movement physics. No inventory, tree points or combat resources are granted.
    const float OldWalk = Stop(672.f,2400.f), OldSprint = Stop(1039.f,2400.f);
    const float Walk = Stop(Move->WalkSpeed,AuthoredBrake), Sprint = Stop(Move->SprintSpeed,AuthoredBrake);
    TestTrue(TEXT("Walking drifts less after release"),Walk < OldWalk);
    TestTrue(TEXT("Sprinting drifts less after release"),Sprint < OldSprint);
    TestTrue(TEXT("Sprint still reaches the slide entry threshold"),Move->SprintSpeed > Move->SlideEntrySpeed);
    return true;
}

#endif