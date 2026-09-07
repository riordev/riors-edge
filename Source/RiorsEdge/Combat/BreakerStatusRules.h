#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Progression/BreakerProgressionTypes.h"

// The status vocabulary as DATA (O186): which statuses behave how when a
// combat verb other than "apply" meets them. The first rule is spread on
// pierce (KIT-3): a pierced target's spreading statuses ride the shot onto
// every later target the same pellet continues through, at a normalized
// payload and depth capped at 2 (combat.md, the proc coefficient law).
//
// Data/statuses.json is the library. A row per status tag; a file-level
// pierceSpreadPayloadFraction. A file that fails validation loads as EMPTY
// rules behind an ensure — no status spreads — never as a nearest fit.
struct RIORSEDGE_API FBreakerStatusRule
{
    FGameplayTag Tag;
    bool bSpreadsOnPierce = false;
    bool bDealsPeriodicDamage = true;
    bool bDealsDamageOnExpiry = false;
    bool bDealsDamageOnApplication = false;
    float DurationSeconds = 0.0f;
    float ArmorReductionPercent = 0.0f;
    float HealingReductionPercent = 0.0f;
    bool IsNonDamagingDebuff() const { return !bDealsPeriodicDamage && !bDealsDamageOnExpiry && !bDealsDamageOnApplication; }
};

namespace BreakerStatusRules
{
    RIORSEDGE_API FString DataRelativePath();

    // Every row, in file order. Empty when the file failed to load.
    RIORSEDGE_API const TArray<FBreakerStatusRule>& GetRules();

    // Every complaint the validator raised, or empty.
    RIORSEDGE_API const TArray<FString>& GetDataErrors();

    // The row for a tag, or null: a status with no row spreads nowhere.
    RIORSEDGE_API const FBreakerStatusRule* FindRule(FGameplayTag Tag);

    // The fraction of the source's per-tick damage a pierce-spread copy
    // carries. Zero when the file failed to load. O2 PLACEHOLDER (0.5,
    // seeded from the ricochet line of the proc coefficient law).
    RIORSEDGE_API float PierceSpreadPayloadFraction();
}
