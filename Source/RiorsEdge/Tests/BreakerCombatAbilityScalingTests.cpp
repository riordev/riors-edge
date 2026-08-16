#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Abilities/BreakerAbility_Cleave.h"
#include "Abilities/BreakerAbility_Fracture.h"
#include "Abilities/BreakerAbility_Rot.h"
#include "Abilities/BreakerAbility_Siphon.h"
#include "Abilities/BreakerGameplayAbility.h"
#include "GameFramework/Actor.h"
#include "Items/BreakerEquipmentComponent.h"
#include "Items/BreakerItemTypes.h"
#include "Weapons/BreakerWeaponComponent.h"
#include "Weapons/BreakerWeaponDefinition.h"
#include "Weapons/BreakerWeaponMath.h"

// ---------------------------------------------------------------------------
// O35: ABILITIES RIDE GEAR DEPTH
// ---------------------------------------------------------------------------
// Weapon base damage climbs (1 + w)^(ilvl - 1); before O35 every class
// ability's damage was flat, so the whole ABILITIES axis decayed ~74x against
// the curve by item level 50. The ruling: all class-ability damage multiplies
// by the EQUIPPED WEAPON's item-level scalar, anchored to exactly 1.0 at item
// level 1 so no authored number moves at the anchor.
//
// These tests pin both halves through the one seam every application site
// reads (UBreakerGameplayAbility::AbilityDamageScalarFor) and through Cleave's
// full swing composition, which is the one damage composition exposed
// actor-parameterised. The activation paths themselves need a world and GAS
// and are exercised by compilation plus these seams, not end to end — the
// same standing limitation every Caster activation already has.
// ---------------------------------------------------------------------------

namespace BreakerAbilityScalingTest
{
    // Distinctively named for the unity build.

    FBreakerItemInstance ScalingTestWeaponItem(EBreakerEquipSlot Slot, int32 ItemLevel)
    {
        FBreakerItemInstance Item;
        Item.ItemId = FGuid::NewGuid();
        Item.Slot = Slot;
        Item.Rarity = EBreakerItemRarity::Exceptional;
        Item.ItemLevel = ItemLevel;
        return Item;
    }
}

// The anchor: at item level 1 (or with no weapon at all) the scalar is EXACTLY
// 1.0, so every currently authored ability number is bit-identical.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerAbilityScalingAnchorTest,
    "RiorsEdge.Combat.AbilityScaling.AnchorAtItemLevelOne",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerAbilityScalingAnchorTest::RunTest(const FString& Parameters)
{
    // No actor, no weapon component: the scalar is the exact identity —
    // ItemLevelDamageScalar returns the literal 1.0f at or below level 1, so
    // the anchor is bit-exact by construction, not merely within tolerance.
    TestEqual(TEXT("No owner at all is the identity scalar"),
        UBreakerGameplayAbility::AbilityDamageScalarFor(nullptr), 1.0f);

    AActor* Bare = NewObject<AActor>(GetTransientPackage());
    TestEqual(TEXT("An owner with no weapon component is the identity scalar"),
        UBreakerGameplayAbility::AbilityDamageScalarFor(Bare), 1.0f);

    // A weapon component with nothing equipped is item level 1: still exact.
    AActor* Owner = NewObject<AActor>(GetTransientPackage());
    UBreakerWeaponComponent* Weapon = NewObject<UBreakerWeaponComponent>(Owner);
    TestEqual(TEXT("An unequipped weapon is the identity scalar"),
        UBreakerGameplayAbility::AbilityDamageScalarFor(Owner), 1.0f);

    // Cleave's full composition at the anchor equals the pre-O35 formula to
    // the bit: scaled weapon base == authored archetype damage at ilvl 1, and
    // the unarmed fallback is unscaled.
    const UBreakerAbility_Cleave* Cleave = GetDefault<UBreakerAbility_Cleave>();
    const UBreakerWeaponDefinition* Definition = Weapon->GetActiveDefinition();
    if (!Definition)
    {
        AddError(TEXT("The weapon component resolves no active definition"));
        return false;
    }
    TestEqual(TEXT("Cleave at item level 1 is bit-identical to the pre-O35 swing"),
        Cleave->ComputeSwingBaseDamage(Owner),
        UBreakerAbility_Cleave::SwingDamage(Definition->Damage, Cleave->WeaponDamageCoefficient, Cleave->UnarmedDamage));
    TestEqual(TEXT("An unarmed Caster at the anchor swings for exactly the authored fallback"),
        GetDefault<UBreakerAbility_Cleave>()->ComputeSwingBaseDamage(Bare),
        UBreakerAbility_Cleave::SwingDamage(0.0f, Cleave->WeaponDamageCoefficient, Cleave->UnarmedDamage));

    // Every flat ability number times the anchor scalar is itself. Stated
    // through the CDOs so a changed default cannot silently invalidate this.
    const float Anchor = UBreakerGameplayAbility::AbilityDamageScalarFor(Owner);
    TestEqual(TEXT("Fracture's impact damage is unmoved at the anchor"),
        GetDefault<UBreakerAbility_Fracture>()->ImpactDamage * Anchor,
        GetDefault<UBreakerAbility_Fracture>()->ImpactDamage);
    TestEqual(TEXT("Rot's poison tick is unmoved at the anchor"),
        GetDefault<UBreakerAbility_Rot>()->PoisonDamagePerTick * Anchor,
        GetDefault<UBreakerAbility_Rot>()->PoisonDamagePerTick);
    TestEqual(TEXT("Siphon's tick is unmoved at the anchor"),
        GetDefault<UBreakerAbility_Siphon>()->DamagePerTick * Anchor,
        GetDefault<UBreakerAbility_Siphon>()->DamagePerTick);
    return true;
}

