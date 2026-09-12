#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Abilities/BreakerGunsmithAbilities.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Classes/BreakerScrapComponent.h"
#include "Combat/BreakerDeployable.h"
#include "GameFramework/Actor.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionLibrary.h"

// ---------------------------------------------------------------------------
// GUNSMITH NODE CONSUMERS (2026-08-16, the branch-tree pay pass).
//
// The three Gunsmith doctrine trees shipped as rules-as-tags with WAITING ON
// comments naming their consumers; this file pins the consumers that now
// exist. The shape is the BuiltClassKit shape throughout: a rig BUYS the node
// through the real purchase path, and the rule observably changes Scrap /
// deployable / ability behaviour — and without the purchase every path is
// bit-identical to the pre-node behaviour, asserted first in each test so a
// consumer can never leak its rule to a build that did not choose it.
//
// Every node is single rank (O272): a travel buys from a standing start, an
// impactful buys behind its one travel, and the rank-one magnitude is what
// the old rank two paid. There is no rank-two purchase anywhere below.
// ---------------------------------------------------------------------------

namespace BreakerGunsmithNodeConsumerTest
{
    // Prefixed rig, per the unity-build house rule. ChoosePermanentClassById
    // (not DevForceClass) because PurchaseNode spends real points.
    struct FBreakerNodeConsumerRig
    {
        AActor* Owner = nullptr;
        UBreakerProgressionComponent* Progression = nullptr;
        UBreakerAttributeSet* Attributes = nullptr;
        UBreakerScrapComponent* Scrap = nullptr;
    };

    static FBreakerNodeConsumerRig BreakerMakeGunsmithRig(int32 ClassPointBudget)
    {
        FBreakerNodeConsumerRig Rig;
        Rig.Owner = NewObject<AActor>();
        Rig.Progression = NewObject<UBreakerProgressionComponent>(Rig.Owner);
        Rig.Attributes = NewObject<UBreakerAttributeSet>();
        Rig.Progression->ChoosePermanentClassById(EBreakerClassId::Gunsmith);
        Rig.Progression->GrantPlaytestPoints(ClassPointBudget, 0);
        Rig.Scrap = NewObject<UBreakerScrapComponent>(Rig.Owner);
        Rig.Scrap->BindAttributes(Rig.Attributes);
        return Rig;
    }

    static bool BreakerBuy(UBreakerProgressionComponent* Progression, UBreakerProgressionTree* Tree, const TCHAR* NodeId, int32 Ranks = 1)
    {
        FText Failure;
        for (int32 Rank = 0; Rank < Ranks; ++Rank)
        {
            if (!Progression->PurchaseNode(Tree, FName(NodeId), Failure)) return false;
        }
        return true;
    }
}

