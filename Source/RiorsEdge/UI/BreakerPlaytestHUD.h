#pragma once

#include "CoreMinimal.h"
#include "Combat/BreakerCombatTypes.h"
#include "Fonts/SlateFontInfo.h"
#include "GameFramework/HUD.h"
#include "Progression/BreakerProgressionTypes.h"
// The class-resource row's resolved description. Pure, header-only, and the
// only part of the cluster that is testable without a viewport.
#include "UI/BreakerHUDResourceRow.h"
// Full include, not a forward declaration: FBreakerShotResult is a UFUNCTION
// parameter, so UHT needs the complete type (same reason
// BreakerMomentumComponent.h includes it).
#include "Weapons/BreakerWeaponComponent.h"
#include "BreakerPlaytestHUD.generated.h"

class ABreakerCharacter;
class UBreakerAbilityComponent;
class UBreakerAbilityStateComponent;
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

// Which edge carries a plate's 3px rail. Left is identity — which system owns
// this panel; Top is transient status, reserved for events and alerts.
// FIELDPLATE §03: one rail per plate, a plate with two rails has no meaning.
enum class EBreakerRail : uint8
{
    Left,
    Top
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

    // Activations are instantaneous and leave no polled state (Skim in
    // particular finishes inside its own frame), so every ability readout below
    // is driven by this latch rather than by a per-frame query.
    UFUNCTION() void HandleAbilityActivated(EBreakerAbilitySlot Slot);
    void EnsureAbilityBinding(const ABreakerCharacter* Character);

    void DrawDefenseFeedback(const FVector2D& Center);
    void DrawStatusReadout(const ABreakerCharacter* Character, float X, float BottomY);
    void DrawVitalsPlate(const ABreakerCharacter* Character, float X, float BottomY);
    void DrawCombatCluster(const ABreakerCharacter* Character, float X, float Y, float Width, float Height);
    // §2's class-resource slot, in two halves: which resource this character
    // has (one component read, no lookup, no iteration) and how the resolved
    // row is painted into the fixed 12px track.
    static BreakerHUD::FResourceRow ResolveResourceRow(const ABreakerCharacter* Character);
    void DrawResourceTrack(const BreakerHUD::FResourceRow& Row, float X, float Y, float Width, float Height);
    void DrawWaveBanner(const FVector2D& Center);
    void DrawTracers();
    void DrawDamageNumbers();
    void DrawEnemyHealthBars(const ABreakerCharacter* Character);
    void DrawLootPickups(const ABreakerCharacter* Character);

    // --- Ability legibility and active-effect feedback -------------------
    static const UBreakerAbilityStateComponent* GetAbilityState(const ABreakerCharacter* Character);
    // Labelled duration bars for every open Window.Swift.* state, stacked
    // upward from BottomY so the cluster below never moves.
    void DrawAbilityWindows(const ABreakerCharacter* Character, float X, float BottomY, float Width);
    // The teaching callout: first few casts of each ability only.
    void DrawAbilityCallout(const FVector2D& Center);
    // FIELDPLATE HUD §5: violet frame, edge bands, title plate, step-down.
    void DrawUltimateTreatment(const ABreakerCharacter* Character);
    void DrawSkimBurst(const FVector2D& Center);
    void DrawMarkedTarget(const ABreakerCharacter* Character);

    UPROPERTY() TObjectPtr<UBreakerCombatComponent> BoundCombat;
    UPROPERTY() TObjectPtr<UBreakerWeaponComponent> BoundWeapon;
    UPROPERTY() TObjectPtr<UBreakerAbilityComponent> BoundAbilities;
    double LastDodgeTime = -1000.0;
    double LastBlockTime = -1000.0;
    double LastEliteKillTime = -1000.0;
    // Latched on the inactive->active edge of the ultimate window: the state
    // component reports remaining time only, and §5's title plate needs
    // elapsed time.
    double UltimateWindowStartTime = -1000.0;
    bool bUltimateWindowWasActive = false;

