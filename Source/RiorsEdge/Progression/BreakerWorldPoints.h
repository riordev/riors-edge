#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "BreakerWorldPoints.generated.h"

// ---------------------------------------------------------------------------
// THE FIFTEEN WORLD CORE POINTS — CANON, O7
// ---------------------------------------------------------------------------
// One Core Point per level to 50, plus roughly fifteen from world content, for
// the ~65 that "two constellations fully developed plus a third partially" was
// validated against. The per-level half has existed since the progression
// component did. THE WORLD HALF HAD NO REPRESENTATION IN CODE AT ALL: the only
// callers that ever moved UnspentCorePoints were the level entitlement and
// GrantPlaytestPoints, a playtest hook. Fifteen of a character's sixty-five
// points were canon in a ruling and unreachable in the game.
//
// That is the whole reason this file exists, and it is worse than a missing
// feature: eight of the twenty-eight authored campaign missions pay a Core
// Point as their entire reward, so a third of the mission list had nothing to
// pay with. A spec can assert a payout; only a registry can be counted.
//
// NOTHING HERE READS A PLAYER OR A WORLD. It is the list and the rules over the
// list, so Progression.WorldPoints.SoloReachable can assert against the shipped
// configuration with no actor and no subsystem — the same precedent as the
// monster chassis and the weapon maths.

// How a source is earned. This is the axis the solo rule is stated over, so it
// is a field rather than a comment: an entry that needs other players is a
// design error the moment it is authored, and O82 makes solo the balance
// target.
UENUM(BlueprintType)
enum class EBreakerWorldPointDelivery : uint8
{
    // A step on the critical path. Cannot be missed by playing the campaign.
    MainPath,
    // Reconstructing a Rior fragment. One-time, and the capability is the
    // point; the Core Point rides along.
    Fragment,
    // First-clear of a rift archetype, grouped. NOT one grant per archetype —
    // see the note on the two grouped entries below, which is a live owner
    // question rather than a settled shape.
    Archetype,
    // Completing an Act III erased Earth.
    Zone
};

// APPEND ONLY once anything serializes an id. Nothing does today — the ids are
// FNames written into the quest flag set, which is text and survives reorder —
// but the enum above is a UENUM and the same rule will apply the moment it
// reaches a Data Asset.
USTRUCT(BlueprintType)
struct RIORSEDGE_API FBreakerWorldPointSource
{
    GENERATED_BODY()

    // Stable id. Becomes the quest flag `World.<SourceId>`, which is how the
    // grant is made idempotent: the flag set is presence-only, monotonic and
    // saved on change, which is exactly the shape a one-time permanent grant
    // needs and is already built.
    UPROPERTY(BlueprintReadOnly) FName SourceId;

    UPROPERTY(BlueprintReadOnly) FText Display;

    // 1, 2 or 3. Design rule 2: the fifteen are spread across all three acts so
    // the Core Tree is never entirely gated behind level pace.
    UPROPERTY(BlueprintReadOnly) int32 Act = 1;

    UPROPERTY(BlueprintReadOnly) EBreakerWorldPointDelivery Delivery = EBreakerWorldPointDelivery::MainPath;

    // Design rule 4, and the one with a test: none require party content.
    // Recorded per entry rather than asserted globally so that authoring an
    // entry that DOES require a party is a visible act rather than an omission.
    UPROPERTY(BlueprintReadOnly) bool bRequiresParty = false;

    // Design rule 3: none are missable. A permanently missed Core Point on a
    // character with a permanent class is unrecoverable.
    UPROPERTY(BlueprintReadOnly) bool bMissable = false;

    // False where the trigger does not exist in the build yet. This is the
    // honest half of the registry: the list is canon, the wiring is not
    // finished, and `make status` counts the gap rather than a document
    // describing it.
    UPROPERTY(BlueprintReadOnly) bool bTriggerBuilt = false;
};

UCLASS()
class RIORSEDGE_API UBreakerWorldPointLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    // O7's ratified list, transcribed. The count is canon: exactly fifteen, and
    // nothing endgame-gated — the old "close 75 rifts lifetime" overflow entry
    // was cut in ratification precisely because it was completable after 50.
    UFUNCTION(BlueprintPure, Category="Progression|WorldPoints")
    static const TArray<FBreakerWorldPointSource>& GetSources();

    // The flag a granted source writes: `World.<SourceId>`. One place, because
    // a grant that spells its own flag is a grant that can spell it twice.
    UFUNCTION(BlueprintPure, Category="Progression|WorldPoints")
    static FName FlagForSource(FName SourceId);

    UFUNCTION(BlueprintPure, Category="Progression|WorldPoints")
    static bool IsKnownSource(FName SourceId);

    // How many of the fifteen a build can actually reach today. The difference
    // between this and fifteen is the campaign's payout gap, as a number.
    UFUNCTION(BlueprintPure, Category="Progression|WorldPoints")
    static int32 CountWithBuiltTrigger();
};
