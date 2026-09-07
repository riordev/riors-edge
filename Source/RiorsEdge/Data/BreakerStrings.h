#pragma once

#include "CoreMinimal.h"
#include "Data/BreakerDataFile.h"
#include "Data/BreakerStringKeys.h"
#include "Templates/IsValidVariadicFunctionArg.h"

// THE STRING TABLE (O195): Data/strings.json read once, served by key.
//
// Shape of the file:
//   { "version": 1, "strings": { "hud.death.redeploying": "REDEPLOYING", ... } }
//
// The loader validates the whole file against BreakerStringKeys.h — every key
// present, no key the list does not name, every value's printf tokens equal
// to the declared signature — and ANY complaint fails the whole file. A failed
// file serves each key's own name ("hud.death.redeploying" draws on screen):
// visible, never silent, and never a nearest fit.
//
// World-free: a static file read, the same as every other Data/ loader, so
// the pure UI headers (the resource row) can ask for a word without a world.
namespace BreakerStrings
{
    RIORSEDGE_API FString DataRelativePath();

    // The value for a key, or the key's own name when the file failed.
    RIORSEDGE_API const FString& Get(EBreakerStringKey Key);

    // The varargs path under Format. FString::Printf accepts only a literal,
    // which is the right rule for code and the wrong one for data; this is
    // the same call it makes underneath, with the format-string check moved
    // to the loader. A format the platform refuses returns the row unformatted.
    RIORSEDGE_API FString VARARGS FormatImpl(const TCHAR* Fmt, ...);

    // Get, formatted. The value's tokens were checked against the key's
    // declared signature at load, which is the whole of the format check: the
    // caller passes the arguments the signature in BreakerStringKeys.h names,
    // as it would to FString::Printf, and the same argument-type rule holds
    // (an FString goes through as *Value, never by value).
    template <typename... Types>
    FString Format(EBreakerStringKey Key, Types... Args)
    {
        static_assert((TIsValidVariadicFunctionArg<Types>::Value && ...), "Invalid argument(s) passed to BreakerStrings::Format");
        return FormatImpl(*Get(Key), Args...);
    }

    // Every complaint the validator raised, or clean.
    RIORSEDGE_API const BreakerDataFile::FBreakerDataErrors& GetDataErrors();

    // The file key and the declared signature for an enumerator.
    RIORSEDGE_API const TCHAR* KeyName(EBreakerStringKey Key);
    RIORSEDGE_API const TCHAR* Signature(EBreakerStringKey Key);

    // The printf tokens of a value, concatenated: "LEVEL %d  (+%d)" -> "%d%d",
    // "ABSORBED -%.0f%%" -> "%.0f%%", a fixed string -> "". Pure.
    RIORSEDGE_API FString PlaceholderSignature(const FString& Value);
}