    static constexpr int32 AbilitySlotCount = 3;
    // Indexed by EBreakerAbilitySlot. Parallel arrays rather than a struct
    // because every one of them is read by a different drawing pass.
    double SlotActivationTime[AbilitySlotCount] = { -1000.0, -1000.0, -1000.0 };
    int32 SlotActivationCount[AbilitySlotCount] = { 0, 0, 0 };
    // Which slot was cast last, and what to say about it. Resolved at
    // activation because the definition lookup is not worth repeating per frame.
    int32 LastActivatedSlotIndex = INDEX_NONE;
    FString CalloutText;
    double CalloutTime = -1000.0;
    // Latched separately from the callout: the crosshair burst fires on every
    // Skim, not only the first three.
    double SkimBurstTime = -1000.0;

    static constexpr int32 MaxTracers = 16;
    TArray<FBreakerHUDTracer> Tracers;
    int32 NextTracerIndex = 0;

    static constexpr int32 MaxDamageNumbers = 24;
    TArray<FBreakerHUDDamageNumber> DamageNumbers;
    int32 NextDamageNumberIndex = 0;

    // --- FIELDPLATE drawing primitives -----------------------------------
    // Every geometry value in this class is authored in the spec's 1920x1080
    // pixels and passed through S() once, so the HUD holds its proportions at
    // any resolution instead of shrinking into the corner.
    float UIScale = 1.0f;
    float S(float SpecPixels) const { return SpecPixels * UIScale; }

    // Flat fill, 1px border, one 3px rail. No gradient, no blur, no radius:
    // Canvas cannot round a corner and the system's 2px radius is below the
    // threshold where its absence reads as wrong.
    // A transparent Face means "the HUD's default plate face" (bg/base at the
    // readability alpha) rather than an actual transparent plate.
    void DrawPlate(float X, float Y, float Width, float Height, const FLinearColor& Rail,
        EBreakerRail RailEdge = EBreakerRail::Left, const FLinearColor& Face = FLinearColor::Transparent);
    void DrawBorder(float X, float Y, float Width, float Height, const FLinearColor& Color, float Thickness);
    void DrawTriangle(const FVector2D& A, const FVector2D& B, const FVector2D& C, const FLinearColor& Color);
    // Chevron-cut block: a rectangle sheared along its top edge. The momentum
    // track changes texture, not just colour, between states.
    void DrawShearedBlock(float X, float Y, float Width, float Height, float Shear, const FLinearColor& Color);

    // A vector face rasterised at the requested pixel size. The engine's small
    // font is a bitmap at one native size, so drawing a 40px number with it
    // magnified pixels instead of rendering glyphs.
    FSlateFontInfo MakeSpecFont(float SpecPixels) const;

    // Text authored in spec pixels. Y is the top of the line, matching Canvas.
    void DrawSpecText(const FString& Text, float X, float Y, const FLinearColor& Color, float SpecPixels, float TextAlpha = 1.0f);
    void DrawSpecTextRight(const FString& Text, float RightX, float Y, const FLinearColor& Color, float SpecPixels, float TextAlpha = 1.0f);
    void DrawSpecTextCentered(const FString& Text, float CenterX, float Y, const FLinearColor& Color, float SpecPixels, float TextAlpha = 1.0f);
    FVector2D MeasureSpecText(const FString& Text, float SpecPixels) const;

    // Outline + weight pass for numbers that sit over the world. The outline
    // is tinted toward the number's own hue so it never reads as grey mud.
    void DrawOutlinedNumber(const FString& Text, float CenterX, float Y, const FLinearColor& Face, float SpecPixels, float TextAlpha);

    void DrawCrosshair(const FVector2D& Center, const FLinearColor& Color, float Size, float Thickness);
    void DrawTrack(float X, float Y, float Width, float Height, float Fraction, const FLinearColor& Fill, const FLinearColor& Track);
    // Hard-edged clockwise sweep from 12 o'clock, clipped to the square's own
    // boundary so it never spills past the plate edge.
    void DrawCooldownWedge(float X, float Y, float Size, float CoveredFraction, const FLinearColor& Color);
    // Code-drawn stand-ins for the commissioned ability marks, built to the
    // icon spec's construction notes. One stroke weight, one hue, no text.
    void DrawAbilityGlyph(const class UBreakerAbilityDefinition* Definition, float CenterX, float CenterY, float BoxSize, const FLinearColor& Color);
    void DrawAbilitySlot(const ABreakerCharacter* Character, const UBreakerAbilityComponent* Abilities,
        EBreakerAbilitySlot Slot, const FString& KeyHint, float X, float Y, float Size, const FLinearColor& Accent);
};
