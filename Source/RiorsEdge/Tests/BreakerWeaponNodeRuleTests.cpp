#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Classes/BreakerGritComponent.h"
#include "GameFramework/Actor.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionLibrary.h"
#include "Weapons/BreakerWeaponComponent.h"
#include "Weapons/BreakerWeaponMath.h"

// ---------------------------------------------------------------------------
// GUNSMITH / TANK WEAPON-HALF NODE RULES (2026-08-16, the weapon-half pay
// pass). The Armory nodes whose consumers live on UBreakerWeaponComponent —
// AR3 Chambered's reload-to-fire boundary, AR5 Last Round's dump timing, AR10
// Overpressure's capacity shrink, AR11 No Reserve's halved ceiling — and Tank
// Bastion B7 Emplacement's stationary-spread read.
//
// The suite is world-free, so the fire path itself (FireOnce / FinishReload)
// cannot run here — the standing every other trace-and-timer path in Weapons/
// has. What IS pinned: each rule's pure half on FBreakerWeaponMath, fed by a
// node bought through the REAL purchase path (the BuiltClassKit shape), the
// capacity shrink end to end on a live component, and — first in every case —
// that a build WITHOUT the node reads the pre-node numbers to the bit.
// ---------------------------------------------------------------------------

namespace BreakerWeaponNodeRuleTest
{
    struct FBreakerWeaponNodeRig
    {
        AActor* Owner = nullptr;
        UBreakerProgressionComponent* Progression = nullptr;
        UBreakerWeaponComponent* Weapon = nullptr;
    };

    static FBreakerWeaponNodeRig BreakerMakeWeaponNodeRig(EBreakerClassId ClassId, int32 ClassPointBudget)
    {
        FBreakerWeaponNodeRig Rig;
        Rig.Owner = NewObject<AActor>();
        Rig.Progression = NewObject<UBreakerProgressionComponent>(Rig.Owner);
        Rig.Progression->ChoosePermanentClassById(ClassId);
        Rig.Progression->GrantPlaytestPoints(0, ClassPointBudget);
        Rig.Weapon = NewObject<UBreakerWeaponComponent>(Rig.Owner);
        return Rig;
    }

