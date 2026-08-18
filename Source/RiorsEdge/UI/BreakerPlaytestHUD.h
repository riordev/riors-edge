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
class ABreakerTracerRenderer;
class UBreakerAbilityComponent;
class UBreakerAbilityStateComponent;
class UBreakerCombatComponent;
class UBreakerWeaponComponent;

// One floating damage number. Also plain: it outlives the target it came from.
struct FBreakerHUDDamageNumber
{
    FVector World = FVector::ZeroVector;
    float Value = 0.0f;
    bool bCritical = false;
    bool bWeakPoint = false;
    // How much of the hit disappeared into mitigation, 0..1, taken from
    // FBreakerDamageResult at the moment the shot resolved: 1 - Mitigated/Raw.
    // Latched rather than looked up, because the number outlives the hit and
    // the target's armour is a facing-dependent value that has already moved
    // by the time this is drawn.
    float MitigatedFraction = 0.0f;
    double Time = -1000.0;
    // Who was hit, so repeated hits on the same target inside the merge window
    // accumulate into one number instead of spawning eight for one shotgun
    // spread or one per DoT tick per target.
    TWeakObjectPtr<AActor> Target;
    // A DoT tick reads differently from a strike and must not merge into one.
    bool bFromDoT = false;
    // A killing blow is the heaviest read on the screen and holds longer; the
    // overkill share is carried separately so it can be printed as its own
    // distinct mark rather than silently inflating the number.
    bool bKilled = false;
    float Overkill = 0.0f;
    // A sibling hit from the same trigger pull on a DIFFERENT target — a chain
    // jump, a ricochet, an AoE's outer victims. Drawn lighter than the parent
    // so the aimed hit stays the loudest of its own family.
    bool bSecondary = false;
    // Per-class-of-hit lifetime, latched at push: DoT ticks die fast so they
    // never spam over gunfire, kills hold longest.
    float Lifetime = 0.56f;
};

// One enemy, reduced to what the minimap needs. Collected during the enemy
// health-bar pass so the minimap costs no second actor iteration.
struct FBreakerHUDMapBlip
{
    FVector World = FVector::ZeroVector;
    bool bElite = false;
    bool bBoss = false;
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
    UFUNCTION() void HandleLevelGained(int32 NewLevel, int32 LevelsGained);
    void EnsureProgressionBinding(const ABreakerCharacter* Character);
    // The experience rail: a thin bar under the combat cluster carrying the
    // level, the fraction into it, and the XP remaining. Progression that only
    // exists in a save file is progression the player cannot feel.
    void DrawExperienceRail(const ABreakerCharacter* Character);
    void DrawLevelUpBanner(const FVector2D& Center);
    void EnsureDamageBinding(const ABreakerCharacter* Character);

    // Same bind/rebind discipline for shots: the tracer trail is the only
    // record of a hitscan line, and polling GetLastShot() would miss every
    // shot fired faster than one per frame.
    UFUNCTION() void HandlePlayerShot(const FBreakerShotResult& Shot);
    // The universal damage feed: every hit the player deals, from any source.
    UFUNCTION() void HandlePlayerHitDealt(const struct FBreakerHitContext& Hit);
    void EnsureWeaponBinding(const ABreakerCharacter* Character);

    // Activations are instantaneous and leave no polled state (Skim in
    // particular finishes inside its own frame), so every ability readout below
    // is driven by this latch rather than by a per-frame query.
    UFUNCTION() void HandleAbilityActivated(EBreakerAbilitySlot Slot);
    void EnsureAbilityBinding(const ABreakerCharacter* Character);

