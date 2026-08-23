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
// The nine Gunsmith branch trees shipped as rules-as-tags with WAITING ON
// comments naming their consumers; this file pins the consumers that now
// exist. The shape is the BuiltClassKit shape throughout: a rig BUYS the node
// through the real purchase path, and the rule observably changes Scrap /
// deployable / ability behaviour — and without the purchase every path is
// bit-identical to the pre-node behaviour, asserted first in each test so a
// consumer can never leak its rule to a build that did not choose it.
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
        Rig.Progression->GrantPlaytestPoints(0, ClassPointBudget);
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
// Armory: the Scrap-side consumers. Field Stripping R2 opens the dump source,
// No Reserve doubles the reload/magazine grants, and neither moves an inch
// for a build without the node.
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
    TestEqual(TEXT("Without Field Stripping R2 a partial dump pays nothing"), Rig.Scrap->GetScrap(), 0.0f);
    Rig.Scrap->NotifyReloadCompleted(true);
    Rig.Scrap->AdvanceLoop(1.0f);
    TestEqual(TEXT("Without No Reserve the reload grant is the authored grant"), Rig.Scrap->GetScrap(), Rig.Scrap->ReloadGrant);

    // Buy AR1 to rank 2: the "magazine was full at cycle start" requirement on
    // the dump source is removed.
    TestTrue(TEXT("Field Stripping purchases to rank 2"), BreakerBuy(Rig.Progression, Armory, TEXT("Gunsmith.Armory.FieldStripping"), 2));
    const float BeforeOpenDump = Rig.Scrap->GetScrap();
    Rig.Scrap->NotifyMagazineEmptied(false);
    Rig.Scrap->AdvanceLoop(1.0f);
    TestEqual(TEXT("Field Stripping R2 pays the dump on a partial cycle"),
        Rig.Scrap->GetScrap(), BeforeOpenDump + Rig.Scrap->MagazineDumpGrant);

    // Reach tier 4 and buy AR11 No Reserve: reload and magazine Scrap double.
    TestTrue(TEXT("Working Stock purchases"), BreakerBuy(Rig.Progression, Armory, TEXT("Gunsmith.Armory.WorkingStock"), 2));
    TestTrue(TEXT("Chambered purchases"), BreakerBuy(Rig.Progression, Armory, TEXT("Gunsmith.Armory.Chambered"), 2));
    TestTrue(TEXT("No Reserve purchases"), BreakerBuy(Rig.Progression, Armory, TEXT("Gunsmith.Armory.NoReserve")));
    const float BeforeDoubled = Rig.Scrap->GetScrap();
    Rig.Scrap->NotifyReloadCompleted(true);
    Rig.Scrap->AdvanceLoop(1.0f);
    Rig.Scrap->NotifyMagazineEmptied(true);
    Rig.Scrap->AdvanceLoop(2.0f);
    TestEqual(TEXT("No Reserve doubles the reload and magazine grants"),
        Rig.Scrap->GetScrap(), BeforeDoubled + 2.0f * Rig.Scrap->ReloadGrant + 2.0f * Rig.Scrap->MagazineDumpGrant);

    // AR2 Working Stock is already owned at rank 2 by the walk above: Dry and
    // Stocked read one tier faster, Surplus never does.
    TestEqual(TEXT("Working Stock R2 shifts the reload tier while low"), Rig.Scrap->GetReloadTierShift(), 1);
    Rig.Attributes->ApplyClassResource(80.0f);   // Surplus at the shipped 0.60
    TestEqual(TEXT("No tier shift while Surplus"), Rig.Scrap->GetReloadTierShift(), 0);
    Rig.Attributes->ApplyClassResource(0.0f);

    // The pure band rule, all ranks.
    TestEqual(TEXT("Rank 1 shifts while Dry only"), UBreakerScrapComponent::ReloadTierShiftFor(1, EBreakerScrapState::Dry), 1);
    TestEqual(TEXT("Rank 1 gives Stocked nothing"), UBreakerScrapComponent::ReloadTierShiftFor(1, EBreakerScrapState::Stocked), 0);
    TestEqual(TEXT("Rank 2 extends to Stocked"), UBreakerScrapComponent::ReloadTierShiftFor(2, EBreakerScrapState::Stocked), 1);
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

    // AR4 Deep Pockets: the overflow receiver pays nothing without the node.
    Rig.Scrap->NotifyAmmoPickupOverflow(10.0f);
    Rig.Scrap->AdvanceLoop(1.0f);
    TestEqual(TEXT("Overflow pays nothing without Deep Pockets"), Rig.Scrap->GetScrap(), 0.0f);
    TestTrue(TEXT("Field Stripping purchases"), BreakerBuy(Rig.Progression, Armory, TEXT("Gunsmith.Armory.FieldStripping"), 2));
    TestTrue(TEXT("Deep Pockets rank 1 purchases"), BreakerBuy(Rig.Progression, Armory, TEXT("Gunsmith.Armory.DeepPockets")));
    Rig.Scrap->NotifyAmmoPickupOverflow(10.0f);
    Rig.Scrap->AdvanceLoop(1.0f);
    TestEqual(TEXT("Rank 1 converts overflow at the fixed rate"),
        Rig.Scrap->GetScrap(), 10.0f * Rig.Scrap->OverflowScrapPerRound);
    TestTrue(TEXT("Deep Pockets rank 2 purchases"), BreakerBuy(Rig.Progression, Armory, TEXT("Gunsmith.Armory.DeepPockets")));
    const float BeforeDoubledOverflow = Rig.Scrap->GetScrap();
    Rig.Scrap->NotifyAmmoPickupOverflow(10.0f);
    Rig.Scrap->AdvanceLoop(1.0f);
    TestEqual(TEXT("Rank 2 doubles the rate"),
        Rig.Scrap->GetScrap(), BeforeDoubledOverflow + 20.0f * Rig.Scrap->OverflowScrapPerRound);

    // AR9 Reciprocal: nothing without the node; with it, the credit lands
    // IMMEDIATELY — outside the metered budget — which is the node's clause.
    const float BeforeReciprocal = Rig.Scrap->GetScrap();
    Rig.Scrap->NotifyAmmoReturnedOnKill(3);
    TestEqual(TEXT("Ammo return pays nothing without Reciprocal"), Rig.Scrap->GetScrap(), BeforeReciprocal);
    TestTrue(TEXT("Last Round purchases"), BreakerBuy(Rig.Progression, Armory, TEXT("Gunsmith.Armory.LastRound"), 2));
    TestTrue(TEXT("Cold Barrel purchases"), BreakerBuy(Rig.Progression, Armory, TEXT("Gunsmith.Armory.Chambered"), 2)
        && BreakerBuy(Rig.Progression, Armory, TEXT("Gunsmith.Armory.ColdBarrel"), 2));
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
    TestEqual(TEXT("Cold Barrel shaves 1.5s at rank 1"), UBreakerAbility_SidearmRig::ColdBarrelShave(1), 1.5f);
    TestEqual(TEXT("Cold Barrel shaves 2.5s at rank 2"), UBreakerAbility_SidearmRig::ColdBarrelShave(2), 2.5f);
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

    // FT1 Salvage: 65% at rank 1, 80% at rank 2, observably at the refund.
    TestTrue(TEXT("Salvage rank 1 purchases"), BreakerBuy(Rig.Progression, FieldTech, TEXT("Gunsmith.FieldTech.Salvage")));
    Rig.Scrap->NotifyDeployableDestroyed(40.0f);
    Rig.Scrap->AdvanceLoop(5.0f);
    TestEqual(TEXT("Rank 1 refunds 65% of a 40-cost deployable"), Rig.Scrap->GetScrap(), 26.0f);
    TestTrue(TEXT("Salvage rank 2 purchases"), BreakerBuy(Rig.Progression, FieldTech, TEXT("Gunsmith.FieldTech.Salvage")));
    TestEqual(TEXT("Rank 2 is the 80% hard ceiling"), Rig.Scrap->GetEffectiveDestructionRefundFraction(), 0.80f);

    // FT4 Tithe: while Surplus the deployable-damage source skips the meter —
    // the credit lands with NO AdvanceLoop at all; while Dry it still queues.
    TestTrue(TEXT("Tithe purchases"), BreakerBuy(Rig.Progression, FieldTech, TEXT("Gunsmith.FieldTech.Tithe"), 2));
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

    // FT9 Redundancy raises the resting cap to 5 for its owner.
    TestTrue(TEXT("Requisition purchases"), BreakerBuy(Rig.Progression, FieldTech, TEXT("Gunsmith.FieldTech.Requisition"), 2));
    TestTrue(TEXT("Redundancy purchases"), BreakerBuy(Rig.Progression, FieldTech, TEXT("Gunsmith.FieldTech.Redundancy")));
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
    TestEqual(TEXT("The discount table is 10 then 18"), ABreakerDeployable::RequisitionDiscountFor(1), 10.0f);
    TestEqual(TEXT("...transcribed at rank 2"), ABreakerDeployable::RequisitionDiscountFor(2), 18.0f);

    // FT3 Second Shift's clock rule: +8/+14, hard 2x-base ceiling.
    TestEqual(TEXT("No node leaves the clock alone"), ABreakerDeployable::SecondShiftLifetime(0, 30.0f, 12.0f), 12.0f);
    TestEqual(TEXT("Rank 1 adds 8s"), ABreakerDeployable::SecondShiftLifetime(1, 30.0f, 12.0f), 20.0f);
    TestEqual(TEXT("Rank 2 adds 14s"), ABreakerDeployable::SecondShiftLifetime(2, 30.0f, 12.0f), 26.0f);
    TestEqual(TEXT("Never past double the base"), ABreakerDeployable::SecondShiftLifetime(2, 30.0f, 55.0f), 60.0f);
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

    // TK5 Attrition Field: nothing without the node; 8 then 14, immediately.
    Rig.Scrap->NotifyDisruptorFieldKill();
    TestEqual(TEXT("A field kill pays nothing without Attrition Field"), Rig.Scrap->GetScrap(), 0.0f);
    TestTrue(TEXT("Cheap Work purchases"), BreakerBuy(Rig.Progression, Tinkerer, TEXT("Gunsmith.Tinkerer.CheapWork"), 2));
    TestTrue(TEXT("Attrition Field rank 1 purchases"), BreakerBuy(Rig.Progression, Tinkerer, TEXT("Gunsmith.Tinkerer.AttritionField")));
    Rig.Scrap->NotifyDisruptorFieldKill();
    TestEqual(TEXT("Rank 1 refunds 8, outside the meter"), Rig.Scrap->GetScrap(), 8.0f);
    TestTrue(TEXT("Attrition Field rank 2 purchases"), BreakerBuy(Rig.Progression, Tinkerer, TEXT("Gunsmith.Tinkerer.AttritionField")));
    Rig.Scrap->NotifyDisruptorFieldKill();
    TestEqual(TEXT("Rank 2 refunds 14"), Rig.Scrap->GetScrap(), 22.0f);

    // TK1 Cheap Work, the whole pricing rule: Tinkerer-only, Dry-only, 10/18
    // to a floor of 10, and the Requisition discount composes after it.
    using DeployAbility = UBreakerGunsmithDeployAbility;
    TestEqual(TEXT("No node, full price"),
        DeployAbility::EffectiveDeployCost(35.0f, EBreakerDeployableType::MineCluster, EBreakerScrapState::Dry, 0, 0.0f), 35.0f);
    TestEqual(TEXT("Rank 1 Dry mine costs 10 less"),
        DeployAbility::EffectiveDeployCost(35.0f, EBreakerDeployableType::MineCluster, EBreakerScrapState::Dry, 1, 0.0f), 25.0f);
    TestEqual(TEXT("Rank 2 Dry mine costs 18 less"),
        DeployAbility::EffectiveDeployCost(35.0f, EBreakerDeployableType::MineCluster, EBreakerScrapState::Dry, 2, 0.0f), 17.0f);
    TestEqual(TEXT("The floor is 10"),
        DeployAbility::EffectiveDeployCost(20.0f, EBreakerDeployableType::Disruptor, EBreakerScrapState::Dry, 2, 0.0f), 10.0f);
    TestEqual(TEXT("Stocked pays full price — rescue, not subsidy"),
        DeployAbility::EffectiveDeployCost(35.0f, EBreakerDeployableType::MineCluster, EBreakerScrapState::Stocked, 2, 0.0f), 35.0f);
    TestEqual(TEXT("A turret is not a Tinkerer deployable"),
        DeployAbility::EffectiveDeployCost(40.0f, EBreakerDeployableType::Turret, EBreakerScrapState::Dry, 2, 0.0f), 40.0f);
    TestEqual(TEXT("The Requisition discount composes after the floor"),
        DeployAbility::EffectiveDeployCost(20.0f, EBreakerDeployableType::Disruptor, EBreakerScrapState::Dry, 2, 18.0f), 0.0f);
    TestTrue(TEXT("The Tinkerer scope is mines and Disruptors exactly"),
        DeployAbility::IsTinkererDeployable(EBreakerDeployableType::MineCluster)
        && DeployAbility::IsTinkererDeployable(EBreakerDeployableType::Disruptor)
        && !DeployAbility::IsTinkererDeployable(EBreakerDeployableType::Turret)
        && !DeployAbility::IsTinkererDeployable(EBreakerDeployableType::AmmoCrate));

    // TK2 Quick Set: halved, then removed with the 1s smaller-radius trade.
    TestEqual(TEXT("No node keeps the authored delay"), ABreakerDeployable::QuickSetArmDelay(0, 1.0f), 1.0f);
    TestEqual(TEXT("Rank 1 halves the arm delay"), ABreakerDeployable::QuickSetArmDelay(1, 1.0f), 0.5f);
    TestEqual(TEXT("Rank 2 removes it"), ABreakerDeployable::QuickSetArmDelay(2, 1.0f), 0.0f);
    TestEqual(TEXT("Rank 2's fresh charge triggers 1 m short"), ABreakerDeployable::QuickSetTriggerRadius(2, 0.5f, 250.0f), 150.0f);
    TestEqual(TEXT("...and reads full after its first second"), ABreakerDeployable::QuickSetTriggerRadius(2, 1.5f, 250.0f), 250.0f);
    TestEqual(TEXT("Rank 1 never trades radius"), ABreakerDeployable::QuickSetTriggerRadius(1, 0.1f, 250.0f), 250.0f);

    // TK4 Rearm, TK7 Ordnance, TK9 Patience: transcribed numbers.
    TestEqual(TEXT("Rearm at 6s"), ABreakerDeployable::RearmInterval(1), 6.0f);
    TestEqual(TEXT("Rearm R2 at 4s"), ABreakerDeployable::RearmInterval(2), 4.0f);
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
