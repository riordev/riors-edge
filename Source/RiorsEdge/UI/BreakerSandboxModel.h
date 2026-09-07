#pragma once
#include "CoreMinimal.h"
#include "Game/BreakerGameInstance.h"
namespace BreakerSandbox
{
    inline int32 ItemLevel(const FString& MapName, int32 ExplicitLevel, int32 GymLevel, int32 CharacterLevel, int32 Maximum)
    {
        FString Name = MapName;
        if (Name.StartsWith(TEXT("UEDPIE_"))) Name = Name.Mid(Name.Find(TEXT("_"), ESearchCase::CaseSensitive, ESearchDir::FromStart, 7) + 1);
        const bool bGym = UBreakerGameInstance::IsGymMapName(Name);
        return FMath::Clamp(ExplicitLevel > 0 ? ExplicitLevel : (bGym ? GymLevel : CharacterLevel), 1, Maximum);
    }
    inline bool Seed(bool bFresh, const FString& Text, int32 RandomSeed, int32& Out)
    {
        if (bFresh) { Out = RandomSeed; return true; }
        const FString Value = Text.TrimStartAndEnd();
        const int32 Start = Value.StartsWith(TEXT("-")) || Value.StartsWith(TEXT("+")) ? 1 : 0;
        const int32 Digits = Value.Len() - Start;
        if (Digits < 1 || Digits > 10) return false;
        int64 Parsed = 0;
        for (int32 Index = Start; Index < Value.Len(); ++Index)
        {
            if (Value[Index] < TCHAR('0') || Value[Index] > TCHAR('9')) return false;
            Parsed = Parsed * 10 + (Value[Index] - TCHAR('0'));
        }
        if (Value.StartsWith(TEXT("-"))) Parsed = -Parsed;
        if (Parsed < MIN_int32 || Parsed > MAX_int32) return false;
        Out = static_cast<int32>(Parsed);
        return true;
    }
}
