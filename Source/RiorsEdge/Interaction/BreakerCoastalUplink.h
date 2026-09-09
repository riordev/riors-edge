#pragma once
#include "CoreMinimal.h"
#include "Interaction/BreakerNPC.h"
#include "BreakerCoastalUplink.generated.h"
class ABreakerCharacter;

// One finite regional objective; no mission waves or separate reward economy.
UCLASS()
class RIORSEDGE_API ABreakerCoastalUplink : public ABreakerNPC
{
    GENERATED_BODY()
public:
    ABreakerCoastalUplink();
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    static FName CompletionFlag();
    bool IsCompleteFor(const ABreakerCharacter* Player) const;
    bool IsInteractionReachable(const ABreakerCharacter* Player) const;
    bool TryInteract(ABreakerCharacter* Player);
    FText GetUplinkPrompt(const ABreakerCharacter* Player) const;
    FText GetObjectiveText(const ABreakerCharacter* Player) const;
    float GetRemainingSeconds() const { return RemainingSeconds; }
    bool IsTransmitting() const { return TransmittingPlayer != nullptr; }
    UPROPERTY(EditAnywhere, Category="Uplink", meta=(ClampMin="0.1")) float TransmissionSeconds=12.f; // O2 PLACEHOLDER.
    UPROPERTY(EditAnywhere, Category="Uplink", meta=(ClampMin="220")) float WorkingRadius=700.f; // O2 PLACEHOLDER, centimetres.
private:
    bool IsAliveNearAndVisible(const ABreakerCharacter* Player,float Radius) const;
    UFUNCTION() void CancelTransmission();
    UPROPERTY(Replicated) TObjectPtr<ABreakerCharacter> TransmittingPlayer=nullptr;
    UPROPERTY(Replicated) float RemainingSeconds=0;
};
