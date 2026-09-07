#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Data/BreakerDataFile.h"
#include "Data/BreakerStrings.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

// THE STRING TABLE'S THREE GUARDS (O195).
//
// Loads: the shipped Data/strings.json reads clean through the loader the
// game reads it through. A failed file names its breaks first.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerStringsLoadsTest,
    "RiorsEdge.Data.Strings.Loads",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerStringsLoadsTest::RunTest(const FString& Parameters)
{
    const BreakerDataFile::FBreakerDataErrors& Errors = BreakerStrings::GetDataErrors();
    for (const FString& Error : Errors.Messages)
    {
        AddError(Error);
    }
    if (!Errors.IsClean())
    {
        return false;
    }

    // A clean load serves the file, not the key name, for every key.
    for (int32 Index = 0; Index < BreakerStringKeyCount; ++Index)
    {
        const EBreakerStringKey Key = static_cast<EBreakerStringKey>(Index);
        const FString& Value = BreakerStrings::Get(Key);
        TestFalse(*FString::Printf(TEXT("%s has a value"), BreakerStrings::KeyName(Key)), Value.IsEmpty());
        TestNotEqual(*FString::Printf(TEXT("%s serves the file, not its own name"), BreakerStrings::KeyName(Key)),
            Value, FString(BreakerStrings::KeyName(Key)));
    }

    // The format path: the value's placeholders take the declared arguments.
    TestEqual(TEXT("A %d row formats"), BreakerStrings::Format(EBreakerStringKey::HudBannerWaveClear, 3),
        BreakerStrings::Get(EBreakerStringKey::HudBannerWaveClear).Replace(TEXT("%d"), TEXT("3")));
    TestEqual(TEXT("A %d%d row formats in order"), BreakerStrings::Format(EBreakerStringKey::HudBackpackFull, 25, 25),
        BreakerStrings::Get(EBreakerStringKey::HudBackpackFull).Replace(TEXT("%d"), TEXT("25")));
    TestEqual(TEXT("A %s row formats"), BreakerStrings::Format(EBreakerStringKey::HudPromptKeyed, TEXT("ENTER")),
        BreakerStrings::Get(EBreakerStringKey::HudPromptKeyed).Replace(TEXT("%s"), TEXT("ENTER")));
    TestEqual(TEXT("A %.0f%% row formats and keeps its literal percent"),
        BreakerStrings::Format(EBreakerStringKey::HudDamageAbsorbed, 42.0f),
        BreakerStrings::Get(EBreakerStringKey::HudDamageAbsorbed).Replace(TEXT("%.0f"), TEXT("42")).Replace(TEXT("%%"), TEXT("%")));
    TestEqual(TEXT("A fixed row formats to itself"), BreakerStrings::Format(EBreakerStringKey::BarBoss),
        BreakerStrings::Get(EBreakerStringKey::BarBoss));
    return true;
}

