#pragma once
#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "BreakerArmsCommandlet.generated.h"

// Editor authoring only: -run=BreakerArms. Never modifies the source mannequin.
// -AuditOnly validates the saved generated asset without writing anything.
UCLASS()
class UBreakerArmsCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    UBreakerArmsCommandlet();
    virtual int32 Main(const FString& Params) override;
};
