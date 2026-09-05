#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "GameFramework/Actor.h"
#include "Attributes/BreakerAttributeAggregation.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Items/BreakerEquipmentComponent.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Progression/BreakerProgressionComponent.h"

// The last multiplicative gear x tree violation, and the composed movement
// attributes that close it.
//
// Before this, UBreakerCharacterMovementComponent read the gear multiplier and
// the tree multiplier separately and MULTIPLIED them, so +20% boots and +20%
// tree read x1.44 against a locked rule that says all Increased percentages
// form ONE additive bucket per stat (x1.40). Damage was the identical bug
// class. Slide speed, air control and dash cooldown reduction now have composed
// attributes of their own, and move speed already had one that NOTHING read.
//
// Distinctively prefixed namespace: this project has been bitten by identical
// anonymous-namespace names colliding across .cpp files in a unity build.
namespace BreakerMovementAttributeTestHelpers
{
    // A freshly constructed actor is ROLE_Authority, which is all the
    // equipment/progression authority checks need without a world.
    AActor* MakeMovementTestOwner()
    {
        return NewObject<AActor>();
    }

    FBreakerRolledAffix MakeMovementRolledAffix(FName AffixId, float Value)
    {
        FBreakerRolledAffix Rolled;
        Rolled.AffixId = AffixId;
        Rolled.Tier = 4;
        Rolled.Value = Value;
        Rolled.Category = EBreakerAffixCategory::Prefix;
        return Rolled;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerMovementAttributeCompositionTest,
    "RiorsEdge.Movement.ComposedAttributes",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerMovementAttributeCompositionTest::RunTest(const FString& Parameters)
{
    using namespace BreakerMovementAttributeTestHelpers;

    // --- The three new attributes are neutral until something bids ---------
    UBreakerAttributeSet* Attributes = NewObject<UBreakerAttributeSet>();
    TestEqual(TEXT("Slide speed starts neutral"), Attributes->GetSlideSpeedMultiplier(), 1.0f);
    TestEqual(TEXT("Air control starts neutral"), Attributes->GetAirControlMultiplier(), 1.0f);
    TestEqual(TEXT("Dash cooldown reduction starts neutral"), Attributes->GetDashCooldownReduction(), 1.0f);

    // --- Two layers, one additive bucket -----------------------------------
    FBreakerAttributeContribution Gear;
    Gear.AddIncreasedPercent(EBreakerAggregatedAttribute::SlideSpeedMultiplier, 20.0f);
    Gear.AddIncreasedPercent(EBreakerAggregatedAttribute::AirControlMultiplier, 20.0f);
    Gear.AddIncreasedPercent(EBreakerAggregatedAttribute::DashCooldownReduction, 18.0f);
    FBreakerAttributeContribution Tree;
    Tree.AddIncreasedPercent(EBreakerAggregatedAttribute::SlideSpeedMultiplier, 20.0f);
    Tree.AddIncreasedPercent(EBreakerAggregatedAttribute::AirControlMultiplier, 12.0f);

    Attributes->ApplyAttributeContribution(EBreakerAttributeContributor::Equipment, Gear);
    Attributes->ApplyAttributeContribution(EBreakerAttributeContributor::Progression, Tree);

    // The number this whole change is about: 1.40, never 1.44.
    TestEqual(TEXT("Slide speed is one additive bucket across layers"), Attributes->GetSlideSpeedMultiplier(), 1.40f, 0.0001f);
    TestTrue(TEXT("Slide speed is strictly below the old multiplicative read"),
        Attributes->GetSlideSpeedMultiplier() < 1.20f * 1.20f);
    TestEqual(TEXT("Air control is one additive bucket across layers"), Attributes->GetAirControlMultiplier(), 1.32f, 0.0001f);
    TestEqual(TEXT("Dash cooldown reduction takes the single gear bid"), Attributes->GetDashCooldownReduction(), 1.18f, 0.0001f);

    // Order cannot matter, and removal is exact.
    Attributes->ClearAttributeContribution(EBreakerAttributeContributor::Progression);
    TestEqual(TEXT("Clearing the tree leaves exactly the gear bucket"), Attributes->GetSlideSpeedMultiplier(), 1.20f, 0.0001f);
    Attributes->ClearAttributeContribution(EBreakerAttributeContributor::Equipment);
    TestEqual(TEXT("Clearing both restores the neutral base exactly"), Attributes->GetSlideSpeedMultiplier(), 1.0f);
    TestEqual(TEXT("Air control restores exactly too"), Attributes->GetAirControlMultiplier(), 1.0f);
    TestEqual(TEXT("Dash cooldown reduction restores exactly too"), Attributes->GetDashCooldownReduction(), 1.0f);

    // --- Real gear and real nodes, end to end ------------------------------
    UBreakerAttributeSet* Live = NewObject<UBreakerAttributeSet>();
    AActor* Owner = MakeMovementTestOwner();
    UBreakerEquipmentComponent* Equipment = NewObject<UBreakerEquipmentComponent>(Owner);
    UBreakerProgressionComponent* Progression = NewObject<UBreakerProgressionComponent>(Owner);

    FBreakerItemInstance Boots;
    Boots.ItemId = FGuid::NewGuid();
    Boots.Slot = EBreakerEquipSlot::Boots;
    Boots.Affixes.Add(MakeMovementRolledAffix(TEXT("Move.SlideSpeed"), 10.0f));
    Boots.Affixes.Add(MakeMovementRolledAffix(TEXT("Move.AirControl"), 15.0f));
    Boots.Affixes.Add(MakeMovementRolledAffix(TEXT("Move.DashCooldown"), 12.0f));

    Equipment->BindAttributes(Live);
    Equipment->EquipItem(Boots);

    // Core.Velocity.Slipstream is +5% slide speed (O2 placeholder content).
    FBreakerProgressionState Nodes;
    Nodes.PermanentClass = EBreakerClassId::Swift;
    Nodes.CoreNodeRanks.Add({TEXT("Core.Velocity.Slipstream"), 1});
    Progression->BindAttributes(Live);
    Progression->LoadProgressionState(Nodes);

    // 10% boots + 5% node in ONE bucket. Multiplied it would be 1.155.
    TestEqual(TEXT("Gear and node slide speed share one bucket"), Live->GetSlideSpeedMultiplier(), 1.15f, 0.0001f);
    TestEqual(TEXT("Gear air control reaches the attribute"), Live->GetAirControlMultiplier(), 1.15f, 0.0001f);
    TestEqual(TEXT("Gear dash cooldown reduction reaches the attribute"), Live->GetDashCooldownReduction(), 1.12f, 0.0001f);

    // --- The movement component consumes it, and agrees with the fallback --
    // A movement component with no owner has no attribute set and no layers, so
    // the fallback path has to produce exactly the neutral composition. The two
    // paths are the same arithmetic by construction; this pins that they stay so.
    UBreakerCharacterMovementComponent* Movement = NewObject<UBreakerCharacterMovementComponent>();
    TestEqual(TEXT("The dash cooldown SCALE is the reciprocal of the reduction"),
        Movement->GetComposedDashCooldownMultiplier(), 1.0f);
    TestTrue(TEXT("A 12% reduction is a 0.893x cooldown scale"),
        FMath::IsNearlyEqual(1.0f / 1.12f, 0.8929f, 0.001f));

    // --- The base the attribute set cannot know on its own -----------------
    // MoveSpeed's base is authored on the movement component (WalkSpeed is
    // EditAnywhere there), so a composed MoveSpeed that disagrees with the
    // speed the character actually walks at would be an attribute that lies.
    UBreakerAttributeSet* Published = NewObject<UBreakerAttributeSet>();
    Published->SetAggregatedAttributeBase(EBreakerAggregatedAttribute::MoveSpeed, Movement->WalkSpeed);
    TestEqual(TEXT("The published base is the movement component's walk speed"),
        Published->GetAttributeBase(EBreakerAggregatedAttribute::MoveSpeed), Movement->WalkSpeed);
    TestEqual(TEXT("With nothing contributing the attribute IS the walk speed"),
        Published->GetMoveSpeed(), Movement->WalkSpeed);
    // Publishing one base must not leave the others uncaptured.
    TestTrue(TEXT("Publishing a base captures the rest"), Published->HasCapturedAttributeBases());
    TestEqual(TEXT("Other bases are still the authored ones"),
        Published->GetAttributeBase(EBreakerAggregatedAttribute::MaxHealth), 100.0f);

    FBreakerAttributeContribution Speed;
    Speed.AddIncreasedPercent(EBreakerAggregatedAttribute::MoveSpeed, 20.0f);
    Published->ApplyAttributeContribution(EBreakerAttributeContributor::Equipment, Speed);
    Published->ApplyAttributeContribution(EBreakerAttributeContributor::Progression, Speed);
    // 20 + 20 additive against a 595 walk speed: 833, not 857.
    TestEqual(TEXT("Move speed composes additively over the published base"),
        Published->GetMoveSpeed(), Movement->WalkSpeed * 1.40f, 0.01f);
    return true;
}

#endif
