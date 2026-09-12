#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerBossEnemy.h"
#include "Combat/BreakerBossPhases.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerHoldfastEnemy.h"
#include "Combat/BreakerRangedBehavior.h"
#include "Combat/BreakerRangedEnemy.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Movement/BreakerCharacterMovementComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

// ---------------------------------------------------------------------------
// THE STANDOFF RING (O273), cycle A: the ring only.
//
// A boss holds a ring and faces the player; it closes only to punish, and the
// slam is its only closer. The pure half below pins the shipped configuration
// against the classifier the ring reuses: where the three bands fall, that
// the ring sits between the slam and the Lattice's band, and that the phase
// library hands the ring back unchanged until cycle C. The runtime half runs
// the real engaged frame and reads the things only a running ring produces.
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerBossHoldRingTest,
    "RiorsEdge.Combat.Boss.HoldRing",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerBossHoldRingTest::RunTest(const FString& Parameters)
{
    const ABreakerRangedEnemy* Lattice = GetDefault<ABreakerRangedEnemy>();
    if (!TestNotNull(TEXT("Lattice CDO"), Lattice)) return false;

    // Both shipped bosses: the Field Marshal and the Holdfast, which is the
    // same machine on a Vestige body and inherits the ring.
    for (const ABreakerBossEnemy* Boss : { static_cast<const ABreakerBossEnemy*>(GetDefault<ABreakerBossEnemy>()),
                                            static_cast<const ABreakerBossEnemy*>(GetDefault<ABreakerHoldfastEnemy>()) })
    {
        if (!TestNotNull(TEXT("boss CDO"), Boss)) return false;
        const FString Name = Boss->GetClass()->GetName();
        const float Ring = Boss->GetHoldRingCm();
        const float Inner = Boss->SweepRangeCm;
        const float Hysteresis = Boss->HoldRingHysteresisCm;
        const EBreakerRangedBand Start = Boss->GetHoldBand();

        auto Band = [&](float Distance)
        {
            return UBreakerRangedBehaviorLibrary::ClassifyBand(Distance, Inner, Ring, Hysteresis, Start);
        };

        // The three bands, from the default-constructed state.
        TestEqual(*FString::Printf(TEXT("%s: 600 is inside the ring and outside the sweep: Hold"), *Name),
            Band(600.0f), EBreakerRangedBand::Hold);
        TestEqual(*FString::Printf(TEXT("%s: 1200 is beyond the ring: Advance"), *Name),
            Band(1200.0f), EBreakerRangedBand::Advance);
        TestEqual(*FString::Printf(TEXT("%s: 300 is inside the sweep: Retreat"), *Name),
            Band(300.0f), EBreakerRangedBand::Retreat);
        // THE INNER EDGE IS THE SWEEP, NOT THE SLAM. A player at 400 is inside
        // the slam's 650 and outside the sweep's 320, and O273 wants that
        // player stood at and punished, not walked at.
        TestTrue(*FString::Printf(TEXT("%s: the slam reaches past the sweep"), *Name),
            Boss->SlamRadiusCm > Inner);
        TestEqual(*FString::Printf(TEXT("%s: 400, inside the slam and outside the sweep, is Hold"), *Name),
            Band(400.0f), EBreakerRangedBand::Hold);
        TestEqual(*FString::Printf(TEXT("%s: a fresh body is walking, not standing"), *Name),
            Start, EBreakerRangedBand::Advance);

        // The ring sits between the slam and the Lattice's band: above the
        // slam so the boss never holds a station its only closer cannot reach,
        // and under the Lattice's minimum EVEN AT ITS WIDEST (ring plus
        // hysteresis) so the phase-2 galleries keep their band outside the
        // boss's ring.
        TestTrue(*FString::Printf(TEXT("%s: the ring is wider than the slam (%.0f > %.0f)"), *Name, Ring, Boss->SlamRadiusCm),
            Ring > Boss->SlamRadiusCm);
        TestTrue(*FString::Printf(TEXT("%s: the ring is under the Lattice's minimum (%.0f < %.0f)"), *Name, Ring, Lattice->MinEngagementDistance),
            Ring < Lattice->MinEngagementDistance);
        TestTrue(*FString::Printf(TEXT("%s: the ring at its widest does not enter the Lattice's band (%.0f + %.0f <= %.0f)"), *Name, Ring, Boss->HoldRingHysteresisCm, Lattice->MinEngagementDistance),
            Ring + Boss->HoldRingHysteresisCm <= Lattice->MinEngagementDistance);
        // The deadband the header promises is the deadband that runs: the
        // classifier clamps hysteresis to half the band, and a clamped value
        // would make the authored number a lie.
        TestTrue(*FString::Printf(TEXT("%s: the hysteresis is not clamped by the classifier"), *Name),
            Hysteresis > 0.0f && Hysteresis <= (Ring - Inner) * 0.5f);

        // Cycle A: identity in every phase, from the shipped params and from
        // default-constructed ones.
        const FBreakerBossPhaseParams Defaults;
        for (EBreakerBossPhase Phase : { EBreakerBossPhase::Deployment, EBreakerBossPhase::Suppression, EBreakerBossPhase::Commitment })
        {
            const FString PhaseName = UBreakerBossPhaseLibrary::GetPhaseName(Phase);
            TestEqual(*FString::Printf(TEXT("%s: %s holds the authored ring (shipped params)"), *Name, *PhaseName),
                UBreakerBossPhaseLibrary::GetPhaseHoldRing(Phase, Boss->HoldRingCm, Boss->PhaseParams), Boss->HoldRingCm, 0.0001f);
            TestEqual(*FString::Printf(TEXT("%s: %s holds the authored ring (default params)"), *Name, *PhaseName),
                UBreakerBossPhaseLibrary::GetPhaseHoldRing(Phase, Boss->HoldRingCm, Defaults), Boss->HoldRingCm, 0.0001f);
        }
        TestEqual(*FString::Printf(TEXT("%s: the accessor is the library's answer"), *Name),
            Ring, Boss->HoldRingCm, 0.0001f);
    }
    return true;
}

