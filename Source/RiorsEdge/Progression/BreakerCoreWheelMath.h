#pragma once

#include "CoreMinimal.h"
#include "Items/BreakerForgeLibrary.h"
#include "Progression/BreakerProgressionNode.h"
#include "Progression/BreakerProgressionTree.h"
#include "Progression/BreakerProgressionTypes.h"

// ---------------------------------------------------------------------------
// The Core wheel's shape and price, pure (O211-O213). LEDGER owns this header;
// the progression component, the census and the wheel's tests read it and
// restate none of it.
//
// O211: one tree for every class, level-driven points, the hub the only
// start. O212: five domain sectors, each holding its constellations as
// wedges; a constellation with a silent node is drawn dark. O213: no bead is
// a travel node, and a Core respec is free until level N and costs Riftglass
// after.
// ---------------------------------------------------------------------------
namespace BreakerCoreWheel
{
    // ---- O212: the five sectors -------------------------------------------
    inline const FName SectorMovement(TEXT("Movement"));
    inline const FName SectorWeapon(TEXT("Weapon"));
    inline const FName SectorDefence(TEXT("Defence"));
    inline const FName SectorAbility(TEXT("Ability"));
    inline const FName SectorElements(TEXT("Elements"));
    constexpr int32 SectorCount = 5;

    // ---- O213: the respec price --------------------------------------------
    // Free below this level; Riftglass at or above it. N is UNRULED — O213
    // names it and gives it no value — so this is a placeholder standing where
    // the ruling will land, held inside [1, CorePointCapLevel] by the test.
    constexpr int32 CoreRespecFreeUntilLevel = 30;   // O2 PLACEHOLDER (O213: N unruled)
    // The one price, once the character is past N. Sized to read as a
    // rift-completion purse rather than a salvage pile; not felt yet.
    constexpr int32 CoreRespecRiftglass = 40;        // O2 PLACEHOLDER
}

// Which sector a constellation is a wedge of. Every assignment is
// O2 PLACEHOLDER: the owner has not felt the wheel's layout. An unknown
// constellation returns NAME_None, which the tests treat as a failure and no
// consumer treats as a sector — a wedge nobody can place must not be quietly
// drawn somewhere.
inline FName BreakerCoreSectorOf(FName Constellation)
{
    static const TMap<FName, FName> Table = {
        { FName(TEXT("Kinesis")),    BreakerCoreWheel::SectorMovement },  // O2 PLACEHOLDER
        { FName(TEXT("Velocity")),   BreakerCoreWheel::SectorMovement },  // O2 PLACEHOLDER
        { FName(TEXT("Precision")),  BreakerCoreWheel::SectorWeapon },    // O2 PLACEHOLDER
        { FName(TEXT("Volley")),     BreakerCoreWheel::SectorWeapon },    // O2 PLACEHOLDER
        { FName(TEXT("Vector")),     BreakerCoreWheel::SectorWeapon },    // O2 PLACEHOLDER
        { FName(TEXT("Ruin")),       BreakerCoreWheel::SectorWeapon },    // O2 PLACEHOLDER
        { FName(TEXT("Bulwark")),    BreakerCoreWheel::SectorDefence },   // O2 PLACEHOLDER
        { FName(TEXT("Aegis")),      BreakerCoreWheel::SectorDefence },   // O2 PLACEHOLDER
        { FName(TEXT("Arc")),        BreakerCoreWheel::SectorAbility },   // O2 PLACEHOLDER
        { FName(TEXT("Reservoir")),  BreakerCoreWheel::SectorAbility },   // O2 PLACEHOLDER
        { FName(TEXT("Elements")),   BreakerCoreWheel::SectorElements },  // O2 PLACEHOLDER
        { FName(TEXT("Affliction")), BreakerCoreWheel::SectorElements },  // O2 PLACEHOLDER
    };
    const FName* Found = Table.Find(Constellation);
    return Found ? *Found : FName(NAME_None);
}

// A node is SILENT when it authors an effect on a stat target the aggregator
// has no lane for, or when it grants no effect, no tag and no ability.
inline bool BreakerCoreNodeIsSilent(const UBreakerProgressionNode& Node)
{
    const bool bGrantsSomething = Node.Effects.Num() > 0 || Node.GrantedTags.Num() > 0 || Node.GrantedAbilityIds.Num() > 0;
    if (!bGrantsSomething) return true;
    for (const FBreakerNodeEffect& Effect : Node.Effects)
    {
        if (!BreakerStatTargetHasAggregationLane(Effect.StatTarget)) return true;
    }
    return false;
}

// O212: the constellations drawn dark — every constellation holding a silent
// node. Derived from the tree, never listed.
//
// GAP, RECORDED AT THE SITE: the census's second silence axis — a granted tag
// that nothing reads — is a source scan (Scripts/status.py's consumer index
// over production code) with no runtime equivalent, so this function cannot
// see it. On the shipped Core this returns the empty set while the census
// reports one silent node. That node's constellation is NOT hand-listed here
// to make the two agree: a hand list is a second source of truth that rots
// the day the reader lands, and it would be faking a derivation the runtime
// cannot make. The day tag consumption is reflected at runtime, it joins
// BreakerCoreNodeIsSilent and the test moves.
inline TSet<FName> BreakerCoreDarkConstellations(const UBreakerProgressionTree* Tree)
{
    TSet<FName> Dark;
    if (!Tree) return Dark;
    for (const UBreakerProgressionNode* Node : Tree->Nodes)
    {
        if (Node && !Node->Constellation.IsNone() && BreakerCoreNodeIsSilent(*Node))
        {
            Dark.Add(Node->Constellation);
        }
    }
    return Dark;
}

// O213: what a Core respec costs at this level. Free below
// CoreRespecFreeUntilLevel; one Riftglass amount at or above it.
inline FBreakerForgeCost BreakerCoreRespecCost(int32 CharacterLevel)
{
    FBreakerForgeCost Cost;
    if (CharacterLevel >= BreakerCoreWheel::CoreRespecFreeUntilLevel)
    {
        Cost.Amount = BreakerCoreWheel::CoreRespecRiftglass;
    }
    return Cost;
}
