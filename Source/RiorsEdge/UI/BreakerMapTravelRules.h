#pragma once

#include "CoreMinimal.h"

// ---------------------------------------------------------------------------
// What the map offers, as a pure rule (O265). World-free so the rule is proved
// on an array rather than on a loaded level.
//
// THE ASYMMETRY IS THE RULE, not an omission: from anywhere that is not the
// Anchor the map offers exactly one destination, the hub, so a player is never
// stranded in an instance and never routes between instances without passing
// through the place that holds their stash and their Forge. From the Anchor it
// offers the ordinary registry, which is what the hub's own travel point has
// always offered.
//
// The combat gate is deliberately BOTH directions. "Only when not in combat"
// is the owner's rule and it exists so travel cannot be used as an escape; a
// gate that let you flee an instance but not enter one would be exactly the
// escape hatch it is meant to close.
// ---------------------------------------------------------------------------
namespace BreakerMapTravel
{
    // The destinations the map lists. RegistryIds is the already-filtered
    // ordinary list (enabled, not door-only, gated entries already dropped) —
    // this rule narrows by LOCATION and nothing else, so it can never widen
    // what the registry refused.
    inline TArray<FName> OfferedDestinations(bool bInHub, const TArray<FName>& RegistryIds, FName HubId)
    {
        if (bInHub)
        {
            TArray<FName> Offered = RegistryIds;
            // The hub never offers itself: the travel point drops its own
            // location for the same reason, and a button that moves you half a
            // metre reads as broken rather than as a no-op.
            Offered.Remove(HubId);
            return Offered;
        }
        // Outside the hub the registry is irrelevant: one way home, whether or
        // not the hub happens to be enabled in the list for this instance.
        return HubId.IsNone() ? TArray<FName>() : TArray<FName>{ HubId };
    }

    inline bool TravelRefusedInCombat(bool bInCombat) { return bInCombat; }
}
