// THE ROSTER ROW FOLLOWS THE CHARACTER SAVE. The owner's bug: "my characters
// on main menu dont display the correct level". The select screen draws
// FBreakerCharacterSummary::CharacterLevel, stamped 1 at CreateCharacter and
// re-derived by RefreshSummaryFromSave. The pawn calls that for the one
// character it saves; every other row keeps whatever stamp it last got until
// that character is played, so the select screen re-derives every row from
// its own slot when the roster is loaded for display.
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
// The rows are built by hand on a roster object that is never saved, and the
// only files these tests write are fresh-GUID character slots they delete
// themselves.

#include "Misc/AutomationTest.h"

#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
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

    // The row CreateCharacter would have stamped: level 1, no XP, a fixed
    // played-stamp so a test can tell a read from a play.
    FBreakerCharacterSummary BreakerRosterFreshRow(const TCHAR* Name, int64 LastPlayedUnixSeconds)
    {
        FBreakerCharacterSummary Row;
        Row.CharacterId = FGuid::NewGuid();
        Row.CharacterName = Name;
        Row.ClassId = EBreakerClassId::Caster;
        Row.CharacterLevel = 1;
        Row.TotalXp = 0;
        Row.LastPlayedUnixSeconds = LastPlayedUnixSeconds;
        return Row;
    }

    // Writes the character slot the pawn would have written after play:
    // enough XP to sit at TargetLevel. Returns false if the write failed.
    bool BreakerRosterWriteSlotAtLevel(const FGuid& CharacterId, int32 TargetLevel, int32& OutTargetXp)
    {
        OutTargetXp = UBreakerExperienceLibrary::TotalXpToReachLevel(TargetLevel, FBreakerExperienceCurve());
        UBreakerSaveGame* Save = Cast<UBreakerSaveGame>(
            UGameplayStatics::CreateSaveGameObject(UBreakerSaveGame::StaticClass()));
        if (!Save) return false;
        Save->SaveVersion = UBreakerSaveGame::CurrentSaveVersion;
        Save->CoreLayoutVersion = UBreakerSaveGame::ActiveCoreLayoutVersion;
        Save->CharacterId = CharacterId;
        Save->bRiftglassFoldedToAccount = true;
        Save->Progression.PermanentClass = EBreakerClassId::Caster;
        Save->Progression.TotalExperience = OutTargetXp;
        Save->Progression.CharacterLevel = TargetLevel;
        return UGameplayStatics::SaveGameToSlot(Save, UBreakerCharacterRoster::SlotNameForCharacter(CharacterId), 0);
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

// EVERY ROW IS RE-DERIVED WHEN THE ROSTER IS LOADED FOR DISPLAY. The
// write-time refresh serves only the pawn that saves; a roster of three
// characters, two of them played in other sessions, showed level 1 for both
// until each was played again. The rule: RefreshAllSummariesFromSaves reads
// every row from its own slot, leaves the played-stamp alone (a read is not a
// play), and leaves a row whose slot is absent at its cached values. The
// shipped configuration: the select screen's roster load calls it.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerRosterLoadRefreshesEveryRowTest,
    "RiorsEdge.Save.Roster.LoadRefreshesEveryRow",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerRosterLoadRefreshesEveryRowTest::RunTest(const FString& Parameters)
{
    // O2 PLACEHOLDER: two distinct levels above 1, so a refresh that read one
    // slot into every row cannot pass; 7 and 3 are the SummaryFollowsSave
    // distance and a nearer one.
    constexpr int32 FirstLevel = 7;
    constexpr int32 SecondLevel = 3;
    // A stamp no clock will produce: any change to it is a refresh that
    // treated a read as a play.
    constexpr int64 CreationStamp = 12345;

    // ---- A roster object that never reaches the roster slot ------------------
    UBreakerCharacterRoster* Roster = Cast<UBreakerCharacterRoster>(
        UGameplayStatics::CreateSaveGameObject(UBreakerCharacterRoster::StaticClass()));
    if (!Roster)
    {
        AddError(TEXT("Could not create a roster object."));
        return false;
    }

    const FBreakerCharacterSummary FirstRow = BreakerRosterFreshRow(TEXT("A"), CreationStamp);
    const FBreakerCharacterSummary SecondRow = BreakerRosterFreshRow(TEXT("B"), CreationStamp);
    // The third row has no slot behind it: the orphan DeleteCharacter's
    // warning describes, or a file the platform refused. It must draw from
    // its cache rather than vanish or reset.
    const FBreakerCharacterSummary OrphanRow = BreakerRosterFreshRow(TEXT("C"), CreationStamp);
    Roster->Characters.Add(FirstRow);
    Roster->Characters.Add(SecondRow);
    Roster->Characters.Add(OrphanRow);

    // ---- The slots two other sessions' pawns would have written --------------
    int32 FirstXp = 0;
    int32 SecondXp = 0;
    const bool bFirstWritten = BreakerRosterWriteSlotAtLevel(FirstRow.CharacterId, FirstLevel, FirstXp);
    const bool bSecondWritten = BreakerRosterWriteSlotAtLevel(SecondRow.CharacterId, SecondLevel, SecondXp);
    if (!bFirstWritten || !bSecondWritten)
    {
        AddError(TEXT("Could not write a character slot."));
        BreakerRosterDeleteSlot(FirstRow.CharacterId);
        BreakerRosterDeleteSlot(SecondRow.CharacterId);
        return false;
    }
    // Belt and braces: the orphan's slot must not exist for the third half of
    // the rule to mean anything.
    BreakerRosterDeleteSlot(OrphanRow.CharacterId);

    // ---- The rule: one call re-derives every row from its own slot ----------
    Roster->RefreshAllSummariesFromSaves();
    FBreakerCharacterSummary FoundFirst;
    FBreakerCharacterSummary FoundSecond;
    FBreakerCharacterSummary FoundOrphan;
    const bool bFoundFirst = Roster->FindCharacter(FirstRow.CharacterId, FoundFirst);
    const bool bFoundSecond = Roster->FindCharacter(SecondRow.CharacterId, FoundSecond);
    const bool bFoundOrphan = Roster->FindCharacter(OrphanRow.CharacterId, FoundOrphan);
    // Cleanup FIRST: nothing below may leave a slot on disk on a red.
    BreakerRosterDeleteSlot(FirstRow.CharacterId);
    BreakerRosterDeleteSlot(SecondRow.CharacterId);

    TestTrue(TEXT("Precondition: reaching the first level costs XP"), FirstXp > 0);
    TestTrue(TEXT("Precondition: reaching the second level costs XP"), SecondXp > 0);
    TestTrue(TEXT("The first row is still in the roster"), bFoundFirst);
    TestTrue(TEXT("The second row is still in the roster"), bFoundSecond);
    TestTrue(TEXT("The orphan row is still in the roster"), bFoundOrphan);
    TestEqual(TEXT("The roster still holds all three rows"), Roster->Characters.Num(), 3);

    TestEqual(TEXT("The first row reports the level its own slot's XP buys"), FoundFirst.CharacterLevel, FirstLevel);
    TestEqual(TEXT("The first row carries its own slot's XP"), FoundFirst.TotalXp, FirstXp);
    TestEqual(TEXT("The second row reports the level its own slot's XP buys"), FoundSecond.CharacterLevel, SecondLevel);
    TestEqual(TEXT("The second row carries its own slot's XP"), FoundSecond.TotalXp, SecondXp);

    // A read is not a play: the select screen's order must not shuffle every
    // time it opens.
    TestEqual(TEXT("The first row's played-stamp is untouched by a display refresh"),
        FoundFirst.LastPlayedUnixSeconds, CreationStamp);
    TestEqual(TEXT("The second row's played-stamp is untouched by a display refresh"),
        FoundSecond.LastPlayedUnixSeconds, CreationStamp);

    // The orphan keeps its cache, field by field.
    TestEqual(TEXT("A row with no slot keeps its cached level"), FoundOrphan.CharacterLevel, OrphanRow.CharacterLevel);
    TestEqual(TEXT("A row with no slot keeps its cached XP"), FoundOrphan.TotalXp, OrphanRow.TotalXp);
    TestEqual(TEXT("A row with no slot keeps its cached class"), FoundOrphan.ClassId, OrphanRow.ClassId);
    TestEqual(TEXT("A row with no slot keeps its cached name"), FoundOrphan.CharacterName, OrphanRow.CharacterName);
    TestEqual(TEXT("A row with no slot keeps its cached played-stamp"),
        FoundOrphan.LastPlayedUnixSeconds, CreationStamp);

    // ---- The shipped configuration: the select screen's load calls it -------
    // SBreakerMenu is a Slate widget that needs a viewport this suite does
    // not have, so the presence half is a source scan of EnsureRosterLoaded's
    // own body, the same scan RiorsEdge.Game.BootFlow runs on the pause
    // screen. A menu that loaded the roster and drew it without the refresh
    // is the owner's bug back.
    const FString MenuPath = FPaths::Combine(FPaths::ProjectDir(),
        TEXT("Source"), TEXT("RiorsEdge"), TEXT("UI"), TEXT("BreakerMenu.cpp"));
    FString Menu;
    if (FFileHelper::LoadFileToString(Menu, *MenuPath))
    {
        const int32 LoadBegin = Menu.Find(TEXT("void SBreakerMenu::EnsureRosterLoaded()"));
        TestTrue(TEXT("The menu defines EnsureRosterLoaded"), LoadBegin != INDEX_NONE);
        if (LoadBegin != INDEX_NONE)
        {
            // The function's own body only: it ends at whichever comes first
            // of the next member definition (" SBreakerMenu::", whatever its
            // return type) or the anonymous namespace that follows it. A
            // later mention of the refresh elsewhere is not the load.
            const int32 LoadEnd = Menu.Find(TEXT("\nnamespace"), ESearchCase::CaseSensitive,
                ESearchDir::FromStart, LoadBegin + 40);
            const int32 NextMember = Menu.Find(TEXT(" SBreakerMenu::"), ESearchCase::CaseSensitive,
                ESearchDir::FromStart, LoadBegin + 40);
            int32 BodyEnd = Menu.Len();
            if (LoadEnd != INDEX_NONE) BodyEnd = FMath::Min(BodyEnd, LoadEnd);
            if (NextMember != INDEX_NONE) BodyEnd = FMath::Min(BodyEnd, NextMember);
            const FString LoadBody = Menu.Mid(LoadBegin, BodyEnd - LoadBegin);
            TestTrue(TEXT("EnsureRosterLoaded re-derives every row from its slot"),
                LoadBody.Contains(TEXT("RefreshAllSummariesFromSaves")));
        }
    }
    else
    {
        AddInfo(TEXT("Menu source not present (packaged build?); the shipped-configuration scan was skipped."));
    }

    return true;
}
