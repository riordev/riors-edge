#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Movement/BreakerCharacterMovementComponent.h"

// ---------------------------------------------------------------------------
// THE TRAVERSAL'S OWN MOVEMENT MODE (the custom prediction pass, owner-ruled).
//
// The old execution was pawn-side SetActorLocation in MOVE_Flying, and the
// post-verbs recon graded its three sibling defects as one root cause: every
// predicate was written against a mode that did not exist. These tests pin
// the mode that now does. The three siblings and where each pin lives:
//   1. the viewmodel's dead pose  -> the stride-speed pins here, consumed by
//      the bob's third mode in Characters/;
//   2. Momentum's airborne-credit refill -> RiorsEdge.Classes.Momentum* (the
//      exploit-patch pin reads IsTraversingLedge through AdvanceLoop);
//   3. the absent HUD/telemetry read -> IsTraversingLedge itself, BlueprintPure,
//      pinned true-in-mode below — the read exists now; consuming it is UI's.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerLedgeTraversalModeTest,
    "RiorsEdge.Movement.LedgeTraversalMode",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerLedgeTraversalModeTest::RunTest(const FString& Parameters)
{
    using FMove = UBreakerCharacterMovementComponent;

    // --- The mode predicate, pure ---------------------------------------
    TestTrue(TEXT("The custom sub-mode is a traversal"),
        FMove::IsLedgeTraversalMode(MOVE_Custom, FMove::CustomModeLedgeTraversal));
    // THE REGRESSION STATEMENT: MOVE_Flying — the old execution's mode — is
    // NOT a traversal. On the old tree the character traversed in exactly
    // that mode and every sibling read it as flying-while-standing-still.
    TestFalse(TEXT("MOVE_Flying is not a traversal — the old invisible execution must stay dead"),
        FMove::IsLedgeTraversalMode(MOVE_Flying, FMove::CustomModeLedgeTraversal));
    TestFalse(TEXT("Walking is not a traversal"),
        FMove::IsLedgeTraversalMode(MOVE_Walking, FMove::CustomModeLedgeTraversal));
    TestFalse(TEXT("Falling is not a traversal"),
        FMove::IsLedgeTraversalMode(MOVE_Falling, FMove::CustomModeLedgeTraversal));
    TestFalse(TEXT("A different custom sub-mode is not a traversal"),
        FMove::IsLedgeTraversalMode(MOVE_Custom, FMove::CustomModeLedgeTraversal + 1));

    // --- The glide's clock and curve, pure -------------------------------
    TestEqual(TEXT("The glide starts at its start"),
        FMove::LedgeTraversalAlpha(0.0f, 0.2f), 0.0f);
    TestEqual(TEXT("The glide ends at its end"),
        FMove::LedgeTraversalAlpha(0.2f, 0.2f), 1.0f);
    TestEqual(TEXT("Past the end clamps"),
        FMove::LedgeTraversalAlpha(5.0f, 0.2f), 1.0f);
    const FVector Start(0.0f, 0.0f, 0.0f);
    const FVector Target(100.0f, 0.0f, 145.0f);
    TestTrue(TEXT("Alpha zero is the start exactly"),
        FMove::LedgeTraversalLocation(Start, Target, 0.0f).Equals(Start, 0.001f));
    TestTrue(TEXT("Alpha one is the target exactly"),
        FMove::LedgeTraversalLocation(Start, Target, 1.0f).Equals(Target, 0.001f));
    // THE CORNER RULE (found by photographing the glide): the path RISES
    // before it advances, so a swept capsule can never clip the ledge's
    // front-top corner — the straight lerp this replaced aborted every
    // standing mantle against a solid face. Rise fraction here is
    // 145/245 = 0.59, so the clock's midpoint (smoothstep 0.5) is still
    // rising: zero ground covered, real height gained.
    {
        const FVector Rising = FMove::LedgeTraversalLocation(Start, Target, 0.5f);
        TestTrue(TEXT("Mid-rise the glide has covered no ground"), FMath::IsNearlyZero(Rising.X, 0.01f));
        TestTrue(TEXT("Mid-rise the glide has real height"), Rising.Z > 50.0f);
    }
    // Once crossing, the glide is already AT the ledge's height — forward
    // motion happens above the top, never through the face.
    {
        // Smoothstep(0.7)=0.784, past the 0.59 rise fraction.
        const FVector Crossing = FMove::LedgeTraversalLocation(Start, Target, 0.7f);
        TestTrue(TEXT("Crossing happens at the ledge's own height"),
            FMath::IsNearlyEqual(Crossing.Z, Target.Z, 0.01f));
        TestTrue(TEXT("Crossing actually advances"), Crossing.X > 0.0f && Crossing.X < 100.0f);
    }
    // The descending mirror (a traversal resolved mid-fall from above the
    // top): cross high first, settle after — same rule, other corner.
    {
        const FVector HighStart(0.0f, 0.0f, 100.0f);
        const FVector LowTarget(100.0f, 0.0f, 50.0f);
        const FVector CrossingHigh = FMove::LedgeTraversalLocation(HighStart, LowTarget, 0.4f);
        TestTrue(TEXT("A descending glide crosses at its STARTING height"),
            FMath::IsNearlyEqual(CrossingHigh.Z, HighStart.Z, 0.01f));
        TestTrue(TEXT("A descending glide still lands on its target"),
            FMove::LedgeTraversalLocation(HighStart, LowTarget, 1.0f).Equals(LowTarget, 0.001f));
    }

    // --- The stride speed (sibling defect 1's number) --------------------
    // The bob's third mode reads this; on the old tree the only available
    // speed source was Velocity, which a traversal zeroes — the dead pose.
    TestEqual(TEXT("The stride is zero at the start of the glide"),
        FMove::LedgeTraversalStrideSpeed(150.0f, 0.2f, 0.0f), 0.0f);
    TestEqual(TEXT("The stride is zero at the end of the glide"),
        FMove::LedgeTraversalStrideSpeed(150.0f, 0.2f, 1.0f), 0.0f);
    // Peak = 1.5x the average speed: 150cm/0.2s avg 750, peak 1125.
    TestTrue(TEXT("Mid-glide the stride peaks at 1.5x the average speed"),
        FMath::IsNearlyEqual(FMove::LedgeTraversalStrideSpeed(150.0f, 0.2f, 0.5f), 1125.0f, 0.01f));
    TestTrue(TEXT("A mid-glide traversal is emphatically not standing still"),
        FMove::LedgeTraversalStrideSpeed(150.0f, 0.2f, 0.5f) > 0.0f);
    TestEqual(TEXT("A degenerate duration cannot divide by zero"),
        FMove::LedgeTraversalStrideSpeed(150.0f, 0.0f, 0.5f), 0.0f);
    TestEqual(TEXT("A zero-length glide has no stride"),
        FMove::LedgeTraversalStrideSpeed(0.0f, 0.2f, 0.5f), 0.0f);

    // --- The wiring, worldless (sibling defect 3's read) -----------------
    // SetMovementMode sets the mode and early-outs before OnMovementModeChanged
    // on a component with no UpdatedComponent, which is exactly what makes the
    // published read pinnable without a world.
    UBreakerCharacterMovementComponent* Movement = NewObject<UBreakerCharacterMovementComponent>();
    TestFalse(TEXT("A fresh component is not traversing"), Movement->IsTraversingLedge());
    TestEqual(TEXT("Outside a traversal the stride read is zero"),
        Movement->GetLedgeTraversalStrideSpeed(), 0.0f);
    TestFalse(TEXT("A worldless component refuses to begin a traversal"),
        Movement->TryBeginLedgeTraversal());
    Movement->SetMovementMode(MOVE_Custom, FMove::CustomModeLedgeTraversal);
    TestTrue(TEXT("In the custom sub-mode the published read answers true"),
        Movement->IsTraversingLedge());
    Movement->SetMovementMode(MOVE_Falling);
    TestFalse(TEXT("Leaving the mode clears the read"), Movement->IsTraversingLedge());

    // --- The saved move's wire format ------------------------------------
    // The request bit rides FLAG_Custom_0; nothing else in the project uses
    // the custom flags, and this pin is what says so out loud.
    FBreakerSavedMove_Character Move;
    Move.Clear();
    TestEqual(TEXT("A cleared move carries no custom flags"),
        Move.GetCompressedFlags() & FSavedMove_Character::FLAG_Custom_0, 0);
    Move.bSavedWantsLedgeTraversal = true;
    TestTrue(TEXT("The traversal request rides FLAG_Custom_0 to the server"),
        (Move.GetCompressedFlags() & FSavedMove_Character::FLAG_Custom_0) != 0);
    Move.Clear();
    TestFalse(TEXT("Clear drops the request"), Move.bSavedWantsLedgeTraversal != 0);
    return true;
}

#endif
