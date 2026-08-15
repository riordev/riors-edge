#include "Save/BreakerCharacterRoster.h"

#include "Kismet/GameplayStatics.h"
#include "Progression/BreakerProgressionLibrary.h"
#include "Progression/BreakerClassDefinition.h"
#include "Progression/BreakerExperience.h"
#include "Save/BreakerSaveGame.h"

#define LOCTEXT_NAMESPACE "BreakerRoster"

FString UBreakerCharacterRoster::SlotNameForCharacter(const FGuid& CharacterId)
{
    // EGuidFormats::Digits gives 32 hex characters and nothing else — no
    // braces, no hyphens — so the slot name is filesystem-safe on every
    // platform without any escaping.
    return FString::Printf(TEXT("BreakerChar_%s"), *CharacterId.ToString(EGuidFormats::Digits));
}

UBreakerCharacterRoster* UBreakerCharacterRoster::LoadOrCreate()
{
    if (UGameplayStatics::DoesSaveGameExist(RosterSlotName(), 0))
    {
        if (UBreakerCharacterRoster* Loaded = Cast<UBreakerCharacterRoster>(
            UGameplayStatics::LoadGameFromSlot(RosterSlotName(), 0)))
        {
            // A roster written by a NEWER build is refused rather than
            // repaired, the same rule UBreakerSaveGame applies to its own
            // version: silently downgrading a file we do not understand is how
            // a player loses characters.
            if (Loaded->RosterVersion > CurrentRosterVersion)
            {
                UE_LOG(LogTemp, Error,
                    TEXT("Character roster is version %d but this build understands %d. Refusing to load it; ")
                    TEXT("the file has NOT been modified."), Loaded->RosterVersion, CurrentRosterVersion);
                return nullptr;
            }
            return Loaded;
        }
    }
    UBreakerCharacterRoster* Fresh = Cast<UBreakerCharacterRoster>(
        UGameplayStatics::CreateSaveGameObject(UBreakerCharacterRoster::StaticClass()));
    return Fresh;
}

bool UBreakerCharacterRoster::SaveRoster() const
{
    return UGameplayStatics::SaveGameToSlot(const_cast<UBreakerCharacterRoster*>(this), RosterSlotName(), 0);
}

bool UBreakerCharacterRoster::FindCharacter(const FGuid& CharacterId, FBreakerCharacterSummary& OutSummary) const
{
    for (const FBreakerCharacterSummary& Summary : Characters)
    {
        if (Summary.CharacterId == CharacterId)
        {
            OutSummary = Summary;
            return true;
        }
    }
    return false;
}

bool UBreakerCharacterRoster::IsAcceptableCharacterName(const FString& Name, FText& OutFailureReason)
{
    const FString Trimmed = Name.TrimStartAndEnd();
    if (Trimmed.IsEmpty())
    {
        OutFailureReason = LOCTEXT("NameEmpty", "A character needs a name.");
        return false;
    }
    if (Trimmed.Len() > MaxCharacterNameLength)
    {
        OutFailureReason = FText::Format(
            LOCTEXT("NameTooLong", "A name can be at most {0} characters."),
            FText::AsNumber(MaxCharacterNameLength));
        return false;
    }
    // The name never reaches the filesystem — the slot is keyed by GUID — so
    // this is about LEGIBILITY, not safety: control characters and newlines
    // would draw as blanks or break the select screen's row layout.
    for (const TCHAR Character : Trimmed)
    {
        if (Character < 32 || Character == 127)
        {
            OutFailureReason = LOCTEXT("NameControlChars", "That name contains characters that cannot be displayed.");
            return false;
        }
    }
    OutFailureReason = FText::GetEmpty();
    return true;
}

