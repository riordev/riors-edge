#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "BreakerEnemyFamily.generated.h"

// THE TWO FAMILIES — Story-Source.md §1.5, which is authoritative for enemy
// ARCHITECTURE (the O-ledger still rules systems).
//
// Before this, "enemy" was one undifferentiated taxonomy and the only axes
// were archetype, rank and area level. §1.5 says there are two families and
// that they are not a reskin: one has no design intent at all and the other is
// a person who has degraded, and those are different fights.
//
//   VESTIGE — rift-native, genuinely alien. "No design intent, no readable
//   anatomy, no tactics that resemble a military." Behaviourally that means no
//   cover discipline, no flanking, no recognisable weapon handling. This is the
//   family that can justify movement and attack patterns a player has no prior
//   intuition for. Field slang for them, and for the event of a rift producing
//   them, is SPILL.
//
//   ALTERED — refugees from timelines Rior already erased. NOT inherently
//   hostile; what makes them hostile is SEVERANCE, the degradation of being cut
//   off from a timeline that no longer exists.
//
// WHY THIS IS A GAMEPLAY FIELD AND NOT A LORE TAG. §1.5's "STAGE AS ART
// DIRECTION" says severance stage must be readable from the model AND the
// behaviour: early stage still wears insignia, still uses cover, still
// flinches; late stage does none of these. That is a behavioural spectrum, and
// it is the cheapest genuine variety available to this project — the same
// silhouette family produces two or three different fights by moving along it.
//
// It also scopes the MODIFIER set. A modifier that reads as tactical discipline
// makes sense on an Altered and actively contradicts the fiction on a Vestige,
// so modifier selection filters on family (see
// UBreakerEnemyModifierLibrary::IsModifierAllowedOnFamily).
UENUM(BlueprintType)
enum class EBreakerEnemyFamily : uint8
{
    // Rift-native. The default, because everything that shipped before the
    // families existed is a Vestige by fiction and by behaviour.
    Vestige,
    // A severed refugee.
    Altered
};

// How far gone an Altered is. Meaningless on a Vestige, which is why the enum
// carries an explicit NotApplicable rather than defaulting to a stage.
//
// The BEHAVIOURAL contract of each stage, which is what makes this more than a
// label:
//   Early — cover discipline, burst fire, FLINCHES when hit, still wears
//           insignia. Fights like a trained soldier. The player's answer is to
//           flush it out, not to out-shoot it.
//   Mid   — still carries and uses its equipment, but the tactics are gone: it
//           holds ground and advances, and it does not take cover or flinch.
//   Late  — "nothing is left but the shape". No equipment discipline, no cover,
//           no flinch.
UENUM(BlueprintType)
enum class EBreakerSeveranceStage : uint8
{
    NotApplicable,
    Early,
    Mid,
    Late
};

UCLASS()
class RIORSEDGE_API UBreakerEnemyFamilyLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    // The formal military term is VESTIGE (§1.2). SPILL is the field slang and
    // is deliberately NOT what appears over an enemy's head — the HUD is
    // instrumentation, and instrumentation uses the formal term.
    UFUNCTION(BlueprintPure, Category="Enemy|Family")
    static FString GetFamilyName(EBreakerEnemyFamily Family);

    UFUNCTION(BlueprintPure, Category="Enemy|Family")
    static FString GetSeveranceStageName(EBreakerSeveranceStage Stage);

    // The one-line readout: "VESTIGE" or "ALTERED - EARLY SEVERANCE". Empty
    // stage text for a Vestige, because a Vestige has no stage.
    UFUNCTION(BlueprintPure, Category="Enemy|Family")
    static FString GetFamilyBanner(EBreakerEnemyFamily Family, EBreakerSeveranceStage Stage);

    // Behavioural contracts, so an archetype asks the STAGE rather than
    // hardcoding "this one flinches". Both are pure and world-free.
    UFUNCTION(BlueprintPure, Category="Enemy|Family")
    static bool StageUsesCover(EBreakerEnemyFamily Family, EBreakerSeveranceStage Stage);

    UFUNCTION(BlueprintPure, Category="Enemy|Family")
    static bool StageFlinches(EBreakerEnemyFamily Family, EBreakerSeveranceStage Stage);
};
