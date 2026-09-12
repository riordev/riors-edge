#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerBossEnemy.h"
#include "Combat/BreakerBossPhases.h"
#include "Combat/BreakerBossProjectile.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerEnemyProjectile.h"
#include "Combat/BreakerHoldfastEnemy.h"
#include "Combat/BreakerRangedBehavior.h"
#include "Combat/BreakerRangedEnemy.h"
#include "Components/CapsuleComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Movement/BreakerCharacterMovementComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

// ---------------------------------------------------------------------------
// THE STANDOFF RING (O273).
//
// A boss holds a ring and faces the player; it closes only to punish, and the
// slam is its only closer. The pure half below pins the shipped configuration
// against the classifier the ring reuses: where the three bands fall, that
// the ring sits between the slam and the Lattice's band, and that the phase
// library hands the ring back unchanged in Deployment and Suppression and at
// 0.85 in Commitment, still above the slam. The runtime half runs the real
// engaged frame (in Deployment) and reads the things only a running ring
// produces.
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

        // Cycle C (O273): phases tighten the ring. Deployment and Suppression
        // hold the authored ring; Commitment holds 0.85 of it, which is 680
        // from the shipped 800 — still above the slam's 650, so the boss
        // never holds a station its only closer cannot reach. Read from the
        // shipped params and from default-constructed ones, on both CDOs.
        const FBreakerBossPhaseParams Defaults;
        for (const FBreakerBossPhaseParams* Params : { &Boss->PhaseParams, &Defaults })
        {
            const TCHAR* Source = Params == &Defaults ? TEXT("default params") : TEXT("shipped params");
            for (EBreakerBossPhase Phase : { EBreakerBossPhase::Deployment, EBreakerBossPhase::Suppression })
            {
                const FString PhaseName = UBreakerBossPhaseLibrary::GetPhaseName(Phase);
                TestEqual(*FString::Printf(TEXT("%s: %s holds the authored ring (%s)"), *Name, *PhaseName, Source),
                    UBreakerBossPhaseLibrary::GetPhaseHoldRing(Phase, Boss->HoldRingCm, *Params), Boss->HoldRingCm, 0.0001f);
            }
            const float Commitment = UBreakerBossPhaseLibrary::GetPhaseHoldRing(EBreakerBossPhase::Commitment, Boss->HoldRingCm, *Params);
            TestEqual(*FString::Printf(TEXT("%s: Commitment tightens the ring to 0.85 (%s)"), *Name, Source),
                Commitment, Boss->HoldRingCm * 0.85f, 0.0001f);
            TestTrue(*FString::Printf(TEXT("%s: the tightened ring is still above the slam (%.0f > %.0f, %s)"), *Name, Commitment, Boss->SlamRadiusCm, Source),
                Commitment > Boss->SlamRadiusCm);
            // The tightened band still carries the authored deadband
            // unclamped: the classifier halves the band, and 680 - 320 is
            // 360, so a 100 cm hysteresis is honoured there too.
            TestTrue(*FString::Printf(TEXT("%s: the tightened ring does not clamp the hysteresis (%s)"), *Name, Source),
                Hysteresis <= (Commitment - Inner) * 0.5f);
        }
        // A fresh CDO is in Deployment, so the accessor is the authored ring.
        TestEqual(*FString::Printf(TEXT("%s: the accessor is the library's answer for Deployment"), *Name),
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

    // The ring volley (cycle B, O273) arms from Hold one cooldown after
    // BeginPlay. Every leg above ran inside that cooldown, so no raise
    // pointed at the stand-in owned a frame of the ring's measurement. A
    // retune that pulls the cooldown under this rig's length reads here,
    // not as a HOLDING label that mysteriously says VOLLEY.
    TestTrue(FString::Printf(TEXT("the ring was measured inside the volley's first cooldown (%.2f s < %.2f s)"),
        World->GetTimeSeconds(), Boss->VolleyCooldownSeconds),
        World->GetTimeSeconds() < Boss->VolleyCooldownSeconds);
    TestFalse(TEXT("no volley wound during the ring's legs"), Boss->IsVolleyWinding());
    return true;
}

