#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Items/BreakerAffixLibrary.h"
#include "Items/BreakerLootLibrary.h"
#include "Weapons/BreakerWeaponArchetype.h"

// ---------------------------------------------------------------------------
// WEAPON DROPS ARE WEAPONS
// ---------------------------------------------------------------------------
// Owner: "weapons should also be randomized on drop ... when primaries and
// secondaries drop they should be different weapon classes with their
// respective affixes ... make sure certain guns have certain leans towards
// affixes ... NOT REQUIRED STATS but they can drop more likely with those."
//
// Two properties matter and they pull against each other, which is why both
// are pinned here rather than only the easy one:
//
//   1. A lean must be VISIBLE. If the SMG's fire-rate bias cannot be measured
//      across a realistic number of drops, the feature is a comment.
//   2. A lean must NOT be a FILTER. Every affix legal on the slot must remain
//      reachable on every archetype. The item you were not supposed to get is
//      the one that makes a looter interesting, and a hard restriction is also
//      how a stat quietly becomes unfindable.
// ---------------------------------------------------------------------------

namespace BreakerWeaponDropTest
{
    // Distinctively prefixed: unity builds concatenate translation units and
    // this project has shipped anonymous-namespace collisions twice.
    int32 BreakerDropCountAffix(const FBreakerItemInstance& Item, const TCHAR* AffixId)
    {
        int32 Count = 0;
        for (const FBreakerRolledAffix& Rolled : Item.Affixes)
        {
            if (Rolled.AffixId == FName(AffixId)) ++Count;
        }
        return Count;
    }