// The depth: with an item level 50 weapon equipped, ability damage rides
// exactly the weapon's own scalar — the two curves cannot separate again.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerAbilityScalingRidesGearDepthTest,
    "RiorsEdge.Combat.AbilityScaling.RidesGearDepth",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerAbilityScalingRidesGearDepthTest::RunTest(const FString& Parameters)
{
    using namespace BreakerAbilityScalingTest;

    AActor* Owner = NewObject<AActor>(GetTransientPackage());
    UBreakerWeaponComponent* Weapon = NewObject<UBreakerWeaponComponent>(Owner);
    UBreakerEquipmentComponent* Equipment = NewObject<UBreakerEquipmentComponent>(Owner);
    if (!TestTrue(TEXT("An item level 50 Primary equips"),
        Equipment->EquipItem(ScalingTestWeaponItem(EBreakerEquipSlot::Primary, 50))))
    {
        return false;
    }

    // The seam every ability reads returns the weapon's own scalar: the two
    // halves of O35 are the same number by construction.
    const float Expected = FBreakerWeaponMath::ItemLevelDamageScalar(50, Weapon->ItemLevelDamageGrowth);
    TestEqual(TEXT("At item level 50 the ability scalar IS the weapon scalar"),
        UBreakerGameplayAbility::AbilityDamageScalarFor(Owner), Expected, 0.0001f);
    TestEqual(TEXT("The seam and the component agree"),
        UBreakerGameplayAbility::AbilityDamageScalarFor(Owner), Weapon->GetItemLevelDamageScalar(), 0.0001f);
    TestTrue(TEXT("Item level 50 is a large, felt ability increase"), Expected > 10.0f);

    // Cleave's weapon-coefficient path rides the SCALED base — the number the
    // weapon's own rounds use — not the raw archetype constant.
    const UBreakerAbility_Cleave* Cleave = GetDefault<UBreakerAbility_Cleave>();
    TestEqual(TEXT("Cleave at item level 50 swings off the scaled weapon base"),
        Cleave->ComputeSwingBaseDamage(Owner),
        UBreakerAbility_Cleave::SwingDamage(Weapon->GetScaledBaseDamage(), Cleave->WeaponDamageCoefficient,
            Cleave->UnarmedDamage * Expected),
        0.01f);
    TestEqual(TEXT("Cleave's growth against the anchor is exactly the weapon scalar"),
        Cleave->ComputeSwingBaseDamage(Owner),
        UBreakerAbility_Cleave::SwingDamage(Weapon->GetActiveDefinition()->Damage, Cleave->WeaponDamageCoefficient, Cleave->UnarmedDamage) * Expected,
        0.01f);

    // Flat ability numbers times the seam match the weapon scalar too. This is
    // the arithmetic every activation performs at its damage line.
    TestEqual(TEXT("A flat ability number at item level 50 matches the weapon scalar"),
        GetDefault<UBreakerAbility_Fracture>()->ImpactDamage * UBreakerGameplayAbility::AbilityDamageScalarFor(Owner),
        GetDefault<UBreakerAbility_Fracture>()->ImpactDamage * Expected,
        0.001f);

    // Unequipping returns every ability to the anchor — no stale scalar.
    TestTrue(TEXT("The Primary unequips"), Equipment->UnequipSlot(EBreakerEquipSlot::Primary));
    TestEqual(TEXT("An emptied slot returns abilities to the anchor"),
        UBreakerGameplayAbility::AbilityDamageScalarFor(Owner), 1.0f);
    return true;
}

