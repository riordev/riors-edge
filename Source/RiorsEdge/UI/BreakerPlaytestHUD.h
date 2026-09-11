#pragma once

#include "CoreMinimal.h"
#include "Combat/BreakerCombatTypes.h"
#include "Fonts/SlateFontInfo.h"
#include "GameFramework/HUD.h"
#include "Progression/BreakerProgressionTypes.h"
// The class-resource row's resolved description. Pure, header-only, and the
// only part of the cluster that is testable without a viewport.
#include "UI/BreakerHUDResourceRow.h"
// The HUD sheet's timelines, world-free: crosshair spread, the health chip,
// the near-death pulse, the damage-number frame, the swap slide.
#include "UI/BreakerHUDMath.h"
// The event banners' queue, world-free: kinds, rectangles, holds, the stagger.
#include "UI/BreakerBannerQueue.h"
// Full include, not a forward declaration: FBreakerShotResult is a UFUNCTION
// parameter, so UHT needs the complete type (same reason
// BreakerMomentumComponent.h includes it).
#include "Weapons/BreakerWeaponComponent.h"
// The local map's marker row, held per frame (a TArray member needs the
// complete type).
#include "Game/BreakerLocalMapComponent.h"
#include "BreakerPlaytestHUD.generated.h"

class ABreakerCharacter;
class ABreakerSoundDirector;
class ABreakerTracerRenderer;
class UBreakerAbilityComponent;
class UBreakerAbilityStateComponent;
class UBreakerCombatComponent;
class UBreakerGameSettings;
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
    EBreakerElement Element = EBreakerElement::None;
    FGameplayTag DamageTypeTag;
    // A killing blow is the heaviest read on the screen and holds longer.
    bool bKilled = false;
    // A sibling hit from the same trigger pull on a DIFFERENT target — a chain
    // jump, a ricochet, an AoE's outer victims. Drawn lighter than the parent
    // so the aimed hit stays the loudest of its own family.
    bool bSecondary = false;
    // Per-class-of-hit lifetime, latched at push: DoT ticks die fast so they
    // never spam over gunfire, kills hold longest.
    float Lifetime = BreakerUI::MotionDamageRise;
};

// One enemy, reduced to a map blip. Collected during the enemy health-bar
// pass. Nothing on the HUD reads it; the fill continues until a consumer
// returns (see EnemyBlips below).
struct FBreakerHUDMapBlip
{
    FVector World = FVector::ZeroVector;
    bool bElite = false;
    bool bBoss = false;
};

// Which edge carries a plate's rail. Left is identity — which system owns
// this panel; Top is transient status, reserved for events and alerts. One
// rail per plate: a plate with two rails has no meaning.
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
    virtual void BeginPlay() override;
    virtual void DrawHUD() override;

    // DECLARED CROSSING (GLASS -> FIELD): the nameplate TU reads this. The
    // profile's larger-nameplates switch as a multiplier: one authored step
    // up (UBreakerGameSettings::LargerNameplateScale) or 1.
    float NameplateScale() const;
    const TArray<FBreakerHUDDamageNumber>& GetDamageNumbers() const { return DamageNumbers; }