    // Roll a run of weapons of ONE archetype by forcing it after the roll's own
    // archetype draw. Done by re-rolling with seeds until the wanted archetype
    // comes up, so the affix selection still sees the real lean path rather
    // than a hand-built item.
    int32 BreakerDropCountAcrossRun(EBreakerWeaponArchetype Archetype, const TCHAR* AffixId, int32 Samples)
    {
        int32 Total = 0;
        int32 Accepted = 0;
        for (int32 Seed = 1; Accepted < Samples && Seed < Samples * 200; ++Seed)
        {
            const FBreakerItemInstance Item = UBreakerLootLibrary::RollItem(
                TEXT("Drop"), EBreakerEquipSlot::Primary, EBreakerItemRarity::Anomalous, 50, Seed);
            if (Item.WeaponArchetype != Archetype) continue;
            ++Accepted;
            Total += BreakerDropCountAffix(Item, AffixId);
        }
        return Total;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerWeaponDropArchetypeTest,
    "RiorsEdge.Items.WeaponDrops.Archetype",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerWeaponDropArchetypeTest::RunTest(const FString& Parameters)
{
    // Armour never carries a meaningful archetype, and must be untouched by
    // the whole feature.
    const FBreakerItemInstance Helmet = UBreakerLootLibrary::RollItem(
        TEXT("Drop"), EBreakerEquipSlot::Helmet, EBreakerItemRarity::Exceptional, 30, 12345);
    TestFalse(TEXT("A helmet is not a weapon"), Helmet.IsWeapon());

    // The same seed must still reproduce the same item exactly: the archetype
    // draw was added to the existing deterministic stream, not beside it.
    const FBreakerItemInstance A = UBreakerLootLibrary::RollItem(
        TEXT("Drop"), EBreakerEquipSlot::Primary, EBreakerItemRarity::Aberrant, 40, 777);
    const FBreakerItemInstance B = UBreakerLootLibrary::RollItem(
        TEXT("Drop"), EBreakerEquipSlot::Primary, EBreakerItemRarity::Aberrant, 40, 777);
    TestEqual(TEXT("A seed reproduces the archetype"), static_cast<int32>(A.WeaponArchetype), static_cast<int32>(B.WeaponArchetype));
    TestEqual(TEXT("A seed reproduces the affix count"), A.Affixes.Num(), B.Affixes.Num());

    // Every archetype appears — WITH THE SLOT DRAWN THE WAY PRODUCTION DRAWS
    // IT. The previous version of this test picked the slot as (Seed % 2),
    // which is exactly why it stayed green for the entire life of a bug that
    // made six archetypes undroppable: GrantLoot drew the slot from the same
    // unsalted seed as RollItem's first draw, so archetype always equalled
    // slot index in the shipped game while the test rolled its own slots and
    // saw all eight. Shipped-configuration testing (O40c) means the slot
    // comes from UBreakerLootLibrary::RollDropSlot here or the assertion is
    // about a game that does not exist.
    bool bSeen[static_cast<int32>(EBreakerWeaponArchetype::Count)] = {};
    for (int32 Seed = 1; Seed <= 600; ++Seed)
    {
        const EBreakerEquipSlot Slot = UBreakerLootLibrary::RollDropSlot(Seed);
        if (!FBreakerItemInstance::IsWeaponSlot(Slot)) continue;
        const FBreakerItemInstance Item = UBreakerLootLibrary::RollItem(TEXT("Drop"), Slot, EBreakerItemRarity::Standard, 20, Seed);
        TestTrue(TEXT("A weapon-slot drop is a weapon"), Item.IsWeapon());
        bSeen[static_cast<int32>(Item.WeaponArchetype)] = true;
    }
    for (int32 Index = 0; Index < static_cast<int32>(EBreakerWeaponArchetype::Count); ++Index)
    {
        TestTrue(*FString::Printf(TEXT("%s drops at all"),
            *BreakerWeaponArchetypeNames::Display(static_cast<EBreakerWeaponArchetype>(Index))), bSeen[Index]);
    }

    // Every archetype has a real name. The owner asked for these specifically:
    // "just name the weapons their actual names so burst rifle machinegun etc".
    TestEqual(TEXT("Burst Rifle is named"), BreakerWeaponArchetypeNames::Display(EBreakerWeaponArchetype::BurstRifle), FString(TEXT("Burst Rifle")));
    TestEqual(TEXT("Machinegun is named"), BreakerWeaponArchetypeNames::Display(EBreakerWeaponArchetype::Machinegun), FString(TEXT("Machinegun")));
    TestEqual(TEXT("Sidearm is named"), BreakerWeaponArchetypeNames::Display(EBreakerWeaponArchetype::Sidearm), FString(TEXT("Sidearm")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerArchetypeAffixLeanTest,
    "RiorsEdge.Items.WeaponDrops.Leans",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerArchetypeAffixLeanTest::RunTest(const FString& Parameters)
{
    using namespace BreakerWeaponDropTest;

    // ---- A lean is never a filter ------------------------------------------
    // Multipliers may only ever raise a weight. A value below 1.0 would mean
    // some stat is harder to find on some gun, which is a different feature
    // with a much worse failure mode, and nobody asked for it.
    const TArray<FBreakerAffixDefinition>& Pool = UBreakerAffixLibrary::GetSliceAffixPool();
    for (int32 Index = 0; Index < static_cast<int32>(EBreakerWeaponArchetype::Count); ++Index)
    {
        const EBreakerWeaponArchetype Archetype = static_cast<EBreakerWeaponArchetype>(Index);
        for (const FBreakerAffixDefinition& Affix : Pool)
        {
            TestTrue(*FString::Printf(TEXT("%s never suppresses %s"),
                *BreakerWeaponArchetypeNames::Display(Archetype), *Affix.AffixId.ToString()),
                UBreakerAffixLibrary::ArchetypeAffixWeightMultiplier(Archetype, Affix.AffixId) >= 1.0f);
        }
    }

    // An unknown affix has no opinion, so a future line is neutral until
    // somebody deliberately leans it.
    TestEqual(TEXT("An unleaned line is exactly neutral"),
        UBreakerAffixLibrary::ArchetypeAffixWeightMultiplier(EBreakerWeaponArchetype::SMG, TEXT("Nope.NotReal")), 1.0f, 0.0f);

    // ---- The three leans the owner named by name ---------------------------
    TestTrue(TEXT("SMG leans fire rate"),
        UBreakerAffixLibrary::ArchetypeAffixWeightMultiplier(EBreakerWeaponArchetype::SMG, TEXT("Weapon.FireRate")) > 1.0f);
    TestTrue(TEXT("Machinegun leans damage"),
        UBreakerAffixLibrary::ArchetypeAffixWeightMultiplier(EBreakerWeaponArchetype::Machinegun, TEXT("Offense.WeaponDamage")) > 1.0f);
    TestTrue(TEXT("Sidearm leans slide speed"),
        UBreakerAffixLibrary::ArchetypeAffixWeightMultiplier(EBreakerWeaponArchetype::Sidearm, TEXT("Move.SlideSpeed")) > 1.0f);

    // ---- The lean is VISIBLE in actual drops -------------------------------
    // The point of the feature is that a run of SMGs looks different from a run
    // of machineguns. Measured across real rolls rather than asserted off the
    // table, because a weight that never reaches the roll is the failure this
    // is guarding against.
    const int32 Samples = 120;
    const int32 SMGFireRate = BreakerDropCountAcrossRun(EBreakerWeaponArchetype::SMG, TEXT("Weapon.FireRate"), Samples);
    const int32 SniperFireRate = BreakerDropCountAcrossRun(EBreakerWeaponArchetype::Sniper, TEXT("Weapon.FireRate"), Samples);
    AddInfo(FString::Printf(TEXT("Fire Rate lines across %d drops: SMG %d, Sniper %d"), Samples, SMGFireRate, SniperFireRate));
    TestTrue(TEXT("SMGs roll fire rate more often than snipers do"), SMGFireRate > SniperFireRate);

    const int32 SidearmSlide = BreakerDropCountAcrossRun(EBreakerWeaponArchetype::Sidearm, TEXT("Move.SlideSpeed"), Samples);
    const int32 RifleSlide = BreakerDropCountAcrossRun(EBreakerWeaponArchetype::Rifle, TEXT("Move.SlideSpeed"), Samples);
    AddInfo(FString::Printf(TEXT("Slide Speed lines across %d drops: Sidearm %d, Rifle %d"), Samples, SidearmSlide, RifleSlide));
    TestTrue(TEXT("Sidearms roll slide speed more often than rifles do"), SidearmSlide > RifleSlide);

    // ---- ...and the off-lean roll still happens ----------------------------
    // The interesting drop. A sniper with fire rate must be possible, or the
    // lean has quietly become the restriction the owner explicitly ruled out.
    TestTrue(TEXT("An off-lean line still rolls: snipers can find fire rate"), SniperFireRate > 0);
    return true;
}

#endif