// ---------------------------------------------------------------------------
// THE WIRING PIN, on the pattern of RiorsEdge.Combat.Warden.ArrivalRingRuntime.
//
// The pure test above cannot see the defect the ring exists to fix: the
// Warden's engaged tick walks at the player whenever it is outside sweep range,
// and the boss inherits that tick. So this runs the real engaged frame on a
// Field Marshal against a real ABreakerCharacter stand-in — the only thing the
// threat selector accepts — and reads the body's position, label and facing.
//
// NOTHING IS GRANTED. The stand-in holds its shipped default health with the
// shipped zero dodge and block, so the one slam that lands below is the game's
// own slam at the game's own number, and the legs that must cost nothing are
// asserted to cost nothing.
//
// The legs run in this order because the slam leaves state: a wind-up that
// plants every frame after it, and a LastSlamTime that keeps the boss off the
// slam for the shipped cooldown. The finding placed the hold leg at 600; a
// fresh boss at 600 is inside SlamRadiusCm with its slam off cooldown and
// SLAMS — that is the ruling working, not the ring failing — so the first
// hold is measured at 680 and 600 is measured after the slam has been spent.
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerBossHoldRingRuntimeTest,
    "RiorsEdge.Combat.Boss.HoldRingRuntime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerBossHoldRingRuntimeTest::RunTest(const FString& Parameters)
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

    // The stand-in. Its movement tick is off as well as its actor tick: a
    // character left to its own gravity in a floorless world falls away from
    // the slam's 3D radius, and a slam that misses for that reason would read
    // as a ring that never punished.
    ABreakerCharacter* Player = World->SpawnActor<ABreakerCharacter>();
    if (!TestNotNull(TEXT("a real threat target"), Player)) return false;
    Player->GetAbilitySystemComponent()->InitAbilityActorInfo(Player, Player);
    Player->GetAbilitySystemComponent()->AddAttributeSetSubobject(Player->GetAttributes());
    Player->GetCombat()->BindAttributes(Player->GetAttributes());
    Player->GetCombat()->BeginPlay();
    Player->SetActorTickEnabled(false);
    Player->GetBreakerMovement()->SetComponentTickEnabled(false);
    Player->SetActorLocation(FVector::ZeroVector);
    const float StartHealth = Player->GetAttributes()->GetHealth();
    if (!TestTrue(TEXT("the stand-in is alive, so it is eligible"), StartHealth > 0.0f)) return false;

    ABreakerBossEnemy* Boss = World->SpawnActor<ABreakerBossEnemy>(ABreakerBossEnemy::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator);
    if (!TestNotNull(TEXT("a Field Marshal"), Boss)) return false;
    if (UBreakerAttributeSet* BossAttributes = Cast<UBreakerAttributeSet>(Boss->GetDefaultSubobjectByName(TEXT("Attributes"))))
    {
        Boss->GetAbilitySystemComponent()->AddAttributeSetSubobject(BossAttributes);
    }
    Boss->ConfigureCrowdProbe();
    Boss->DispatchBeginPlay();

    const float Ring = Boss->GetHoldRingCm();
    const float Slam = Boss->SlamRadiusCm;
    const float Sweep = Boss->SweepRangeCm;

    // Facing the target before every leg. A spawn faces +X, which is straight
    // away from a player at the origin, and TURN BEFORE WALK stands a body
    // that is 180 degrees off until it has come round — longer than any leg
    // here. The ring is what is measured; the turn is another test's.
    auto Place = [&](float DistanceCm)
    {
        const FVector At(DistanceCm, 0.0f, 0.0f);
        Boss->SetActorLocation(At);
        Boss->SetActorRotation((Player->GetActorLocation() - At).GetSafeNormal2D().Rotation());
    };
    auto TickFrames = [&](int32 Count)
    {
        for (int32 Step = 0; Step < Count; ++Step)
        {
            ++GFrameCounter;
            World->Tick(LEVELTICK_All, 0.02f);
        }
    };
    auto DistanceNow = [&]() { return FVector::Dist2D(Boss->GetActorLocation(), Player->GetActorLocation()); };
    auto FacingDot = [&]()
    {
        const FVector ToPlayer = (Player->GetActorLocation() - Boss->GetActorLocation()).GetSafeNormal2D();
        return static_cast<float>(FVector::DotProduct(Boss->GetActorForwardVector().GetSafeNormal2D(), ToPlayer));
    };

    // ---- HOLD, outside the slam ------------------------------------------
    // Inside the ring, outside both attacks: the frames the Warden used to
    // walk on. It must stand, face, and say so. A FRESH body is Advance and
    // the classifier's hysteresis keeps it walking until ring minus the
    // deadband, so the stand is measured inside that edge.
    const float HoldEntry = Ring - Boss->HoldRingHysteresisCm;
    if (!TestTrue(TEXT("680 is inside the ring's entry edge and outside the slam"), 680.0f < HoldEntry && 680.0f > Slam)) return false;
    Place(680.0f);
    const float HoldStart = DistanceNow();
    TickFrames(20);
    TestEqual(TEXT("inside the ring the band holds"), Boss->GetHoldBand(), EBreakerRangedBand::Hold);
    TestTrue(FString::Printf(TEXT("it stands (moved %.1f cm)"), FMath::Abs(DistanceNow() - HoldStart)),
        FMath::Abs(DistanceNow() - HoldStart) <= 5.0f);
    TestTrue(FString::Printf(TEXT("and says so: %s"), *Boss->GetEnemyStateLabel()),
        Boss->GetEnemyStateLabel().Contains(TEXT("HOLDING")));
    TestTrue(FString::Printf(TEXT("and faces the player (%.3f)"), FacingDot()), FacingDot() > 0.99f);
    TestEqual(TEXT("holding costs the stand-in nothing"), Player->GetAttributes()->GetHealth(), StartHealth);

    // ---- ADVANCE, beyond the ring ----------------------------------------
    // Beyond the ring it is a Warden and walks. Twenty frames at the shipped
    // speed cannot carry it inside the slam from here, so this leg is free
    // and asserted to be.
    if (!TestTrue(TEXT("1200 is beyond the ring by more than its hysteresis"), 1200.0f > Ring + Boss->HoldRingHysteresisCm)) return false;
    Place(1200.0f);
    TickFrames(1);
    TestEqual(TEXT("beyond the ring the band advances"), Boss->GetHoldBand(), EBreakerRangedBand::Advance);
    TickFrames(20);
    const float AdvanceEnd = DistanceNow();
    TestTrue(FString::Printf(TEXT("it closes on the player (%.1f from 1200)"), AdvanceEnd), AdvanceEnd < 1200.0f);
    TestTrue(TEXT("and never enters slam radius"), AdvanceEnd > Slam);
    TestFalse(TEXT("a walking boss does not claim to hold"), Boss->GetEnemyStateLabel().Contains(TEXT("HOLDING")));
    TestEqual(TEXT("walking costs the stand-in nothing"), Player->GetAttributes()->GetHealth(), StartHealth);

    // ---- THE SLAM FROM HOLD ----------------------------------------------
    // 400 is inside the slam and outside the sweep. The band is Hold, and the
    // slam still arms from it: the ring cancels the walk, never the punish.
    if (!TestTrue(TEXT("400 is inside the slam and outside the sweep"), 400.0f < Slam && 400.0f > Sweep)) return false;
    Place(400.0f);
    TickFrames(1);
    TestEqual(TEXT("400 classifies as Hold"), Boss->GetHoldBand(), EBreakerRangedBand::Hold);
    TestTrue(TEXT("and the slam arms from Hold on the first frame"), Boss->IsSlamming());
    TestTrue(FString::Printf(TEXT("the label is the slam's, not the ring's: %s"), *Boss->GetEnemyStateLabel()),
        Boss->GetEnemyStateLabel().Contains(TEXT("SLAM")));
    // Past the shipped wind-up. The stand-in is inside the radius and has the
    // shipped zero dodge and block, so the slam that resolves lands.
    const int32 WindupFrames = FMath::CeilToInt(Boss->SlamWindupSeconds / 0.02f) + 10;
    TickFrames(WindupFrames);
    TestFalse(TEXT("the wind-up has resolved"), Boss->IsSlamming());
    const float AfterSlam = Player->GetAttributes()->GetHealth();
    TestTrue(FString::Printf(TEXT("the slam landed (%.1f -> %.1f)"), StartHealth, AfterSlam), AfterSlam < StartHealth);

    // ---- HOLD, inside the slam, on cooldown ------------------------------
    // The finding's 600: inside the slam radius with the slam spent, the boss
    // stands and faces. It does not walk the last 600 in; the slam is its
    // only closer and the slam is on cooldown.
    Place(600.0f);
    const float LateHoldStart = DistanceNow();
    TickFrames(20);
    TestEqual(TEXT("600 with the slam spent is Hold"), Boss->GetHoldBand(), EBreakerRangedBand::Hold);
    TestFalse(TEXT("the slam is on cooldown"), Boss->IsSlamming());
    TestTrue(FString::Printf(TEXT("it stands inside its own slam radius (moved %.1f cm)"), FMath::Abs(DistanceNow() - LateHoldStart)),
        FMath::Abs(DistanceNow() - LateHoldStart) <= 5.0f);
    TestTrue(FString::Printf(TEXT("and says so: %s"), *Boss->GetEnemyStateLabel()),
        Boss->GetEnemyStateLabel().Contains(TEXT("HOLDING")));
    TestTrue(FString::Printf(TEXT("and faces the player (%.3f)"), FacingDot()), FacingDot() > 0.99f);
    TestEqual(TEXT("holding on cooldown costs the stand-in nothing more"), Player->GetAttributes()->GetHealth(), AfterSlam);
    return true;
}

#endif   // WITH_DEV_AUTOMATION_TESTS
