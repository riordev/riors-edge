#pragma once
#include "CoreMinimal.h"
#include "Interaction/BreakerNPC.h"
#include "BreakerBasinRecorder.generated.h"
class ABreakerCharacter;
UCLASS()
class RIORSEDGE_API ABreakerBasinRecorder : public ABreakerNPC
{
    GENERATED_BODY()
public:
    ABreakerBasinRecorder();
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    void ConfigureExtraction();
    bool IsExtraction() const { return bExtraction; }
    static FName RecoveredFlag();
    static FName ExtractedFlag();
    bool IsCurrentStep(const ABreakerCharacter* Player) const;
    bool IsInteractionReachable(const ABreakerCharacter* Player) const;
    bool TryInteract(ABreakerCharacter* Player);
    FText GetRecorderPrompt() const;
private:
    UPROPERTY(Replicated) bool bExtraction=false;
};
