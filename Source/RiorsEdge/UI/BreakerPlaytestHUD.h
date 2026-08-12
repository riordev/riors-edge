#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "BreakerPlaytestHUD.generated.h"

UCLASS()
class RIORSEDGE_API ABreakerPlaytestHUD : public AHUD
{
    GENERATED_BODY()

public:
    virtual void DrawHUD() override;

private:
    void DrawCrosshair(const FVector2D& Center, const FLinearColor& Color, float Size, float Thickness);
    void DrawLabel(const FString& Text, float X, float Y, const FLinearColor& Color, float Scale = 1.0f);
    void DrawPanel(float X, float Y, float Width, float Height, const FLinearColor& Accent);
    void DrawBar(const FString& Label, float Value, float Maximum, float X, float Y, float Width, const FLinearColor& Color);
    void DrawAbilitySlot(const FString& Label, float X, float Y, const FLinearColor& Accent);
};
