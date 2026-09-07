#include "Input/BreakerInputConfig.h"
#include "InputAction.h"

UBreakerInputConfig::UBreakerInputConfig()
{
    // The shipped asset predates Parry. A default subobject supplies a stable,
    // owned action without modifying that asset or sharing mutable mappings.
    Parry = CreateDefaultSubobject<UInputAction>(TEXT("ParryAction"));
    Parry->ValueType = EInputActionValueType::Boolean;
}
