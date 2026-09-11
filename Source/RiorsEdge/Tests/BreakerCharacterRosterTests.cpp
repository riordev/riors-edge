// THE ROSTER ROW FOLLOWS THE CHARACTER SAVE. The owner's bug: "my characters
// on main menu dont display the correct level". The select screen draws
// FBreakerCharacterSummary::CharacterLevel, stamped 1 at CreateCharacter and
// re-derived only by RefreshSummaryFromSave — which nothing called. The pawn
// wrote the character slot on every save and never told the roster.
//
// Two halves, in the RiorsEdge.Game.BootFlow mold. The rule: a refreshed row
// reports the level the save's XP buys. The shipped configuration: the same
// XP loaded into a default-constructed progression component (the curve the
// game ships with) reports the same level, so the roster and the HUD can
// never disagree about what a character is.
//
// THE OWNER'S ROSTER IS NEVER TOUCHED. CreateCharacter and DeleteCharacter
// both call SaveRoster, which writes the ONE roster slot on disk — a test
// that used them would overwrite the owner's character list with its own.
// The row is built by hand on a roster object that is never saved, and the
// only file this test writes is a fresh-GUID character slot it deletes itself.

#include "Misc/AutomationTest.h"

#include "Kismet/GameplayStatics.h"
#include "Progression/BreakerExperience.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionTypes.h"
#include "Save/BreakerCharacterRoster.h"
#include "Save/BreakerSaveGame.h"

namespace
{
    // Removes the character slot this test wrote. Called before the asserts
    // that could early-return, so a red never leaves a file behind.
    void BreakerRosterDeleteSlot(const FGuid& CharacterId)
    {
        const FString Slot = UBreakerCharacterRoster::SlotNameForCharacter(CharacterId);
        if (UGameplayStatics::DoesSaveGameExist(Slot, 0))
        {
            UGameplayStatics::DeleteGameInSlot(Slot, 0);
        }
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerRosterSummaryFollowsSaveTest,
    "RiorsEdge.Save.Roster.SummaryFollowsSave",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerRosterSummaryFollowsSaveTest::RunTest(const FString& Parameters)
{
    // O2 PLACEHOLDER: any level above 1 proves the rule; 7 is far enough from
    // the stamp that an off-by-one cannot pass by accident.
    constexpr int32 TargetLevel = 7;
    const int32 TargetXp = UBreakerExperienceLibrary::TotalXpToReachLevel(TargetLevel, FBreakerExperienceCurve());
    TestTrue(TEXT("Precondition: reaching the target level costs XP"), TargetXp > 0);

    // ---- A roster object that never reaches the roster slot ------------------
    UBreakerCharacterRoster* Roster = Cast<UBreakerCharacterRoster>(
        UGameplayStatics::CreateSaveGameObject(UBreakerCharacterRoster::StaticClass()));
    if (!Roster)
    {
        AddError(TEXT("Could not create a roster object."));
        return false;
    }

    // The row CreateCharacter would have stamped: level 1, no XP.
    FBreakerCharacterSummary Row;
    Row.CharacterId = FGuid::NewGuid();
    Row.CharacterName = TEXT("T");
    Row.ClassId = EBreakerClassId::Caster;
    Row.CharacterLevel = 1;
    Row.TotalXp = 0;
    Row.LastPlayedUnixSeconds = 0;
    Roster->Characters.Add(Row);
    const FGuid Id = Row.CharacterId;

    // ---- The character save the pawn would write after play -----------------
    UBreakerSaveGame* Save = Cast<UBreakerSaveGame>(
        UGameplayStatics::CreateSaveGameObject(UBreakerSaveGame::StaticClass()));
    if (!Save)
    {
        AddError(TEXT("Could not create a character save object."));
        return false;
    }
    Save->SaveVersion = UBreakerSaveGame::CurrentSaveVersion;
    Save->CoreLayoutVersion = UBreakerSaveGame::ActiveCoreLayoutVersion;
    Save->CharacterId = Id;
    Save->bRiftglassFoldedToAccount = true;
    Save->Progression.PermanentClass = EBreakerClassId::Caster;
    Save->Progression.TotalExperience = TargetXp;
    Save->Progression.CharacterLevel = TargetLevel;
    if (!UGameplayStatics::SaveGameToSlot(Save, UBreakerCharacterRoster::SlotNameForCharacter(Id), 0))
    {
        AddError(TEXT("Could not write the character slot."));
        BreakerRosterDeleteSlot(Id);
        return false;
    }

    // ---- The rule: the refreshed row reports the save's level ---------------
    const bool bRefreshed = Roster->RefreshSummaryFromSave(Id);
    FBreakerCharacterSummary Found;
    const bool bFound = Roster->FindCharacter(Id, Found);
    // Cleanup FIRST: nothing below may leave the slot on disk on a red.
    BreakerRosterDeleteSlot(Id);

    TestTrue(TEXT("RefreshSummaryFromSave finds the row and its save"), bRefreshed);
    TestTrue(TEXT("The row is still in the roster after the refresh"), bFound);
    TestEqual(TEXT("The roster row reports the level the save's XP buys"), Found.CharacterLevel, TargetLevel);
    TestEqual(TEXT("The roster row carries the save's XP"), Found.TotalXp, TargetXp);
    TestEqual(TEXT("The roster row keeps the save's class"), Found.ClassId, EBreakerClassId::Caster);
    TestTrue(TEXT("A refreshed row is stamped as played"), Found.LastPlayedUnixSeconds > 0);

    TestFalse(TEXT("A refresh of a character not in the roster is refused, not invented"),
        Roster->RefreshSummaryFromSave(FGuid::NewGuid()));

    // ---- The shipped configuration: the component agrees with the row -------
    // The same replay CreatedCharacterKeepsItsClass uses: a fresh component
    // loads the state the save carries. LoadProgressionState re-derives the
    // level through the component's own ExperienceCurve — the default the
    // game ships with — so this is the number the HUD shows in play.
    UBreakerProgressionComponent* Progression = NewObject<UBreakerProgressionComponent>();
    FBreakerProgressionState Loaded;
    Loaded.PermanentClass = EBreakerClassId::Caster;
    Loaded.TotalExperience = TargetXp;
    Loaded.CharacterLevel = 1;
    Progression->LoadProgressionState(Loaded);
    TestEqual(TEXT("The shipped component derives the same level from the same XP"),
        Progression->GetCharacterLevel(), TargetLevel);
    TestEqual(TEXT("The shipped component's level matches the roster row"),
        Progression->GetCharacterLevel(), Found.CharacterLevel);

    return true;
}
