#pragma once

#include "CoreMinimal.h"

// The ability numerics, as data (O186). Every ResourceCost, CooldownSeconds
// and WindowDuration on a fallback row, and every keystone variant's four
// numbers, live in Data/abilities.json; the registry in
// BreakerAbilityDefinition.cpp keeps the identity of each row (id, class,
// tags, text, verb, ability class) and is overlaid from the file once, before
// it is first returned. A magnitude change is a data change with no C++ diff.
//
// A file that fails validation applies NOTHING: every row keeps its
// default-constructed numerics (zero cost, zero cooldown, zero window)
// behind an ensure, never a nearest fit. RiorsEdge.Data.Abilities.Fresh
// reads the errors first so a broken file names its breaks.
//
// Kept apart from the definition header, which is included widely, so a
// change here does not recompile every ability.
namespace BreakerAbilityData
{
    // Data/abilities.json, repo-relative, the path the registry loads from.
    RIORSEDGE_API FString DataRelativePath();

    // Every complaint the loader raised, or empty. Builds the registry if it
    // has not been built yet, so the answer is always about the live load.
    RIORSEDGE_API const TArray<FString>& GetDataErrors();
}
