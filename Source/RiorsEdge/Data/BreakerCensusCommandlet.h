#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "BreakerCensusCommandlet.generated.h"

// Writes Data/progression.json from the built trees. Run it after any edit
// to the node library and commit the file; RiorsEdge.Data.Census.Fresh is
// red until you do. `bash Scripts/ue-census.sh` wraps the invocation.
UCLASS()
class UBreakerCensusCommandlet : public UCommandlet
{
    GENERATED_BODY()

public:
    UBreakerCensusCommandlet();
    virtual int32 Main(const FString& Params) override;
};