// KeysCovered: the file and BreakerStringKeys.h name the same set, read off
// the raw JSON rather than the loader so a loader that dropped a row could
// not hide it; and each value's placeholders are the declared signature.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerStringsKeysCoveredTest,
    "RiorsEdge.Data.Strings.KeysCovered",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerStringsKeysCoveredTest::RunTest(const FString& Parameters)
{
    // The signature reader itself, on the shapes the table uses.
    TestEqual(TEXT("A fixed string has no signature"), BreakerStrings::PlaceholderSignature(TEXT("RELOADING")), FString());
    TestEqual(TEXT("One token"), BreakerStrings::PlaceholderSignature(TEXT("WAVE %d CLEAR")), FString(TEXT("%d")));
    TestEqual(TEXT("Two tokens in order"), BreakerStrings::PlaceholderSignature(TEXT("LEVEL %d  (+%d)")), FString(TEXT("%d%d")));
    TestEqual(TEXT("Precision and the literal percent"), BreakerStrings::PlaceholderSignature(TEXT("ABSORBED -%.0f%%")), FString(TEXT("%.0f%%")));
    TestEqual(TEXT("A string token"), BreakerStrings::PlaceholderSignature(TEXT("F  %s")), FString(TEXT("%s")));
    TestEqual(TEXT("A trailing percent is a token, not text"), BreakerStrings::PlaceholderSignature(TEXT("50%")), FString(TEXT("%")));

    BreakerDataFile::FBreakerDataErrors Errors;
    const TSharedPtr<FJsonObject> Root = BreakerDataFile::Load(BreakerStrings::DataRelativePath(), Errors);
    for (const FString& Error : Errors.Messages)
    {
        AddError(Error);
    }
    if (!Root.IsValid())
    {
        return false;
    }
    const TSharedPtr<FJsonObject>* Strings = nullptr;
    if (!TestTrue(TEXT("The file has a \"strings\" object"), Root->TryGetObjectField(TEXT("strings"), Strings)))
    {
        return false;
    }

    // Every key the code names is a row, and its value carries the signature.
    for (int32 Index = 0; Index < BreakerStringKeyCount; ++Index)
    {
        const EBreakerStringKey Key = static_cast<EBreakerStringKey>(Index);
        const TCHAR* Name = BreakerStrings::KeyName(Key);
        FString Value;
        if (!TestTrue(*FString::Printf(TEXT("%s is a string row in the file"), Name), (*Strings)->TryGetStringField(Name, Value)))
        {
            continue;
        }
        TestEqual(*FString::Printf(TEXT("%s placeholders match the declared signature"), Name),
            BreakerStrings::PlaceholderSignature(Value), FString(BreakerStrings::Signature(Key)));
        TestEqual(*FString::Printf(TEXT("%s is served as the file has it"), Name), BreakerStrings::Get(Key), Value);
    }

    // No row the code does not name, and no key named twice in the list.
    TArray<FString> Named;
    for (const FBreakerStringKeyRow& Row : BreakerStringKeyRows)
    {
        TestFalse(*FString::Printf(TEXT("%s is listed once"), Row.Key), Named.Contains(Row.Key));
        Named.Add(Row.Key);
    }
    for (const TPair<FString, TSharedPtr<FJsonValue>>& Field : (*Strings)->Values)
    {
        TestTrue(*FString::Printf(TEXT("%s is a key BreakerStringKeys.h names"), *Field.Key), Named.Contains(Field.Key));
    }
    TestEqual(TEXT("The file has exactly one row per key"), (*Strings)->Values.Num(), BreakerStringKeyCount);
    return true;
}

// Lexicon: nothing in the table names the build's own scaffolding, and
// nothing spends an Act I word (the same list BreakerActOneLexiconTests.cpp
// sweeps over the quest and dialogue rows).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerStringsLexiconTest,
    "RiorsEdge.Data.Strings.Lexicon",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerStringsLexiconTest::RunTest(const FString& Parameters)
{
    // Scaffolding words: the tileset, the resource rest words the fill now
    // carries, the graybox grid, stubs and dev prefixes.
    const TCHAR* Scaffolding[] = { TEXT("TILESET"), TEXT("BANKED"), TEXT("SETTLED"), TEXT("GRID"), TEXT("STUB"), TEXT("DEV:") };
    // The Act I lexicon, case-insensitive for the same reason that test is.
    const TCHAR* ActOne[] = { TEXT("altered"), TEXT("bastion"), TEXT("aberrant"), TEXT("anomalous") };

    for (int32 Index = 0; Index < BreakerStringKeyCount; ++Index)
    {
        const EBreakerStringKey Key = static_cast<EBreakerStringKey>(Index);
        const FString& Value = BreakerStrings::Get(Key);
        for (const TCHAR* Word : Scaffolding)
        {
            if (Value.Contains(Word, ESearchCase::IgnoreCase))
            {
                AddError(FString::Printf(TEXT("%s reaches the screen with '%s': \"%s\""), BreakerStrings::KeyName(Key), Word, *Value));
            }
        }
        for (const TCHAR* Word : ActOne)
        {
            if (Value.Contains(Word, ESearchCase::IgnoreCase))
            {
                AddError(FString::Printf(TEXT("Act I lexicon violation: %s contains '%s' — \"%s\""), BreakerStrings::KeyName(Key), Word, *Value));
            }
        }
    }
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