    void DrawDefenseFeedback(const FVector2D& Center);
    // Crosshair kill confirm: an eight-point burst, distinct from the hit
    // ticks by geometry (through-centre strokes, expanding) as well as colour.
    void DrawKillConfirm(const FVector2D& Center);
    // Stepped screen-edge bands in the harm accent when health runs low.
    // Solid fills only — FIELDPLATE has no gradients, so the "vignette" is
    // two nested full-bleed frames, and urgency is a blink, not a fade.
    void DrawLowHealthCue(const ABreakerCharacter* Character);
    void DrawStatusReadout(const ABreakerCharacter* Character, float X, float BottomY);
    void DrawVitalsPlate(const ABreakerCharacter* Character, float X, float BottomY);
    void DrawCombatCluster(const ABreakerCharacter* Character, float X, float Y, float Width, float Height);
    // §2's class-resource slot, in two halves: which resource this character
    // has (one component read, no lookup, no iteration) and how the resolved
    // row is painted into the fixed 12px track.
    static BreakerHUD::FResourceRow ResolveResourceRow(const ABreakerCharacter* Character);
    void DrawResourceTrack(const BreakerHUD::FResourceRow& Row, float X, float Y, float Width, float Height);
    void DrawWaveBanner(const FVector2D& Center);
    // Top-right field plate. Cheap by construction: it consumes EnemyBlips,
    // which DrawEnemyHealthBars has already filled from the one enemy
    // iteration the HUD was making anyway, and allocates nothing per frame.
    void DrawMinimap(const ABreakerCharacter* Character, float X, float Y, float Width, float Height);
    // Compact quest panel directly below the minimap, on EVERY map: the active
    // quest's name and, by state, either a directive line (Offered / ready to
    // turn in) or its objectives with live counters. Reads the pawn's
    // UBreakerQuestJournal through the pure quest-library helpers, so it can
    // never disagree with the dialogue system about a quest's state.
    void DrawQuestTracker(const ABreakerCharacter* Character, float X, float Y, float Width);
    // Top-left playtest chrome: key legend, the F3 diagnostics plate, world
    // diagnostic labels, and the report-copied toast. Factored out because the
    // Anchor's trimmed HUD keeps exactly this block and nothing else of the
    // combat chrome.
    void DrawPlaytestInstrumentation(const ABreakerCharacter* Character, const FVector2D& Center);
    // Rounds in flight are no longer drawn here at all. The HUD records the
    // shot and hands it to a world-space pooled renderer, spawned lazily on
    // the first shot and never replicated; see BreakerTracerRenderer.h.
    ABreakerTracerRenderer* GetTracerRenderer();
    void DrawDamageNumbers();
    void DrawEnemyHealthBars(const ABreakerCharacter* Character);
    void DrawLootPickups(const ABreakerCharacter* Character);
    // Overhead floating labels for the friendly interactables — every
    // ABreakerNPC prints its name in the warm person accent, every
    // ABreakerTravelPoint prints TRAVEL in rift-teal — in the same
    // project-and-DrawSpecTextCentered idiom as the enemy labels above. This
    // is what makes Kess, the Quartermaster and the gate read as INTERACTIVE
    // from across the plaza rather than only inside F-prompt range (owner
    // playtest 2026-08-17).
    void DrawInteractableLabels(const ABreakerCharacter* Character);

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
    UPROPERTY() TObjectPtr<class UBreakerProgressionComponent> BoundProgression;
    // Latched from OnLevelGained. A level-up is the single most earned moment
    // in the progression loop and it gets its own banner rather than sharing
    // the ability callout, which retires itself after three shows.
    double LevelUpTime = -1000.0;
    int32 LevelUpShownLevel = 0;
    int32 LevelUpShownGain = 0;
    // What the level actually granted, stated on the banner: "+1 CLASS +1
    // CORE". Computed from the cap levels at the moment the event fired, so a
    // level past a cap never claims a point it did not pay.
    int32 LevelUpClassGain = 0;
    int32 LevelUpCoreGain = 0;
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
    // The keystone variant last announced for each slot. A keystone rewrite is
    // bought long after the teaching callout has retired itself, so without
    // this the rewrite would announce itself NEVER — the player's build would
    // change their ultimate in silence. Comparing against the last announced
    // name buys exactly one more callout on the cast after the rewrite first
    // resolves, and none thereafter.
    FString SlotLastVariantName[AbilitySlotCount];
    // Latched separately from the callout: the crosshair burst fires on every
    // Skim, not only the first three.
    double SkimBurstTime = -1000.0;

