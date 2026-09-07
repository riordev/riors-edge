#pragma once

#include "CoreMinimal.h"

class FNumericProperty;
class UClass;

// The ability numerics, as data (O186). Every ResourceCost, CooldownSeconds
// and WindowDuration on a fallback row, every keystone variant's four
// numbers, and every ability class's own numeric defaults live in
// Data/abilities.json; the registry in BreakerAbilityDefinition.cpp keeps the
// identity of each row (id, class, tags, text, verb, ability class) and is
// overlaid from the file once, before it is first returned. The class
// defaults are written onto each ability's class default object through
// reflection, so an ability body reads its own members unchanged and every
// instance granted afterwards copies the file's values. A magnitude change is
// a data change with no C++ diff.
//
// A file that fails validation applies NOTHING: every row keeps its
// default-constructed numerics (zero cost, zero cooldown, zero window) and
// every ability class keeps its compiled member initialisers, behind an
// ensure, never a nearest fit. RiorsEdge.Data.Abilities.Fresh reads the
// errors first so a broken file names its breaks.
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

    // The numbers an ability class carries: every float and int32 UPROPERTY
    // with CPF_Edit declared on a class strictly below UBreakerGameplayAbility,
    // walked super first and in declaration order within each class. One
    // walk shared by the loader, the census exporter and the tests, so the
    // file's keys, the exporter's key order and the applied set cannot
    // disagree. Runtime state (Stacks, ShotsRemaining) is plain C++ and so
    // never appears; nothing is filtered by name. Empty for a null class or a
    // class outside the ability hierarchy.
    RIORSEDGE_API TArray<FNumericProperty*> NumberProperties(const UClass* AbilityClass);
}