private:
    // The player's profile, loaded once here and re-read on the pause menu's
    // open -> closed edge, which is the only place it is edited.
    UPROPERTY(Transient) TObjectPtr<UBreakerGameSettings> Profile;
    bool bMenuWasOpen = false;

    // Bound once to the player's combat component; dodge and block are
    // instantaneous results, so they have to be latched when broadcast
    // rather than polled from a persistent state.
    UFUNCTION() void HandlePlayerDamageReceived(const FBreakerDamageResult& Result);
    // The victim-side context: the same hit, with WHO dealt it. This is the
    // hit tell's feed — OnDamageReceived hands out a bare result and cannot
    // say where the hit came from.
    UFUNCTION() void HandlePlayerDamageTaken(const struct FBreakerHitContext& Context);
    UFUNCTION() void HandleLevelGained(int32 NewLevel, int32 LevelsGained);
    void EnsureProgressionBinding(const ABreakerCharacter* Character);
    // Levelling is the pause plate's (O210); the field carries only the
    // level-up banner, through the queue below.
    void EnsureDamageBinding(const ABreakerCharacter* Character);

    // THE BANNERS (04-death-banners, O202). Every event banner — rift
    // complete, level up, wave clear — is enqueued here and drawn by one pass
    // in priority order at staggered arrivals. Strings are latched at
    // enqueue: a rift's name is cleared by the teardown its banner outlives.
    TArray<FBreakerBanner> Banners;
    void EnqueueBanner(EBreakerBannerKind Kind, const FString& Title, const FString& Line);
    void DrawBanners(const FVector2D& Center);
    // The wave countdown seen last frame: a wave clear is the <0 -> >0 edge
    // of GetWaveAdvanceRemaining(), the moment the next wave starts counting.
    float LastWaveAdvanceRemaining = -1.0f;

    // THE LOOP'S ENDING, MARKED. `OnRiftCompleted` is GROUND's published seam
    // (declared in Game/BreakerRiftDefinition.h so a consumer needs that header
    // and not the 500-line game-mode one), and LEDGER's payout was its ONLY
    // binder — a rift could be entered, fought, terminated and PAID with
    // nothing on screen saying it had ended. Bound weakly to the game mode,
    // rebinding when the mode changes, exactly as the component seams above
    // rebind on their component.
    void EnsureRiftBinding();
    void HandleRiftCompleted(const struct FBreakerRiftDefinition& Rift, APawn* Player);
    TWeakObjectPtr<class ABreakerGameMode> BoundRiftMode;

    // Same bind/rebind discipline for shots: the tracer trail is the only
    // record of a hitscan line, and polling GetLastShot() would miss every
    // shot fired faster than one per frame.
    UFUNCTION() void HandlePlayerShot(const FBreakerShotResult& Shot);
    // The universal damage feed: every hit the player deals, from any source.
    UFUNCTION() void HandlePlayerHitDealt(const struct FBreakerHitContext& Hit);
    void EnsureWeaponBinding(const ABreakerCharacter* Character);

    // Instant activations can finish inside their own frame, so each readout below
    // is driven by this latch rather than by a per-frame query.
    UFUNCTION() void HandleAbilityActivated(EBreakerAbilitySlot Slot);
    void EnsureAbilityBinding(const ABreakerCharacter* Character);

    void DrawDefenseFeedback(const FVector2D& Center);
    // The near-death frame: a full-screen harm border pulsing 8→16 px under
    // 20 % health, with four corner brackets that do not pulse.
    // Returns the height it consumed, so the stack above it knows where it ends.
    float DrawStatusReadout(const ABreakerCharacter* Character, float X, float BottomY, float Width);
    // Vitals, bottom-left: value + max, the shield layer, the health bar with
    // its chip and 20 % tick, and the resource track beneath.
    void DrawVitals(const ABreakerCharacter* Character);
    // The three ability tiles, bottom-centre: 64 / 88 / 64.
    void DrawAbilityCluster(const ABreakerCharacter* Character);
    // Magazine, reserve, the ammo rail at rest, and the name only on a swap.
    void DrawWeaponReadout(const ABreakerCharacter* Character);
    // The class-resource slot, in two halves: which resource this character
    // has (one component read, no lookup, no iteration) and how the resolved
    // row is painted into the fixed 8px track.
    static BreakerHUD::FResourceRow ResolveResourceRow(const ABreakerCharacter* Character);
    void DrawResourceTrack(const BreakerHUD::FResourceRow& Row, float X, float Y, float Width, float Height);
    // Top-left: the zone name and the mm:ss countdown to the next wave.
    void DrawZoneLine(const ABreakerCharacter* Character);
    // Top-right: the tracked quest as one right-aligned line.
    void DrawQuestLine(const ABreakerCharacter* Character);
    // The local map's markers, walked once a frame. GetMarkers is four actor
    // walks and a sort; the quest line and the world labels both read it, and
    // a draw that asked three times a frame was three walks for one answer.
    const TArray<FBreakerLocalMapMarker>& MarkersThisFrame(const ABreakerCharacter* Character);
    TArray<FBreakerLocalMapMarker> FrameMarkers;
    uint64 FrameMarkersStamp = 0;
    // The crosshair's hit / kill / weak-point marks, on the arrival clock.
    void DrawCrosshairMarks(const FVector2D& Center);
    // The hit tells: one harm arc per source outside the crosshair, on the
    // bearing the damage came from, swinging with the camera as the player
    // turns. Fed by HandlePlayerDamageTaken; the arithmetic is
    // BreakerHUDMath's.
    void DrawHitTells(const FVector2D& Center);
    TArray<BreakerHUDMath::FHitTell> HitTells;
    // ---- Density instruments (see DrawHUD) --------------------------------
    bool bHudStressSpawned = false;
    double HudCostWindowStart = 0.0;
    double HudCostAccumMs = 0.0;
    double HudCostMaxMs = 0.0;
    double HudCostBarsMs = 0.0;
    double HudCostNumbersMs = 0.0;
    int32 HudCostFrames = 0;
    // Top-left playtest chrome: key legend, the F3 diagnostics plate, world
    // diagnostic labels, and the report-copied toast. Factored out because the
    // Anchor's trimmed HUD keeps exactly this block and nothing else of the
    // combat chrome.
    void DrawPlaytestInstrumentation(const ABreakerCharacter* Character, const FVector2D& Center);
    // Rounds in flight are no longer drawn here at all. The HUD records the
    // shot and hands it to a world-space pooled renderer, spawned lazily on
    // the first shot and never replicated; see BreakerTracerRenderer.h.
    ABreakerTracerRenderer* GetTracerRenderer();
    // The interact prompt, resolving loot > travel > talk in exactly F's own
    // precedence. Loot's plate is DrawLootPickups' (the key tile on the one
    // F would take); this draws the plate over the NPC or travel point, and
    // the BACKPACK FULL line when the loot refusal has to be said.
    void DrawInteractPrompt(const ABreakerCharacter* Character, const FVector2D& Center);
    // One plate over a thing F acts on, to BreakerHUDMath::InteractPlateLayout:
    // rail, key tile (only when bKeyTile), tally cells in the rail colour, the
    // name. Centred on CenterX with its bottom edge on BottomY, screen pixels.
    void DrawInteractPlate(float CenterX, float BottomY, const FLinearColor& Rail, int32 TallyCells,
        const FString& Name, bool bKeyTile);
    // The game's three sounds ride the same two events the visuals already
    // ride (OnShot, OnHitDealt), through the same lazily-spawned cosmetic
    // sibling; see BreakerSoundDirector.h.
    ABreakerSoundDirector* GetSoundDirector();
    // Plays the hit-confirm on the arrival clock: now for a zero delay, through
    // a one-shot timer for a round still in flight. A KILL PLAYS THE SAME
    // CONFIRM — "no death sound for now" (2026-08-26) retired the sting, and
    // the fall-through is the ruling: a silent killing shot would remove the
    // feedback that it landed at all. bKill is retained and unread; see the
    // definition.
    void ScheduleArrivalSound(float DelaySeconds, bool bKill);
    void DrawDamageNumbers();
    // DEFINED IN Combat/BreakerEnemyHealthBars.cpp, not beside its siblings
    // here. The bar answers a combat question -- which enemy am I fighting,
    // how close is it to dead -- and two lanes editing it inside this file on
    // the same day is what moved it out. The declaration stays because it is
    // still a member drawing on this HUD's canvas; only the body left.
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
    // FIELDPLATE HUD §5: violet frame, edge bands, title plate, step-down.
    void DrawUltimateTreatment(const ABreakerCharacter* Character);
    void DrawMarkedTarget(const ABreakerCharacter* Character);

    UPROPERTY() TObjectPtr<UBreakerCombatComponent> BoundCombat;
    UPROPERTY() TObjectPtr<class UBreakerProgressionComponent> BoundProgression;
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
    // The keystone variant last announced for each slot. A keystone rewrite is
    // bought long after the teaching callout has retired itself, so without
    // this the rewrite would announce itself NEVER — the player's build would
    // change their ultimate in silence. Comparing against the last announced
    // name buys exactly one more callout on the cast after the rewrite first
    // resolves, and none thereafter.
    FString SlotLastVariantName[AbilitySlotCount];

    // The trash-bar focus linger (selective bars, ruled): when the aim last
    // left each enemy, so its bar fades instead of blinking.
    //
    // DECLARED CROSSING (FIELD -> GLASS). Written and read only by
    // Combat/BreakerEnemyHealthBars.cpp, which owns the bar. This was
    // LastFocusBarEnemy + LastFocusBarTime — ONE slot — and one slot meant a
    // crosshair sweep across a pack overwrote the previous body's clock the
    // same frame, so only the most recently released enemy could fade and
    // only until the next was focused. Per-enemy now, pruned every frame by
    // the same clock that reads it, so it cannot grow.
    TMap<TWeakObjectPtr<const class ABreakerEnemy>, double> FocusBarReleaseTimes;
    // The health each enemy's bar is SHOWING, for the chip hatch on a drop.
    //
    // DECLARED CROSSING (FIELD -> GLASS), the fourth member O155 names.
    // Written and read only by Combat/BreakerEnemyHealthBars.cpp, which owns
    // the bar; pruned by the same pass that prunes FocusBarReleaseTimes, so it
    // cannot grow. The chip's arithmetic is BreakerHUDMath's so the player's
    // bar and the enemy's bar cannot disagree about a hold or a recovery.
    TMap<TWeakObjectPtr<const class ABreakerEnemy>, BreakerHUDMath::FHealthChip> ShownEnemyHealth;

    // --- Crosshair state ----------------------------------------------------
    // The tick gap in spec pixels, following the weapon's cone through
    // BreakerHUDMath::CrosshairGapFollow. Persisted so the 60/200 ms travel is
    // a rate, not a snap.
    float CrosshairGapPx = BreakerUI::HudCrosshairGapRest;
    // When the aim state last flipped, for the 80 ms ADS collapse.
    double AdsChangeTime = -1000.0;
    bool bWasAiming = false;

    // --- The player's own health chip ---------------------------------------
    BreakerHUDMath::FHealthChip HealthChip;
    // The health fraction drawn last frame, so a drop is detected by the HUD
    // rather than needing a seam from Combat/.
    float HealthShownFraction = 1.0f;

    // --- Riftglass gains ----------------------------------------------------
    // Fed the wallet balance each frame by DrawVitals; prints "+N RIFTGLASS"
    // for a hold after it rises. The gain, never the balance.
    BreakerHUDMath::FBreakerWalletGainReadout WalletGain;

    // --- The weapon name on swap -------------------------------------------
    // Latched on the falling edge of IsSwapping(): the name that arrived.
    double SwapStartTime = -1000.0;
    bool bWasSwapping = false;
    FString SwapName;

    // Latched from the session's PendingRift each frame it is set, so the line
    // survives the frame the rift is torn down.
    FString ZoneName;

    // -BreakerCaptureHUD forcing: which of reload / swap / cooldown / loot
    // plate the current preview phase holds on, so a capture run photographs
    // all four.
    int32 PreviewPhase = 0;
    float PreviewCooldownFraction = 0.0f;
    bool bPreviewReload = false;
    // The reload's ramp while the preview holds it, 0..1 over the phase.
    float PreviewReloadFraction = 0.0f;
    // The loot plate over a fabricated Aberrant drop ahead of the player,
    // preview-only: the harness cannot make a drop fall.
    bool bPreviewLootPlate = false;

    UPROPERTY() TObjectPtr<ABreakerTracerRenderer> TracerRenderer;
    UPROPERTY() TObjectPtr<ABreakerSoundDirector> SoundDirector;
    // Every round the player has fired, used only to decide which of them get
    // a visible streak. Never reset: the modulo is what matters, not the count.
    int32 RoundsFired = 0;

    static constexpr int32 MaxDamageNumbers = 24;
    TArray<FBreakerHUDDamageNumber> DamageNumbers;
    // The write cursor that used to live here is gone. It evicted in insertion
    // order where art-and-ui says the cap culls oldest-first, and the two are
    // not the same thing once merges refresh a number in place: the busiest
    // number on the player's target was the first one dropped. Eviction is now
    // BreakerDamageFeed::IndexToEvict, which prefers an expired slot and
    // otherwise takes the genuinely oldest.

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
    // The absorbed read, latched per hit off the mitigated fraction so the
    // one unified crosshair tick can tell all three of its states.
    bool bHitDealtAbsorbed = false;
    double LastKillConfirmTime = -1000.0;
    bool bKillConfirmWeakPoint = false;

    // The last non-DoT damage-number SPAWN, used to mark same-instant siblings
    // on other targets as secondary (chain / ricochet / AoE spill).
    double LastSiblingSpawnTime = -1000.0;
    TWeakObjectPtr<AActor> LastSiblingSpawnTarget;

    // One ring-buffer push, shared by the live feed and the capture preview so
    // the two can never disagree about how a number enters the pool.
    void PushDamageNumber(const FBreakerHUDDamageNumber& Number);

    // Filled once per frame by DrawEnemyHealthBars. A member rather than a
    // local so the allocation happens on the first few frames and never
    // again: DrawHUD runs every frame and a TArray built in it is a per-frame
    // allocation by definition.
    //
    // THE TWO HALVES HAVE DIFFERENT OWNERS (O155). The fill is the COMBAT
    // lane's, in Combat/BreakerEnemyHealthBars.cpp. NOTHING READS IT: the
    // minimap that consumed it was deleted (02-hud: enemy state is read off
    // the enemy), and this is the consumer end of that crossing, GLASS ->
    // FIELD. The fill continues until a consumer returns, because stopping it
    // is the producer's edit, not this lane's. Changing its shape, its meaning
    // or its fill order remains a declared crossing.
    TArray<FBreakerHUDMapBlip> EnemyBlips;
    // Current-frame canvas pixels, populated before damage-number placement.
    TArray<FBox2D> EnemyPlateBounds;
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

    // Flat fill, 1px border, one 4px identity rail. No gradient, no blur, no
    // radius: Canvas cannot round a corner and the system's 2px radius is
    // below the threshold where its absence reads as wrong.
    // A transparent Face means "the HUD's default plate face" (bg/base at the
    // readability alpha) rather than an actual transparent plate.
    void DrawPlate(float X, float Y, float Width, float Height, const FLinearColor& Rail,
        EBreakerRail RailEdge = EBreakerRail::Left, const FLinearColor& Face = FLinearColor::Transparent);
    void DrawBorder(float X, float Y, float Width, float Height, const FLinearColor& Color, float Thickness);
    void DrawTriangle(const FVector2D& A, const FVector2D& B, const FVector2D& C, const FLinearColor& Color);
    void DrawAbilityRecoveryDisc(const FVector2D& Center, float Radius, float Fraction, const FLinearColor& Color);
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
    enum class ESpecFontRole : uint8 { Body, Display, Mono };
    const UFont* GetSpecFont(ESpecFontRole FontRole);
    bool CanDrawSpecFont(ESpecFontRole FontRole);
    FSlateFontInfo MakeSpecFont(float SpecPixels, ESpecFontRole FontRole);
    UPROPERTY() TObjectPtr<const UFont> SpecFont;
    UPROPERTY() TObjectPtr<const UFont> SpecDisplayFont;
    UPROPERTY() TObjectPtr<const UFont> SpecMonoFont;

    // Text authored in spec pixels. Y is the top of the line, matching Canvas.
    void DrawSpecText(const FString& Text, float X, float Y, const FLinearColor& Color, float SpecPixels, float TextAlpha = 1.0f, ESpecFontRole FontRole = ESpecFontRole::Body);
    void DrawSpecTextRight(const FString& Text, float RightX, float Y, const FLinearColor& Color, float SpecPixels, float TextAlpha = 1.0f, ESpecFontRole FontRole = ESpecFontRole::Body);
    void DrawSpecTextCentered(const FString& Text, float CenterX, float Y, const FLinearColor& Color, float SpecPixels, float TextAlpha = 1.0f, ESpecFontRole FontRole = ESpecFontRole::Body);
    FVector2D MeasureSpecText(const FString& Text, float SpecPixels, ESpecFontRole FontRole = ESpecFontRole::Body);
    // The largest size at or below DesiredPixels at which Text measures no
    // wider than MaxWidth, never below MinPixels. MaxWidth is derived from the
    // MEASUREMENT of a different string, never from this widget's own
    // arrangement, so — like every other measured fit in this codebase — it is
    // a pure function of inputs known before layout and cannot oscillate.
    float FitSpecPixels(const FString& Text, float DesiredPixels, float MaxWidth, float MinPixels, ESpecFontRole FontRole = ESpecFontRole::Body);

    // Outline + weight pass for numbers that sit over the world. The outline
    // is tinted toward the number's own hue so it never reads as grey mud.
    void DrawOutlinedNumber(const FString& Text, float CenterX, float Y, const FLinearColor& Face, float SpecPixels, float TextAlpha);

    // The four-tick crosshair with its spread gap, blending to the ADS dot
    // and ring. Sizes are already scaled by the caller.
    void DrawCrosshair(const FVector2D& Center, float GapPx, float AdsBlend);
    void DrawTrack(float X, float Y, float Width, float Height, float Fraction, const FLinearColor& Fill, const FLinearColor& Track);
    // Flat 135° stripes: A for HudHatchStripe of every HudHatchPeriod, B for
    // the rest. The only texture the system has; it means disabled, locked,
    // or the chip of a value that was just lost.
    void DrawHatch(float X, float Y, float Width, float Height, const FLinearColor& A, const FLinearColor& B,
        float Period, float Stripe);
    // Code-drawn stand-ins for the commissioned ability marks, built to the
    // icon spec's construction notes. One stroke weight, one hue, no text.
    void DrawAbilityGlyph(const class UBreakerAbilityDefinition* Definition, float CenterX, float CenterY, float BoxSize, const FLinearColor& Color);
    void DrawAbilitySlot(const ABreakerCharacter* Character, const UBreakerAbilityComponent* Abilities,
        EBreakerAbilitySlot Slot, const FString& KeyHint, float X, float Y, float Size, float MarkSize, const FLinearColor& Accent);
};