FGuid UBreakerCharacterRoster::CreateCharacter(const FString& CharacterName, EBreakerClassId ClassId, FText& OutFailureReason)
{
    if (IsFull())
    {
        OutFailureReason = FText::Format(
            LOCTEXT("RosterFull", "You already have {0} characters. Delete one first."),
            FText::AsNumber(MaxCharacters));
        return FGuid();
    }
    if (!IsAcceptableCharacterName(CharacterName, OutFailureReason))
    {
        return FGuid();
    }
    // O39 SLICE CLASS HONESTY, enforced HERE as well as at the progression
    // component and the class screen. A character permanently locked into a
    // class that grants nothing is unrecoverable — class choice is permanent —
    // so the check belongs at every door, not only the one the UI happens to
    // use today.
    if (ClassId == EBreakerClassId::None || !UBreakerProgressionLibrary::GetFallbackClassDefinition(ClassId))
    {
        OutFailureReason = LOCTEXT("ClassNotImplemented", "That class has no kit yet and cannot be chosen.");
        return FGuid();
    }

    FBreakerCharacterSummary Summary;
    Summary.CharacterId = FGuid::NewGuid();
    Summary.CharacterName = CharacterName.TrimStartAndEnd();
    Summary.ClassId = ClassId;
    Summary.CharacterLevel = 1;
    Summary.TotalXp = 0;
    Summary.LastPlayedUnixSeconds = FDateTime::UtcNow().ToUnixTimestamp();

    // The character's save is written BEFORE the roster row exists, so the
    // failure mode is an orphaned save (harmless, invisible, overwritten by the
    // next GUID) rather than a roster row pointing at a save that was never
    // written (a character that appears in the list and cannot be played).
    UBreakerSaveGame* Save = Cast<UBreakerSaveGame>(
        UGameplayStatics::CreateSaveGameObject(UBreakerSaveGame::StaticClass()));
    if (!Save)
    {
        OutFailureReason = LOCTEXT("SaveCreateFailed", "The character could not be created.");
        return FGuid();
    }
    Save->SaveVersion = UBreakerSaveGame::CurrentSaveVersion;
    Save->Progression.PermanentClass = ClassId;
    Save->Progression.CharacterLevel = 1;
    if (!UGameplayStatics::SaveGameToSlot(Save, SlotNameForCharacter(Summary.CharacterId), 0))
    {
        OutFailureReason = LOCTEXT("SaveWriteFailed", "The character's save could not be written.");
        return FGuid();
    }

    Characters.Add(Summary);
    LastPlayedCharacterId = Summary.CharacterId;
    SaveRoster();
    OutFailureReason = FText::GetEmpty();
    return Summary.CharacterId;
}

bool UBreakerCharacterRoster::DeleteCharacter(const FGuid& CharacterId)
{
    const int32 Index = Characters.IndexOfByPredicate(
        [&CharacterId](const FBreakerCharacterSummary& S) { return S.CharacterId == CharacterId; });
    if (Index == INDEX_NONE) return false;

    // The row goes first. If the file delete fails (locked, read-only, already
    // gone) the character must still disappear from the player's list — a row
    // they cannot remove is worse than a file they cannot see.
    Characters.RemoveAt(Index);
    if (LastPlayedCharacterId == CharacterId)
    {
        LastPlayedCharacterId = Characters.Num() > 0 ? Characters[0].CharacterId : FGuid();
    }

    const FString Slot = SlotNameForCharacter(CharacterId);
    if (UGameplayStatics::DoesSaveGameExist(Slot, 0) && !UGameplayStatics::DeleteGameInSlot(Slot, 0))
    {
        UE_LOG(LogTemp, Warning,
            TEXT("Deleted character %s from the roster, but its save slot '%s' could not be removed. ")
            TEXT("The character is gone from the player's list; the file is orphaned."),
            *CharacterId.ToString(EGuidFormats::Digits), *Slot);
    }
    SaveRoster();
    return true;
}