    static bool BreakerBuyNode(UBreakerProgressionComponent* Progression, UBreakerProgressionTree* Tree, const TCHAR* NodeId, int32 Ranks = 1)
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
// Armory boundary rules: AR3 Chambered's free round, AR5 Last Round's dump
// threshold, AR11 No Reserve's halved ceiling. Bought for real; the pure rule
// flips on the bought tag and reads the pre-node number without it.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerGunsmithWeaponHalfNodesTest,
    "RiorsEdge.Weapons.GunsmithNodes.ArmoryWeaponHalves",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerGunsmithWeaponHalfNodesTest::RunTest(const FString& Parameters)
{
    using namespace BreakerWeaponNodeRuleTest;
    FBreakerWeaponNodeRig Rig = BreakerMakeWeaponNodeRig(EBreakerClassId::Gunsmith, 30);
    UBreakerProgressionTree* Armory = UBreakerProgressionLibrary::GetGunsmithArmoryTree();

    // ---- WITHOUT the nodes: the pre-node numbers, to the bit --------------
    TestEqual(TEXT("Unarmed, every shot debits exactly one round"), FBreakerWeaponMath::MagazineDebitRounds(false), 1);
    TestEqual(TEXT("Without Last Round the dump fires on empty (threshold 0)"), FBreakerWeaponMath::MagazineDumpThresholdRounds(false), 0);
    TestEqual(TEXT("Without No Reserve the SMG ceiling is 2x starting"), FBreakerWeaponMath::ReserveCapRounds(175, 2.0f, false), 350);
    TestFalse(TEXT("A fresh weapon has no chambered round armed"), Rig.Weapon->IsChamberedRoundArmed());
    TestFalse(TEXT("No node, no tag"), Rig.Progression->HasNodeTag(BreakerNodeTags::Node_AR_Chambered.GetTag()));

    // ---- AR3 Chambered ----------------------------------------------------
    TestTrue(TEXT("Chambered purchases"), BreakerBuyNode(Rig.Progression, Armory, TEXT("Gunsmith.Armory.Chambered")));
    TestTrue(TEXT("The bought tag is the one the weapon reads at the reload boundary"),
        Rig.Progression->HasNodeTag(BreakerNodeTags::Node_AR_Chambered.GetTag()));
    // The rule the boundary applies: the armed round is free, and only it —
    // the arm site is FinishReload (world-only), node-gated there.
    TestEqual(TEXT("The armed chambered round debits nothing"), FBreakerWeaponMath::MagazineDebitRounds(true), 0);

    // ---- AR5 Last Round ---------------------------------------------------
    TestTrue(TEXT("Field Stripping purchases to rank 2"), BreakerBuyNode(Rig.Progression, Armory, TEXT("Gunsmith.Armory.FieldStripping"), 2));
    TestTrue(TEXT("Last Round purchases"), BreakerBuyNode(Rig.Progression, Armory, TEXT("Gunsmith.Armory.LastRound")));
    TestTrue(TEXT("The bought tag is the one the weapon reads at the dump boundary"),
        Rig.Progression->HasNodeTag(BreakerNodeTags::Node_AR_LastRound.GetTag()));
    // The payout moves to "your last round": the event fires with one round
    // still chambered. (The rig-side half — the window ignoring that event —
    // is UBreakerAbility_SidearmRig::WindowClosesOnMagazineEmptied, already
    // pinned in the Gunsmith consumer suite.)
    TestEqual(TEXT("With Last Round the dump threshold is one chambered round"), FBreakerWeaponMath::MagazineDumpThresholdRounds(true), 1);

    // ---- AR11 No Reserve --------------------------------------------------
    TestTrue(TEXT("Working Stock purchases to rank 2"), BreakerBuyNode(Rig.Progression, Armory, TEXT("Gunsmith.Armory.WorkingStock"), 2));
    TestTrue(TEXT("No Reserve purchases"), BreakerBuyNode(Rig.Progression, Armory, TEXT("Gunsmith.Armory.NoReserve")));
    TestTrue(TEXT("The bought tag is the one the reserve ceiling reads"),
        Rig.Progression->HasNodeTag(BreakerNodeTags::Node_AR_NoReserve.GetTag()));
    TestEqual(TEXT("The halving is exact on an even ceiling"), FBreakerWeaponMath::ReserveCapRounds(175, 2.0f, true), 175);
    // Halved INSIDE the ceil: 15 x 2 x 0.5 = 15 exactly, not ceil(30)/2's
    // ambiguity — one rounding, at the end.
    TestEqual(TEXT("An odd starting reserve halves exactly"), FBreakerWeaponMath::ReserveCapRounds(15, 2.0f, true), 15);
    TestEqual(TEXT("The same odd reserve unowned keeps the full ceiling"), FBreakerWeaponMath::ReserveCapRounds(15, 2.0f, false), 30);

    // ---- The live grant path honours the halved ceiling -------------------
    // A control rig without the node banks to the full 2x cap...
    FBreakerWeaponNodeRig Bare = BreakerMakeWeaponNodeRig(EBreakerClassId::Gunsmith, 0);
    Bare.Weapon->SetSlotArchetype(1, EBreakerWeaponArchetype::SMG);
    Bare.Weapon->AddReserveAmmoFraction(1.0f);
    TestEqual(TEXT("Without the node a full-fraction grant banks to 2x starting"), Bare.Weapon->GetReserveAmmo(), 350);
    // ...and the No Reserve build to exactly half of it.
    Rig.Weapon->SetSlotArchetype(1, EBreakerWeaponArchetype::SMG);
    Rig.Weapon->AddReserveAmmoFraction(1.0f);
    TestEqual(TEXT("With No Reserve the same grant caps at half"), Rig.Weapon->GetReserveAmmo(), 175);
    return true;
}

// ---------------------------------------------------------------------------
// AR10 Overpressure's seam: the capacity hook's SHRINK form, end to end on a
// live component — clamped at one chambered round, displaced rounds settling
// to reserve 1:1, nothing owed on the pop, and the positive (conversion) form
// bit-identical to before the shrink existed.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerMagazineCapacityShrinkTest,
    "RiorsEdge.Weapons.GunsmithNodes.MagazineCapacityShrink",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerMagazineCapacityShrinkTest::RunTest(const FString& Parameters)
{
    // The pure clamp first.
    TestEqual(TEXT("A growth delta passes through the clamp untouched"), FBreakerWeaponMath::ClampMagazineCapacityDelta(8, 4), 4);
    TestEqual(TEXT("A legal shrink passes through"), FBreakerWeaponMath::ClampMagazineCapacityDelta(8, -3), -3);
    TestEqual(TEXT("An over-shrink clamps to leave one round"), FBreakerWeaponMath::ClampMagazineCapacityDelta(8, -100), -7);
    TestEqual(TEXT("A shrink against a one-round magazine clamps to nothing"), FBreakerWeaponMath::ClampMagazineCapacityDelta(1, -5), 0);

    UBreakerWeaponComponent* Weapon = NewObject<UBreakerWeaponComponent>();
    Weapon->EquipArchetype(EBreakerWeaponArchetype::Shotgun);   // 8 / 40
    TestEqual(TEXT("The shotgun starts at its authored capacity"), Weapon->GetEffectiveMagazineSize(), 8);

    // A shrink reduces capacity and settles the displaced rounds to reserve
    // 1:1 — real rounds changing pockets, not a conversion.
    TestEqual(TEXT("A -3 shrink applies in full"), Weapon->PushMagazineCapacityOverride(TEXT("TestShrink"), -3), -3);
    TestEqual(TEXT("Capacity shrank"), Weapon->GetEffectiveMagazineSize(), 5);
    TestEqual(TEXT("The magazine clamped to the shrunk capacity"), Weapon->GetMagazineAmmo(), 5);
    TestEqual(TEXT("The displaced rounds settled to reserve 1:1"), Weapon->GetReserveAmmo(), 43);

    // An over-shrink under a live shrink clamps against what remains.
    TestEqual(TEXT("An over-shrink clamps to keep one chambered round"), Weapon->PushMagazineCapacityOverride(TEXT("TestShrinkDeep"), -100), -4);
    TestEqual(TEXT("Capacity floors at one round"), Weapon->GetEffectiveMagazineSize(), 1);
    TestEqual(TEXT("The magazine follows the floor"), Weapon->GetMagazineAmmo(), 1);
    TestEqual(TEXT("Every displaced round is in reserve"), Weapon->GetReserveAmmo(), 47);

    // At the floor there is nothing left to shrink: refused, no entry stored.
    TestEqual(TEXT("A shrink at the floor is refused"), Weapon->PushMagazineCapacityOverride(TEXT("TestShrinkRefused"), -5), 0);
    Weapon->PopMagazineCapacityOverride(TEXT("TestShrinkRefused"));
    TestEqual(TEXT("The refused key stored nothing to pop"), Weapon->GetEffectiveMagazineSize(), 1);

    // The pop restores capacity and owes nothing — the player reloads into
    // the headroom; no round moves and none is minted or lost.
    Weapon->PopMagazineCapacityOverride(TEXT("TestShrinkDeep"));
    TestEqual(TEXT("Popping the deep shrink restores its capacity"), Weapon->GetEffectiveMagazineSize(), 5);
    Weapon->PopMagazineCapacityOverride(TEXT("TestShrink"));
    TestEqual(TEXT("Popping the last shrink restores the authored capacity"), Weapon->GetEffectiveMagazineSize(), 8);
    TestEqual(TEXT("The pop moves no rounds"), Weapon->GetMagazineAmmo(), 1);
    TestEqual(TEXT("Rounds are conserved across the whole shrink cycle"),
        Weapon->GetMagazineAmmo() + Weapon->GetReserveAmmo(), 48);

    // Re-pushing a key replaces rather than stacks, matching every other
    // keyed push on the component.
    Weapon->PushMagazineCapacityOverride(TEXT("TestShrink"), -2);
    Weapon->PushMagazineCapacityOverride(TEXT("TestShrink"), -3);
    TestEqual(TEXT("A re-pushed shrink replaces its delta"), Weapon->GetEffectiveMagazineSize(), 5);
    Weapon->PopMagazineCapacityOverride(TEXT("TestShrink"));

    // BIT-IDENTITY of the positive path: the conversion form draws, debits
    // and settles exactly as it did before negative deltas existed.
    UBreakerWeaponComponent* Conversion = NewObject<UBreakerWeaponComponent>();
    Conversion->EquipArchetype(EBreakerWeaponArchetype::Shotgun);   // 8 / 40
    TestEqual(TEXT("The conversion form still draws in full"), Conversion->PushMagazineCapacityOverride(TEXT("TestGrow"), 4, 3), 4);
    TestEqual(TEXT("Conversion capacity is base plus the draw"), Conversion->GetEffectiveMagazineSize(), 12);
    TestEqual(TEXT("Conversion still debits 3:1"), Conversion->GetReserveAmmo(), 28);
    TestEqual(TEXT("Conversion still fills the magazine"), Conversion->GetMagazineAmmo(), 12);
    Conversion->PopMagazineCapacityOverride(TEXT("TestGrow"));
    TestEqual(TEXT("The conversion pop still settles the unspent remainder"), Conversion->GetMagazineAmmo(), 8);
    TestEqual(TEXT("The settle still refunds at the bought ratio"), Conversion->GetReserveAmmo(), 40);
    TestEqual(TEXT("A zero delta is still refused"), Conversion->PushMagazineCapacityOverride(TEXT("TestZero"), 0), 0);
    return true;
}

// ---------------------------------------------------------------------------
// Tank Bastion B7 Emplacement, the weapon half: spread reads as stationary
// behind your own Anchor Point. The geometry rule is pure; the component read
// is bought for real and stays honest — owned but with no anchor standing it
// grants nothing.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerEmplacementStationarySpreadTest,
    "RiorsEdge.Weapons.TankNodes.EmplacementStationarySpread",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerEmplacementStationarySpreadTest::RunTest(const FString& Parameters)
{
    using namespace BreakerWeaponNodeRuleTest;

    // The pure rule: node AND proximity, by the Grit layer's own radius.
    TestFalse(TEXT("Unowned, no posture rewrite at any distance"), FBreakerWeaponMath::SpreadReadsStationary(false, 0.0f, 300.0f));
    TestTrue(TEXT("Owned and at the anchor reads stationary"), FBreakerWeaponMath::SpreadReadsStationary(true, 200.0f, 300.0f));
    TestTrue(TEXT("The radius edge is inclusive, matching the Grit rules"), FBreakerWeaponMath::SpreadReadsStationary(true, 300.0f, 300.0f));
    TestFalse(TEXT("Beyond the radius the posture is honest"), FBreakerWeaponMath::SpreadReadsStationary(true, 400.0f, 300.0f));
    TestFalse(TEXT("A zeroed radius grants nothing"), FBreakerWeaponMath::SpreadReadsStationary(true, 0.0f, 0.0f));

    // Bought for real, through Bastion's own gates.
    FBreakerWeaponNodeRig Rig = BreakerMakeWeaponNodeRig(EBreakerClassId::Tank, 30);
    UBreakerGritComponent* Grit = NewObject<UBreakerGritComponent>(Rig.Owner);
    TestFalse(TEXT("Without the node the weapon never reads stationary"), Rig.Weapon->IsSpreadReadingStationary());

    UBreakerProgressionTree* Bastion = UBreakerProgressionLibrary::GetTankBastionTree();
    TestTrue(TEXT("Loud purchases to rank 2"), BreakerBuyNode(Rig.Progression, Bastion, TEXT("Tank.Bastion.Loud"), 2));
    TestTrue(TEXT("Answering Fire purchases to rank 2"), BreakerBuyNode(Rig.Progression, Bastion, TEXT("Tank.Bastion.AnsweringFire"), 2));
    TestTrue(TEXT("Emplacement purchases"), BreakerBuyNode(Rig.Progression, Bastion, TEXT("Tank.Bastion.Emplacement")));
    TestTrue(TEXT("The bought tag is the one the weapon reads"),
        Rig.Progression->HasNodeTag(BreakerNodeTags::Node_B_Emplacement.GetTag()));

    // Owned but with NO anchor standing (a world-free rig can raise none):
    // GetOwnAnchorDistanceCm reads as infinite and the posture stays honest.
    // The true case is the pure rule above; the live distance is the Grit
    // component's own tested query.
    TestTrue(TEXT("The rig's grit reports no anchor standing"), Grit->GetOwnAnchorDistanceCm() > Grit->AnchorNearRadiusCm);
    TestFalse(TEXT("Owned with no anchor standing grants nothing"), Rig.Weapon->IsSpreadReadingStationary());
    return true;
}

#endif