// ---------------------------------------------------------------------------
// Armory: the Scrap-side consumers. Field Stripping opens the dump source,
// No Reserve doubles the reload/magazine grants, and neither moves an inch
// for a build without the node. Every node is one rank (O272): the rank-one
// value is what the two-rank node used to pay at rank two.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerGunsmithArmoryScrapNodesTest,
    "RiorsEdge.Classes.GunsmithNodes.ArmoryScrapConsumers",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerGunsmithArmoryScrapNodesTest::RunTest(const FString& Parameters)
{
    using namespace BreakerGunsmithNodeConsumerTest;
    FBreakerNodeConsumerRig Rig = BreakerMakeGunsmithRig(30);
    UBreakerProgressionTree* Armory = UBreakerProgressionLibrary::GetGunsmithArmoryTree();

    // WITHOUT the nodes: a partial-magazine dump pays nothing (the base
    // anti-farm clause), and the reload grant is the authored grant.
    Rig.Scrap->NotifyMagazineEmptied(false);
    Rig.Scrap->AdvanceLoop(1.0f);
    TestEqual(TEXT("Without Field Stripping a partial dump pays nothing"), Rig.Scrap->GetScrap(), 0.0f);
    Rig.Scrap->NotifyReloadCompleted(true);
    Rig.Scrap->AdvanceLoop(1.0f);
    TestEqual(TEXT("Without No Reserve the reload grant is the authored grant"), Rig.Scrap->GetScrap(), Rig.Scrap->ReloadGrant);

    // Buy AR1 Field Stripping: the "magazine was full at cycle start"
    // requirement on the dump source is removed at rank one.
    TestTrue(TEXT("Field Stripping purchases"), BreakerBuy(Rig.Progression, Armory, TEXT("Gunsmith.Armory.FieldStripping")));
    const float BeforeOpenDump = Rig.Scrap->GetScrap();
    Rig.Scrap->NotifyMagazineEmptied(false);
    Rig.Scrap->AdvanceLoop(1.0f);
    TestEqual(TEXT("Field Stripping pays the dump on a partial cycle"),
        Rig.Scrap->GetScrap(), BeforeOpenDump + Rig.Scrap->MagazineDumpGrant);

    // AR11 No Reserve is Field Stripping's impactful half: it buys behind
    // that one travel, and reload and magazine Scrap double.
    TestTrue(TEXT("No Reserve purchases"), BreakerBuy(Rig.Progression, Armory, TEXT("Gunsmith.Armory.NoReserve")));
    const float BeforeDoubled = Rig.Scrap->GetScrap();
    Rig.Scrap->NotifyReloadCompleted(true);
    Rig.Scrap->AdvanceLoop(1.0f);
    Rig.Scrap->NotifyMagazineEmptied(true);
    Rig.Scrap->AdvanceLoop(2.0f);
    TestEqual(TEXT("No Reserve doubles the reload and magazine grants"),
        Rig.Scrap->GetScrap(), BeforeDoubled + 2.0f * Rig.Scrap->ReloadGrant + 2.0f * Rig.Scrap->MagazineDumpGrant);

    // AR2 Working Stock, a travel root: Dry and Stocked read one tier faster,
    // Surplus never does.
    TestEqual(TEXT("Without Working Stock the reload tier does not shift"), Rig.Scrap->GetReloadTierShift(), 0);
    TestTrue(TEXT("Working Stock purchases"), BreakerBuy(Rig.Progression, Armory, TEXT("Gunsmith.Armory.WorkingStock")));
    TestEqual(TEXT("Working Stock shifts the reload tier while low"), Rig.Scrap->GetReloadTierShift(), 1);
    Rig.Attributes->ApplyClassResource(80.0f);   // Surplus at the shipped 0.60
    TestEqual(TEXT("No tier shift while Surplus"), Rig.Scrap->GetReloadTierShift(), 0);
    Rig.Attributes->ApplyClassResource(0.0f);

    // The pure band rule: rank one shifts Dry and Stocked, never Surplus.
    TestEqual(TEXT("Rank 1 shifts while Dry"), UBreakerScrapComponent::ReloadTierShiftFor(1, EBreakerScrapState::Dry), 1);
    TestEqual(TEXT("Rank 1 shifts while Stocked"), UBreakerScrapComponent::ReloadTierShiftFor(1, EBreakerScrapState::Stocked), 1);
    TestEqual(TEXT("Rank 1 gives Surplus nothing"), UBreakerScrapComponent::ReloadTierShiftFor(1, EBreakerScrapState::Surplus), 0);
    TestEqual(TEXT("No node, no shift"), UBreakerScrapComponent::ReloadTierShiftFor(0, EBreakerScrapState::Dry), 0);
    return true;
}

