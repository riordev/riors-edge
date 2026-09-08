#pragma once
#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "BreakerEnvironmentKitCommandlet.generated.h"

// -run=BreakerEnvironmentKit; existing destinations are audit-only unless
// -ReplaceExisting is explicit. -AuditOnly never imports.
UCLASS()
class UBreakerEnvironmentKitCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    UBreakerEnvironmentKitCommandlet();
    virtual int32 Main(const FString& Params) override;
};