// ---------------------------------------------------------------------------
// AUDIT F1: MELEE COEFFICIENTS KEY OFF THE FULL BLAST, NOT THE PELLET
// ---------------------------------------------------------------------------
// The per-pellet base read a shotgun at 10 (x8 pellets) against a sniper's 72,
// so "1.5x weapon damage" made the shotgun melee the weakest swing in the game
// — a 7.2x thematic inversion. GetScaledFullBlastDamage is per-pellet base
// times pellet count, so the shotgun (80 at ilvl 1) is the heavy swing and
// every single-projectile weapon is bit-identical to before.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerAbilityScalingFullBlastSwingTest,
    "RiorsEdge.Combat.AbilityScaling.MeleeSwingsTheFullBlast",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerAbilityScalingFullBlastSwingTest::RunTest(const FString& Parameters)
{
    AActor* Owner = NewObject<AActor>(GetTransientPackage());
    UBreakerWeaponComponent* Weapon = NewObject<UBreakerWeaponComponent>(Owner);
    const UBreakerAbility_Cleave* Cleave = GetDefault<UBreakerAbility_Cleave>();

    // All three at the same (unequipped => item level 1) anchor, so the ONLY
    // variable between them is the archetype's per-pull payload.
    auto SwingFor = [&](EBreakerWeaponArchetype Archetype) -> float
    {
        Weapon->EquipArchetype(Archetype);
        return Cleave->ComputeSwingBaseDamage(Owner);
    };
    const float SmgSwing = SwingFor(EBreakerWeaponArchetype::SMG);
    const float RifleSwing = SwingFor(EBreakerWeaponArchetype::Rifle);
    const float ShotgunSwing = SwingFor(EBreakerWeaponArchetype::Shotgun);

    // The accessor is exactly per-pellet base times pellet count. With the
    // shotgun equipped at the anchor that is the authored 10 x 8 = 80, the
    // heaviest hitscan pull in the archetype table — the theme, restored.
    const UBreakerWeaponDefinition* ShotgunDefinition = Weapon->GetActiveDefinition();
    if (!ShotgunDefinition)
    {
        AddError(TEXT("The weapon component resolves no active shotgun definition"));
        return false;
    }
    TestEqual(TEXT("Full blast is per-pellet base times pellet count"),
        Weapon->GetScaledFullBlastDamage(),
        Weapon->GetScaledBaseDamage() * ShotgunDefinition->PelletsPerShot);
    TestEqual(TEXT("The shotgun swing reads the whole 80-damage blast"),
        ShotgunSwing,
        UBreakerAbility_Cleave::SwingDamage(
            ShotgunDefinition->Damage * ShotgunDefinition->PelletsPerShot,
            Cleave->WeaponDamageCoefficient, Cleave->UnarmedDamage));

    // The ordering the archetypes' per-pull payloads author: 80 > 24 > 13.
    TestTrue(*FString::Printf(TEXT("Shotgun swing (%.1f) beats rifle swing (%.1f)"), ShotgunSwing, RifleSwing),
        ShotgunSwing > RifleSwing);
    TestTrue(*FString::Printf(TEXT("Rifle swing (%.1f) beats SMG swing (%.1f)"), RifleSwing, SmgSwing),
        RifleSwing > SmgSwing);

    // Bare hands never see the accessor: no weapon component means the
    // unarmed fallback, unchanged to the bit.
    AActor* Bare = NewObject<AActor>(GetTransientPackage());
    TestEqual(TEXT("A bare-hands cast is the unarmed fallback, untouched"),
        Cleave->ComputeSwingBaseDamage(Bare),
        UBreakerAbility_Cleave::SwingDamage(0.0f, Cleave->WeaponDamageCoefficient, Cleave->UnarmedDamage));
    return true;
}

#endif
