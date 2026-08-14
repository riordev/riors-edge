#pragma once

#include "CoreMinimal.h"
#include "BreakerWeaponArchetype.generated.h"

// The weapon archetype enum, split out of BreakerWeaponComponent.h so that
// Items/ can name a weapon class without pulling in the whole weapon
// component (which drags the combat types and the feel layer with it).
//
// This split is what lets a dropped ITEM know which gun it is. Before it, an
// item instance carried an item level and nothing else, so which of the
// archetypes a Primary drop actually WAS had no answer — the loadout screen
// picked the gun and the item supplied only numbers. Power-Curve.md flagged
// exactly that as the open boundary.

UENUM(BlueprintType)
enum class EBreakerWeaponArchetype : uint8
{
    Rifle,
    SMG,
    Sniper,
    Shotgun,
    Rocket,
    // O27 breadth pass. APPENDED, never inserted: the archetype is stored as a
    // uint8 in UBreakerSaveGame and replicated as one, so renumbering the
    // existing five would silently rearm every saved loadout with a different
    // gun. New archetypes go on the end, forever.
    BurstRifle,
    Machinegun,
    Sidearm,
    Count UMETA(Hidden)
};

// Named separately from the weapon definition's DisplayName because a dropped
// item needs the CLASS name before any definition is resolved, and because the
// owner's instruction is explicit: these are called what they are. No
// codenames — a burst rifle is a Burst Rifle.
namespace BreakerWeaponArchetypeNames
{
    inline FString Display(EBreakerWeaponArchetype Archetype)
    {
        switch (Archetype)
        {
            case EBreakerWeaponArchetype::Rifle:      return TEXT("Rifle");
            case EBreakerWeaponArchetype::SMG:        return TEXT("SMG");
            case EBreakerWeaponArchetype::Sniper:     return TEXT("Sniper");
            case EBreakerWeaponArchetype::Shotgun:    return TEXT("Shotgun");
            case EBreakerWeaponArchetype::Rocket:     return TEXT("Rocket Launcher");
            case EBreakerWeaponArchetype::BurstRifle: return TEXT("Burst Rifle");
            case EBreakerWeaponArchetype::Machinegun: return TEXT("Machinegun");
            case EBreakerWeaponArchetype::Sidearm:    return TEXT("Sidearm");
            default:                                  return TEXT("Rifle");
        }
    }

    // Uppercase, for the HUD and the menu chrome, which are set in caps.
    inline FString Short(EBreakerWeaponArchetype Archetype)
    {
        return Display(Archetype).ToUpper();
    }
}
