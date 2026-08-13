#pragma once

#include "CoreMinimal.h"
#include "Combat/BreakerCombatTypes.h"
#include "GameFramework/HUD.h"
#include "Progression/BreakerProgressionTypes.h"
// Full include, not a forward declaration: FBreakerShotResult is a UFUNCTION
// parameter, so UHT needs the complete type (same reason
// BreakerMomentumComponent.h includes it).
#include "Weapons/BreakerWeaponComponent.h"
#include "BreakerPlaytestHUD.generated.h"

class ABreakerCharacter;
class UBreakerAbilityComponent;
class UBreakerCombatComponent;
class UBreakerWeaponComponent;

// One recorded hitscan line. Plain (non-reflected) because it holds no UObject
// references: the ring buffer must survive the shot actor being destroyed.
struct FBreakerHUDTracer
{
    FVector Start = FVector::ZeroVector;
    FVector End = FVector::ZeroVector;
    FVector Impact = FVector::ZeroVector;
    bool bHit = false;
    bool bWeakPoint = false;
    double Time = -1000.0;
};

// One floating damage number. Also plain: it outlives the target it came from.
struct FBreakerHUDDamageNumber
{
    FVector World = FVector::ZeroVector;
    float Value = 0.0f;
    bool bCritical = false;
    bool bWeakPoint = false;
    double Time = -1000.0;
};

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

    // Same bind/rebind discipline for shots: the tracer trail is the only
    // record of a hitscan line, and polling GetLastShot() would miss every
    // shot fired faster than one per frame.
    UFUNCTION() void HandlePlayerShot(const FBreakerShotResult& Shot);
    void EnsureWeaponBinding(const ABreakerCharacter* Character);

    void DrawDefenseFeedback(const FVector2D& Center);
    void DrawStatusReadout(const ABreakerCharacter* Character, float X, float Y);
    void DrawVitalsBand(const ABreakerCharacter* Character, float X, float BottomY);
    void DrawCombatCluster(const ABreakerCharacter* Character, float X, float Y, float Width, float Height);
    void DrawTracers();
    void DrawDamageNumbers();
    void DrawEnemyHealthBars(const ABreakerCharacter* Character);
    void DrawLootPickups(const ABreakerCharacter* Character);

    UPROPERTY() TObjectPtr<UBreakerCombatComponent> BoundCombat;
    UPROPERTY() TObjectPtr<UBreakerWeaponComponent> BoundWeapon;
    double LastDodgeTime = -1000.0;
    double LastBlockTime = -1000.0;
    double LastEliteKillTime = -1000.0;

    static constexpr int32 MaxTracers = 16;
    TArray<FBreakerHUDTracer> Tracers;
    int32 NextTracerIndex = 0;

    static constexpr int32 MaxDamageNumbers = 24;
    TArray<FBreakerHUDDamageNumber> DamageNumbers;
    int32 NextDamageNumberIndex = 0;

    void DrawCrosshair(const FVector2D& Center, const FLinearColor& Color, float Size, float Thickness);
    void DrawLabel(const FString& Text, float X, float Y, const FLinearColor& Color, float Scale = 1.0f);
    // Outline + shadow + face pass. Legible over any world background without
    // resorting to a backing plate.
    void DrawBoldLabel(const FString& Text, float X, float Y, const FLinearColor& Face, float Scale, float Alpha);
    void DrawPanel(float X, float Y, float Width, float Height, const FLinearColor& Accent);
    void DrawBar(const FString& Label, float Value, float Maximum, float X, float Y, float Width, const FLinearColor& Color);
    void DrawChip(const FString& Text, float X, float Y, float Width, float Height, const FLinearColor& Accent, bool bFilled);
    void DrawAbilitySlot(const UBreakerAbilityComponent* Abilities, EBreakerAbilitySlot Slot, const FString& KeyHint, float X, float Y, float Size, const FLinearColor& Accent);
};