// ---------------------------------------------------------------------------
// Armory: the receiver-half consumers (Deep Pockets, Reciprocal) and the
// Sidearm Rig / Overhaul window rules as pure boundaries.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerGunsmithArmoryReceiverAndWindowTest,
    "RiorsEdge.Classes.GunsmithNodes.ArmoryReceiversAndWindows",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerGunsmithArmoryReceiverAndWindowTest::RunTest(const FString& Parameters)
{
    using namespace BreakerGunsmithNodeConsumerTest;
    FBreakerNodeConsumerRig Rig = BreakerMakeGunsmithRig(30);
    UBreakerProgressionTree* Armory = UBreakerProgressionLibrary::GetGunsmithArmoryTree();

    // AR4 Deep Pockets: the overflow receiver pays nothing without the node;
    // the single rank converts at the doubled rate (O272: rank one carries
    // what rank two used to).
    Rig.Scrap->NotifyAmmoPickupOverflow(10.0f);
    Rig.Scrap->AdvanceLoop(1.0f);
    TestEqual(TEXT("Overflow pays nothing without Deep Pockets"), Rig.Scrap->GetScrap(), 0.0f);
    TestTrue(TEXT("Deep Pockets purchases"), BreakerBuy(Rig.Progression, Armory, TEXT("Gunsmith.Armory.DeepPockets")));
    const float BeforeDoubledOverflow = Rig.Scrap->GetScrap();
    Rig.Scrap->NotifyAmmoPickupOverflow(10.0f);
    Rig.Scrap->AdvanceLoop(1.0f);
    TestEqual(TEXT("Rank 1 converts overflow at the doubled rate"),
        Rig.Scrap->GetScrap(), BeforeDoubledOverflow + 20.0f * Rig.Scrap->OverflowScrapPerRound);

    // AR9 Reciprocal: nothing without the node; with it, the credit lands
    // IMMEDIATELY — outside the metered budget — which is the node's clause.
    // Reciprocal is Last Round's impactful half and buys behind it alone.
    const float BeforeReciprocal = Rig.Scrap->GetScrap();
    Rig.Scrap->NotifyAmmoReturnedOnKill(3);
    TestEqual(TEXT("Ammo return pays nothing without Reciprocal"), Rig.Scrap->GetScrap(), BeforeReciprocal);
    TestTrue(TEXT("Last Round purchases"), BreakerBuy(Rig.Progression, Armory, TEXT("Gunsmith.Armory.LastRound")));
    TestTrue(TEXT("Reciprocal purchases"), BreakerBuy(Rig.Progression, Armory, TEXT("Gunsmith.Armory.Reciprocal")));
    const float BeforeReciprocalPaid = Rig.Scrap->GetScrap();
    Rig.Scrap->NotifyAmmoReturnedOnKill(3);
    TestEqual(TEXT("Reciprocal pays per returned round, uncapped and unqueued"),
        Rig.Scrap->GetScrap(), BeforeReciprocalPaid + 3.0f * Rig.Scrap->ReciprocalScrapPerReturn);

    // AR5/AR8: the Sidearm Rig's window boundary as the pure rule it is.
    TestTrue(TEXT("Bare rig closes on the magazine emptying"),
        UBreakerAbility_SidearmRig::WindowClosesOnMagazineEmptied(false, false));
    TestFalse(TEXT("Last Round keeps the window past that round"),
        UBreakerAbility_SidearmRig::WindowClosesOnMagazineEmptied(true, false));
    TestFalse(TEXT("Rig Discipline ignores the magazine boundary"),
        UBreakerAbility_SidearmRig::WindowClosesOnMagazineEmptied(false, true));
    TestTrue(TEXT("Bare rig closes on reload"),
        UBreakerAbility_SidearmRig::WindowClosesOnReloadStart(false, 0));
    TestFalse(TEXT("Rig Discipline survives one reload"),
        UBreakerAbility_SidearmRig::WindowClosesOnReloadStart(true, 0));
    TestTrue(TEXT("...and only one"),
        UBreakerAbility_SidearmRig::WindowClosesOnReloadStart(true, 1));

    // AR6 Cold Barrel's shave and AR7 Bench Work's tail, transcribed numbers.
    // O2 PLACEHOLDER: the single rank shaves what rank two used to.
    TestEqual(TEXT("Cold Barrel shaves 2.5s at rank 1"), UBreakerAbility_SidearmRig::ColdBarrelShave(1), 2.5f);
    TestEqual(TEXT("No node, no shave"), UBreakerAbility_SidearmRig::ColdBarrelShave(0), 0.0f);
    TestEqual(TEXT("Bench Work's tail is half the drawn capacity"), UBreakerAbility_Overhaul::BenchWorkTailRounds(10), 5);
    TestEqual(TEXT("A one-round window has no tail"), UBreakerAbility_Overhaul::BenchWorkTailRounds(1), 0);
    return true;
}