// ---------------------------------------------------------------------------
// THE RING VOLLEY (O273), cycle B, on the pattern of
// RiorsEdge.Combat.Ranged.ShipsDodgeable.
//
// From its ring a boss fires one large, slow, telegraphed projectile at the
// sweep's damage; the tell is the apparatus raise pointed at the player. The
// pure half reads the shipped configuration off both boss CDOs and the round's
// CDO: the round is dodgeable in flight at the player's own sprint, the tell
// is no shorter than the slam's ring (O1), the damage is the sweep's so
// BossHitsToDie stands, and the round is the class and size the finding named.
// The runtime half runs the real engaged frame on a fresh Field Marshal inside
// the ring and watches one round leave the apparatus at the stand-in.
//
// NOTHING IS GRANTED. The stand-in holds its shipped default health with the
// shipped zero dodge and block; the sprint the sidestep is measured against is
// the movement component's shipped default.
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerBossVolleyShipsDodgeableTest,
    "RiorsEdge.Combat.Boss.VolleyShipsDodgeable",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerBossVolleyShipsDodgeableTest::RunTest(const FString& Parameters)
{
    // ---- The shipped configuration ---------------------------------------
    const UBreakerCharacterMovementComponent* Movement = GetDefault<UBreakerCharacterMovementComponent>();
    const ABreakerCharacter* PlayerDefaults = GetDefault<ABreakerCharacter>();
    const ABreakerBossProjectile* Round = GetDefault<ABreakerBossProjectile>();
    const ABreakerEnemyProjectile* LatticeRound = GetDefault<ABreakerEnemyProjectile>();
    if (!TestNotNull(TEXT("movement CDO"), Movement)) return false;
    if (!TestNotNull(TEXT("player CDO"), PlayerDefaults)) return false;
    if (!TestNotNull(TEXT("boss round CDO"), Round)) return false;
    if (!TestNotNull(TEXT("Lattice round CDO"), LatticeRound)) return false;
    if (!TestNotNull(TEXT("player capsule"), PlayerDefaults->GetCapsuleComponent())) return false;

    // The round the finding named: a ~150 cm orb with a 90 cm collision
    // radius, and strictly bigger than the Lattice's on both counts.
    TestEqual(TEXT("the boss round is a 3.0 orb"), Round->VisualScale, 3.0f, 0.0001f);
    TestEqual(TEXT("the boss round has a 90 cm collision radius"), Round->CollisionRadiusCm, 90.0f, 0.0001f);
    TestTrue(TEXT("it is visibly larger than the Lattice's"), Round->VisualScale > LatticeRound->VisualScale);
    TestTrue(TEXT("and hits larger than the Lattice's"), Round->CollisionRadiusCm > LatticeRound->CollisionRadiusCm);

    // THE SIDESTEP. To clear a 90 cm round the player's own capsule has to
    // leave the aim line by the two radii together, and the fastest they can
    // do that is the shipped sprint. That is the reaction budget the flight
    // alone must exceed — before the wind-up is counted at all.
    const float CapsuleRadius = PlayerDefaults->GetCapsuleComponent()->GetScaledCapsuleRadius();
    const float SidestepSeconds = (Round->CollisionRadiusCm + CapsuleRadius) / FMath::Max(Movement->SprintSpeed, UE_SMALL_NUMBER);
    TestTrue(TEXT("the sidestep is measured against a real sprint"), Movement->SprintSpeed > 0.0f);

    for (const ABreakerBossEnemy* Boss : { static_cast<const ABreakerBossEnemy*>(GetDefault<ABreakerBossEnemy>()),
                                            static_cast<const ABreakerBossEnemy*>(GetDefault<ABreakerHoldfastEnemy>()) })
    {
        if (!TestNotNull(TEXT("boss CDO"), Boss)) return false;
        const FString Name = Boss->GetClass()->GetName();

        TestTrue(*FString::Printf(TEXT("%s: the round it ships is the boss round"), *Name),
            Boss->ProjectileClass == ABreakerBossProjectile::StaticClass());
        TestFalse(*FString::Printf(TEXT("%s: a fresh body is not winding a volley"), *Name), Boss->IsVolleyWinding());

        // Flight over the ring, the round's whole journey at the station the
        // boss holds. The Ranged test's arithmetic: distance over speed.
        const float Flight = ABreakerProjectileBase::FlightTimeOver(Boss->HoldRingCm, Boss->VolleySpeed);
        TestTrue(*FString::Printf(TEXT("%s: the flight over the ring (%.2f s) covers the sidestep (%.2f s)"), *Name, Flight, SidestepSeconds),
            Flight >= SidestepSeconds);
        TestTrue(*FString::Printf(TEXT("%s: the round is visible in flight for over half a second (%.2f s)"), *Name, Flight),
            Flight > 0.5f);
        TestTrue(*FString::Printf(TEXT("%s: the wind-up is a spatial tell, not a reaction frame"), *Name),
            Boss->VolleyWindupSeconds >= 0.5f);
        TestTrue(*FString::Printf(TEXT("%s: tell plus flight gives over a second of warning"), *Name),
            Boss->VolleyWindupSeconds + Flight > 1.0f);
        // O1: no tell shorter than the ring. The slam's ring is the boss's
        // longest melee tell, and the volley's raise is not under it.
        TestTrue(*FString::Printf(TEXT("%s: the volley's tell (%.2f) is no shorter than the slam's ring (%.2f)"), *Name, Boss->VolleyWindupSeconds, Boss->SlamWindupSeconds),
            Boss->VolleyWindupSeconds >= Boss->SlamWindupSeconds);
        TestTrue(*FString::Printf(TEXT("%s: wind-up plus cooldown leaves real gaps between rounds"), *Name),
            Boss->VolleyCooldownSeconds >= Boss->VolleyWindupSeconds);
        TestTrue(*FString::Printf(TEXT("%s: the lead is partial by design"), *Name),
            Boss->VolleyLeadFraction > 0.0f && Boss->VolleyLeadFraction < 1.0f);
        // The round outlives the ring at its widest, or the far edge of Hold
        // is a silent dead zone.
        TestTrue(*FString::Printf(TEXT("%s: the round lives long enough to cross the ring at its widest"), *Name),
            ABreakerProjectileBase::MaximumTravelDistance(Boss->VolleySpeed, Round->MaximumLifetime)
                > Boss->HoldRingCm + Boss->HoldRingHysteresisCm);

        // THE DAMAGE IS THE SWEEP'S, which is the chassis AttackDamage
        // unmodified (BreakerWardenEnemy.cpp GetSweepDamage). BossHitsToDie
        // proves no single boss attack kills the baseline from the SLAM's
        // number; a volley at the sweep's is under it and needs no row.
        TestEqual(*FString::Printf(TEXT("%s: the volley deals exactly the sweep's damage"), *Name),
            Boss->GetVolleyDamage(), Boss->GetAttackDamage(), 0.0001f);
        TestTrue(*FString::Printf(TEXT("%s: the sweep is under the slam, so the volley is too"), *Name),
            Boss->SlamDamageRelativeToSweep >= 1.0f);
    }

    // ---- The runtime pin -------------------------------------------------
    // The rig of HoldRingRuntime, with the world begun so a late-spawned
    // round gets its own BeginPlay (the base applies the collision radius and
    // visual scale there). The stand-in is spawned BEFORE the world begins,
    // deliberately, so its BeginPlay never runs and never loads the owner's
    // save; the boss is spawned deferred so its attribute set is registered
    // before its BeginPlay, exactly as the ring rig registers it by hand.
    UWorld::InitializationValues Init;
    Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true,
        ERHIFeatureLevel::Num, &Init);
    if (!TestNotNull(TEXT("isolated engagement world"), World)) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL());
    const uint64 Frame = GFrameCounter;
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); GFrameCounter = Frame; };

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
    World->SetBegunPlay(true);

    const FTransform BossTransform(FRotator::ZeroRotator, FVector::ZeroVector);
    ABreakerBossEnemy* Boss = World->SpawnActorDeferred<ABreakerBossEnemy>(ABreakerBossEnemy::StaticClass(), BossTransform);
    if (!TestNotNull(TEXT("a Field Marshal"), Boss)) return false;
    if (UBreakerAttributeSet* BossAttributes = Cast<UBreakerAttributeSet>(Boss->GetDefaultSubobjectByName(TEXT("Attributes"))))
    {
        Boss->GetAbilitySystemComponent()->AddAttributeSetSubobject(BossAttributes);
    }
    Boss->ConfigureCrowdProbe();
    Boss->FinishSpawning(BossTransform);
    if (!TestTrue(TEXT("the boss has begun play"), Boss->HasActorBegunPlay())) return false;

    const float Ring = Boss->GetHoldRingCm();
    const float Slam = Boss->SlamRadiusCm;
    const float Step = 0.02f;
    auto Place = [&](float DistanceCm)
    {
        const FVector At(DistanceCm, 0.0f, 0.0f);
        Boss->SetActorLocation(At);
        Boss->SetActorRotation((Player->GetActorLocation() - At).GetSafeNormal2D().Rotation());
    };
    auto TickOne = [&]()
    {
        ++GFrameCounter;
        World->Tick(LEVELTICK_All, Step);
    };
    auto DistanceNow = [&]() { return FVector::Dist2D(Boss->GetActorLocation(), Player->GetActorLocation()); };
    auto FacingDot = [&]()
    {
        const FVector ToPlayer = (Player->GetActorLocation() - Boss->GetActorLocation()).GetSafeNormal2D();
        return static_cast<float>(FVector::DotProduct(Boss->GetActorForwardVector().GetSafeNormal2D(), ToPlayer));
    };
    auto CountRounds = [&]()
    {
        int32 Count = 0;
        for (TActorIterator<ABreakerBossProjectile> It(World); It; ++It) if (IsValid(*It)) ++Count;
        return Count;
    };

    // In the ring, outside the slam: the Hold the ring rig measured at 680,
    // inside the fresh body's entry edge. The slam never arms from here, so
    // the volley is the only attack this station can produce.
    const float HoldEntry = Ring - Boss->HoldRingHysteresisCm;
    if (!TestTrue(TEXT("680 is inside the ring's entry edge and outside the slam"), 680.0f < HoldEntry && 680.0f > Slam)) return false;
    Place(680.0f);
    const float HoldStart = DistanceNow();

    // ---- THE CLOCK, then THE RAISE ---------------------------------------
    // The volley arms one cooldown after BeginPlay. Until then the boss holds
    // and the stand-in pays nothing; the frame it arms is read exactly.
    const int32 CooldownFrames = FMath::CeilToInt(Boss->VolleyCooldownSeconds / Step);
    int32 ArmedAt = -1;
    for (int32 F = 0; F < CooldownFrames + 20 && ArmedAt < 0; ++F)
    {
        TickOne();
        if (Boss->IsVolleyWinding()) ArmedAt = F;
    }
    if (!TestTrue(TEXT("the volley armed from Hold"), ArmedAt >= 0)) return false;
    TestTrue(FString::Printf(TEXT("it armed one cooldown after BeginPlay, not before (frame %d, cooldown %d)"), ArmedAt, CooldownFrames),
        ArmedAt >= CooldownFrames - 2);
    TestEqual(TEXT("it armed from the Hold band"), Boss->GetHoldBand(), EBreakerRangedBand::Hold);
    TestEqual(TEXT("holding for the cooldown cost the stand-in nothing"), Player->GetAttributes()->GetHealth(), StartHealth);
    TestEqual(TEXT("no round exists before the tell has run"), CountRounds(), 0);

    // The wind-up: it plants, faces the player, says VOLLEY, and does NOT
    // open the weak point — the raise is a tell, not a punish window.
    const int32 WindupFrames = FMath::CeilToInt(Boss->VolleyWindupSeconds / Step);
    for (int32 F = 0; F < WindupFrames / 2; ++F) TickOne();
    TestTrue(TEXT("mid-tell it is still winding"), Boss->IsVolleyWinding());
    TestTrue(FString::Printf(TEXT("and says so: %s"), *Boss->GetEnemyStateLabel()),
        Boss->GetEnemyStateLabel().Contains(TEXT("VOLLEY")));
    TestTrue(FString::Printf(TEXT("it plants through the tell (moved %.1f cm)"), FMath::Abs(DistanceNow() - HoldStart)),
        FMath::Abs(DistanceNow() - HoldStart) <= 5.0f);
    TestTrue(FString::Printf(TEXT("and points at the player (%.3f)"), FacingDot()), FacingDot() > 0.99f);
    TestFalse(TEXT("the volley's raise is not an order"), Boss->IsGivingOrder());
    TestFalse(TEXT("and does not open the weak point"), Boss->IsApparatusExposed());
    TestEqual(TEXT("the tell costs the stand-in nothing"), Player->GetAttributes()->GetHealth(), StartHealth);
    TestEqual(TEXT("no round exists mid-tell"), CountRounds(), 0);

    // ---- THE SHOT --------------------------------------------------------
    // Past the wind-up, one round leaves the apparatus at the sweep's damage,
    // aimed at the stand-in, and the wind-up is spent.
    TWeakObjectPtr<ABreakerBossProjectile> Seen;
    int32 MostSeenAtOnce = 0;
    for (int32 F = 0; F < WindupFrames + 10 && !Seen.IsValid(); ++F)
    {
        TickOne();
        MostSeenAtOnce = FMath::Max(MostSeenAtOnce, CountRounds());
        for (TActorIterator<ABreakerBossProjectile> It(World); It; ++It) { Seen = *It; break; }
    }
    if (!TestTrue(TEXT("one boss round was spawned"), Seen.IsValid())) return false;
    TestFalse(TEXT("the wind-up is spent"), Boss->IsVolleyWinding());
    TestEqual(TEXT("exactly one round at a time"), MostSeenAtOnce, 1);
    TestTrue(TEXT("the round's instigator is the boss"), Seen->GetInstigator() == Boss);
    TestEqual(TEXT("the round carries the sweep's damage exactly"),
        Seen->GetProjectileDamage().BaseDamage, Boss->GetAttackDamage(), 0.0001f);
    TestTrue(TEXT("and the sweep's damage is a real number after the chassis"), Boss->GetAttackDamage() > 0.0f);
    TestFalse(TEXT("enemies do not crit"), Seen->GetProjectileDamage().bCanCritical);
    if (const USphereComponent* Sphere = Seen->FindComponentByClass<USphereComponent>())
    {
        TestEqual(TEXT("the begun round carries the boss radius"), Sphere->GetUnscaledSphereRadius(), 90.0f, 0.0001f);
    }
    if (const UStaticMeshComponent* Mesh = Seen->FindComponentByClass<UStaticMeshComponent>())
    {
        TestEqual(TEXT("the begun round carries the boss scale"), static_cast<float>(Mesh->GetRelativeScale3D().X), 3.0f, 0.0001f);
    }
    auto TowardPlayerDot = [&]() -> float
    {
        const UProjectileMovementComponent* Move = Seen.IsValid() ? Seen->FindComponentByClass<UProjectileMovementComponent>() : nullptr;
        if (!Move) return -2.0f;
        const FVector ToPlayer = (Player->GetActorLocation() - Seen->GetActorLocation()).GetSafeNormal();
        return static_cast<float>(FVector::DotProduct(Move->Velocity.GetSafeNormal(), ToPlayer));
    };
    const float LaunchDot = TowardPlayerDot();
    TestTrue(FString::Printf(TEXT("it leaves toward the player (%.3f)"), LaunchDot), LaunchDot > 0.9f);
    if (const UProjectileMovementComponent* Move = Seen->FindComponentByClass<UProjectileMovementComponent>())
    {
        TestEqual(TEXT("at the authored speed"), static_cast<float>(Move->Velocity.Size()), Boss->VolleySpeed, 1.0f);
    }

    // ---- THE FLIGHT ------------------------------------------------------
    // A flight time over this station, with slack. The round either lands —
    // the stand-in has the shipped zero dodge and block, so a hit is a loss —
    // or is still in the air on its way to the player. Either way exactly
    // one round was ever fired: the cooldown is longer than the flight.
    const int32 FlightFrames = FMath::CeilToInt(ABreakerProjectileBase::FlightTimeOver(680.0f, Boss->VolleySpeed) / Step) + 10;
    for (int32 F = 0; F < FlightFrames; ++F)
    {
        TickOne();
        MostSeenAtOnce = FMath::Max(MostSeenAtOnce, CountRounds());
    }
    const float AfterFlight = Player->GetAttributes()->GetHealth();
    const bool bLanded = AfterFlight < StartHealth;
    const bool bStillInbound = Seen.IsValid() && !Seen->HasImpacted() && TowardPlayerDot() > 0.9f;
    TestTrue(FString::Printf(TEXT("the round landed (%.1f -> %.1f) or is still inbound (%s)"),
        StartHealth, AfterFlight, bStillInbound ? TEXT("yes") : TEXT("no")),
        bLanded || bStillInbound);
    if (bLanded)
    {
        TestTrue(TEXT("a landed round took no more than the sweep's number"),
            StartHealth - AfterFlight <= Boss->GetAttackDamage() + 0.01f);
    }
    TestEqual(TEXT("never more than one round in the air"), MostSeenAtOnce, 1);
    TestFalse(TEXT("no second volley inside the cooldown"), Boss->IsVolleyWinding());
    return true;
}

#endif   // WITH_DEV_AUTOMATION_TESTS
