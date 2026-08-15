#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Progression/BreakerProgressionTypes.h"
#include "BreakerCharacterRoster.generated.h"

// ---------------------------------------------------------------------------
// THE CHARACTER ROSTER
// ---------------------------------------------------------------------------
// The project shipped with exactly ONE character: a single save in a single
// fixed slot, written by ABreakerCharacter and read back at BeginPlay. Class
// choice is permanent per character (a locked decision), so with one slot a
// player who picked Swift could never see Caster without a dev override — the
// permanence rule and the single slot together made the class screen a trap
// rather than a choice.
//
// The roster is the index, NOT the data. Each character's actual save stays in
// its own `UBreakerSaveGame` under its own slot name; this object holds only
// what the SELECT screen needs to draw a row without loading five full saves.
// That split is the point: adding a field to a character's save must never
// mean rewriting the roster's format, and listing characters must never mean
// deserializing their inventories.
//
// The summary is therefore DERIVED state and always re-derivable from the
// character's own save. If the two ever disagree, the character save wins —
// see RefreshSummaryFromSave.
// ---------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct RIORSEDGE_API FBreakerCharacterSummary
{
    GENERATED_BODY()

    // Stable identity. A GUID rather than the slot index, because deleting the
    // middle character must not silently re-point another character's save at
    // a different row — which is exactly what an index-keyed roster does.
    UPROPERTY(BlueprintReadOnly) FGuid CharacterId;
    UPROPERTY(BlueprintReadOnly) FString CharacterName;
    UPROPERTY(BlueprintReadOnly) EBreakerClassId ClassId = EBreakerClassId::None;
    UPROPERTY(BlueprintReadOnly) int32 CharacterLevel = 1;
    UPROPERTY(BlueprintReadOnly) int32 TotalXp = 0;
    // Unix-ish seconds, only ever used to sort the select screen so the
    // character you last played is the one under the cursor.
    UPROPERTY(BlueprintReadOnly) int64 LastPlayedUnixSeconds = 0;

    bool IsValid() const { return CharacterId.IsValid(); }
};

UCLASS()
class RIORSEDGE_API UBreakerCharacterRoster : public USaveGame
{
    GENERATED_BODY()

public:
    // Owner ruling, 2026-08-14: "players can have a maximum of 5".
    static constexpr int32 MaxCharacters = 5;

    UPROPERTY() TArray<FBreakerCharacterSummary> Characters;
    // Which character the PLAY button resumes into. Cleared when that
    // character is deleted rather than left dangling.
    UPROPERTY() FGuid LastPlayedCharacterId;

    static constexpr int32 CurrentRosterVersion = 1;
    UPROPERTY() int32 RosterVersion = CurrentRosterVersion;

    // ---- Slot naming ---------------------------------------------------
    static FString RosterSlotName() { return TEXT("BreakerRoster"); }
    // One save per character, keyed by the GUID so a delete can never orphan
    // or collide. Deliberately NOT the display name: names are not unique and
    // are user-supplied, which makes them a filesystem hazard.
    static FString SlotNameForCharacter(const FGuid& CharacterId);

    // ---- Roster operations ---------------------------------------------
    // Loads the roster, creating an empty one if none exists. Never returns
    // null: an absent roster is the normal first-run state, not an error.
    UFUNCTION(BlueprintCallable, Category="Save|Roster")
    static UBreakerCharacterRoster* LoadOrCreate();

    UFUNCTION(BlueprintCallable, Category="Save|Roster")
    bool SaveRoster() const;

    UFUNCTION(BlueprintPure, Category="Save|Roster")
    bool IsFull() const { return Characters.Num() >= MaxCharacters; }

    UFUNCTION(BlueprintPure, Category="Save|Roster")
    bool FindCharacter(const FGuid& CharacterId, FBreakerCharacterSummary& OutSummary) const;

    // Creates the character AND writes its (empty) save, so a character that
    // appears in the roster always has a save behind it. Returns an invalid
    // GUID and a reason if it cannot — full roster, blank name, or a class
    // with no implemented kit (O39: locking into a kitless class is a trap,
    // and the same rule is enforced at the progression component).
    UFUNCTION(BlueprintCallable, Category="Save|Roster")
    FGuid CreateCharacter(const FString& CharacterName, EBreakerClassId ClassId, FText& OutFailureReason);

    // Deletes the character's save AND its roster row. Irreversible by design
    // — the confirm belongs in the UI, not here.
    UFUNCTION(BlueprintCallable, Category="Save|Roster")
    bool DeleteCharacter(const FGuid& CharacterId);

    // Re-derives a summary from the character's own save. The summary is a
    // cache for the select screen; the save is the truth.
    UFUNCTION(BlueprintCallable, Category="Save|Roster")
    bool RefreshSummaryFromSave(const FGuid& CharacterId);

    // ---- Pure rules, exposed for test ----------------------------------
    // A name the roster will accept. Trimmed, non-empty, bounded, and free of
    // the characters that would make a save slot name ambiguous.
    static bool IsAcceptableCharacterName(const FString& Name, FText& OutFailureReason);
    static constexpr int32 MaxCharacterNameLength = 20;

    // ---- Migration ------------------------------------------------------
    // Adopts a pre-roster single save as character one, so an existing player
    // does not lose the character they have been playing all session. Returns
    // the adopted id, or an invalid GUID if there was nothing to adopt.
    UFUNCTION(BlueprintCallable, Category="Save|Roster")
    FGuid AdoptLegacySaveIfPresent();
};
