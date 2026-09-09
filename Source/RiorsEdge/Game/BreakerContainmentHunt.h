#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BreakerContainmentHunt.generated.h"
class ABreakerEnemy;
// Station Zero's one authored priority encounter, not a generic quest framework.
UCLASS()
class RIORSEDGE_API UBreakerContainmentHunt : public UActorComponent
{
    GENERATED_BODY()
public:
    UBreakerContainmentHunt();
    static FName TargetTag();
    static FName CompletionFlag();
    static void AttachTo(ABreakerEnemy* Target);
protected:
    virtual void BeginPlay() override;
private:
    UFUNCTION() void OnTargetDeath();
    bool bClaimed = false;
};
