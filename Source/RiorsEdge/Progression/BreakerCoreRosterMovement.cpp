#include "Progression/BreakerCoreRoster.h"
#include "Progression/BreakerCoreTree.h"
#include "Progression/BreakerProgressionNode.h"

void BreakerCoreRoster::AppendMovement(UObject* Outer, TArray<FBreakerCoreWedgeDefinition>& Wedges)
{
    {
        auto* Grind = Node(Outer, TEXT("Core.Velocity.Grind"), TEXT("Grind"),
            TEXT("+8% movement speed."), {Effect(EBreakerNodeStatTarget::MoveSpeed, EBreakerNodeStatBucket::IncreasedPercent, 8.0f)}, {}); // O2 PLACEHOLDER
        auto* Stride = Node(Outer, TEXT("Core.Velocity.Stride"), TEXT("Stride"),
            TEXT("+4% movement speed per rank."), {Effect(EBreakerNodeStatTarget::MoveSpeed, EBreakerNodeStatBucket::IncreasedPercent, 4.0f)}, {}); // O2 PLACEHOLDER
        auto* Momentum = Node(Outer, TEXT("Core.Velocity.Momentum"), TEXT("Momentum"),
            TEXT("1% Increased Weapon Damage per 2% movement speed above base."), {}, {TEXT("Progression.Node.Core.Velocity.Momentum")});
        auto* Slide = Node(Outer, TEXT("Core.Velocity.Slide"), TEXT("Slide"),
            TEXT("+8% slide speed per rank."), {Effect(EBreakerNodeStatTarget::SlideSpeed, EBreakerNodeStatBucket::IncreasedPercent, 8.0f)}, {}); // O2 PLACEHOLDER
        auto* Carry = Node(Outer, TEXT("Core.Velocity.Carry"), TEXT("Carry"),
            TEXT("+12% movement and slide speed."), {Effect(EBreakerNodeStatTarget::MoveSpeed, EBreakerNodeStatBucket::IncreasedPercent, 12.0f), Effect(EBreakerNodeStatTarget::SlideSpeed, EBreakerNodeStatBucket::IncreasedPercent, 12.0f)}, {}); // O2 PLACEHOLDER
        auto* Sprint = Node(Outer, TEXT("Core.Velocity.Sprint"), TEXT("Sprint"),
            TEXT("+5% sprint speed per rank."), {Effect(EBreakerNodeStatTarget::SprintSpeed, EBreakerNodeStatBucket::IncreasedPercent, 5.0f)}, {}); // O2 PLACEHOLDER
        auto* Downforce = Node(Outer, TEXT("Core.Velocity.Downforce"), TEXT("Downforce"),
            TEXT("1% Increased Ability Damage per 2% movement speed above base."), {}, {TEXT("Progression.Node.Core.Velocity.Downforce")});
        auto* Traction = Node(Outer, TEXT("Core.Velocity.Traction"), TEXT("Traction"),
            TEXT("+5% movement speed."), {Effect(EBreakerNodeStatTarget::MoveSpeed, EBreakerNodeStatBucket::IncreasedPercent, 5.0f)}, {}); // O2 PLACEHOLDER
        auto* Afterburn = Node(Outer, TEXT("Core.Velocity.Afterburn"), TEXT("Afterburn"),
            TEXT("+8% acceleration."), {Effect(EBreakerNodeStatTarget::Acceleration, EBreakerNodeStatBucket::IncreasedPercent, 8.0f)}, {}); // O2 PLACEHOLDER
        auto* TerminalVelocity = Node(Outer, TEXT("Core.Velocity.TerminalVelocity"), TEXT("Terminal Velocity"),
            TEXT("1.22x More damage."), {Effect(EBreakerNodeStatTarget::Damage, EBreakerNodeStatBucket::MorePercent, 22.0f)}, {}); // O2 PLACEHOLDER
        auto* NoGround = Node(Outer, TEXT("Core.Velocity.NoGround"), TEXT("No Ground"),
            TEXT("Movement speed bonuses are doubled. FORFEIT: you take 30% more damage."), {}, {TEXT("Progression.Node.Core.Velocity.NoGround")});
        Wedges.Add({TEXT("Velocity"), TEXT("Movement"), true, Grind,
            {{Stride, Momentum}, {Slide, Carry}, {Sprint, Downforce}},
            {Traction, Afterburn}, TerminalVelocity, NoGround});
    }
    {
        auto* LightFooting = Node(Outer, TEXT("Core.Kinesis.LightFooting"), TEXT("Light Footing"),
            TEXT("+10% mantle and vault speed."), {Effect(EBreakerNodeStatTarget::LedgeSpeed, EBreakerNodeStatBucket::IncreasedPercent, 10.0f)}, {}); // O2 PLACEHOLDER
        auto* Loft = Node(Outer, TEXT("Core.Kinesis.Loft"), TEXT("Loft"),
            TEXT("+6% jump height per rank."), {Effect(EBreakerNodeStatTarget::JumpHeight, EBreakerNodeStatBucket::IncreasedPercent, 6.0f)}, {}); // O2 PLACEHOLDER
        auto* AirWork = Node(Outer, TEXT("Core.Kinesis.AirWork"), TEXT("Air Work"),
            TEXT("+25% air control."), {Effect(EBreakerNodeStatTarget::AirControl, EBreakerNodeStatBucket::IncreasedPercent, 25.0f)}, {}); // O2 PLACEHOLDER
        auto* Landing = Node(Outer, TEXT("Core.Kinesis.Landing"), TEXT("Landing"),
            TEXT("Fall damage threshold +2m per rank."), {Effect(EBreakerNodeStatTarget::SafeFallDistance, EBreakerNodeStatBucket::Flat, 2.0f)}, {}); // O2 PLACEHOLDER
        auto* AirJump = Node(Outer, TEXT("Core.Kinesis.AirJump"), TEXT("Air Jump"),
            TEXT("+1 air jump."), {Effect(EBreakerNodeStatTarget::AirJumpCount, EBreakerNodeStatBucket::Flat, 1.0f)}, {}); // O2 PLACEHOLDER
        auto* PhantomStep = Node(Outer, TEXT("Core.Kinesis.PhantomStep"), TEXT("Phantom Step"),
            TEXT("Vaults and mantles are instant and do not interrupt fire."), {}, {TEXT("Progression.Node.Core.PhantomStep")});
        Wedges.Add({TEXT("Kinesis"), TEXT("Movement"), false, LightFooting,
            {{Loft, AirWork}, {Landing, AirJump}},
            {}, PhantomStep, nullptr});
    }
}

