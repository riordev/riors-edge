#pragma once

#include "CoreMinimal.h"
#include "Combat/BreakerCombatTypes.h"
#include "GameFramework/HUD.h"
#include "BreakerPlaytestHUD.generated.h"

class ABreakerCharacter;
class UBreakerCombatComponent;

UCLASS()
class RIORSEDGE_API ABreakerPlaytestHUD : public AHUD
{
    GENERATED_BODY()

public:
    virtual void DrawHUD() override;

private:
    // Bound once to the player's combat component; dodge and block are
    // instantaneous results, so they have to be latched when broadcast
    // rather than polled from a persistent state.
    UFUNCTION() void HandlePlayerDamageReceived(const FBreakerDamageResult& Result);
    void EnsureDamageBinding(const ABreakerCharacter* Character);
    void DrawDefenseFeedback(const FVector2D& Center);
    void DrawStatusReadout(const ABreakerCharacter* Character, float X, float Y);

    UPROPERTY() TObjectPtr<UBreakerCombatComponent> BoundCombat;
    double LastDodgeTime = -1000.0;
    double LastBlockTime = -1000.0;
    double LastEliteKillTime = -1000.0;

    void DrawCrosshair(const FVector2D& Center, const FLinearColor& Color, float Size, float Thickness);
    void DrawLabel(const FString& Text, float X, float Y, const FLinearColor& Color, float Scale = 1.0f);
    void DrawPanel(float X, float Y, float Width, float Height, const FLinearColor& Accent);
    void DrawBar(const FString& Label, float Value, float Maximum, float X, float Y, float Width, const FLinearColor& Color);
    void DrawAbilitySlot(const FString& Label, float X, float Y, const FLinearColor& Accent);
};
