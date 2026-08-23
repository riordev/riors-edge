#pragma once

#if WITH_DEV_AUTOMATION_TESTS

#include "CoreMinimal.h"
#include "Items/BreakerItemTypes.h"

// ---------------------------------------------------------------------------
// THE ONE AUTHORED BASELINE CHARACTER
// ---------------------------------------------------------------------------
// A full set of gear, every point spent, no direction. Mid-band rolls, Weapon
// Damage wherever it happened to land, a little crit, one conditional line it
// did not build around, and no Convergence node at all — so no More multiplier.
// O27's "hitting 50 must be satisfying with decent power" is what this is.
//
// IT LIVES HERE BECAUSE TWO FILES NEED THE SAME CHARACTER AND HAD TWO.
// BreakerPowerBandTests measured the build-variance band against this loadout,
// which carries FOUR Health lines. BreakerPromotedFindingTests measured
// time-to-die against a character carrying EIGHT — Core.Health on every slot
// the affix allows. Both called their character "the baseline", and the gap
// between them is not cosmetic: TTD at the cap reads 4.97s for the eight-line
// character and 2.78s for this one, so the two files disagreed about whether
// the top of the level range met O18 at all, and the whole O116 retune was
// solved against the answer that happened to be in the other file.
//
// Eight Health lines is the theoretical BEST roll. The at-cap band was already
// corrected once, on 2026-08-22, for using the theoretical WORST roll — which
// "made the number uninterpretable", in the words of its own pin. This is that
// same mistake inverted, and one authored baseline in one place is the fix for
// both directions of it.
//
// WHAT "BARE" MEANS, since it is the phrase that invited the confusion. O18's
// time-to-die target says "no resources or sustain". That describes what the
// character does not USE — no healing, no shield, no lifesteal — not what they
// are wearing. A bare measurement is this loadout with sustain off, never a
// naked character and never a maximally defensive one.
//
// The CEILING character — Core.Health on every allowed slot — is still the
// right subject for one question, and BreakerPromotedFindingTests names it
// separately where it asks it. Two questions, two characters, both labelled.
namespace BreakerBaselineLoadout
{
    struct FSlotAffixes
    {
        EBreakerEquipSlot Slot;
        TArray<FName> AffixIds;
    };

    // Named for the unity build, per the twice-shipped rule about helpers that
    // collide across merged translation units.
    inline const TArray<FSlotAffixes>& BreakerBaselineSlots()
    {
        static const TArray<FSlotAffixes> Slots = {
            {EBreakerEquipSlot::Helmet,     {TEXT("Offense.WeaponDamage"), TEXT("Crit.Chance"), TEXT("Crit.Damage"), TEXT("Core.Health")}},
            {EBreakerEquipSlot::BodyArmour, {TEXT("Offense.WeaponDamage"), TEXT("Core.Health"), TEXT("Core.PhysicalDR")}},
            {EBreakerEquipSlot::Gloves,     {TEXT("Offense.WeaponDamage"), TEXT("Crit.Chance"), TEXT("Crit.Damage")}},
            {EBreakerEquipSlot::Boots,      {TEXT("Offense.WeaponDamage"), TEXT("Offense.AirborneDamage"), TEXT("Core.MoveSpeed")}},
            {EBreakerEquipSlot::Necklace,   {TEXT("Offense.WeaponDamage"), TEXT("Crit.Chance"), TEXT("Crit.Damage"), TEXT("Offense.AddedDamage")}},
            {EBreakerEquipSlot::Waist,      {TEXT("Offense.WeaponDamage"), TEXT("Offense.AddedDamage"), TEXT("Core.Health")}},
            {EBreakerEquipSlot::Primary,    {TEXT("Offense.WeaponDamage"), TEXT("Offense.AddedDamage"), TEXT("Core.MaxResource")}},
            {EBreakerEquipSlot::Secondary,  {TEXT("Offense.WeaponDamage"), TEXT("Core.ResourceRegen"), TEXT("Core.Health")}},
        };
        return Slots;
    }

    // How many of one affix this character carries. Counted from the loadout
    // rather than written down beside it, so adding a Health line to a slot
    // moves the defence figures with it instead of leaving them behind.
    inline int32 BreakerBaselineLineCount(FName AffixId)
    {
        int32 Count = 0;
        for (const FSlotAffixes& Slot : BreakerBaselineSlots())
        {
            for (const FName Id : Slot.AffixIds)
            {
                if (Id == AffixId) ++Count;
            }
        }
        return Count;
    }
}

#endif
