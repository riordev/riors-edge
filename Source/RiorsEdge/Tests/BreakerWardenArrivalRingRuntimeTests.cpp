#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerRangedBehavior.h"
#include "Combat/BreakerWardenEnemy.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

#if WITH_DEV_AUTOMATION_TESTS

// ---------------------------------------------------------------------------
// THE WARDEN'S WIRING PIN — the half the ring extraction owed.
//
// RiorsEdge.Combat.EnemySeams.ArrivalRing already pins the ring's shipped
// CONFIGURATION on every closing archetype. It cannot see the defect that
// actually shipped: ABreakerWardenEnemy::TickEngagedBehaviour replaces the base
// chase wholesale and never calls Super, so the ring's ten lines simply did not
// run for this class and the body closed at full speed and walked through the
// player. Every configuration assertion passed the whole time.
//
// So this test runs the real engaged frame and reads the two things only a
// called ring can produce.
//
// THE TARGET IS A REAL ABreakerCharacter, AND THE DESK'S ANSWER WAS WRONG.
// The recorded plan was a bare AActor, on the grounds that ResolveSweep and
// ResolveSlam both bail without a combat component. They do — but it never gets
// that far: ABreakerEnemy::IsEligibleThreatTarget refuses anything that is not
// an ABreakerCharacter or an ABreakerDeployable carrying a live combat
// component, so a bare AActor is never selected as a threat and
// TickEngagedBehaviour is never called with it at all.
//
// NOTHING IS GRANTED TO KEEP IT ALIVE EITHER, which is what killed the earlier
// fixture. Both attacks are simply never armed: the sweep needs the body inside
// SweepRangeCm (320) and the slam inside SlamRadiusCm (650), so the ADVANCE leg
// below runs entirely outside 650 and asserts it stayed there. The target holds
// its shipped default health and takes not one point of damage.
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerWardenArrivalRingRuntimeTest,
    "RiorsEdge.Combat.Warden.ArrivalRingRuntime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerWardenArrivalRingRuntimeTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Init;
    Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true,
        ERHIFeatureLevel::Num, &Init);
    if (!TestNotNull(TEXT("isolated engagement world"), World)) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL());
    const uint64 Frame = GFrameCounter;
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); GFrameCounter = Frame; };

    // The stand-in the Warden will actually accept as a threat, at its shipped
    // health. It never moves and never ticks: it is the thing being approached,
    // not a second body under test.
    ABreakerCharacter* Player = World->SpawnActor<ABreakerCharacter>();
    if (!TestNotNull(TEXT("a real threat target"), Player)) return false;
    Player->GetAbilitySystemComponent()->InitAbilityActorInfo(Player, Player);
    Player->GetAbilitySystemComponent()->AddAttributeSetSubobject(Player->GetAttributes());
    Player->GetCombat()->BindAttributes(Player->GetAttributes());
    Player->SetActorTickEnabled(false);
    Player->SetActorLocation(FVector::ZeroVector);
    const float StartHealth = Player->GetAttributes()->GetHealth();
    if (!TestTrue(TEXT("the stand-in is alive, so it is eligible"), StartHealth > 0.0f)) return false;

    ABreakerWardenEnemy* Warden = World->SpawnActor<ABreakerWardenEnemy>();
    if (!TestNotNull(TEXT("a Warden"), Warden)) return false;
    Warden->ConfigureCrowdProbe();
    Warden->DispatchBeginPlay();

    const float Ring = Warden->GetAttackRange();
    const float Inner = Ring * Warden->GetArrivalInnerRatio();

    // ---- OUTSIDE THE RING, AND OUT OF BOTH ATTACKS -----------------------
    // This leg runs FIRST and the order is load-bearing: closing inside slam
    // radius arms a 0.9s wind-up that survives into later frames and plants the
    // body wherever it is next placed, so an approach measured after it would
    // measure a slam instead of a ring.
    //
    // Far enough out that neither the slam (650) nor the sweep (320) can arm,
    // so the frames measured here are exactly the frames the pass-through
    // happened on: no attack armed, the ring the only thing writing a
    // direction.
    const FVector Start(900.0f, 0.0f, 0.0f);
    Warden->SetActorLocation(Start);
    ++GFrameCounter;
    World->Tick(LEVELTICK_All, 0.02f);
    // A guard rather than the proof — Advance is also the band's default, so
    // this one fact would read the same on a body that never called the ring.
    // The alignment measurement below is what only a called ring can produce.
    TestEqual(TEXT("beyond the ring the band advances"),
        Warden->GetArrivalBand(), EBreakerRangedBand::Advance);

    for (int32 Step = 0; Step < 20; ++Step)
    {
        ++GFrameCounter;
        World->Tick(LEVELTICK_All, 0.02f);
    }

    const FVector Ended = Warden->GetActorLocation();
    const float EndDistance = FVector::Dist2D(Ended, Player->GetActorLocation());
    TestTrue(TEXT("it closes on the player"), EndDistance < Start.Size2D());
    // The claim that this leg cost the target nothing, asserted rather than
    // assumed: it never entered slam radius, and the health is untouched.
    TestTrue(TEXT("and never enters slam radius"), EndDistance > 650.0f);
    TestEqual(TEXT("so the stand-in takes no damage"), Player->GetAttributes()->GetHealth(), StartHealth);

    // THE ARRIVAL ANGLE, which is the ring's other half and the reason two
    // closers stop stacking on one line: Advance walks to a point ON the ring,
    // offset by ArrivalOffsetDeg, not to the player. So the travelled direction
    // must be measurably off the straight line. A body that inherited nothing
    // walks dead at the target and scores 1.0 here.
    const FVector Travelled = (Ended - Start).GetSafeNormal2D();
    const FVector Straight = (Player->GetActorLocation() - Start).GetSafeNormal2D();
    if (!TestTrue(TEXT("the body actually moved"), !Travelled.IsNearlyZero())) return false;
    const float Alignment = FVector::DotProduct(Travelled, Straight);
    TestTrue(FString::Printf(TEXT("it approaches off-axis, not straight down the line (%.3f)"), Alignment),
        Alignment < 0.99f);
    TestTrue(FString::Printf(TEXT("but is still closing rather than circling (%.3f)"), Alignment),
        Alignment > 0.5f);

    // ---- INSIDE THE RING -------------------------------------------------
    // The other side of the band, and the assertion that cannot be produced by
    // accident: Retreat is not the default, and ArrivalBand is written in
    // exactly one statement inside ApplyArrivalRing. A body already closer than
    // the inner edge must be told to back off. Before the extraction this frame
    // produced a full-speed vector at the player's chest instead.
    //
    // Last, because the slam arms at this distance and the wind-up it leaves
    // behind would plant every frame after it.
    Warden->SetActorLocation(FVector(Inner * 0.65f, 0.0f, 0.0f));
    ++GFrameCounter;
    World->Tick(LEVELTICK_All, 0.02f);
    TestEqual(TEXT("inside the inner edge the ring says back off"),
        Warden->GetArrivalBand(), EBreakerRangedBand::Retreat);
    return true;
}

#endif   // WITH_DEV_AUTOMATION_TESTS
