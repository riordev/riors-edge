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
#endif