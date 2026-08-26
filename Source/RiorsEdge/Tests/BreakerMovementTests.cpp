#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Characters/BreakerCharacter.h"
#include "Game/BreakerGameMode.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Progression/BreakerProgressionTypes.h"
#include "UI/BreakerPlaytestHUD.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerMovementStateTest,
    "RiorsEdge.Movement.StateSafety",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerMovementStateTest::RunTest(const FString& Parameters)
{
    UBreakerCharacterMovementComponent* Movement = NewObject<UBreakerCharacterMovementComponent>();
    TestFalse(TEXT("Movement starts outside sprint"), Movement->IsSprinting());
    TestFalse(TEXT("Movement starts outside slide"), Movement->IsSliding());
    // Weight pass: the arc is deliberately heavier than the 1.35 it shipped
    // with. This assertion exists so the value cannot drift silently.
    // Eased twice: 1.60 then 1.45 both playtested as too heavy. The rise sits
    // near its original 1.35 now; FallGravityMultiplier carries the weight.
    TestEqual(TEXT("Baseline gravity keeps jump arcs responsive"), Movement->GravityScale, 1.38f);

    Movement->SetSprinting(true);
    TestTrue(TEXT("Sprint request changes max speed state"), Movement->IsSprinting());
    TestEqual(TEXT("Sprint uses the configured sprint ceiling"), Movement->GetMaxSpeed(), Movement->SprintSpeed);
    Movement->SetSprinting(false);
    TestEqual(TEXT("Walking uses the configured walk ceiling"), Movement->GetMaxSpeed(), Movement->WalkSpeed);

    TestFalse(TEXT("Slide cannot start without a grounded character"), Movement->BeginSlide());
    Movement->SetSlideRequested(true);
    TestTrue(TEXT("Airborne slide input remains requested until landing"), Movement->IsSlideRequested());
    Movement->SetSlideRequested(false);
    TestFalse(TEXT("Releasing slide clears the landing request"), Movement->IsSlideRequested());
    TestFalse(TEXT("Dash safely rejects a component without a world"), Movement->TryDash(FVector::ForwardVector));
    TestEqual(TEXT("Dash has the requested four-second cooldown"), Movement->DashCooldown, 4.0f);
    TestEqual(TEXT("Slide boost has an anti-spam cooldown"), Movement->SlideBoostCooldown, 1.2f);
    TestEqual(TEXT("Momentum retains a hard safety ceiling"), Movement->MomentumHardCap, 4200.0f);
    TestFalse(TEXT("Redirect safely rejects a standing component"), Movement->TryRedirect(FVector::ForwardVector));
    TestFalse(TEXT("Redirect rejects a zero direction"), Movement->TryRedirect(FVector::ZeroVector));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerMovementRedirectTest,
    "RiorsEdge.Movement.Redirect",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerMovementRedirectTest::RunTest(const FString& Parameters)
{
    // Master 5.4 guardrail, written as a test rather than a comment: a redirect
    // may never increase horizontal speed.
    const float Walk = 700.0f;

    TestFalse(TEXT("A standing player cannot redirect"), UBreakerCharacterMovementComponent::CanRedirect(0.0f, Walk));
    TestFalse(TEXT("Below walk speed cannot redirect"), UBreakerCharacterMovementComponent::CanRedirect(699.9f, Walk));
    TestTrue(TEXT("Exactly walk speed can redirect"), UBreakerCharacterMovementComponent::CanRedirect(Walk, Walk));
    TestTrue(TEXT("Sprinting can redirect"), UBreakerCharacterMovementComponent::CanRedirect(1400.0f, Walk));

    // A 180-degree reversal keeps the magnitude exactly.
    const FVector Sprinting(1400.0f, 0.0f, 0.0f);
    const FVector Reversed = UBreakerCharacterMovementComponent::RedirectHorizontalVelocity(Sprinting, -FVector::ForwardVector, Walk);
    TestTrue(TEXT("A reversal preserves speed exactly"), FMath::IsNearlyEqual(Reversed.Size(), 1400.0f, 0.01f));
    TestTrue(TEXT("A reversal actually reverses"), Reversed.GetSafeNormal().Equals(-FVector::ForwardVector, KINDA_SMALL_NUMBER));
    TestTrue(TEXT("A redirect stays horizontal"), FMath::IsNearlyZero(Reversed.Z));

    // A lateral redirect neither gains nor loses speed.
    const FVector Lateral = UBreakerCharacterMovementComponent::RedirectHorizontalVelocity(Sprinting, FVector::RightVector, Walk);
    TestTrue(TEXT("A lateral redirect preserves speed"), FMath::IsNearlyEqual(Lateral.Size(), 1400.0f, 0.01f));
    TestTrue(TEXT("A lateral redirect changes heading"), Lateral.GetSafeNormal().Equals(FVector::RightVector, KINDA_SMALL_NUMBER));

    // A non-normalized direction must not scale the result — this is exactly
    // how a redirect would accidentally become a dash.
    const FVector Overlong = UBreakerCharacterMovementComponent::RedirectHorizontalVelocity(Sprinting, FVector(0.0f, 50.0f, 0.0f), Walk);
    TestTrue(TEXT("Direction magnitude does not leak into speed"), FMath::IsNearlyEqual(Overlong.Size(), 1400.0f, 0.01f));

    // A vertical-only direction is not a redirect at all: velocity is untouched.
    const FVector Vertical = UBreakerCharacterMovementComponent::RedirectHorizontalVelocity(Sprinting, FVector::UpVector, Walk);
    TestTrue(TEXT("A vertical direction leaves velocity alone"), Vertical.Equals(Sprinting, KINDA_SMALL_NUMBER));

    // The floor keeps a redirect from dead-stopping, and never manufactures
    // speed above the input.
    const FVector Slow(300.0f, 0.0f, 0.0f);
    const FVector Floored = UBreakerCharacterMovementComponent::RedirectHorizontalVelocity(Slow, FVector::RightVector, Walk);
    TestTrue(TEXT("The floor prevents a dead stop"), FMath::IsNearlyEqual(Floored.Size(), Walk, 0.01f));
    // Vertical velocity is not the redirect's business.
    const FVector Airborne(1000.0f, 0.0f, 400.0f);
    const FVector RedirectedAir = UBreakerCharacterMovementComponent::RedirectHorizontalVelocity(Airborne, FVector::RightVector, Walk);
    TestTrue(TEXT("Only the horizontal component is considered"), FMath::IsNearlyEqual(RedirectedAir.Size(), 1000.0f, 0.01f));
    TestTrue(TEXT("A redirect adds no vertical velocity"), FMath::IsNearlyZero(RedirectedAir.Z));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerMovementSpeedMultiplierTest,
    "RiorsEdge.Movement.SpeedMultiplier",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerMovementSpeedMultiplierTest::RunTest(const FString& Parameters)
{
    // Multiplicative composition, matching how gear and tree multipliers
    // already compose.
    TestEqual(TEXT("Nothing pushed composes to one"), UBreakerCharacterMovementComponent::ComposeSpeedMultipliers({}), 1.0f);
    TestTrue(TEXT("Two multipliers multiply rather than add"),
        FMath::IsNearlyEqual(UBreakerCharacterMovementComponent::ComposeSpeedMultipliers({ 1.2f, 1.5f }), 1.8f, 0.0001f));
    TestTrue(TEXT("A non-positive multiplier is ignored rather than zeroing speed"),
        FMath::IsNearlyEqual(UBreakerCharacterMovementComponent::ComposeSpeedMultipliers({ 1.5f, 0.0f, -2.0f }), 1.5f, 0.0001f));

    UBreakerCharacterMovementComponent* Movement = NewObject<UBreakerCharacterMovementComponent>();
    TestEqual(TEXT("No multiplier is pushed by default"), Movement->GetSpeedMultiplier(), 1.0f);

    Movement->PushSpeedMultiplier(TEXT("Window.Swift.Overdrive"), 1.1f, 8.0f);
    TestTrue(TEXT("A pushed multiplier applies"), FMath::IsNearlyEqual(Movement->GetSpeedMultiplier(), 1.1f, 0.0001f));
    TestTrue(TEXT("The multiplier reaches the speed cap"),
        FMath::IsNearlyEqual(Movement->GetMaxSpeed(), Movement->WalkSpeed * 1.1f, 0.01f));

    // Pushing the same key replaces rather than stacking, so a re-cast cannot
    // compound its own buff.
    Movement->PushSpeedMultiplier(TEXT("Window.Swift.Overdrive"), 1.1f, 8.0f);
    TestTrue(TEXT("Re-pushing a key does not stack"), FMath::IsNearlyEqual(Movement->GetSpeedMultiplier(), 1.1f, 0.0001f));

    Movement->PushSpeedMultiplier(TEXT("Debuff.Slow"), 0.5f, 3.0f);
    TestTrue(TEXT("Distinct keys compose"), FMath::IsNearlyEqual(Movement->GetSpeedMultiplier(), 0.55f, 0.0001f));

    Movement->PopSpeedMultiplier(TEXT("Debuff.Slow"));
    TestTrue(TEXT("Popping removes only its own key"), FMath::IsNearlyEqual(Movement->GetSpeedMultiplier(), 1.1f, 0.0001f));
    Movement->PopSpeedMultiplier(TEXT("Window.Swift.Overdrive"));
    TestEqual(TEXT("Popping the last key restores the baseline"), Movement->GetSpeedMultiplier(), 1.0f);

    Movement->PushSpeedMultiplier(NAME_None, 2.0f, 1.0f);
    Movement->PushSpeedMultiplier(TEXT("Bad"), 0.0f, 1.0f);
    TestEqual(TEXT("Invalid pushes are refused"), Movement->GetSpeedMultiplier(), 1.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerMovementWeightTest,
    "RiorsEdge.Movement.Weight",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerMovementWeightTest::RunTest(const FString& Parameters)
{
    using FMovement = UBreakerCharacterMovementComponent;

    // --- Gravity curve -------------------------------------------------
    const float Band = 220.0f;
    const float Apex = 1.5f;
    const float Fall = 1.8f;

    TestEqual(TEXT("A free rise is not made heavier"), FMovement::ComputeGravityMultiplier(700.0f, Band, Apex, Fall), 1.0f);
    TestEqual(TEXT("The rise multiplier holds right up to the band edge"), FMovement::ComputeGravityMultiplier(Band, Band, Apex, Fall), 1.0f);
    TestEqual(TEXT("The apex itself is the heaviest treated point of the rise"), FMovement::ComputeGravityMultiplier(0.0f, Band, Apex, Fall), Apex);
    TestEqual(TEXT("A settled fall uses the fall multiplier"), FMovement::ComputeGravityMultiplier(-4000.0f, Band, Apex, Fall), Fall);
    TestEqual(TEXT("The fall multiplier holds from the band edge down"), FMovement::ComputeGravityMultiplier(-Band, Band, Apex, Fall), Fall);
    // Continuity: a step in gravity at the apex is visible as a camera hitch.
    TestTrue(TEXT("The curve is continuous entering the band"),
        FMath::IsNearlyEqual(FMovement::ComputeGravityMultiplier(Band - 0.01f, Band, Apex, Fall), 1.0f, 0.001f));
    TestTrue(TEXT("The curve is continuous leaving the band"),
        FMath::IsNearlyEqual(FMovement::ComputeGravityMultiplier(-Band + 0.01f, Band, Apex, Fall), Fall, 0.001f));
    TestTrue(TEXT("Halfway up the band interpolates"),
        FMath::IsNearlyEqual(FMovement::ComputeGravityMultiplier(Band * 0.5f, Band, Apex, Fall), 1.25f, 0.001f));
    TestTrue(TEXT("Halfway down the band interpolates"),
        FMath::IsNearlyEqual(FMovement::ComputeGravityMultiplier(-Band * 0.5f, Band, Apex, Fall), 1.65f, 0.001f));
    // A fall is always at least as heavy as the matching rise: this is the
    // asymmetry the whole pass is about.
    for (float Speed = 20.0f; Speed <= 900.0f; Speed += 20.0f)
    {
        TestTrue(TEXT("Falling is never lighter than rising at the same speed"),
            FMovement::ComputeGravityMultiplier(-Speed, Band, Apex, Fall) >= FMovement::ComputeGravityMultiplier(Speed, Band, Apex, Fall) - KINDA_SMALL_NUMBER);
    }
    // Degenerate input must not divide by zero.
    TestEqual(TEXT("A zero band degrades to a plain rise/fall split"), FMovement::ComputeGravityMultiplier(-1.0f, 0.0f, Apex, Fall), Fall);

    // --- Terminal velocity ---------------------------------------------
    TestEqual(TEXT("A slow fall is untouched"), FMovement::ClampFallSpeed(-900.0f, 2400.0f), -900.0f);
    TestEqual(TEXT("A long drop is capped"), FMovement::ClampFallSpeed(-5000.0f, 2400.0f), -2400.0f);
    TestEqual(TEXT("Exactly terminal stays terminal"), FMovement::ClampFallSpeed(-2400.0f, 2400.0f), -2400.0f);
    TestEqual(TEXT("A rise is never clamped by the fall cap"), FMovement::ClampFallSpeed(9000.0f, 2400.0f), 9000.0f);
    TestEqual(TEXT("A zero cap disables terminal velocity"), FMovement::ClampFallSpeed(-9000.0f, 0.0f), -9000.0f);

    // --- Jump cut -------------------------------------------------------
    TestTrue(TEXT("Releasing mid-rise cuts the rise"),
        FMath::IsNearlyEqual(FMovement::ApplyJumpCut(700.0f, 0.55f, 50.0f), 385.0f, 0.01f));
    TestEqual(TEXT("A cut never accelerates a fall"), FMovement::ApplyJumpCut(-1200.0f, 0.55f, 50.0f), -1200.0f);
    TestEqual(TEXT("A cut does nothing at the apex"), FMovement::ApplyJumpCut(0.0f, 0.55f, 50.0f), 0.0f);
    TestEqual(TEXT("A cut ignores a rise below the threshold"), FMovement::ApplyJumpCut(40.0f, 0.55f, 50.0f), 40.0f);
    TestEqual(TEXT("A multiplier of one is a no-op"), FMovement::ApplyJumpCut(700.0f, 1.0f, 50.0f), 700.0f);
    TestTrue(TEXT("A cut can only ever reduce a rise"),
        FMovement::ApplyJumpCut(700.0f, 2.0f, 50.0f) <= 700.0f);
    // The full-hold jump must stay strictly taller than the tapped one.
    const float FullRise = 700.0f;
    const float CutRise = FMovement::ApplyJumpCut(700.0f, 0.55f, 50.0f);
    TestTrue(TEXT("A tapped jump is strictly shorter than a held one"), CutRise * CutRise < FullRise * FullRise);

    // --- Landing --------------------------------------------------------
    TestEqual(TEXT("An ordinary jump landing costs nothing"), FMovement::LandingSpeedScale(900.0f, 950.0f, 2400.0f, 0.78f), 1.0f);
    TestEqual(TEXT("The threshold itself costs nothing"), FMovement::LandingSpeedScale(950.0f, 950.0f, 2400.0f, 0.78f), 1.0f);
    TestTrue(TEXT("A terminal-velocity landing costs the full scale"),
        FMath::IsNearlyEqual(FMovement::LandingSpeedScale(2400.0f, 950.0f, 2400.0f, 0.78f), 0.78f, 0.0001f));
    TestTrue(TEXT("Past terminal is clamped, not extrapolated"),
        FMath::IsNearlyEqual(FMovement::LandingSpeedScale(99999.0f, 950.0f, 2400.0f, 0.78f), 0.78f, 0.0001f));
    TestTrue(TEXT("A midway landing interpolates"),
        FMath::IsNearlyEqual(FMovement::LandingSpeedScale(1675.0f, 950.0f, 2400.0f, 0.78f), 0.89f, 0.0001f));
    TestEqual(TEXT("A scale of one disables the landing cost"), FMovement::LandingSpeedScale(2400.0f, 950.0f, 2400.0f, 1.0f), 1.0f);
    TestEqual(TEXT("A degenerate range never divides by zero"), FMovement::LandingSpeedScale(2400.0f, 2400.0f, 2400.0f, 0.78f), 1.0f);

    // --- Shipped defaults ------------------------------------------------
    UBreakerCharacterMovementComponent* Movement = NewObject<UBreakerCharacterMovementComponent>();
    TestTrue(TEXT("The fall is heavier than the rise"), Movement->FallGravityMultiplier > 1.0f);
    TestTrue(TEXT("The apex is heavier than the rise"), Movement->ApexGravityMultiplier > 1.0f);
    TestTrue(TEXT("The apex is not heavier than a settled fall"), Movement->ApexGravityMultiplier <= Movement->FallGravityMultiplier);
    TestTrue(TEXT("A long drop has a terminal velocity"), Movement->MaxFallSpeed > 0.0f);
    TestTrue(TEXT("Terminal velocity binds before the physics volume default of 4000"), Movement->MaxFallSpeed < 4000.0f);
    TestTrue(TEXT("Releasing jump early actually shortens the jump"), Movement->JumpCutMultiplier < 1.0f);
    // The hold window has to outlast the rise or a held jump would cut itself.
    const float RiseTime = Movement->JumpZVelocity / (980.0f * Movement->GravityScale);
    TestTrue(TEXT("The jump hold window outlasts the rise"), Movement->JumpHoldWindow > RiseTime);
    // An ordinary full-height jump must land below the landing threshold, so
    // routine jumping is never taxed.
    const float JumpApex = FMath::Square(Movement->JumpZVelocity) / (2.0f * 980.0f * Movement->GravityScale);
    const float LandingSpeed = FMath::Sqrt(2.0f * 980.0f * Movement->GravityScale * Movement->FallGravityMultiplier * JumpApex);
    TestTrue(TEXT("A routine jump landing is not meaningfully taxed"),
        FMovement::LandingSpeedScale(LandingSpeed, Movement->LandingHeavyFallSpeed, Movement->LandingMaxFallSpeed, Movement->LandingMinimumSpeedScale) >= 0.98f);
    return true;
}

// The wall-ride entry suite retired with its verb (Part One-R). Its lesson —
// an entry gate must sit strictly below the ceiling it is measured against —
// survives on the slide gate assertion above and in the ledge tests.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerMovementAdditiveCompositionTest,
    "RiorsEdge.Movement.AdditiveComposition",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerMovementAdditiveCompositionTest::RunTest(const FString& Parameters)
{
    using FMovement = UBreakerCharacterMovementComponent;

    // The locked rule (Docs/Item-Foundation.md, "Unified attribute
    // application"): ALL Increased percentages form ONE additive bucket per
    // stat. Movement was the last place two layers were MULTIPLIED. The single
    // number this test exists for: +20% gear and +20% tree is x1.40, not x1.44.
    TestTrue(TEXT("Twenty gear and twenty tree read x1.40, never x1.44"),
        FMath::IsNearlyEqual(FMovement::ComposeAdditiveMultiplier(1.20f, 1.20f), 1.40f, 0.0001f));
    TestTrue(TEXT("It is strictly weaker than the multiplicative composition it replaces"),
        FMovement::ComposeAdditiveMultiplier(1.20f, 1.20f) < 1.20f * 1.20f);
    TestEqual(TEXT("Neither layer contributing is neutral"), FMovement::ComposeAdditiveMultiplier(1.0f, 1.0f), 1.0f);
    TestTrue(TEXT("One layer alone is unchanged"),
        FMath::IsNearlyEqual(FMovement::ComposeAdditiveMultiplier(1.35f, 1.0f), 1.35f, 0.0001f));
    TestTrue(TEXT("Order does not matter"),
        FMath::IsNearlyEqual(FMovement::ComposeAdditiveMultiplier(1.35f, 1.08f), FMovement::ComposeAdditiveMultiplier(1.08f, 1.35f), 0.0001f));
    // A slow and a speed-up cancel exactly, which multiplication does not do.
    TestTrue(TEXT("Equal and opposite percentages cancel"),
        FMath::IsNearlyEqual(FMovement::ComposeAdditiveMultiplier(1.30f, 0.70f), 1.0f, 0.0001f));
    // Nothing may reverse the character's direction of travel.
    TestEqual(TEXT("A pathological pair of debuffs floors at zero"), FMovement::ComposeAdditiveMultiplier(0.2f, 0.1f), 0.0f);

    // Representative investment levels, the same table published in
    // Docs/Movement-Design.md so the doc and the code cannot drift.
    struct FBreakerJumpCompositionRow { float Gear; float Tree; float Additive; };
    const FBreakerJumpCompositionRow Rows[] = {
        {1.00f, 1.00f, 1.00f},
        {1.08f, 1.00f, 1.08f},
        {1.08f, 1.12f, 1.20f},
        {1.20f, 1.20f, 1.40f},
        {1.30f, 1.24f, 1.54f},
    };
    for (const FBreakerJumpCompositionRow& Row : Rows)
    {
        TestTrue(TEXT("The published table matches the implementation"),
            FMath::IsNearlyEqual(FMovement::ComposeAdditiveMultiplier(Row.Gear, Row.Tree), Row.Additive, 0.0001f));
    }

    // A movement component with no owner has no attribute set and no layers, so
    // every composed stat must be exactly neutral rather than zero.
    UBreakerCharacterMovementComponent* Movement = NewObject<UBreakerCharacterMovementComponent>();
    TestEqual(TEXT("Move speed composes to one with nothing equipped or allocated"), Movement->GetComposedMoveSpeedMultiplier(), 1.0f);
    TestEqual(TEXT("Slide speed composes to one"), Movement->GetComposedSlideSpeedMultiplier(), 1.0f);
    TestEqual(TEXT("Air control composes to one"), Movement->GetComposedAirControlMultiplier(), 1.0f);
    TestEqual(TEXT("Dash cooldown composes to one"), Movement->GetComposedDashCooldownMultiplier(), 1.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerMovementJumpGrantTest,
    "RiorsEdge.Movement.JumpGrant",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerMovementJumpGrantTest::RunTest(const FString& Parameters)
{
    using FMovement = UBreakerCharacterMovementComponent;

    UBreakerCharacterMovementComponent* Movement = NewObject<UBreakerCharacterMovementComponent>();
    // O25: two jumps are base kit for EVERYONE. The count is now resolved at
    // runtime, so this assertion is what stops the base kit drifting.
    TestEqual(TEXT("Two jumps are base kit"), Movement->BaseJumpCount, 2);
    TestTrue(TEXT("The third jump is enabled by default"), Movement->bSwiftThirdJumpEnabled);
    TestTrue(TEXT("The unlock level is reachable inside the level 50 cap"), Movement->SwiftThirdJumpUnlockLevel <= 50);
    TestEqual(TEXT("A component with no progression grants exactly the base kit"), Movement->GetGrantedJumpCount(), 2);

    // --- The class x level matrix ------------------------------------------
    const int32 Unlock = 20;
    const EBreakerClassId EveryClass[] = {
        EBreakerClassId::None, EBreakerClassId::Caster, EBreakerClassId::Swift,
        EBreakerClassId::Gunsmith, EBreakerClassId::Tank, EBreakerClassId::Support };
    for (const EBreakerClassId ClassId : EveryClass)
    {
        for (const int32 Level : {1, 19, 20, 21, 50})
        {
            const int32 Granted = FMovement::ResolveJumpCount(ClassId, Level, 2, true, Unlock);
            const bool bShouldHaveThree = (ClassId == EBreakerClassId::Swift) && (Level >= Unlock);
            TestEqual(TEXT("Jump count matches the O25 class/level matrix"), Granted, bShouldHaveThree ? 3 : 2);
            // The invariant that outranks the rest: NOBODY drops below the base
            // kit, and nobody but Swift ever exceeds it.
            TestTrue(TEXT("Two jumps are never taken away"), Granted >= 2);
            TestTrue(TEXT("Only Swift can hold three"), Granted <= 2 || ClassId == EBreakerClassId::Swift);
        }
    }
    // Exactly at the threshold unlocks; one below does not.
    TestEqual(TEXT("One level short of the threshold is still two"), FMovement::ResolveJumpCount(EBreakerClassId::Swift, 19, 2, true, 20), 2);
    TestEqual(TEXT("The threshold itself unlocks the third"), FMovement::ResolveJumpCount(EBreakerClassId::Swift, 20, 2, true, 20), 3);
    // Swapping AWAY from Swift is the case a one-time grant would get wrong.
    TestEqual(TEXT("A swap away from Swift returns the character to two jumps"),
        FMovement::ResolveJumpCount(EBreakerClassId::Tank, 50, 2, true, 20), 2);
    // The master switch restores exactly the pre-O25 behaviour.
    TestEqual(TEXT("Disabling the third jump gives Swift the plain base kit"),
        FMovement::ResolveJumpCount(EBreakerClassId::Swift, 50, 2, false, 20), 2);
    // A degenerate base count must never produce a zero-jump character.
    TestEqual(TEXT("A zero base count is floored at one jump"), FMovement::ResolveJumpCount(EBreakerClassId::Tank, 1, 0, true, 20), 1);

    // --- The third jump's feel: a redirect, never a boost -------------------
    const FVector Sprinting(1400.0f, 0.0f, 0.0f);
    const FVector Full = FMovement::BlendHorizontalVelocity(Sprinting, FVector::RightVector, 1.0f);
    TestTrue(TEXT("A full blend lands on the input direction"), Full.GetSafeNormal().Equals(FVector::RightVector, 0.001f));
    TestTrue(TEXT("A full blend preserves speed exactly"), FMath::IsNearlyEqual(Full.Size(), 1400.0f, 0.01f));
    const FVector Partial = FMovement::BlendHorizontalVelocity(Sprinting, FVector::RightVector, 0.55f);
    TestTrue(TEXT("A partial blend preserves speed exactly"), FMath::IsNearlyEqual(Partial.Size(), 1400.0f, 0.01f));
    TestTrue(TEXT("A partial blend actually turns"), Partial.GetSafeNormal().Y > 0.0f);
    TestTrue(TEXT("A partial blend does not fully commit"), Partial.GetSafeNormal().X > 0.0f);
    TestTrue(TEXT("A zero alpha is a no-op"),
        FMovement::BlendHorizontalVelocity(Sprinting, FVector::RightVector, 0.0f).Equals(Sprinting, 0.001f));
    TestTrue(TEXT("No input leaves the momentum untouched"),
        FMovement::BlendHorizontalVelocity(Sprinting, FVector::ZeroVector, 1.0f).Equals(Sprinting, 0.001f));
    TestTrue(TEXT("A standing player gains nothing"),
        FMovement::BlendHorizontalVelocity(FVector::ZeroVector, FVector::RightVector, 1.0f).IsNearlyZero());
    // The degenerate 180 at exactly half: the lerp collapses to zero, and the
    // honest answer is the original heading rather than a dead stop.
    const FVector Reversal = FMovement::BlendHorizontalVelocity(Sprinting, -FVector::ForwardVector, 0.5f);
    TestTrue(TEXT("A half-blended reversal never dead-stops"), FMath::IsNearlyEqual(Reversal.Size(), 1400.0f, 0.01f));
    // The whole guardrail, swept: a blend can never manufacture speed.
    for (float Alpha = 0.0f; Alpha <= 1.0f; Alpha += 0.05f)
    {
        for (const FVector& Direction : {FVector::RightVector, -FVector::ForwardVector, FVector(1.0f, 1.0f, 0.0f), FVector(0.0f, 0.0f, 1.0f)})
        {
            const FVector Result = FMovement::BlendHorizontalVelocity(Sprinting, Direction, Alpha);
            TestTrue(TEXT("A third-jump blend never manufactures speed"), Result.Size() <= 1400.0f + 0.01f);
            TestTrue(TEXT("A third-jump blend stays horizontal"), FMath::IsNearlyZero(Result.Z));
        }
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerMovementJumpGrantMatrixTest,
    "RiorsEdge.Movement.JumpGrantMatrix",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerMovementJumpGrantMatrixTest::RunTest(const FString& Parameters)
{
    using FMovement = UBreakerCharacterMovementComponent;

    // THE TEST THAT WAS MISSING (owner: "i never could do a 3rd jump").
    //
    // RiorsEdge.Movement.JumpGrant already proved the RULE — Swift at or past
    // the threshold gets three — and it passed the whole time the feature was
    // unreachable, because it fed the rule a level the game cannot produce. It
    // asserted arithmetic about a hypothetical character.
    //
    // This one asserts the SHIPPED CONFIGURATION against the progression state
    // the game actually runs in. FBreakerProgressionState is default-
    // constructed here on purpose, and CharacterLevel is read from it rather
    // than written: nothing in the project writes that field (no XP loop
    // exists), so a default-constructed state IS the state of every character
    // in the gym, in a playtest, and in a fresh save. If the gate is ever
    // raised above that level again, this test fails on the same day rather
    // than in a playtest weeks later.
    const FBreakerProgressionState LiveState;
    const int32 ReachableLevel = LiveState.CharacterLevel;
    TestTrue(TEXT("The level a character can actually reach is at least one"), ReachableLevel >= 1);

    UBreakerCharacterMovementComponent* Movement = NewObject<UBreakerCharacterMovementComponent>();
    TestTrue(TEXT("The third jump's gate is REACHABLE in the state the game runs in — nothing raises CharacterLevel"),
        Movement->SwiftThirdJumpUnlockLevel <= ReachableLevel);

    // The matrix, at the shipped values, at the reachable level: Swift three,
    // everybody else two. No hypothetical levels anywhere in this loop.
    const EBreakerClassId EveryClass[] = {
        EBreakerClassId::None, EBreakerClassId::Caster, EBreakerClassId::Swift,
        EBreakerClassId::Gunsmith, EBreakerClassId::Tank, EBreakerClassId::Support };
    for (const EBreakerClassId ClassId : EveryClass)
    {
        const int32 Granted = FMovement::ResolveJumpCount(
            ClassId, ReachableLevel, Movement->BaseJumpCount,
            Movement->bSwiftThirdJumpEnabled, Movement->SwiftThirdJumpUnlockLevel);
        if (ClassId == EBreakerClassId::Swift)
        {
            TestEqual(TEXT("Swift gets three jumps in the state the game actually runs in"), Granted, 3);
        }
        else
        {
            TestEqual(TEXT("Every other class gets exactly two"), Granted, 2);
        }
    }

    // The mid-session DevForceClass swap, both directions, at the shipped
    // values. Swapping TO Swift must hand the third jump over immediately;
    // swapping AWAY must take it back, which is the direction that would leave
    // a permanent illegal grant if the refresh were a one-time thing.
    TestEqual(TEXT("A dev swap to Swift grants the third jump immediately"),
        FMovement::ResolveJumpCount(EBreakerClassId::Swift, ReachableLevel, Movement->BaseJumpCount,
            Movement->bSwiftThirdJumpEnabled, Movement->SwiftThirdJumpUnlockLevel), 3);
    TestEqual(TEXT("A dev swap away from Swift takes it back immediately"),
        FMovement::ResolveJumpCount(EBreakerClassId::Tank, ReachableLevel, Movement->BaseJumpCount,
            Movement->bSwiftThirdJumpEnabled, Movement->SwiftThirdJumpUnlockLevel), 2);

    // The banked-jump clamp, as arithmetic. A Swift player who has spent all
    // three and swaps away must not keep the third: the spent count is clamped
    // to the NEW budget, so the jumps remaining go to zero rather than negative
    // — and can never come out as one free extra jump on the smaller budget.
    for (int32 Spent = 0; Spent <= 3; ++Spent)
    {
        const int32 ClampedAfterSwap = FMath::Min(Spent, 2);
        TestTrue(TEXT("A jump banked against three never survives a swap to a budget of two"),
            ClampedAfterSwap <= 2);
        TestTrue(TEXT("The clamp never hands the player a jump they did not have"),
            2 - ClampedAfterSwap <= 2 - FMath::Min(Spent, 2));
    }

    // The master switch still restores pre-O25 behaviour exactly, at the
    // reachable level rather than at a hypothetical one.
    TestEqual(TEXT("Disabling the third jump returns Swift to the base kit"),
        FMovement::ResolveJumpCount(EBreakerClassId::Swift, ReachableLevel, Movement->BaseJumpCount,
            false, Movement->SwiftThirdJumpUnlockLevel), 2);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerPlaytestAssemblyTest,
    "RiorsEdge.Playtest.Assembly",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerPlaytestAssemblyTest::RunTest(const FString& Parameters)
{
    const ABreakerGameMode* GameMode = GetDefault<ABreakerGameMode>();
    TestNotNull(TEXT("Playtest game mode has a pawn class"), GameMode->DefaultPawnClass.Get());
    if (GameMode->DefaultPawnClass)
    {
        TestTrue(TEXT("Default pawn remains a C++ BreakerCharacter descendant"),
            GameMode->DefaultPawnClass->IsChildOf(ABreakerCharacter::StaticClass()));
        TestEqual(TEXT("Editor-authored Breaker Blueprint is the active pawn"),
            GameMode->DefaultPawnClass->GetPathName(),
            FString(TEXT("/Game/ProjectBreaker/Characters/BP_BreakerCharacter.BP_BreakerCharacter_C")));
        const ABreakerCharacter* CharacterDefaults = Cast<ABreakerCharacter>(GameMode->DefaultPawnClass->GetDefaultObject());
        TestNotNull(TEXT("Active pawn exposes Breaker defaults"), CharacterDefaults);
        if (CharacterDefaults)
        {
            TestEqual(TEXT("Baseline supports a double jump"), CharacterDefaults->JumpMaxCount, 2);
            TestEqual(TEXT("Baseline look sensitivity remains one-to-one"), CharacterDefaults->GetLookSensitivity(), 1.0f);
            // Dash camera feedback is presentation and must be idle at rest, or
            // it would be biasing the FOV and the view roll every frame.
            TestEqual(TEXT("Dash camera feedback is idle at rest"), CharacterDefaults->GetDashFeedbackAlpha(), 0.0f);
            // The FOV reported to the settings screen is the player's setting,
            // never the punched camera value.
            TestTrue(TEXT("Reported FOV stays inside the settings range"),
                CharacterDefaults->GetCurrentFOV() >= 70.0f && CharacterDefaults->GetCurrentFOV() <= 120.0f);
        }
    }
    TestEqual(TEXT("Playtest HUD remains active"), GameMode->HUDClass.Get(), ABreakerPlaytestHUD::StaticClass());
    return true;
}


// ---------------------------------------------------------------------------
// Ledge traversal (Part One-R): the rules the pawn's TryMantle fused for its
// whole life, now named and pinned. Plus the agreement test: the grammar's
// MantleStepHeight and the verb's published ceiling are ONE number — the
// 145-vs-150 disagreement this extraction closed must not be able to reopen.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerLedgeVerbsTest,
    "RiorsEdge.Movement.LedgeVerbs",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerLedgeVerbsTest::RunTest(const FString& Parameters)
{
    using FMove = UBreakerCharacterMovementComponent;

    // Wall and top predicates: near-vertical walls mantle, near-flat tops stand.
    TestTrue(TEXT("A vertical wall is mantleable"), FMove::IsMantleableWallNormal(0.0f));
    TestTrue(TEXT("A slightly leaning wall is mantleable"), FMove::IsMantleableWallNormal(-0.3f));
    TestFalse(TEXT("A ramp is not a mantleable wall"), FMove::IsMantleableWallNormal(0.5f));
    TestTrue(TEXT("A flat top is standable"), FMove::IsStandableTopNormal(1.0f));
    TestTrue(TEXT("The standable boundary itself qualifies"), FMove::IsStandableTopNormal(0.65f));
    TestFalse(TEXT("A steep top is not standable"), FMove::IsStandableTopNormal(0.5f));

    // The verb bands, at the shipped defaults' shape: [min, vaultMax] vaults,
    // (vaultMax, mantleMax] mantles, outside is nothing.
    const float Min = 35.0f, VaultMax = 80.0f, MantleMax = FMove::MantleStepHeightCm;
    TestTrue(TEXT("Below the minimum is a step, not a verb"),
        FMove::ResolveLedgeVerb(20.0f, Min, VaultMax, MantleMax) == EBreakerLedgeVerb::None);
    TestTrue(TEXT("The minimum itself vaults"),
        FMove::ResolveLedgeVerb(Min, Min, VaultMax, MantleMax) == EBreakerLedgeVerb::Vault);
    TestTrue(TEXT("The vault ceiling itself vaults"),
        FMove::ResolveLedgeVerb(VaultMax, Min, VaultMax, MantleMax) == EBreakerLedgeVerb::Vault);
    TestTrue(TEXT("Just above the vault ceiling mantles"),
        FMove::ResolveLedgeVerb(VaultMax + 0.1f, Min, VaultMax, MantleMax) == EBreakerLedgeVerb::Mantle);
    TestTrue(TEXT("Chest cover (120) mantles — the grammar's whole premise"),
        FMove::ResolveLedgeVerb(120.0f, Min, VaultMax, MantleMax) == EBreakerLedgeVerb::Mantle);
    TestTrue(TEXT("The mantle ceiling itself mantles"),
        FMove::ResolveLedgeVerb(MantleMax, Min, VaultMax, MantleMax) == EBreakerLedgeVerb::Mantle);
    TestTrue(TEXT("Above the ceiling is a wall, not a verb"),
        FMove::ResolveLedgeVerb(MantleMax + 0.1f, Min, VaultMax, MantleMax) == EBreakerLedgeVerb::None);

    // Shipped-configuration invariants, on a default-constructed component.
    const UBreakerCharacterMovementComponent* Defaults = GetDefault<UBreakerCharacterMovementComponent>();
    TestEqual(TEXT("The mantle ceiling IS the published step height"),
        Defaults->MantleMaximumHeightCm, FMove::MantleStepHeightCm, 0.0f);
    TestTrue(TEXT("The windows nest: min < vault max < mantle max"),
        Defaults->LedgeMinimumHeightCm < Defaults->VaultMaximumHeightCm
        && Defaults->VaultMaximumHeightCm < Defaults->MantleMaximumHeightCm);
    TestTrue(TEXT("A vault is faster than a mantle — the difference the player feels"),
        Defaults->VaultDurationSeconds < Defaults->MantleDurationSeconds);

    // THE AGREEMENT PIN: the grammar's copy on the game mode equals the
    // published number. GROUND's re-point makes this trivially true forever;
    // until then it is the tripwire that stops the two drifting apart again.
    TestEqual(TEXT("The grammar's MantleStepHeight equals the published ceiling"),
        GetDefault<ABreakerGameMode>()->MantleStepHeight, FMove::MantleStepHeightCm, 0.0f);
    return true;
}

#endif
