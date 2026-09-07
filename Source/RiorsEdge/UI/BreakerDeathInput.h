#pragma once
#include "CoreMinimal.h"
#include "InputCoreTypes.h"

enum class EBreakerDeathAction : uint8 { None, Retry, Return };
namespace BreakerDeathInput
{
    inline EBreakerDeathAction Confirm(const FKey& Key, bool bRepeat, bool bRetryAllowed, bool bPending)
    {
        if (bRepeat || bPending || (Key != EKeys::Enter && Key != EKeys::SpaceBar && Key != EKeys::Gamepad_FaceButton_Bottom))
            return EBreakerDeathAction::None;
        return bRetryAllowed ? EBreakerDeathAction::Retry : EBreakerDeathAction::Return;
    }
}
