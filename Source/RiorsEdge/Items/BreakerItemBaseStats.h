#pragma once

#include "CoreMinimal.h"
#include "Items/BreakerItemTypes.h"

// ---------------------------------------------------------------------------
// GEAR BASE STATS. Until this header an item was exactly its affixes; the
// base pool is the first thing a piece grants BEFORE any affix, and the
// affix roster now balances against it (O106 rewrite).
//
// Pure and derived, never stored: a piece's base is a function of its slot,
// its item level and its archetype, so a retune is an edit here and never a
// save migration. World-free so the suite can walk the whole ladder.
//
// THE LOAD-BEARING ASYMMETRY (design ruling, recorded once here): the POOLS
// blend and the SUSTAIN does not. Life pieces feed the health pool that
// leech and healing refill; Shield pieces feed the shield pool that only
// the out-of-combat recharge refills (Combat/BreakerShieldMath.h). A mixed
// loadout owns more total pool than either purist and cannot fully refill
// any of it — the commitment enforces itself, so there is NO exclusivity
// rule anywhere and none may be added.
//
// Every figure is O2 PLACEHOLDER. The shapes that are deliberate: life is
// the BIGGER pool (shield trades size for recharge); the body carries the
// most; both grow linearly with item level so the base matters at every
// level without competing with the exponential weapon curve.
// ---------------------------------------------------------------------------
namespace BreakerItemBase
{
    // Share of the base a slot carries. Weapons and None carry nothing.
    inline float SlotBaseWeight(EBreakerEquipSlot Slot)
    {
        switch (Slot)
        {
        case EBreakerEquipSlot::BodyArmour: return 1.0f;    // O2 PLACEHOLDER
        case EBreakerEquipSlot::Helmet:     return 0.6f;    // O2 PLACEHOLDER
        case EBreakerEquipSlot::Gloves:     return 0.45f;   // O2 PLACEHOLDER
        case EBreakerEquipSlot::Boots:      return 0.45f;   // O2 PLACEHOLDER
        case EBreakerEquipSlot::Waist:      return 0.5f;    // O2 PLACEHOLDER
        case EBreakerEquipSlot::Necklace:   return 0.35f;   // O2 PLACEHOLDER
        default:                            return 0.0f;
        }
    }

    // The body-piece life base across the ladder: 30 at level 1 rising ~2.2
    // per level (~294 at the level-120 ceiling — comparable to the Health
    // affix's T1 anchor of 400, deliberately below it so a rolled line still
    // beats a base). O2 PLACEHOLDER.
    inline float BaseLifeAt(EBreakerEquipSlot Slot, int32 ItemLevel)
    {
        const float Level = static_cast<float>(FMath::Max(ItemLevel, 1));
        return SlotBaseWeight(Slot) * (30.0f + 2.2f * (Level - 1.0f));
    }

    // Shield is the smaller pool — 60% of the life base at the same slot and
    // level — because it comes back on its own. O2 PLACEHOLDER.
    inline float BaseShieldAt(EBreakerEquipSlot Slot, int32 ItemLevel)
    {
        return BaseLifeAt(Slot, ItemLevel) * 0.6f;
    }

    // What one piece grants, by its archetype. None (weapons, every item
    // saved before archetypes existed) grants nothing.
    inline float BaseLifeOf(const FBreakerItemInstance& Item)
    {
        return Item.ArmourArchetype == EBreakerArmourArchetype::Life
            ? BaseLifeAt(Item.Slot, Item.ItemLevel) : 0.0f;
    }
    inline float BaseShieldOf(const FBreakerItemInstance& Item)
    {
        return Item.ArmourArchetype == EBreakerArmourArchetype::Shield
            ? BaseShieldAt(Item.Slot, Item.ItemLevel) : 0.0f;
    }
}
