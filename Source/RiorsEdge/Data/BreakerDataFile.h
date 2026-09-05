#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "UObject/Class.h"

// The one door from Data/*.json into the game.
//
// A content library that lives in Data/ is read through here and nowhere
// else: one path rule (repo-relative, resolved against FPaths::ProjectDir(),
// which is also where DirectoriesToAlwaysStageAsUFS puts the folder in a
// packaged build), one JSON reader, one way of spelling an enum (by NAME,
// the string UHT reflects — the value is save data and never appears in a
// data file). Loaders collect every complaint into FBreakerDataErrors rather
// than stopping at the first, so a broken file reports all of its breaks in
// one read.
//
// World-free: nothing here touches a UWorld, an actor or a subsystem.
namespace BreakerDataFile
{
    struct FBreakerDataErrors
    {
        TArray<FString> Messages;

        void Add(const FString& Message) { Messages.Add(Message); }
        bool IsClean() const { return Messages.IsEmpty(); }
        FString Join() const { return FString::Join(Messages, TEXT("\n")); }
    };

    // Absolute path of a repo-relative data file.
    RIORSEDGE_API FString FullPath(const FString& RelativePath);

    // The parsed root object, or null with the reason in Errors. A file that
    // is missing, unreadable or not a JSON object is an error, never an empty
    // library: an empty library would be a silent zero.
    RIORSEDGE_API TSharedPtr<FJsonObject> Load(const FString& RelativePath, FBreakerDataErrors& Errors);

    // An enumerator by its reflected name. "Count" and the generated _MAX
    // entry are refused: they are array bounds, not values a row may carry.
    template <typename TEnum>
    bool ParseEnum(const FString& Name, TEnum& Out)
    {
        if (Name.IsEmpty() || Name == TEXT("Count") || Name.EndsWith(TEXT("_MAX")))
        {
            return false;
        }
        const int64 Value = StaticEnum<TEnum>()->GetValueByNameString(Name);
        if (Value == INDEX_NONE)
        {
            return false;
        }
        Out = static_cast<TEnum>(Value);
        return true;
    }
}