// ---------------------------------------------------------------------------
// Field Tech: refund economy and density. Salvage moves the refund fraction,
// Tithe uncaps deployable damage while Surplus, Redundancy raises the resting
// cap to 5, Logistics lifts the crate from the total count.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerGunsmithFieldTechNodesTest,
    "RiorsEdge.Classes.GunsmithNodes.FieldTechConsumers",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerGunsmithFieldTechNodesTest::RunTest(const FString& Parameters)
{
    using namespace BreakerGunsmithNodeConsumerTest;
    FBreakerNodeConsumerRig Rig = BreakerMakeGunsmithRig(30);
    UBreakerProgressionTree* FieldTech = UBreakerProgressionLibrary::GetGunsmithFieldTechTree();

    // WITHOUT any node: authored refund fraction, metered damage source,
    // resting cap of 4, everything counts.
    TestEqual(TEXT("Without Salvage the refund fraction is the authored 50%"),
        Rig.Scrap->GetEffectiveDestructionRefundFraction(), Rig.Scrap->DestructionRefundFraction);
    TestEqual(TEXT("Without Redundancy the resting cap is 4"),
        ABreakerDeployable::TotalCapFor(Rig.Owner), ABreakerDeployable::BaseTotalDensityCap);
    TestTrue(TEXT("Without Logistics the crate counts"),
        ABreakerDeployable::CountsAgainstDensityCap(EBreakerDeployableType::AmmoCrate, false));

    // FT1 Salvage: the single rank is the 80% hard ceiling, observably at the
    // refund. O2 PLACEHOLDER.
    TestTrue(TEXT("Salvage purchases"), BreakerBuy(Rig.Progression, FieldTech, TEXT("Gunsmith.FieldTech.Salvage")));
    TestEqual(TEXT("Rank 1 is the 80% hard ceiling"), Rig.Scrap->GetEffectiveDestructionRefundFraction(), 0.80f);
    Rig.Scrap->NotifyDeployableDestroyed(40.0f);
    Rig.Scrap->AdvanceLoop(5.0f);
    TestEqual(TEXT("Rank 1 refunds 80% of a 40-cost deployable"), Rig.Scrap->GetScrap(), 32.0f);

    // FT4 Tithe: while Surplus the deployable-damage source skips the meter —
    // the credit lands with NO AdvanceLoop at all; while Dry it still queues.
    TestTrue(TEXT("Tithe purchases"), BreakerBuy(Rig.Progression, FieldTech, TEXT("Gunsmith.FieldTech.Tithe")));
    Rig.Attributes->ApplyClassResource(80.0f);   // Surplus
    const float BeforeTithe = Rig.Scrap->GetScrap();
    Rig.Scrap->NotifyDeployableDamageDealt(500.0f);
    TestEqual(TEXT("Surplus Tithe credits instantly, outside the meter"), Rig.Scrap->GetScrap(), BeforeTithe + 1.0f);
    Rig.Attributes->ApplyClassResource(0.0f);   // Dry again
    const float BeforeMetered = Rig.Scrap->GetScrap();
    Rig.Scrap->NotifyDeployableDamageDealt(500.0f);
    TestEqual(TEXT("Dry deployable damage still queues"), Rig.Scrap->GetScrap(), BeforeMetered);
    Rig.Scrap->AdvanceLoop(1.0f);
    TestEqual(TEXT("...and pays through the loop"), Rig.Scrap->GetScrap(), BeforeMetered + 1.0f);

    // FT9 Redundancy, Salvage's impactful half, raises the resting cap to 5
    // for its owner.
    TestTrue(TEXT("Redundancy purchases behind Salvage"), BreakerBuy(Rig.Progression, FieldTech, TEXT("Gunsmith.FieldTech.Redundancy")));
    TestEqual(TEXT("Redundancy's resting cap is 5"), ABreakerDeployable::TotalCapFor(Rig.Owner), 5);
    TestEqual(TEXT("The pure rule agrees"), ABreakerDeployable::BaseTotalCapFor(true), 5);

    // FT8 Logistics as the pure rule (the live count needs spawned actors).
    TestFalse(TEXT("Logistics lifts the crate from the cap"),
        ABreakerDeployable::CountsAgainstDensityCap(EBreakerDeployableType::AmmoCrate, true));
    TestTrue(TEXT("Turrets always count"),
        ABreakerDeployable::CountsAgainstDensityCap(EBreakerDeployableType::Turret, true));
    TestFalse(TEXT("The Anchor Point never counts (Tank rule, §T3)"),
        ABreakerDeployable::CountsAgainstDensityCap(EBreakerDeployableType::AnchorPoint, false));

    // FT5's replacement credit registry: registered, read, expired, consumed.
    ABreakerDeployable::RegisterReplacementCredit(Rig.Owner, EBreakerDeployableType::Turret, 10.0f, 100.0);
    TestEqual(TEXT("A live credit answers its type"),
        ABreakerDeployable::PendingReplacementDiscount(Rig.Owner, EBreakerDeployableType::Turret, 50.0), 10.0f);
    TestEqual(TEXT("Another type reads nothing"),
        ABreakerDeployable::PendingReplacementDiscount(Rig.Owner, EBreakerDeployableType::Disruptor, 50.0), 0.0f);
    TestEqual(TEXT("An expired credit reads nothing"),
        ABreakerDeployable::PendingReplacementDiscount(Rig.Owner, EBreakerDeployableType::Turret, 150.0), 0.0f);
    ABreakerDeployable::ConsumeReplacementCredit(Rig.Owner, EBreakerDeployableType::Turret);
    TestEqual(TEXT("A consumed credit is gone"),
        ABreakerDeployable::PendingReplacementDiscount(Rig.Owner, EBreakerDeployableType::Turret, 50.0), 0.0f);
    TestEqual(TEXT("The discount is 18 at the single rank"), ABreakerDeployable::RequisitionDiscountFor(1), 18.0f);
    TestEqual(TEXT("No node, no discount"), ABreakerDeployable::RequisitionDiscountFor(0), 0.0f);

    // FT3 Second Shift's clock rule: +14 at the single rank, hard 2x-base
    // ceiling.
    TestEqual(TEXT("No node leaves the clock alone"), ABreakerDeployable::SecondShiftLifetime(0, 30.0f, 12.0f), 12.0f);
    TestEqual(TEXT("Rank 1 adds 14s"), ABreakerDeployable::SecondShiftLifetime(1, 30.0f, 12.0f), 26.0f);
    TestEqual(TEXT("Never past double the base"), ABreakerDeployable::SecondShiftLifetime(1, 30.0f, 55.0f), 60.0f);

    // FT6 Foreman's heal per charge is the single-rank number, read off the
    // class default object: no rank multiplier is left for a reader to apply.
    // O2 PLACEHOLDER.
    TestEqual(TEXT("Foreman heals 30 per charge, default-constructed"), GetDefault<ABreakerDeployable>()->ForemanHealPerCharge, 30.0f);
    return true;
}