bool UBreakerCharacterRoster::RefreshSummaryFromSave(const FGuid& CharacterId)
{
    FBreakerCharacterSummary* Summary = Characters.FindByPredicate(
        [&CharacterId](const FBreakerCharacterSummary& S) { return S.CharacterId == CharacterId; });
    if (!Summary) return false;

    const UBreakerSaveGame* Save = Cast<UBreakerSaveGame>(
        UGameplayStatics::LoadGameFromSlot(SlotNameForCharacter(CharacterId), 0));
    if (!Save) return false;

    // The save is the truth and the summary is a cache of it. Level is
    // RE-DERIVED from stored XP rather than copied from the save's own level
    // field, so a retuned curve moves every character's displayed level the
    // moment the build changes instead of leaving five stale numbers on the
    // select screen.
    Summary->ClassId = Save->Progression.PermanentClass;
    Summary->TotalXp = Save->Progression.TotalExperience;
    Summary->CharacterLevel = UBreakerExperienceLibrary::LevelForTotalXp(
        Save->Progression.TotalExperience, FBreakerExperienceCurve());
    Summary->LastPlayedUnixSeconds = FDateTime::UtcNow().ToUnixTimestamp();
    return true;
}

FGuid UBreakerCharacterRoster::AdoptLegacySaveIfPresent()
{
    // One-time bridge from the single-slot world. An existing player has a
    // character in BreakerSave0 with hours on it; the roster arriving must not
    // read as "all your progress is gone". Adopted only into an EMPTY roster,
    // so this can never run twice and can never duplicate a character.
    if (Characters.Num() > 0) return FGuid();
    if (!UGameplayStatics::DoesSaveGameExist(UBreakerSaveGame::DefaultSlotName(), 0)) return FGuid();

    const UBreakerSaveGame* Legacy = Cast<UBreakerSaveGame>(
        UGameplayStatics::LoadGameFromSlot(UBreakerSaveGame::DefaultSlotName(), 0));
    if (!Legacy) return FGuid();

    FBreakerCharacterSummary Summary;
    Summary.CharacterId = FGuid::NewGuid();
    Summary.CharacterName = TEXT("BREAKER");
    Summary.ClassId = Legacy->Progression.PermanentClass;
    Summary.TotalXp = Legacy->Progression.TotalExperience;
    Summary.CharacterLevel = UBreakerExperienceLibrary::LevelForTotalXp(
        Legacy->Progression.TotalExperience, FBreakerExperienceCurve());
    Summary.LastPlayedUnixSeconds = FDateTime::UtcNow().ToUnixTimestamp();

    // COPIED, not moved: the legacy slot is left exactly where it is. If this
    // migration is wrong in some way nobody has thought of yet, the original
    // file is still on disk and untouched.
    UBreakerSaveGame* Copy = Cast<UBreakerSaveGame>(
        UGameplayStatics::CreateSaveGameObject(UBreakerSaveGame::StaticClass()));
    if (!Copy) return FGuid();
    // Field by field: a UObject's copy assignment is deleted, and this is the
    // better shape anyway — a new save field that nobody adds here shows up as
    // a compile-clean but silently unmigrated character, so keeping the list
    // explicit is what makes the omission visible in review. Kept in the same
    // order as the declarations in BreakerSaveGame.h so the two can be diffed.
    Copy->SaveVersion = Legacy->SaveVersion;
    Copy->Progression = Legacy->Progression;
    Copy->EquippedItems = Legacy->EquippedItems;
    Copy->BackpackItems = Legacy->BackpackItems;
    Copy->SlotOneArchetype = Legacy->SlotOneArchetype;
    Copy->SlotTwoArchetype = Legacy->SlotTwoArchetype;
    Copy->QuestFlags = Legacy->QuestFlags;
    Copy->QuestCounters = Legacy->QuestCounters;
    Copy->ForgeWallet = Legacy->ForgeWallet;
    if (!UGameplayStatics::SaveGameToSlot(Copy, SlotNameForCharacter(Summary.CharacterId), 0)) return FGuid();

    Characters.Add(Summary);
    LastPlayedCharacterId = Summary.CharacterId;
    SaveRoster();
    UE_LOG(LogTemp, Log, TEXT("Adopted the pre-roster save as character '%s'. The original slot is untouched."),
        *Summary.CharacterName);
    return Summary.CharacterId;
}

#undef LOCTEXT_NAMESPACE