    UPROPERTY() TObjectPtr<ABreakerTracerRenderer> TracerRenderer;
    // Every round the player has fired, used only to decide which of them get
    // a visible streak. Never reset: the modulo is what matters, not the count.
    int32 RoundsFired = 0;

    static constexpr int32 MaxDamageNumbers = 24;
    TArray<FBreakerHUDDamageNumber> DamageNumbers;
    int32 NextDamageNumberIndex = 0;

    // How much of the last landed shot was absorbed, latched in the shot
    // handler. Drives the crosshair's third tick state — the read the owner is
    // missing when a Warden's frontal armour eats a hit.
    float LastShotMitigatedFraction = 0.0f;
    double LastShotHitTime = -1000.0;

    // Crosshair confirm latches, fed by HandlePlayerHitDealt so an ability's
    // cleave confirms at the crosshair exactly as a bullet does. DoT ticks are
    // deliberately excluded from all three — a Bleed on three targets would
    // strobe the crosshair forever over nothing the player just did.
    double LastHitDealtTime = -1000.0;
    bool bHitDealtWeakPoint = false;
    double LastKillConfirmTime = -1000.0;
    bool bKillConfirmWeakPoint = false;

    // The last non-DoT damage-number SPAWN, used to mark same-instant siblings
    // on other targets as secondary (chain / ricochet / AoE spill).
    double LastSiblingSpawnTime = -1000.0;
    TWeakObjectPtr<AActor> LastSiblingSpawnTarget;

    // One ring-buffer push, shared by the live feed and the capture preview so
    // the two can never disagree about how a number enters the pool.
    void PushDamageNumber(const FBreakerHUDDamageNumber& Number);

    // Filled once per frame by DrawEnemyHealthBars, consumed by DrawMinimap.
    // A member rather than a local so the allocation happens on the first few
    // frames and never again: DrawHUD runs every frame and a TArray built in
    // it is a per-frame allocation by definition.
    TArray<FBreakerHUDMapBlip> EnemyBlips;
    // Screen-space rectangles (CentreX, TopY, Width, Height) already claimed by
    // an enemy label this frame, so a second enemy projecting to nearly the
    // same point does not print through the first. Rebuilt every frame.
    TArray<FVector4> DrawnLabelBounds;

    // -BreakerCaptureHUD. Dev-only, command-line, and the reason it exists is
    // that the states this HUD gets WRONG are the states a headless capture run
    // cannot reach: nothing presses F4 to start a wave and nothing pulls a
    // trigger, so the wave banner and every damage number were unphotographable
    // and both shipped broken. Resolved once, never per frame.
    bool IsCapturePreview() const;
    void TickCapturePreview(const ABreakerCharacter* Character);
    double LastPreviewSpawnTime = -1000.0;

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
    //
    // Non-const because the font is loaded on first use, and guarded because
    // FCanvasTextItem draws NOTHING when its UFont is null — which is exactly
    // how a whole HUD's worth of text once disappeared.
    const UFont* GetSpecFont();
    bool CanDrawSpecFont();
    FSlateFontInfo MakeSpecFont(float SpecPixels);
    UPROPERTY() TObjectPtr<const UFont> SpecFont;

    // Text authored in spec pixels. Y is the top of the line, matching Canvas.
    void DrawSpecText(const FString& Text, float X, float Y, const FLinearColor& Color, float SpecPixels, float TextAlpha = 1.0f);
    void DrawSpecTextRight(const FString& Text, float RightX, float Y, const FLinearColor& Color, float SpecPixels, float TextAlpha = 1.0f);
    void DrawSpecTextCentered(const FString& Text, float CenterX, float Y, const FLinearColor& Color, float SpecPixels, float TextAlpha = 1.0f);
    FVector2D MeasureSpecText(const FString& Text, float SpecPixels);
    // The largest size at or below DesiredPixels at which Text measures no
    // wider than MaxWidth, never below MinPixels. MaxWidth is derived from the
    // MEASUREMENT of a different string, never from this widget's own
    // arrangement, so — like every other measured fit in this codebase — it is
    // a pure function of inputs known before layout and cannot oscillate.
    float FitSpecPixels(const FString& Text, float DesiredPixels, float MaxWidth, float MinPixels);

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