// ---------------------------------------------------------------------------
// Tinkerer: cost, arm/trigger and rearm rules. Cheap Work discounts the broke
// Gunsmith only; Quick Set, Rearm, Ordnance and Patience carry transcribed
// numbers; Attrition Field pays through the Scrap component's field-kill
// notify. All of it rank-driven off a real purchase walk.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerGunsmithTinkererNodesTest,
    "RiorsEdge.Classes.GunsmithNodes.TinkererConsumers",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerGunsmithTinkererNodesTest::RunTest(const FString& Parameters)
{
    using namespace BreakerGunsmithNodeConsumerTest;
    FBreakerNodeConsumerRig Rig = BreakerMakeGunsmithRig(30);
    UBreakerProgressionTree* Tinkerer = UBreakerProgressionLibrary::GetGunsmithTinkererTree();

    // TK5 Attrition Field: nothing without the node; 14 at the single rank,
    // immediately. Attrition Field is a travel root and buys from a standing
    // start.
    Rig.Scrap->NotifyDisruptorFieldKill();
    TestEqual(TEXT("A field kill pays nothing without Attrition Field"), Rig.Scrap->GetScrap(), 0.0f);
    TestTrue(TEXT("Attrition Field purchases"), BreakerBuy(Rig.Progression, Tinkerer, TEXT("Gunsmith.Tinkerer.AttritionField")));
    Rig.Scrap->NotifyDisruptorFieldKill();
    TestEqual(TEXT("Rank 1 refunds 14, outside the meter"), Rig.Scrap->GetScrap(), 14.0f);

    // TK1 Cheap Work, the whole pricing rule: Tinkerer-only, Dry-only, 18 off
    // to a floor of 10, and the Requisition discount composes after it.
    using DeployAbility = UBreakerGunsmithDeployAbility;
    TestEqual(TEXT("No node, full price"),
        DeployAbility::EffectiveDeployCost(35.0f, EBreakerDeployableType::MineCluster, EBreakerScrapState::Dry, 0, 0.0f), 35.0f);
    TestEqual(TEXT("Rank 1 Dry mine costs 18 less"),
        DeployAbility::EffectiveDeployCost(35.0f, EBreakerDeployableType::MineCluster, EBreakerScrapState::Dry, 1, 0.0f), 17.0f);
    TestEqual(TEXT("The floor is 10"),
        DeployAbility::EffectiveDeployCost(20.0f, EBreakerDeployableType::Disruptor, EBreakerScrapState::Dry, 1, 0.0f), 10.0f);
    TestEqual(TEXT("Stocked pays full price — rescue, not subsidy"),
        DeployAbility::EffectiveDeployCost(35.0f, EBreakerDeployableType::MineCluster, EBreakerScrapState::Stocked, 1, 0.0f), 35.0f);
    TestEqual(TEXT("A turret is not a Tinkerer deployable"),
        DeployAbility::EffectiveDeployCost(40.0f, EBreakerDeployableType::Turret, EBreakerScrapState::Dry, 1, 0.0f), 40.0f);
    TestEqual(TEXT("The Requisition discount composes after the floor"),
        DeployAbility::EffectiveDeployCost(20.0f, EBreakerDeployableType::Disruptor, EBreakerScrapState::Dry, 1, 18.0f), 0.0f);
    TestTrue(TEXT("The Tinkerer scope is mines and Disruptors exactly"),
        DeployAbility::IsTinkererDeployable(EBreakerDeployableType::MineCluster)
        && DeployAbility::IsTinkererDeployable(EBreakerDeployableType::Disruptor)
        && !DeployAbility::IsTinkererDeployable(EBreakerDeployableType::Turret)
        && !DeployAbility::IsTinkererDeployable(EBreakerDeployableType::AmmoCrate));

    // TK2 Quick Set: the arm delay is removed at the single rank, with the
    // 1 s smaller-radius trade that comes with it.
    TestEqual(TEXT("No node keeps the authored delay"), ABreakerDeployable::QuickSetArmDelay(0, 1.0f), 1.0f);
    TestEqual(TEXT("Rank 1 removes the arm delay"), ABreakerDeployable::QuickSetArmDelay(1, 1.0f), 0.0f);
    TestEqual(TEXT("Rank 1's fresh charge triggers 1 m short"), ABreakerDeployable::QuickSetTriggerRadius(1, 0.5f, 250.0f), 150.0f);
    TestEqual(TEXT("...and reads full after its first second"), ABreakerDeployable::QuickSetTriggerRadius(1, 1.5f, 250.0f), 250.0f);
    TestEqual(TEXT("No node never trades radius"), ABreakerDeployable::QuickSetTriggerRadius(0, 0.1f, 250.0f), 250.0f);

    // TK4 Rearm, TK7 Ordnance, TK9 Patience: transcribed numbers.
    TestEqual(TEXT("Rearm at 4s at the single rank"), ABreakerDeployable::RearmInterval(1), 4.0f);
    TestEqual(TEXT("No Rearm, no interval"), ABreakerDeployable::RearmInterval(0), 0.0f);
    TestEqual(TEXT("Ordnance scatters 4"), ABreakerDeployable::OrdnanceMineCount(true, 3), 4);
    TestEqual(TEXT("Without it, the authored 3"), ABreakerDeployable::OrdnanceMineCount(false, 3), 3);
    TestEqual(TEXT("Ordnance merges same-second detonations for procs"),
        ABreakerDeployable::OrdnanceProcCoefficient(true, 10.5, 10.0), 0.0f);
    TestEqual(TEXT("...and pays full coefficient past the window"),
        ABreakerDeployable::OrdnanceProcCoefficient(true, 12.0, 10.0), 1.0f);
    TestEqual(TEXT("Without Ordnance the coefficient never moves"),
        ABreakerDeployable::OrdnanceProcCoefficient(false, 10.5, 10.0), 1.0f);
    TestTrue(TEXT("Patience arms at 10s"), ABreakerDeployable::PatienceQualifies(10.0f));
    TestFalse(TEXT("...and not before"), ABreakerDeployable::PatienceQualifies(9.9f));
    return true;
}

#endif  // WITH_DEV_AUTOMATION_TESTS
