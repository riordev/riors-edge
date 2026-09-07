#include "Data/BreakerStrings.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/CString.h"

// ---------------------------------------------------------------------------
// THE LOADER — Data/strings.json into one value per key.
// ---------------------------------------------------------------------------
// The same shape as the status rules' loader: every complaint is collected,
// and any complaint fails the WHOLE load. A file that served the rows it had
// and the key names for the rest would put one wrong word on screen among
// sixty right ones, which is the failure nobody notices in a playtest.
namespace
{
    using BreakerDataFile::FBreakerDataErrors;

    // The one shape the file may have.
    constexpr int32 BreakerStringsVersion = 1;

    struct FBreakerStringsLoad
    {
        // One per EBreakerStringKey, in enumerator order. The key's own name
        // until the file loads clean.
        TArray<FString> Values;
        FBreakerDataErrors Errors;
    };

    FBreakerStringsLoad BreakerStringsLoadData()
    {
        FBreakerStringsLoad Load;
        Load.Values.Reserve(BreakerStringKeyCount);
        for (const FBreakerStringKeyRow& Row : BreakerStringKeyRows)
        {
            Load.Values.Emplace(Row.Key);
        }

        FBreakerDataErrors Errors;
        TArray<FString> Values;
        Values.SetNum(BreakerStringKeyCount);
        const FString File = BreakerStrings::DataRelativePath();

        const TSharedPtr<FJsonObject> Root = BreakerDataFile::Load(File, Errors);
        if (Root.IsValid())
        {
            double Version = 0.0;
            if (!Root->TryGetNumberField(TEXT("version"), Version))
            {
                Errors.Add(FString::Printf(TEXT("%s: no \"version\" number"), *File));
            }
            else if (static_cast<int32>(Version) != BreakerStringsVersion)
            {
                Errors.Add(FString::Printf(TEXT("%s: version %g is not %d"), *File, Version, BreakerStringsVersion));
            }

            const TSharedPtr<FJsonObject>* Strings = nullptr;
            if (!Root->TryGetObjectField(TEXT("strings"), Strings) || !Strings->IsValid())
            {
                Errors.Add(FString::Printf(TEXT("%s: no \"strings\" object"), *File));
            }
            else
            {
                // Every key the code names has a row of the declared shape.
                for (int32 Index = 0; Index < BreakerStringKeyCount; ++Index)
                {
                    const FBreakerStringKeyRow& Row = BreakerStringKeyRows[Index];
                    FString Value;
                    if (!(*Strings)->HasField(Row.Key))
                    {
                        Errors.Add(FString::Printf(TEXT("%s: missing row"), Row.Key));
                        continue;
                    }
                    if (!(*Strings)->TryGetStringField(Row.Key, Value))
                    {
                        Errors.Add(FString::Printf(TEXT("%s: value is not a string"), Row.Key));
                        continue;
                    }
                    const FString Found = BreakerStrings::PlaceholderSignature(Value);
                    if (Found != Row.Signature)
                    {
                        Errors.Add(FString::Printf(TEXT("%s: placeholders \"%s\" do not match the declared \"%s\""),
                            Row.Key, *Found, Row.Signature));
                        continue;
                    }
                    Values[Index] = MoveTemp(Value);
                }

                // No row the code does not name: an orphan row is copy nothing
                // draws, and the file must not become a place to park text.
                for (const TPair<FString, TSharedPtr<FJsonValue>>& Field : (*Strings)->Values)
                {
                    bool bNamed = false;
                    for (const FBreakerStringKeyRow& Row : BreakerStringKeyRows)
                    {
                        if (Field.Key == Row.Key)
                        {
                            bNamed = true;
                            break;
                        }
                    }
                    if (!bNamed)
                    {
                        Errors.Add(FString::Printf(TEXT("%s: no key in BreakerStringKeys.h names this row"), *Field.Key));
                    }
                }
            }
        }

        if (!Errors.IsClean())
        {
            Load.Errors = Errors;
            ensureMsgf(false, TEXT("%s failed to load; every string draws as its own key name.\n%s"), *File, *Errors.Join());
            return Load;
        }
        Load.Values = MoveTemp(Values);
        return Load;
    }

    const FBreakerStringsLoad& BreakerStringsLoaded()
    {
        static const FBreakerStringsLoad Load = BreakerStringsLoadData();
        return Load;
    }

    // Formatted output longer than this returns the key name; the longest
    // shipped row is a short line of caption text. O2 PLACEHOLDER.
    constexpr int32 BreakerStringsFormatBuffer = 512;
}

FString BreakerStrings::DataRelativePath()
{
    return TEXT("Data/strings.json");
}

const FString& BreakerStrings::Get(EBreakerStringKey Key)
{
    const int32 Index = static_cast<int32>(Key);
    if (Index < 0 || Index >= BreakerStringKeyCount)
    {
        static const FString OutOfRange(TEXT("strings.outOfRange"));
        return OutOfRange;
    }
    return BreakerStringsLoaded().Values[Index];
}

FString VARARGS BreakerStrings::FormatImpl(const TCHAR* Fmt, ...)
{
    TCHAR Buffer[BreakerStringsFormatBuffer];
    va_list Args;
    va_start(Args, Fmt);
    const int32 Written = FCString::GetVarArgs(Buffer, UE_ARRAY_COUNT(Buffer), Fmt, Args);
    va_end(Args);
    if (Written < 0 || Written >= BreakerStringsFormatBuffer)
    {
        // Fmt IS the key's value here, so when the value is the key name
        // (failed file) the two readings agree; when it is a real row that
        // overran the buffer, the row itself is the visible thing.
        return FString(Fmt);
    }
    Buffer[Written] = TEXT('\0');
    return FString(Buffer);
}

const BreakerDataFile::FBreakerDataErrors& BreakerStrings::GetDataErrors()
{
    return BreakerStringsLoaded().Errors;
}

const TCHAR* BreakerStrings::KeyName(EBreakerStringKey Key)
{
    const int32 Index = static_cast<int32>(Key);
    return Index >= 0 && Index < BreakerStringKeyCount ? BreakerStringKeyRows[Index].Key : TEXT("");
}

const TCHAR* BreakerStrings::Signature(EBreakerStringKey Key)
{
    const int32 Index = static_cast<int32>(Key);
    return Index >= 0 && Index < BreakerStringKeyCount ? BreakerStringKeyRows[Index].Signature : TEXT("");
}

FString BreakerStrings::PlaceholderSignature(const FString& Value)
{
    // A token is '%' through its conversion letter; "%%" is the literal
    // percent and is a token too, so a stray one cannot pass as fixed text.
    FString Signature;
    const int32 Len = Value.Len();
    for (int32 Index = 0; Index < Len; ++Index)
    {
        if (Value[Index] != TEXT('%'))
        {
            continue;
        }
        Signature.AppendChar(TEXT('%'));
        if (Index + 1 < Len && Value[Index + 1] == TEXT('%'))
        {
            Signature.AppendChar(TEXT('%'));
            ++Index;
            continue;
        }
        while (++Index < Len)
        {
            const TCHAR Char = Value[Index];
            Signature.AppendChar(Char);
            if (FChar::IsAlpha(Char))
            {
                break;
            }
        }
    }
    return Signature;
}
