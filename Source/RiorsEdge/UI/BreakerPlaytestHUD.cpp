#include "UI/BreakerPlaytestHUD.h"
#include "Interaction/BreakerFeedstockPickup.h"
#include "Game/BreakerLocalMapComponent.h"
#include "Game/BreakerPrototypeDestinations.h"
#include "Combat/BreakerStatusCycleComponent.h"

#include "UI/BreakerDamageFeed.h"

#include "Abilities/BreakerAbilityComponent.h"
#include "Abilities/BreakerAbilityDefinition.h"
#include "Abilities/BreakerAbilityStateComponent.h"
#include "Abilities/BreakerGameplayAbility.h"
#include "Abilities/BreakerSkillLevelMath.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Progression/BreakerProgressionComponent.h"
// Cap levels only, for stating what a level actually granted on the banner.
#include "Progression/BreakerProgressionLibrary.h"
#include "Characters/BreakerCharacter.h"
#include "Classes/BreakerChargeComponent.h"
#include "Classes/BreakerGritComponent.h"
#include "Classes/BreakerManaComponent.h"
#include "Classes/BreakerMomentumComponent.h"
#include "Classes/BreakerScrapComponent.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "UI/BreakerUIStyle.h"
#include "UI/BreakerRiftFeedback.h"
#include "UI/BreakerTracerMath.h"
#include "UI/BreakerTracerRenderer.h"
#include "UI/BreakerEffectRenderer.h"
#include "Components/CapsuleComponent.h"
#include "Audio/BreakerSoundDirector.h"
#include "Weapons/BreakerWeaponComponent.h"
#include "Weapons/BreakerWeaponDefinition.h"
#include "Items/BreakerEquipmentComponent.h"
#include "Playtest/BreakerPlaytestComponent.h"
#include "Combat/BreakerTargetDummy.h"
#include "Combat/BreakerEnemy.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerStatusComponent.h"
#include "Interaction/BreakerNPC.h"
#include "Interaction/BreakerFernhallCache.h"
#include "Interaction/BreakerSupplyChest.h"
#include "Interaction/BreakerBasinRecorder.h"
#include "Interaction/BreakerCoastalUplink.h"
#include "Interaction/BreakerTravelPoint.h"
#include "Game/BreakerGameInstance.h"
#include "Game/BreakerGameMode.h"
#include "Game/BreakerRiftDefinition.h"
// Every word the HUD draws for the player is a row in Data/strings.json
// (O195); the literals left in this file are dev instruments and fixtures.
#include "Data/BreakerStrings.h"
// Quest line: definitions and the pure state helpers (read-only — the HUD
// derives, never writes). The journal type itself comes through
// BreakerQuestContent.h's own include.
#include "Save/BreakerQuestContent.h"
// The mission tracker: the current beat's line outranks the quest line (O195).
#include "Save/BreakerMissionContent.h"
#include "Interaction/BreakerSurvivor.h"
// The player's profile: damage-number scale and the larger-nameplate switch.
#include "Settings/BreakerGameSettings.h"
#include "EngineUtils.h"
#include "AbilitySystemComponent.h"
#include "Items/BreakerItemTypes.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
// The resource mark and the momentum track's chevron blocks are triangles;
// Canvas has no shape primitive for either, so they go through CanvasItem.
#include "CanvasItem.h"
#include "CanvasTypes.h"
#include "Engine/Texture2D.h"
// Canvas text goes through Slate's font path so it rasterises at the size it
// is asked for rather than magnifying a bitmap face.
#include "Framework/Application/SlateApplication.h"
#include "Fonts/FontMeasure.h"
#include "Engine/Font.h"
#include "TextureResource.h"

// INTEGRATION: ABreakerLootPickup is being authored in parallel. The hover
// popup compiles out cleanly until that header lands, so this file never
// blocks on a merge it does not own.
#if defined(__has_include)
#  if __has_include("Items/BreakerLootPickup.h")
#    include "Items/BreakerLootPickup.h"
#    define BREAKER_HAS_LOOT_PICKUP 1
#  endif
#endif
#ifndef BREAKER_HAS_LOOT_PICKUP
#  define BREAKER_HAS_LOOT_PICKUP 0
#endif

// ---------------------------------------------------------------------------
// The combat HUD, to Assets/design/02-hud/spec.md. Every colour comes from
// BreakerUI; every geometry value is a BreakerUI::Hud* token authored in the
// sheet's 1920x1080 pixels and scaled once by S(). Positions never change
// between states; only content does. The timelines are BreakerHUDMath's.
// ---------------------------------------------------------------------------
namespace BreakerHUD
{
    // The HUD's default plate face: bg/base, near-opaque. The system is
    // stamped metal, not glass — a translucent plate over a bright sky reads
    // as mud, and the flat-fill rule exists to stop exactly that.
    static const FLinearColor PlateFace = BreakerUI::Alpha(BreakerUI::BgBase, 0.96f);

    // --- Rounds in flight ---------------------------------------------------
    // The HUD no longer DRAWS a round. It records one, decides whether this
    // one gets a visible streak at all, and hands both facts to
    // ABreakerTracerRenderer, which puts real primitives in the world where
    // the depth buffer can occlude them. See BreakerTracerRenderer.h for why
    // that move happened; the canvas version is gone, not disabled.
    // Flight figures come from the shared LIVE copy (Breaker.Tracer.*
    // console tuning), never a private const one — a tuned speed must move
    // the scheduled spark exactly as it moves the drawn streak.

    // A number's life is BreakerHUDMath::DamageNumberLifetime — the sheet's
    // 700 ms rise, plus the crit hold — resolved at push time per kind.

    // §4: numbers within 60px of one another stack at 8px offsets, and a
    // fourth simultaneous number in the same cluster is dropped, not drawn.
    //
    // Both distances are expressed as RATIOS of the body-number size rather
    // than as the spec's raw pixels. At the spec's 40px body they evaluate to
    // exactly 60 and 8, so nothing about the authored look changes — but the
    // damage sizes are being retuned in BreakerUIStyle.h, and a stack offset
    // that stays at 8px while the glyphs shrink turns a tidy stack into a
    // pile. A cluster rule that does not scale with its own type is a bug
    // waiting for the next token edit.
    static constexpr float DamageClusterRadiusRatio = 1.5f;   // 60 / 40
    static constexpr float DamageClusterOffsetRatio = 0.2f;   // 8 / 40
    static constexpr float DamageClusterRadius =
        BreakerUI::DamageBodyPixels * DamageClusterRadiusRatio;
    static constexpr int32 DamageClusterMax = 3;

    // The merge windows moved to UI/BreakerDamageFeed.h, which owns both of
    // them because the spec authors two — direct and damage-over-time — and one
    // shared 0.18s could satisfy neither. It sat above the Sidearm's 0.143s
    // semi-automatic interval, so two deliberate shots merged into one number,
    // and below the Bleed's 0.5s tick, so a damage-over-time effect could never
    // merge at all.

    // --- Damage-number hierarchy timings/magnitudes. All presentation, all
    // tuned by eye from capture screenshots. O2 PLACEHOLDER, every one.
    // DoT ticks die fast (they recur forever; a long tail is spam), kills hold
    // longest (the one number worth reading after the fight moves on).
    // Must outlive the DoT merge window or the window cannot be used: at 0.35s
    // against a 0.5s Bleed tick, each tick's number was already dead when its
    // successor arrived, so one Bleed printed six numbers across its 3.0s life.
    // Derived from the window rather than set beside it, so the two cannot
    // drift back apart.
    static constexpr float DamageDoTLifetime = BreakerDamageFeed::MinimumDoTLifetime;
    // Longer than a crit's 1.1 s: the kill is the one number worth reading
    // after the fight moves on.
    static constexpr float DamageKillLifetime = 1.25f;   // O2 PLACEHOLDER
    // Two non-DoT numbers born this close together on DIFFERENT targets are
    // one trigger pull spilling over — chain, ricochet, AoE. The later ones
    // are secondary and draw lighter than the parent.
    static constexpr float DamageSecondaryWindow = 0.06f;
    static constexpr float DamageSecondaryScale = 0.78f;
    // Size-by-magnitude, logarithmic: each decade above the reference adds a
    // twentieth, capped well before it can blur the kind hierarchy. A 100k hit
    // reads a step heavier than a 1k hit of the same kind, never heavier than
    // the next kind up.
    static constexpr float DamageMagnitudeReference = 1000.0f;
    static constexpr float DamageMagnitudeGainPerDecade = 0.05f;
    static constexpr float DamageMagnitudeScaleCap = 1.15f;
    // Overkill below a tenth of the printed number is trivia, not a mark.
    static constexpr float DamageOverkillCaptionFraction = 0.10f;
    // The pop's peak: how far over its resting size a number swells before
    // the settle. Crits and kills pop harder than body hits. O2 PLACEHOLDER.
    static constexpr float DamagePopScale = 1.15f;
    static constexpr float DamageCritPopScale = 1.4f;

    // The banners' holds, ins, out and rectangles are BreakerBannerQueue's
    // (04-death-banners). The label and title sizes on the plate. O2 PLACEHOLDER.
    static constexpr float BannerLabelPixels = 11.0f;
    static constexpr float BannerTitlePixels = 24.0f;
    static constexpr float BannerLinePixels = 13.0f;

    // THE ULTIMATE'S IGNITION FLASH. Brief on purpose, and its peak sits
    // deliberately below the low-health cue's 0.45-0.75 so a wash can never be
    // mistaken for danger — violet is the ultimates' colour and Harm is not,
    // but at equal weight a full-bleed reads as an alarm whatever its hue.
    static constexpr float UltimateIgnitionSeconds = 0.18f;     // O2 PLACEHOLDER
    static constexpr float UltimateIgnitionPeakAlpha = 0.22f;   // O2 PLACEHOLDER

    // Ability feedback timings. All cosmetic: nothing here gates a rule.
    static constexpr float AbilityFlashSeconds = 0.3f;
    static constexpr float MarkHeadroomCm = 160.0f;

    // Every ability state window shares this prefix; the HUD shows the whole
    // family rather than a hard-coded list, so a new Swift ability gets its
    // duration bar for free.
    static const TCHAR* WindowPrefix = TEXT("Window.Swift.");
    static const FName OverdriveWindow(TEXT("Window.Swift.Overdrive"));

    // Ultimates carry violet; the Swift windows carry the movement verb.
    // O179 colours by VERB. The definition carries one (see the ability
    // tiles' rails), but a window is keyed by its state TAG and nothing on
    // the state component names the definition that opened it, so this
    // cannot reach the verb without a seam from Abilities/. Every
    // non-ultimate window therefore reads as movement, which is right for
    // Swift's kit and wrong for a weapon-economy window the day one exists.
    static FLinearColor WindowColor(const FString& ShortKey)
    {
        return ShortKey.Equals(TEXT("Overdrive"), ESearchCase::IgnoreCase)
            ? BreakerUI::Violet : BreakerUI::VerbMove;
    }

    // First sentence of a description, used verbatim for the teaching callout.
    // Falls back to the whole text when it carries no terminator.
    static FString FirstSentence(const FString& Text)
    {
        int32 Stop = INDEX_NONE;
        if (Text.FindChar(TEXT('.'), Stop) && Stop > 0)
        {
            return Text.Left(Stop);
        }
        return Text;
    }

    // Loot pickup rules: every drop inside this range gets its plate. An
    // item's affix lines are read in the backpack, not off the ground, so no
    // plate opens a popup.
    static constexpr float PickupChipDistance = 1500.0f;

    // The plate's name size and the key tile's glyph size. O2 PLACEHOLDER.
    static constexpr float InteractPlateNamePixels = 14.0f;

    // The one non-loot interactable the prompt plates this frame, in F's own
    // precedence (feedstock > loot > travel > talk): null when a pickup wins, or
    // when nothing is in reach. The over-actor labels skip this actor so the
    // plate and the label never print the same name at the same anchor.
    static const AActor* BreakerHUDPlatedInteractable(const ABreakerCharacter* Character)
    {
        if (!Character) return nullptr;
        if (Character->FindNearbyFeedstock()) return nullptr;
#if BREAKER_HAS_LOOT_PICKUP
        if (Character->FindNearbyPickup()) return nullptr;
#endif
        if (const AActor* Travel = Character->FindNearbyTravelPoint()) return Travel;
        return Character->FindNearbyNPC();
    }
}

void ABreakerPlaytestHUD::BeginPlay()
{
    Super::BeginPlay();
    // One profile object per HUD life, read from the same ini the settings
    // screen writes. Re-read on the menu's close edge in DrawHUD.
    Profile = NewObject<UBreakerGameSettings>(this);
    Profile->LoadOrDefaults();
}

float ABreakerPlaytestHUD::NameplateScale() const
{
    return Profile && Profile->bLargerNameplates ? UBreakerGameSettings::LargerNameplateScale : 1.0f;
}

void ABreakerPlaytestHUD::DrawHUD()
{
    Super::DrawHUD();
    if (!Canvas) return;

    // The spec is authored at 1080p. Scaling by height (not by area) keeps the
    // cluster the same share of the screen on an ultrawide, where scaling by
    // width would inflate it.
    UIScale = FMath::Clamp(Canvas->ClipY / 1080.0f, 0.6f, 2.5f);

    const FVector2D Center(Canvas->ClipX * 0.5f, Canvas->ClipY * 0.5f);
    const ABreakerCharacter* Character = Cast<ABreakerCharacter>(GetOwningPawn());
    if (!Character)
    {
        DrawCrosshair(Center, S(BreakerUI::HudCrosshairGapRest), 0.0f);
        return;
    }
    EnsureDamageBinding(Character);
    EnsureWeaponBinding(Character);
    EnsureAbilityBinding(Character);
    EnsureProgressionBinding(Character);
    // Not a component seam: this one binds the GAME MODE, so it takes no
    // character and must re-bind after every travel, which is exactly when a
    // rift run begins.
    EnsureRiftBinding();

    // ---- Density instruments (dev, command-line gated by construction) ----
    // -BreakerHUDStress=N spawns N enemies in a ring once the field is up, so
    // the overlay can be MEASURED at hundred-enemy density — the harness
    // cannot herd a real horde. Every fourth ranks up through the enemy's own
    // public SetMonsterRank + ApplyChassis so the permanent-bar path draws.
    // -BreakerHUDCost logs a [HUDCost] line every 5s: whole-overlay ms plus
    // the two sections that could dominate. Scope of every number: THIS
    // HUD's DrawHUD only — world tick, enemy AI and rendering are not in it.
    static int32 HudStressCount = -1;
    if (HudStressCount < 0)
    {
        HudStressCount = 0;
        FParse::Value(FCommandLine::Get(), TEXT("BreakerHUDStress="), HudStressCount);
    }
    UWorld* StressWorld = GetWorld();
    if (HudStressCount > 0 && !bHudStressSpawned && StressWorld
        && StressWorld->GetAuthGameMode<ABreakerGameMode>()
        && UBreakerGameInstance::IsGymMap(StressWorld)
        && StressWorld->GetTimeSeconds() > 8.0f)
    {
        bHudStressSpawned = true;
        const FVector Base = Character->GetActorLocation();
        for (int32 Index = 0; Index < HudStressCount; ++Index)
        {
            const float Angle = 2.0f * PI * static_cast<float>(Index) / FMath::Max(HudStressCount, 1);
            const float Radius = 900.0f + 2600.0f * (static_cast<float>(Index % 10) / 10.0f);
            const FVector At = Base + FVector(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius, 60.0f);
            FActorSpawnParameters Params;
            Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
            if (ABreakerEnemy* Spawned = StressWorld->SpawnActor<ABreakerEnemy>(At, FRotator::ZeroRotator, Params))
            {
                if (Index % 4 == 3)
                {
                    // Rank only — ApplyChassis is the enemy's own protected
                    // business, and this measure exercises the DRAWING path,
                    // which reads rank, not the chassis it would retune.
                    Spawned->SetMonsterRank(EBreakerMonsterRank::Elite);
                }
            }
        }
        UE_LOG(LogTemp, Display, TEXT("[HUDStress] spawned %d enemies (every 4th elite) around the player"), HudStressCount);
    }
    const bool bHudCostLogging = FParse::Param(FCommandLine::Get(), TEXT("BreakerHUDCost"));
    const double HudDrawStart = FPlatformTime::Seconds();
    // Everything below, ability callouts included, is suppressed while the
    // pause/inventory menu owns the screen. The menu is the only writer of
    // the profile, so its open -> closed edge is when the profile is re-read.
    const bool bMenuOpen = Character->IsMenuOpen();
    if (bMenuWasOpen && !bMenuOpen && Profile) Profile->LoadOrDefaults();
    bMenuWasOpen = bMenuOpen;
    if (bMenuOpen) return;

    // The hub keeps vitals, quest guidance and interaction prompts visible.
    // Ammo and combat markers stay hidden; wallet balances live at vendors.
    if (UBreakerGameInstance::IsAnchorMap(this))
    {
        // No enemy pass runs here, so the blip array is cleared by hand
        // rather than left holding the last combat frame's hostiles.
        EnemyBlips.Reset();
        // What the player IS — health, resource — at the same corner as the
        // field, so the body does not move between the plaza and the fight.
        DrawVitals(Character);
        // Wallet balances belong at the vendor, not permanently over gameplay.
        DrawQuestLine(Character);
        DrawBanners(Center);
        // Who can be talked to and where the way out is, readable from
        // anywhere on the plaza — the Anchor's whole verb set, floating over
        // the actors that own it.
        DrawInteractableLabels(Character);
        DrawInteractPrompt(Character, Center);
        DrawPlaytestInstrumentation(Character, Center);
        return;
    }

    // The death beat (O82 campaign respawn): input is dead and the respawn
    // timer is running, so the one honest thing to draw is what happens
    // next. Dying with nothing on screen read as a bug in the first
    // playtest; dying with this line reads as a rule.
    //
    // A STATUS LINE, NOT AN ALARM (owner playtest 2026-08-26). It was one
    // 35-character string in Harm red at 16px, forty pixels off dead centre —
    // so it crossed the middle of the screen, competed with the crosshair and
    // printed over every enemy label behind it. Three things changed and none
    // of them is the wording:
    //  * COLOUR. Harm is the damage accent and it read as an alarm. System
    //    bone is the player/system accent, which is what a respawn state is.
    //  * WEIGHT. One line, and it is still only 14px. WHERE the player comes
    //    back is not stated: no name for the campaign's start point has been
    //    authored, and the state alone is the read until one is.
    //  * PLACE. Below the crosshair rather than through it. A dead player is
    //    not aiming, but the crosshair is still drawn and two things in the
    //    same 40 pixels is what made it read as competing.
    // The sting that used to land on this same frame is gone by the same
    // ruling; the whole beat was over-produced rather than under-produced.
    // THE REDEPLOYING LINE IS GONE (owner, second playtest: "when you die theres
    // deploy text on the bottom of the screen we dont need that either"). The
    // black, the lowered weapon and the death screen already say the player is
    // dead; a word underneath saying it again is the over-production this beat
    // has now been trimmed for twice.

    TickCapturePreview(Character);

    const UBreakerWeaponComponent* Weapon = Character->GetWeapon();
    const bool bRecentShot = Weapon && Weapon->GetSecondsSinceLastShot() < 0.14f;
    const FBreakerShotResult* Shot = Weapon ? &Weapon->GetLastShot() : nullptr;

    // World-anchored layers first: they sit over the world but under every
    // screen-anchored plate, so the HUD frame always wins a collision.
    // Rounds in flight are NOT in this list any more — they are world
    // primitives now (ABreakerTracerRenderer) and the renderer draws them,
    // correctly occluded, before the HUD gets the canvas at all.
    const double HudBarsStart = FPlatformTime::Seconds();
    DrawEnemyHealthBars(Character);
    HudCostBarsMs += (FPlatformTime::Seconds() - HudBarsStart) * 1000.0;
    DrawMarkedTarget(Character);
    const double HudNumbersStart = FPlatformTime::Seconds();
    DrawDamageNumbers();
    HudCostNumbersMs += (FPlatformTime::Seconds() - HudNumbersStart) * 1000.0;
    DrawLootPickups(Character);
    // The gym camp's Kess/Quartermaster and its travel point get the same
    // over-actor labels the Anchor draws; in a wave they sit far outside the
    // arena, so they cost nothing to the combat read.
    DrawInteractableLabels(Character);

    // Under the crosshair and under every plate: the ultimate frame is
    // ambient, never something the eye has to read past.
    DrawUltimateTreatment(Character);

    // --- Crosshair: four ticks, a gap that follows the cone, ADS collapse ---
    // The gap is driven by the weapon's HONEST next-shot cone, so movement,
    // sustained fire and ADS all move it through the same number the trace
    // uses. The travel is a rate (60 ms out, 200 ms back), held in
    // CrosshairGapPx across frames; the ADS blend is a clock off the last
    // aim flip. Both are BreakerHUDMath's.
    const bool bAiming = Weapon && Weapon->IsAiming();
    const double TickNow = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
    const float DeltaSeconds = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.0f;
    if (bAiming != bWasAiming)
    {
        AdsChangeTime = TickNow;
        bWasAiming = bAiming;
    }
    const float SpreadDegrees = Weapon ? Weapon->GetNextShotSpreadDegrees() : 0.0f;
    CrosshairGapPx = BreakerHUDMath::CrosshairGapFollow(CrosshairGapPx,
        BreakerHUDMath::CrosshairGapTarget(SpreadDegrees), DeltaSeconds);
    const float AdsBlend = BreakerHUDMath::CrosshairAdsBlend(
        static_cast<float>(TickNow - AdsChangeTime), bAiming);
    DrawCrosshair(Center, S(CrosshairGapPx), AdsBlend);
    // Hit, kill and weak-point marks over the ticks, on the arrival clock.
    DrawCrosshairMarks(Center);

    // --- Bottom-left: what the player IS ---------------------------------
    DrawVitals(Character);
    // The effect column grows upward from just above the vitals, so an
    // expiring status never shifts the numbers beneath it.
    const float VitalsTop = S(BreakerUI::HudVitalsTop);
    DrawStatusReadout(Character, S(BreakerUI::HudVitalsLeft), VitalsTop - S(BreakerUI::Space8),
        S(BreakerUI::HudVitalsWidth));

    // --- Bottom-centre: the ability tiles ---------------------------------
    DrawAbilityCluster(Character);
    // Duration bars stack upward from above the tiles: windows are HUD bars
    // (O179), never a tile state.
    const float ClusterX = S(BreakerUI::HudAbilityOneX);
    const float ClusterW = S(BreakerUI::HudAbilityTwoX + BreakerUI::HudAbilityTile - BreakerUI::HudAbilityOneX);
    DrawAbilityWindows(Character, ClusterX, VitalsTop - S(BreakerUI::Space8), ClusterW);

    // --- Bottom-right: the weapon -----------------------------------------
    DrawWeaponReadout(Character);

    // --- Periphery: zone and countdown top-left, the quest line top-right --
    DrawZoneLine(Character);
    DrawQuestLine(Character);

    // --- Centre: feedback only, nothing persistent ------------------------
    // The event banners, one pass: rift complete, level up, wave clear, in
    // priority order at staggered arrivals, each in its own rectangle.
    DrawBanners(Center);
    DrawDefenseFeedback(Center);
    DrawInteractPrompt(Character, Center);
    if (const UBreakerCombatComponent* PlayerCombat = Character->GetCombat(); PlayerCombat && PlayerCombat->GetSecondsSinceDamage() < 0.28f)
    {
        // Harm is instant: full-bleed edge lines, no inset, no fade in.
        //
        // THE WORD IS GONE (owner, playtest 2026-09-10): "theres no need for
        // DAMAGE to appear when im taking damage". The frame IS the tell — it
        // is instant, it is peripheral, and it does not ask to be read. A word
        // in the middle of the screen asks to be read, every time, for
        // something the player already knows happened to them.
        //
        // The plate-avoidance search went with it. It existed ONLY to keep this
        // label off the enemy status rows; with no label there is nothing to
        // move, and EnemyPlateBounds keeps its other readers.
        const FLinearColor DamageColor = BreakerUI::Alpha(BreakerUI::Harm, 0.85f);
        const float T = S(4.0f);
        DrawRect(DamageColor, 0.0f, 0.0f, Canvas->ClipX, T);
        DrawRect(DamageColor, 0.0f, Canvas->ClipY - T, Canvas->ClipX, T);
        DrawRect(DamageColor, 0.0f, 0.0f, T, Canvas->ClipY);
        DrawRect(DamageColor, Canvas->ClipX - T, 0.0f, T, Canvas->ClipY);
    }
    // The near-death frame, after the transient damage flash so the flash
    // always reads over it.
    DrawNearDeathFrame(Character);
    // RELOADING NO LONGER SHOUTS (owner, playtest 2026-09-10): "reloading
    // doesnt need to have giant text on screen at all theres an animation".
    // Three things already say it — the reload animation, the magazine rail
    // turning orange, and the rail filling as the reload runs — so the centred
    // callout was the fourth and the only one that interrupted aiming.

    // The old fixed-position damage readout is gone: floating world-space
    // numbers say the same thing at the impact point. Only the weak-point
    // callout survives, because it is a skill confirmation, not a value.
    if (bRecentShot && Shot && Shot->bHit && Shot->bWeakPoint)
    {
        DrawSpecText(BreakerStrings::Get(EBreakerStringKey::HudCalloutWeakPoint), Center.X + S(24.0f), Center.Y + S(18.0f), BreakerUI::Gold, 11.0f, 1.0f, ESpecFontRole::Display);
    }

    // Latch elite kills: the shot feedback window is far shorter than the
    // time this callout should stay readable.
    if (bRecentShot && Shot && Shot->bHit && Shot->DamageResult.bKilled)
    {
        // The same rank-predicate shape again: == Elite exactly kept the
        // callout silent for a ModifierBearing kill — the harder version of
        // the kill it announces. Boss stays EXCLUDED on purpose: a boss death
        // ends the encounter through OnBossDefeated, and a 1.2s "ELITE DOWN"
        // under that moment would be the smaller fact shouting over the
        // bigger one.
        if (const ABreakerEnemy* KilledEnemy = Cast<ABreakerEnemy>(Shot->HitActor.Get());
            KilledEnemy && KilledEnemy->IsEliteOrBetter()
            && KilledEnemy->GetMonsterRank() != EBreakerMonsterRank::Boss)
            LastEliteKillTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
    }
    const double EliteKillAge = (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0) - LastEliteKillTime;
    if (EliteKillAge >= 0.0 && EliteKillAge < 1.2f)
    {
        const float Fade = 1.0f - static_cast<float>(EliteKillAge) / 1.2f;
        DrawSpecTextCentered(BreakerStrings::Get(EBreakerStringKey::HudCalloutEliteDown), Center.X, Center.Y - S(118.0f), BreakerUI::Gold, 20.0f, Fade, ESpecFontRole::Display);
    }

    DrawPlaytestInstrumentation(Character, Center);

    // The cost window closes: whole-overlay time this frame, reported as a
    // 5-second average with its worst frame, sections beside it.
    if (bHudCostLogging)
    {
        const double FrameMs = (FPlatformTime::Seconds() - HudDrawStart) * 1000.0;
        HudCostAccumMs += FrameMs;
        HudCostMaxMs = FMath::Max(HudCostMaxMs, FrameMs);
        ++HudCostFrames;
        const double NowSeconds = FPlatformTime::Seconds();
        if (HudCostWindowStart <= 0.0) HudCostWindowStart = NowSeconds;
        if (NowSeconds - HudCostWindowStart >= 5.0 && HudCostFrames > 0)
        {
            UE_LOG(LogTemp, Display,
                TEXT("[HUDCost] frames=%d avg=%.3fms max=%.3fms | bars=%.3f numbers=%.3f (avg ms; scope: DrawHUD only)"),
                HudCostFrames, HudCostAccumMs / HudCostFrames, HudCostMaxMs,
                HudCostBarsMs / HudCostFrames, HudCostNumbersMs / HudCostFrames);
            HudCostAccumMs = HudCostMaxMs = HudCostBarsMs = HudCostNumbersMs = 0.0;
            HudCostFrames = 0;
            HudCostWindowStart = NowSeconds;
        }
    }
}

// --------------------------------------------------------------------------
// Top-left playtest instrumentation, shared by the combat HUD and the
// Anchor's trimmed HUD — which is WHY it is a function: the Anchor keeps
// exactly this block (key legend, F3 diagnostics, report toast) and nothing
// else of the chrome, and duplicating it there would fork it.
// --------------------------------------------------------------------------
void ABreakerPlaytestHUD::DrawPlaytestInstrumentation(const ABreakerCharacter* Character, const FVector2D& Center)
{
    if (!Character) return;
    const UBreakerPlaytestComponent* Playtest = Character->GetPlaytest();

    // Not in the design canvas and never shipping. It still has to obey the
    // system: muted text on its own plate, because unbacked grey text over a
    // bright sky is unreadable — which is exactly how the first pass shipped.
    // Below the zone line and its countdown, which own the top-left corner.
    const float LegendTop = BreakerUI::HudCountdownTop + BreakerUI::HudCountdownPixels + BreakerUI::Space16;   // O2 PLACEHOLDER
    // THE LEGEND IS PART OF THE DIAGNOSTICS NOW, not permanent chrome (owner,
    // playtest 2026-09-10): "the top left menu is ugly and half the mechanics
    // are unnecessary now". It was on screen in every frame of every session,
    // advertising dev keys over the top-left corner of a game it says of itself
    // it is "never shipping". F3 brings it back with the numbers it belongs to.
    if (Playtest && Playtest->AreDiagnosticsVisible())
    {
        const FString KeyLegend(TEXT("F1 RESET   F2 REPORT   F3 DIAGNOSTICS   ESC MENU"));
        const FVector2D LegendSize = MeasureSpecText(KeyLegend, 11.0f, ESpecFontRole::Mono);
        const float LegendX = S(BreakerUI::HudSafeMargin);
        const float LegendY = S(LegendTop);
        const float LegendH = LegendSize.Y + S(BreakerUI::Space8);
        DrawPlate(LegendX, LegendY, LegendSize.X + S(BreakerUI::Space24) + S(BreakerUI::HudRailIdentity), LegendH, BreakerUI::TextMuted);
        DrawSpecText(KeyLegend, LegendX + S(BreakerUI::HudRailIdentity) + S(BreakerUI::Space8), LegendY + S(BreakerUI::Space4),
            BreakerUI::TextSecondary, 11.0f, 1.0f, ESpecFontRole::Mono);
    }
    if (Playtest && Playtest->AreDiagnosticsVisible())
    {
        const FBreakerPlaytestStats& Stats = Playtest->GetStats();
        const float FPS = GetWorld() && GetWorld()->GetDeltaSeconds() > UE_SMALL_NUMBER ? 1.0f / GetWorld()->GetDeltaSeconds() : 0.0f;
        const float DiagX = S(BreakerUI::HudSafeMargin);
        // Clear of the key legend above it: the two used to overlap.
        const float DiagY = S(LegendTop + 32.0f);
        const float DiagW = S(300.0f);
        const float DiagH = S(74.0f);
        DrawPlate(DiagX, DiagY, DiagW, DiagH, BreakerUI::TextMuted);
        const float TextX = DiagX + S(BreakerUI::Space16);
        // The speed readout is playtest chrome, so it lives here and not
        // beside the resource track it used to ride.
        DrawSpecText(FString::Printf(TEXT("FPS %.0f   FOV %.0f   SENS %.1f   %.0f M/S"), FPS, Character->GetCurrentFOV(),
                Character->GetLookSensitivity(), Character->GetHorizontalSpeed() / 100.0f),
            TextX, DiagY + S(10.0f), BreakerUI::TextSecondary, 11.0f, 1.0f, ESpecFontRole::Mono);
        DrawSpecText(FString::Printf(TEXT("SHOTS %d   ACC %.1f%%   WEAK %.1f%%"), Stats.ShotsFired, Stats.Accuracy(), Stats.WeakPointRate()),
            TextX, DiagY + S(30.0f), BreakerUI::TextSecondary, 11.0f, 1.0f, ESpecFontRole::Mono);
        DrawSpecText(FString::Printf(TEXT("DMG %.0f   RELOADS %d"), Stats.DamageDealt, Stats.Reloads),
            TextX, DiagY + S(50.0f), BreakerUI::TextSecondary, 11.0f, 1.0f, ESpecFontRole::Mono);

        // Diagnostics world labels stay short-range and small: past 25m they
        // were pure screen noise.
        for (TActorIterator<ABreakerTargetDummy> It(GetWorld()); It; ++It)
        {
            const float Distance = FVector::Distance(Character->GetActorLocation(), It->GetActorLocation());
            if (Distance > 2500.0f) continue;
            FVector2D Screen;
            if (PlayerOwner && PlayerOwner->ProjectWorldLocationToScreen(It->GetActorLocation() + FVector(0.0f, 0.0f, 130.0f), Screen))
            {
                DrawSpecTextCentered(FString::Printf(TEXT("%s  %.0fm"), *It->GetProfileLabel(), Distance / 100.0f),
                    Screen.X, Screen.Y, BreakerUI::TextMuted, 11.0f, 0.8f, ESpecFontRole::Display);
            }
        }
        // OCCLUSION-SUPPRESSED, AND IT WAS NOT. This pass drew
        // GetEnemyStateLabel over EVERY enemy within 25 m with no focus gate,
        // no overlap test and no cap, so twenty bodies in a pocket printed
        // PATROL fifteen times through itself. That mush is what the owner's
        // "the feedback needs to be better" frame actually showed — the
        // collision was never the enemy BARS, it was this debug overlay drawing
        // a second label pass on top of them.
        //
        // The bar TU solved this exact problem already; this is the same
        // screen-space test, deliberately kept LOCAL rather than sharing
        // DrawnLabelBounds — that array is the enemy-bar pass's, and coupling a
        // debug overlay to a shipping read across a lane boundary to save one
        // allocation is a bad trade. The allocation is debug-path only: nothing
        // here runs unless the diagnostics overlay is up.
        //
        // A DIAGNOSTIC MUST NOT LIE BY OMISSION. What is suppressed is counted
        // and printed, so the overlay can never quietly show six of twenty and
        // read as though there were six — which is the failure mode that makes
        // an instrument worse than no instrument.
        TArray<FVector4> DiagnosticLabelBounds;
        int32 SuppressedLabels = 0;
        for (TActorIterator<ABreakerEnemy> It(GetWorld()); It; ++It)
        {
            if (FVector::DistSquared(Character->GetActorLocation(), It->GetActorLocation()) > FMath::Square(2500.0f)) continue;
            FVector2D Screen;
            if (!PlayerOwner || !PlayerOwner->ProjectWorldLocationToScreen(
                It->GetActorLocation() + FVector(0.0f, 0.0f, 130.0f), Screen)) continue;

            const FString Label = It->GetEnemyStateLabel();
            const FVector2D Size = MeasureSpecText(Label, 11.0f, ESpecFontRole::Display);
            bool bOccluded = false;
            for (const FVector4& Taken : DiagnosticLabelBounds)
            {
                if (FMath::Abs(Screen.X - Taken.X) < (Size.X + Taken.Z) * 0.5f
                    && FMath::Abs(Screen.Y - Taken.Y) < (Size.Y + Taken.W) * 0.5f)
                {
                    bOccluded = true;
                    break;
                }
            }
            if (bOccluded) { ++SuppressedLabels; continue; }
            DiagnosticLabelBounds.Emplace(Screen.X, Screen.Y, Size.X, Size.Y);
            DrawSpecTextCentered(Label, Screen.X, Screen.Y, BreakerUI::Orange, 11.0f, 0.7f, ESpecFontRole::Display);
        }
        if (SuppressedLabels > 0)
        {
            DrawSpecTextCentered(FString::Printf(TEXT("+%d STATE LABEL(S) HIDDEN — OVERLAP"), SuppressedLabels),
                Center.X, Center.Y + S(110.0f), BreakerUI::TextMuted, 10.0f, 0.7f, ESpecFontRole::Mono);
        }
    }
    if (Playtest && Playtest->GetSecondsSinceReportCopy() < 2.0f)
    {
        DrawSpecTextCentered(TEXT("PLAYTEST REPORT COPIED"), Center.X, Center.Y + S(72.0f), BreakerUI::System, 14.0f, 1.0f, ESpecFontRole::Display);
    }
}

// --------------------------------------------------------------------------
// Vitals — bottom-left, 480 wide at (40, 936). Health value in the large
// numeric with the max after it in the small numeric (O199); the shield layer
// only when a pool exists; the health bar with its chip and its 20 % tick;
// the resource track beneath. Nothing here moves between states.
// --------------------------------------------------------------------------
void ABreakerPlaytestHUD::DrawVitals(const ABreakerCharacter* Character)
{
    const UBreakerAttributeSet* Attributes = Character ? Character->GetAttributes() : nullptr;
    if (!Attributes || !Canvas) return;
    const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;

    const float Left = S(BreakerUI::HudVitalsLeft);
    const float Width = S(BreakerUI::HudVitalsWidth);
    const float Right = Left + Width;
    const float MaxHealth = Attributes->GetMaxHealth();
    // The preview forces the near-death state: nothing in a headless run can
    // lose health, so without this the harm value, the tick's meaning and the
    // frame are unphotographable.
    const float HealthFraction = IsCapturePreview() ? 0.12f
        : (MaxHealth > UE_SMALL_NUMBER ? FMath::Clamp(Attributes->GetHealth() / MaxHealth, 0.0f, 1.0f) : 0.0f);

    // AND IT FABRICATES A SHIELD POOL, for the same reason and under the same
    // gate. The playable classes ship with no shield at all today, so the
    // combined bar's SEAM — the whole point of putting two pools on one track
    // — has never once appeared in a capture, and neither has the blue that
    // tells them apart. A layout nobody can photograph is a layout that ships
    // broken; the wave banner and the damage numbers both did.
    //
    // Numbers only. The split, the tick, the seam and the fills all go through
    // the same code the real pools would.
    const bool bFabricateShield = IsCapturePreview() && Attributes->GetMaxShield() <= 0.0f;
    const float MaxShield = bFabricateShield ? 60.0f : Attributes->GetMaxShield();
    const float Shield = bFabricateShield ? 41.0f : Attributes->GetShield();
    // AND THE NUMBER AGREES WITH THE BAR IN A CAPTURE. The preview forces the
    // FRACTION and used to leave the figure at the character's real health, so
    // every frame this harness has ever produced showed a full-health number
    // over a near-empty bar — and the frames are how this cluster is read.
    const float ShownHealth = IsCapturePreview() ? MaxHealth * HealthFraction : Attributes->GetHealth();
    const BreakerUI::FVitalsRow Vitals =
        BreakerUI::FormatVitalsRow(Shield, MaxShield, ShownHealth, MaxHealth);

    // --- Value and max --------------------------------------------------------
    // Both share one baseline: the max is small and sits on the value's
    // bottom edge, not its own top.
    const float ValueTop = S(BreakerUI::HudVitalsTop);
    const FVector2D ValueSize = MeasureSpecText(Vitals.HealthText, BreakerUI::HudVitalsValuePixels, ESpecFontRole::Mono);
    const FVector2D MaxSize = MeasureSpecText(Vitals.MaxText, BreakerUI::HudVitalsMaxPixels, ESpecFontRole::Mono);
    DrawSpecText(Vitals.HealthText, Left, ValueTop,
        BreakerHUDMath::VitalsValueIsHarm(HealthFraction) ? BreakerUI::Harm : BreakerUI::System,
        BreakerUI::HudVitalsValuePixels, 1.0f, ESpecFontRole::Mono);
    // THE MAX ALWAYS PRINTS, and this overturns my own de-clutter from the last
    // cycle. It used to be hidden at full health on the argument that "162 162"
    // is the same number twice; the owner played that and asked for "a proper
    // read" of health, and he is right — a readout that drops half of itself
    // when the value is full is a readout you cannot learn the shape of. Out of
    // WHAT is part of the read, not context for it.
    DrawSpecText(Vitals.MaxText, Left + ValueSize.X + S(BreakerUI::HudVitalsMaxGap),
        ValueTop + ValueSize.Y - MaxSize.Y, BreakerUI::TextMuted, BreakerUI::HudVitalsMaxPixels, 1.0f, ESpecFontRole::Mono);
    // The shield numbers, small, at the row's right edge, only when a pool
    // exists (O199). Right-aligned so they cannot collide with a long health
    // max, and in the shield's OWN BLUE (O269) — otherwise it is a third
    // number in a row of greys and which pool it counts is left to position.
    //
    // BOTH POOLS PRINT OUT OF WHAT (owner: "make sure maximum shield/health is
    // displayed next to current in some fashion"). The shield used to print a
    // bare current, so 41 could have been a scratch on a big pool or almost all
    // of a small one, and the two pools were formatted differently from each
    // other for no reason a player could see. Same shape as health now: the
    // value, then the total behind it in the muted tone.
    if (Vitals.bDrawShield)
    {
        const FVector2D ShieldSize = MeasureSpecText(Vitals.ShieldText, BreakerUI::HudVitalsMaxPixels, ESpecFontRole::Mono);
        const FString ShieldMaxText = BreakerUI::FormatTicker(MaxShield);
        const FVector2D ShieldMaxSize = MeasureSpecText(ShieldMaxText, BreakerUI::HudVitalsMaxPixels, ESpecFontRole::Mono);
        const float ShieldBaseline = ValueTop + ValueSize.Y - ShieldSize.Y;
        DrawSpecTextRight(ShieldMaxText, Right, ShieldBaseline,
            BreakerUI::TextMuted, BreakerUI::HudVitalsMaxPixels, 1.0f, ESpecFontRole::Mono);
        DrawSpecTextRight(Vitals.ShieldText, Right - ShieldMaxSize.X - S(BreakerUI::HudVitalsMaxGap), ShieldBaseline,
            BreakerUI::VitalShield, BreakerUI::HudVitalsMaxPixels, 1.0f, ESpecFontRole::Mono);
    }

    // --- ONE BAR, TWO POOLS -------------------------------------------------
    // Owner: "multiple bars appearing over my hud which dont help at all as to
    // what they are ... keep it as one combined bar". The shield used to have
    // its own 6 px track fourteen pixels above the health bar, and two stacked
    // rails of the same width read as one thing with a seam in it rather than
    // as two pools.
    //
    // So the shield lives INSIDE the health bar now, drawn below with the two
    // pools sharing one track and splitting it by their maxima — health from
    // the left, shield from the right — so the bar's total length is what you
    // have and the split is where one pool ends and the other begins. The two
    // numbers still print separately above it, which is the other half of what
    // he asked for.

    // --- Health bar ---------------------------------------------------------------
    // The fill drains at once; the chip holds where the health WAS and
    // recovers linearly. The drop is detected here against last frame's
    // fraction, so the HUD needs no seam from Combat/ to know it was hit.
    const float HealthY = S(BreakerUI::HudHealthTop);
    const float HealthH = S(BreakerUI::HudHealthHeight);
    if (HealthFraction < HealthShownFraction)
    {
        HealthChip = BreakerHUDMath::HealthChipOnDrop(HealthChip,
            BreakerHUDMath::HealthChipShown(HealthChip, HealthShownFraction, Now), Now);
    }
    HealthShownFraction = HealthFraction;
    const float ChipFraction = BreakerHUDMath::HealthChipShown(HealthChip, HealthFraction, Now);

    // THE SPLIT. The track's width is shared between the two pools in
    // proportion to their MAXIMA, so the seam does not move while you are
    // being shot — a boundary that slid around would be a third moving thing
    // to read. A character with no shield gets the whole bar and no seam,
    // which is the shape this HUD has always had.
    const float HealthShare = Vitals.bDrawShield && (MaxHealth + MaxShield) > UE_SMALL_NUMBER
        ? MaxHealth / (MaxHealth + MaxShield) : 1.0f;
    const float HealthWidth = Width * HealthShare;
    const float ShieldWidth = Width - HealthWidth;

    DrawTrack(Left, HealthY, HealthWidth, HealthH, HealthFraction, BreakerUI::System, BreakerUI::BgBase);
    if (ChipFraction > HealthFraction)
    {
        const float ChipX = Left + HealthWidth * HealthFraction;
        const float ChipW = HealthWidth * (ChipFraction - HealthFraction);
        DrawHatch(ChipX, HealthY, ChipW, HealthH, BreakerUI::HarmDeep, BreakerUI::Harm,
            BreakerUI::HudHealthChipHatchPeriod, BreakerUI::HudHealthChipHatchStripe);
    }
    // The 20 % tick belongs to the HEALTH pool and is cut at 20 % of ITS
    // length, not of the whole bar. Against a shielded character the two are
    // different places, and the one that means "you are nearly dead" is this.
    DrawRect(BreakerUI::BgVoid, Left + HealthWidth * BreakerUI::HudHealthLowFraction, HealthY,
        FMath::Max(S(1.0f), 1.0f), HealthH);

    if (Vitals.bDrawShield && ShieldWidth > 0.0f)
    {
        // The shield fills from the SEAM outward, in its own blue rather than in
        // the bone the health wears: same bar, same height, plainly a different
        // pool. Text-2 against bone was two greys a hairline apart, so the
        // combined bar read as one fill with a seam in it — which is the thing
        // combining them was supposed to fix.
        const float ShieldFraction = MaxShield > UE_SMALL_NUMBER
            ? FMath::Clamp(Shield / MaxShield, 0.0f, 1.0f) : 0.0f;
        DrawTrack(Left + HealthWidth, HealthY, ShieldWidth, HealthH, ShieldFraction,
            BreakerUI::VitalShield, BreakerUI::VitalShieldDeep);
        DrawRect(BreakerUI::BgVoid, Left + HealthWidth, HealthY, FMath::Max(S(2.0f), 1.0f), HealthH);
    }
    // THE BAR IS OUTLINED (owner: "give the bar a subtle outline as well so it
    // stands out a little bit more"). Two strokes rather than a thicker one:
    // a dark ring OUTSIDE the bar and the bright border on its edge. The bar is
    // drawn over whatever the world happens to be — a sunlit concrete yard is
    // as likely as a dark interior — and a single border cannot hold against
    // both. The dark ring is what separates it from a bright ground; the bright
    // edge is what holds it on a dark one.
    const float Outline = FMath::Max(S(BreakerUI::BorderThin), 1.0f);
    DrawBorder(Left - Outline, HealthY - Outline, Width + Outline * 2.0f, HealthH + Outline * 2.0f,
        BreakerUI::BgVoid, Outline);
    DrawBorder(Left, HealthY, Width, HealthH, BreakerUI::BorderHigh, S(BreakerUI::BorderThin));

    // THE NUMBER COLUMN AT THE RIGHT END OF A RAIL. Owner: "can we get numeric
    // values for xp and mana that are shown". Both rails below want the same
    // shape — label at the left, figures at the right, rail taking what is
    // between — so it is one lambda rather than the same eight lines twice.
    //
    // RIGHT-ALIGNED AND THE RAIL SHORTENS. Drawing the pair OVER the rail was
    // the other option and it is wrong twice: an 8 px track cannot hold a 12 px
    // glyph, and a number whose background is a moving fill is a number you
    // read twice. Returns the x the rail must stop at.
    const auto DrawRailPair = [this, Right](const FString& ValueText, const FString& TotalText,
        float RowY, float RowH) -> float
    {
        if (ValueText.IsEmpty()) return Right;
        const float Pixels = BreakerUI::HudXpLevelPixels;
        float Cursor = Right;
        if (!TotalText.IsEmpty())
        {
            // The total is MUTED against the current value's text-2: the thing
            // that moves is the thing the eye should land on.
            const FVector2D TotalSize = MeasureSpecText(TotalText, Pixels, ESpecFontRole::Mono);
            DrawSpecTextRight(TotalText, Cursor, RowY + RowH * 0.5f - TotalSize.Y * 0.5f,
                BreakerUI::TextMuted, Pixels, 1.0f, ESpecFontRole::Mono);
            Cursor -= TotalSize.X + S(BreakerUI::Space4);
        }
        const FVector2D CurrentSize = MeasureSpecText(ValueText, Pixels, ESpecFontRole::Mono);
        DrawSpecTextRight(ValueText, Cursor, RowY + RowH * 0.5f - CurrentSize.Y * 0.5f,
            BreakerUI::TextSecondary, Pixels, 1.0f, ESpecFontRole::Mono);
        return Cursor - CurrentSize.X - S(BreakerUI::Space8);
    };

    // --- Resource track ---------------------------------------------------------
    // AND IT SAYS WHAT IT IS. Owner: "multiple bars appearing over my hud which
    // dont help at all as to what they are". The row has computed a Label —
    // MANA, MOMENTUM, SCRAP, GRIT, CHARGE — since it was written, and nothing
    // ever drew it. Three unlabelled rails of the same width stacked on top of
    // each other are not three readouts, they are one shape with seams.
    //
    // LABEL FIRST AND THE RAIL TAKES WHAT IS LEFT, the same row the XP rail
    // below already uses. There is no vertical room down here — the XP rail
    // learned that when its label landed on the health bar — so it is
    // horizontal or it collides.
    {
        const BreakerHUD::FResourceRow Row = ResolveResourceRow(Character);
        const float ResourceY = S(BreakerUI::HudResourceTop);
        const float ResourceH = S(BreakerUI::HudResourceHeight);
        float TrackLeft = Left;
        if (!Row.Label.IsEmpty())
        {
            const FVector2D LabelSize = MeasureSpecText(Row.Label, BreakerUI::HudXpLevelPixels, ESpecFontRole::Mono);
            DrawSpecText(Row.Label, Left, ResourceY + ResourceH * 0.5f - LabelSize.Y * 0.5f,
                BreakerUI::TextMuted, BreakerUI::HudXpLevelPixels, 1.0f, ESpecFontRole::Mono);
            TrackLeft = Left + LabelSize.X + S(BreakerUI::Space8);
        }
        const float TrackRight = DrawRailPair(Row.ValueText, Row.MaxText, ResourceY, ResourceH);
        DrawResourceTrack(Row, TrackLeft, ResourceY, FMath::Max(0.0f, TrackRight - TrackLeft), ResourceH);
    }

    // --- XP -----------------------------------------------------------------
    // "i cant see my xp" (owner, playtest 2026-09-10). It was not small or
    // hidden — the HUD had NO experience readout of any kind, in a game whose
    // first contract's whole reward is a level.
    //
    // A LEVEL, A RAIL AND THE PAIR. The figures are owner-ruled ("can we get
    // numeric values for xp and mana that are shown") and they overturn this
    // block's own argument that the exact total belongs in a menu. What they
    // answer that the rail cannot: whether the next pocket finishes the level.
    //
    // XP INTO THE CURRENT LEVEL, NOT THE CUMULATIVE TOTAL. The stored quantity
    // is cumulative on purpose — it is the representation that survives a
    // curve retune — but "14 402" beside a bar a third full tells a player
    // nothing. At the cap there is no next level, so the lifetime total prints
    // alone rather than as a pair against a zero.
    //
    // The fraction is the progression library's own
    // (LevelProgressFraction), so the bar cannot disagree with the level-up it
    // is predicting. At the cap the rail fills and stays full rather than
    // vanishing — an empty space where a bar was reads as a bug.
    if (const UBreakerProgressionComponent* Progression = Character->GetProgression())
    {
        const FBreakerProgressionState& State = Progression->GetProgressionState();
        const bool bCapped = State.CharacterLevel >= UBreakerExperienceLibrary::MaxCharacterLevel;
        const float Fraction = bCapped ? 1.0f
            : FMath::Clamp(UBreakerExperienceLibrary::LevelProgressFraction(
                State.TotalExperience, Progression->ExperienceCurve), 0.0f, 1.0f);
        // LABEL AND RAIL SHARE ONE ROW, the label first and the rail taking
        // what is left. The first attempt stacked the label ABOVE the rail and
        // the capture showed it sitting on top of the health bar — there is no
        // vertical room down here, so the row is horizontal or it collides.
        const float XpY = S(BreakerUI::HudXpTop);
        const float XpH = S(BreakerUI::HudXpHeight);
        const FString LevelText = FString::Printf(TEXT("LV %d"), State.CharacterLevel);
        const FVector2D LevelSize = MeasureSpecText(LevelText, BreakerUI::HudXpLevelPixels, ESpecFontRole::Mono);
        DrawSpecText(LevelText, Left, XpY + XpH * 0.5f - LevelSize.Y * 0.5f,
            BreakerUI::TextMuted, BreakerUI::HudXpLevelPixels, 1.0f, ESpecFontRole::Mono);
        const float RailLeft = Left + LevelSize.X + S(BreakerUI::Space8);
        const int32 Reached = UBreakerExperienceLibrary::TotalXpToReachLevel(
            State.CharacterLevel, Progression->ExperienceCurve);
        const int32 Needed = UBreakerExperienceLibrary::XpToNextLevel(
            State.CharacterLevel, Progression->ExperienceCurve);
        const int32 IntoLevel = FMath::Max(0, State.TotalExperience - Reached);
        const float RailRight = (bCapped || Needed <= 0)
            ? DrawRailPair(BreakerUI::FormatTicker(static_cast<float>(State.TotalExperience)),
                FString(), XpY, XpH)
            : DrawRailPair(BreakerUI::FormatTicker(static_cast<float>(IntoLevel)),
                BreakerUI::FormatTicker(static_cast<float>(Needed)), XpY, XpH);
        DrawTrack(RailLeft, XpY, FMath::Max(0.0f, RailRight - RailLeft), XpH, Fraction,
            BreakerUI::TextSecondary, BreakerUI::BgBase);
    }
}

// --------------------------------------------------------------------------
// Abilities — bottom-centre: ability 1 at 844, the ultimate at 916 (88, taller
// so it bottoms on the same y = 1024), ability 2 at 1012. Keys as the
// bindings already are: E / G / T.
// --------------------------------------------------------------------------
void ABreakerPlaytestHUD::DrawAbilityCluster(const ABreakerCharacter* Character)
{
    if (!Character || !Canvas) return;
    const UBreakerAbilityComponent* Abilities = Character->GetAbilities();
    const float Bottom = S(BreakerUI::HudAbilityBottom);
    const float Tile = S(BreakerUI::HudAbilityTile);
    const float Ultimate = S(BreakerUI::HudUltimateTile);

    // Each rail is the granted definition's VERB (O179), never the slot's:
    // Swift's Skim is movement-cyan, Lead beside it is reward-gold. The
    // ultimate slot is violet whatever it holds; an ungranted slot has no
    // definition and takes the resting border through the same function.
    const auto RailFor = [Abilities](EBreakerAbilitySlot Slot)
    {
        const UBreakerAbilityDefinition* Definition = Abilities && Abilities->IsSlotGranted(Slot)
            ? Abilities->GetDefinitionForSlot(Slot) : nullptr;
        return BreakerHUDMath::AbilityRailColor(Definition ? Definition->Verb : EBreakerAbilityVerb::None,
            Slot == EBreakerAbilitySlot::Ultimate);
    };
    DrawAbilitySlot(Character, Abilities, EBreakerAbilitySlot::ClassAbilityOne, TEXT("E"),
        S(BreakerUI::HudAbilityOneX), Bottom - Tile, Tile, S(BreakerUI::HudAbilityMark), RailFor(EBreakerAbilitySlot::ClassAbilityOne));
    DrawAbilitySlot(Character, Abilities, EBreakerAbilitySlot::Ultimate, TEXT("G"),
        S(BreakerUI::HudUltimateX), Bottom - Ultimate, Ultimate, S(BreakerUI::HudUltimateMark), RailFor(EBreakerAbilitySlot::Ultimate));
    DrawAbilitySlot(Character, Abilities, EBreakerAbilitySlot::ClassAbilityTwo, TEXT("T"),
        S(BreakerUI::HudAbilityTwoX), Bottom - Tile, Tile, S(BreakerUI::HudAbilityMark), RailFor(EBreakerAbilitySlot::ClassAbilityTwo));

    // O252's skill level, ONCE for the cluster rather than once per slot. The
    // pool is shared, so every slot carries the same number and printing it
    // three times would say three times that they are different. It sits above
    // the row's left edge, muted and small: it changes a few times a session,
    // so it is a readout the player can find, not a thing competing with the
    // cooldowns beneath it. Without this the chase O253 sells is invisible,
    // which by this project's own rule makes it dead content.
    //
    // IT ONLY DRAWS ONCE IT MEANS SOMETHING (owner, playtest 2026-09-10). He
    // listed "the SKILL 1 above the ability on E" among the things that make
    // the HUD feel bad, and at MinLevel that is exactly what it was: a label
    // permanently announcing its own default. A readout that changes a few
    // times a session should be absent until it has changed.
    //
    // NOT DELETED OUTRIGHT, and this is a judgement worth overruling if he
    // wants it gone: the comment above is the site's own warning that without
    // any readout O253's chase is invisible, which by this project's rule
    // makes it dead content. Hiding the default keeps the noise off the screen
    // and keeps the earned number findable. Deleting it entirely is this
    // if-statement and its body.
    if (Character->GetProgression()
        && UBreakerGameplayAbility::SkillLevelFor(Character) > BreakerSkillLevel::MinLevel)
    {
        DrawSpecText(FString::Printf(TEXT("%s %d"),
                *BreakerStrings::Get(EBreakerStringKey::HudAbilitySkillLevel),
                UBreakerGameplayAbility::SkillLevelFor(Character)),
            S(BreakerUI::HudAbilityOneX), Bottom - Tile - S(BreakerUI::Space12),
            BreakerUI::TextMuted, 10.0f, 1.0f, ESpecFontRole::Mono);
    }

    const UBreakerCombatComponent* Combat = Character->GetCombat();
    const bool bParryPreview = IsCapturePreview() && FParse::Param(FCommandLine::Get(), TEXT("BreakerCaptureParry"));
    if (Combat && !Combat->IsDead() && (bParryPreview || Combat->IsParryUnlocked()))
    {
        const float X = S(BreakerUI::HudAbilityOneX - 80.0f);
        const float Y = Bottom - Tile;
        const FVector2D Center(X + Tile * 0.5f, Y + Tile * 0.42f);
        const float Remaining = Combat->GetParryCooldownRemaining();
        const float Recovery = BreakerHUDMath::AbilityRecoveryFraction(Remaining, Combat->ParryCooldownSeconds);
        DrawRect(BreakerUI::BgBase, X, Y, Tile, Tile);
        DrawBorder(X, Y, Tile, Tile, BreakerUI::BorderRest, S(BreakerUI::BorderThin));
        DrawAbilityRecoveryDisc(Center, Tile * 0.30f, 1, BreakerUI::Panel20);
        DrawAbilityRecoveryDisc(Center, Tile * 0.30f, Recovery,
            BreakerUI::Alpha(BreakerUI::System, Combat->IsParryActive() ? 0.9f : 0.5f));
        // Shield outline: distinct from the three ability placeholders.
        const FVector2D Points[] = { {-12,-12}, {12,-12}, {10,5}, {0,14}, {-10,5}, {-12,-12} };
        for (int32 I = 1; I < UE_ARRAY_COUNT(Points); ++I)
            DrawLine(Center.X + S(Points[I-1].X), Center.Y + S(Points[I-1].Y),
                Center.X + S(Points[I].X), Center.Y + S(Points[I].Y), BreakerUI::TextPrimary, S(1.5f));
        static const TMap<FName, FKey> Defaults = UBreakerGameSettingsLibrary::FirstKeyPerAction(UBreakerGameSettingsLibrary::ProjectDefaultKeybinds());
        const FKey Key = Profile ? UBreakerGameSettingsLibrary::ResolveActionKey(TEXT("Parry"), Profile->KeybindOverrides, Defaults) : EKeys::V;
        const FString Hint = Key.GetDisplayName().ToString();
        const float KeyFont = 11.0f * FMath::Min(1.0f, (Tile - S(4)) / FMath::Max(1.0f, MeasureSpecText(Hint, 11, ESpecFontRole::Mono).X));
        DrawSpecTextCentered(Hint, Center.X, Y - S(16), BreakerUI::TextSecondary, KeyFont, 1.0f, ESpecFontRole::Mono);
        if (Combat->IsParryCounterActive())
            DrawSpecTextCentered(BreakerStrings::Get(EBreakerStringKey::HudParrySuccess), Center.X, Y - S(31), BreakerUI::TextPrimary, 10, 1.0f, ESpecFontRole::Display);
        const FString State = Remaining > 0 && !Combat->IsParryActive()
            ? BreakerHUDMath::AbilityCooldownText(Remaining)
            : BreakerStrings::Get(Combat->IsParryActive() ? EBreakerStringKey::HudParryActive
                : EBreakerStringKey::HudParryLabel);
        DrawSpecTextCentered(State, Center.X, Y + Tile - S(15), BreakerUI::TextPrimary, 10.0f, 1.0f, ESpecFontRole::Mono);
    }

    const UBreakerStatusCycleComponent* Cycle = Character->FindComponentByClass<UBreakerStatusCycleComponent>();
    bool bFracture = false;
    if (Abilities)
        for (EBreakerAbilitySlot Slot : { EBreakerAbilitySlot::ClassAbilityOne, EBreakerAbilitySlot::ClassAbilityTwo })
        {
            const UBreakerAbilityDefinition* Definition = Abilities->IsSlotGranted(Slot) ? Abilities->GetDefinitionForSlot(Slot) : nullptr;
            bFracture |= Definition && Abilities->GetEquippedAbilityId(Slot) == FName(TEXT("Caster.Fracture"));
        }
    // Visual-only capture fixture: no loadout changes, purchases, or saves.
    const bool bCaptureCycle = IsCapturePreview() && FParse::Param(FCommandLine::Get(), TEXT("BreakerCaptureCycle"));
    const bool bCaptureAhead = bCaptureCycle && FParse::Param(FCommandLine::Get(), TEXT("BreakerCaptureCyclePreview"));
    if ((bFracture || bCaptureCycle) && Cycle && Cycle->GetCycleLength() > 0)
    {
        const FString Current = Cycle->PeekNextEntry().DisplayName.ToString();
        const FString Label = (Cycle->CanPreviewAhead() || bCaptureAhead)
            ? BreakerStrings::Format(EBreakerStringKey::CyclePreview, *Current, *Cycle->PeekNextEntry(1).DisplayName.ToString())
            : BreakerStrings::Format(EBreakerStringKey::CycleCurrent, *Current);
        DrawSpecTextCentered(Label, S(BreakerUI::HudUltimateX + BreakerUI::HudUltimateTile * 0.5f), Bottom - Ultimate - S(22.0f), BreakerUI::TextPrimary, 11.0f, 1.0f, ESpecFontRole::Display);
    }
    if (Abilities && Abilities->GetGrantedCount() == 0)
    {
        DrawSpecTextCentered(BreakerStrings::Get(EBreakerStringKey::HudAbilitiesNoKit),
            S(BreakerUI::HudUltimateX + BreakerUI::HudUltimateTile * 0.5f), Bottom + S(BreakerUI::Space4),
            BreakerUI::Orange, 11.0f, 1.0f, ESpecFontRole::Display);
    }
}

// --------------------------------------------------------------------------
// Weapon — bottom-right, right edge 1880. Magazine and reserve on one
// baseline, the ammo rail beneath at rest, and the name only on a swap:
// sliding up the rail axis and fading over 1.2 s.
// --------------------------------------------------------------------------
void ABreakerPlaytestHUD::DrawWeaponReadout(const ABreakerCharacter* Character)
{
    const UBreakerWeaponComponent* Weapon = Character ? Character->GetWeapon() : nullptr;
    if (!Weapon || !Canvas) return;
    const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
    const bool bPreviewReloading = IsCapturePreview() && bPreviewReload;
    const bool bReloading = Weapon->IsReloading() || bPreviewReloading;

    const float Right = S(BreakerUI::HudWeaponRight);
    const float MagazineTop = S(BreakerUI::HudMagazineTop);
    const int32 Magazine = Weapon->GetMagazineAmmo();
    const int32 Capacity = Weapon->GetEffectiveMagazineSize();
    const FString MagazineText = FString::FromInt(Magazine);
    const FString ReserveText = BreakerUI::FormatTicker(static_cast<float>(Weapon->GetReserveAmmo()));
    const FVector2D MagazineSize = MeasureSpecText(MagazineText, BreakerUI::HudMagazinePixels, ESpecFontRole::Mono);
    const FVector2D ReserveSize = MeasureSpecText(ReserveText, BreakerUI::HudReservePixels, ESpecFontRole::Mono);
    DrawSpecTextRight(MagazineText, Right, MagazineTop,
        BreakerHUDMath::MagazineIsLow(Magazine, Capacity) ? BreakerUI::Orange : BreakerUI::System,
        BreakerUI::HudMagazinePixels, 1.0f, ESpecFontRole::Mono);
    // Reserve to the LEFT of the magazine, baseline-aligned.
    DrawSpecTextRight(ReserveText, Right - MagazineSize.X - S(BreakerUI::HudReserveGap),
        MagazineTop + MagazineSize.Y - ReserveSize.Y, BreakerUI::TextSecondary, BreakerUI::HudReservePixels, 1.0f, ESpecFontRole::Mono);

    // The ammo rail. At rest it is the magazine's fill in bone. During a reload
    // it is the reload's progress 0→100 left to right in weapon orange, read
    // off the reload timer's own clock so every affix and aura that moves the
    // duration moves the bar. The component answers -1 when it has the state
    // and not the clock — a non-authority client sees bReloading replicate
    // and never sets the timer — and the rail then states RELOADING as a full
    // orange bar rather than a progress it cannot know.
    const float RailX = S(BreakerUI::HudAmmoRailX);
    const float RailY = S(BreakerUI::HudAmmoRailY);
    const float RailW = S(BreakerUI::HudAmmoRailWidth);
    const float RailH = S(BreakerUI::HudAmmoRailHeight);
    const float MagazineFraction = Capacity > 0
        ? FMath::Clamp(static_cast<float>(Magazine) / static_cast<float>(Capacity), 0.0f, 1.0f) : 0.0f;
    const float ReloadFraction = bPreviewReloading ? PreviewReloadFraction : Weapon->GetReloadFraction();
    const float RailFraction = !bReloading ? MagazineFraction : (ReloadFraction >= 0.0f ? ReloadFraction : 1.0f);
    DrawTrack(RailX, RailY, RailW, RailH, RailFraction,
        bReloading ? BreakerUI::Orange : BreakerUI::System, BreakerUI::BorderRest);

    const bool bRampPreview = IsCapturePreview() && FParse::Param(FCommandLine::Get(), TEXT("BreakerCaptureDamageRamp"));
    if (Weapon->IsDamageRampEquipped() || bRampPreview)
    {
        const int32 Maximum = Weapon->GetDamageRampMaxStacks();
        const int32 Stacks = bRampPreview ? FMath::Min(7, Maximum) : Weapon->GetDamageRampStacks();
        const float Gap = S(3.0f);
        const float CellWidth = (RailW - Gap * (Maximum - 1)) / FMath::Max(1, Maximum);
        for (int32 Index = 0; Index < Maximum; ++Index)
            DrawRect(Index < Stacks ? BreakerUI::Orange : BreakerUI::BorderRest,
                RailX + Index * (CellWidth + Gap), RailY + S(12.0f), CellWidth, S(3.0f));
        DrawSpecTextRight(BreakerStrings::Format(EBreakerStringKey::HudDamageRamp, Stacks, Maximum),
            Right, RailY + S(20.0f), BreakerUI::TextSecondary, 11.0f, 1.0f, ESpecFontRole::Mono);
    }

    // The name on swap. Latched on the FALLING edge of IsSwapping, when the
    // new weapon is in the hand and its name is the right one; the preview
    // re-arms SwapStartTime on its own cadence.
    const bool bSwapping = Weapon->IsSwapping();
    if (bWasSwapping && !bSwapping)
    {
        SwapStartTime = Now;
        SwapName = Weapon->GetArchetypeName().ToUpper();
    }
    bWasSwapping = bSwapping;
    // THE NAME RESTS ON SCREEN NOW (owner, playtest 2026-09-10: "i dont know
    // what weapon is in my hand"). It was never missing — it was TRANSIENT. It
    // animated in on the swap and then left, so the answer to "what am I
    // holding" was only available in the two seconds after it changed, which is
    // exactly when the player already knows.
    //
    // The slide is kept as what it always was: the ENTRY. Sliding and settled
    // are the same readout at two moments, so the name is drawn from the live
    // archetype rather than the latched one and the slide supplies only the
    // offset and the alpha while it runs. Settled it is TextSecondary — present
    // to be found, not competing with the magazine count beside it.
    const BreakerHUDMath::FSwapSlide Slide = BreakerHUDMath::WeaponNameSwap(static_cast<float>(Now - SwapStartTime));
    const FString HeldName = Weapon->GetArchetypeName().ToUpper();
    if (!HeldName.IsEmpty())
    {
        const FVector2D NameSize = MeasureSpecText(HeldName, BreakerUI::HudWeaponNamePixels, ESpecFontRole::Display);
        const float SlideOffset = Slide.bVisible ? S(Slide.OffsetPixels) : 0.0f;
        DrawSpecTextRight(HeldName, Right,
            MagazineTop - NameSize.Y - S(BreakerUI::Space4) + SlideOffset,
            Slide.bVisible ? BreakerUI::TextPrimary : BreakerUI::TextSecondary,
            BreakerUI::HudWeaponNamePixels, Slide.bVisible ? Slide.Alpha : 1.0f, ESpecFontRole::Display);
    }
}

// --------------------------------------------------------------------------
// §2 — which resource this character carries. One member-pointer read per
// class component, in the order the classes were implemented; no component
// lookup, no actor iteration, nothing that was not already on this path.
//
// Only one of these can ever be active: the permanent class is one value, and
// each loop gates itself on it.
// --------------------------------------------------------------------------
BreakerHUD::FResourceRow ABreakerPlaytestHUD::ResolveResourceRow(const ABreakerCharacter* Character)
{
    if (!Character) return BreakerHUD::ResolveEmptyResourceRow();

    // ALL FIVE SHARE ONE MAXIMUM — MaxClassResource on the attribute set is what
    // every one of the five components clamps its own bank against — so the
    // numeric pair is read once here rather than threaded through five resolver
    // signatures and the fixtures behind them.
    const UBreakerAttributeSet* Attributes = Character->GetAttributes();
    const float MaxResource = Attributes ? Attributes->GetMaxClassResource() : 0.0f;

    if (const UBreakerMomentumComponent* Momentum = Character->GetMomentum(); Momentum && Momentum->IsActiveForOwner())
    {
        BreakerHUD::FResourceRow Row = BreakerHUD::ResolveMomentumRow(
            Momentum->GetMomentumFraction(), Momentum->GetMomentumState());
        BreakerHUD::SetResourceValue(Row, Momentum->GetMomentum(), MaxResource);
        return Row;
    }
    if (const UBreakerManaComponent* Mana = Character->GetMana(); Mana && Mana->IsActiveForOwner())
    {
        // GetManaFraction() clamps to [0,1] and so cannot express the debt;
        // the raw bank and the floor can, and both are already public.
        BreakerHUD::FResourceRow Row = BreakerHUD::ResolveManaRow(
            Mana->GetMana(), MaxResource, Mana->GetOvercastFloor());
        BreakerHUD::SetResourceValue(Row, Mana->GetMana(), MaxResource);
        return Row;
    }
    if (const UBreakerScrapComponent* Scrap = Character->GetScrap(); Scrap && Scrap->IsActiveForOwner())
    {
        BreakerHUD::FResourceRow Row = BreakerHUD::ResolveScrapRow(
            Scrap->GetScrapFraction(), Scrap->GetScrapState());
        BreakerHUD::SetResourceValue(Row, Scrap->GetScrap(), MaxResource);
        return Row;
    }
    if (const UBreakerGritComponent* Grit = Character->GetGrit(); Grit && Grit->IsActiveForOwner())
    {
        BreakerHUD::FResourceRow Row = BreakerHUD::ResolveGritRow(
            Grit->GetGritFraction(), Grit->GetGritBand());
        BreakerHUD::SetResourceValue(Row, Grit->GetGrit(), MaxResource);
        return Row;
    }
    if (const UBreakerChargeComponent* Charge = Character->GetCharge(); Charge && Charge->IsActiveForOwner())
    {
        BreakerHUD::FResourceRow Row = BreakerHUD::ResolveChargeRow(
            Charge->GetChargeFraction(), Charge->GetChargeBand());
        BreakerHUD::SetResourceValue(Row, Charge->GetCharge(), MaxResource);
        return Row;
    }
    return BreakerHUD::ResolveEmptyResourceRow();
}

// --------------------------------------------------------------------------
// The resource track: 480×8 on a 1px border-low, fill text-2 with a 12×8 mark
// at the fill edge and a 1×14 notch at 70 %. BANKED — the resource's own loud
// state, the one that widened its border — turns fill and mark bone and lights
// an 8×8 gold cell at the right end. No state word: the texture and the mark
// are the read.
//
// ONE FOOTPRINT, FIVE BEHAVIOURS. The sheet says each class behaves
// differently on this same track and the four non-Swift treatments are the
// plumbing the desk owes; every class draws Swift's — the fill textures the
// resolved row already carries (continuous / blocks / wide blocks) inside
// this footprint, and the Signed (Overcast) track's debt as a harm fill from
// the left, which states the sign and nothing more.
// --------------------------------------------------------------------------
void ABreakerPlaytestHUD::DrawResourceTrack(const BreakerHUD::FResourceRow& Row, float X, float Y, float Width, float Height)
{
    DrawRect(BreakerUI::BgBase, X, Y, Width, Height);

    const bool bBanked = Row.bActive && Row.BorderPixels >= 2.0f && Row.Fraction >= 0.0f;
    const bool bDebt = Row.Track == BreakerHUD::EResourceTrack::Signed && Row.Fraction < 0.0f;
    // The loud states own the fill; beneath them the row's own resting fill
    // (Scrap's weapon orange, text-2 for the rest).
    const FLinearColor Fill = bDebt ? BreakerUI::Harm : bBanked ? BreakerUI::System : Row.FillColor;
    const float Magnitude = FMath::Clamp(FMath::Abs(Row.Fraction), 0.0f, 1.0f);
    const float FillW = Width * Magnitude;

    switch (Row.Track)
    {
    case BreakerHUD::EResourceTrack::Blocks:
    case BreakerHUD::EResourceTrack::WideBlocks:
        if (FillW > 0.0f)
        {
            // Chevron-cut blocks: the texture itself changes with state, so
            // peripheral vision reads the tier without a word.
            const bool bWide = Row.Track == BreakerHUD::EResourceTrack::WideBlocks;
            const float BlockW = S(bWide ? 14.0f : 8.0f);
            const float BlockGap = S(3.0f);
            const float Shear = S(3.0f);
            for (float BX = X; BX < X + FillW - BlockW * 0.5f; BX += BlockW + BlockGap)
            {
                DrawShearedBlock(BX, Y, FMath::Min(BlockW, X + FillW - BX), Height, Shear, Fill);
            }
        }
        break;

    case BreakerHUD::EResourceTrack::Continuous:
    case BreakerHUD::EResourceTrack::Signed:
        if (FillW > 0.0f) DrawRect(Fill, X, Y, FillW, Height);
        break;

    case BreakerHUD::EResourceTrack::Empty:
    default:
        break;
    }

    // The mark: a 12×8 triangle standing at the fill edge, apex on the
    // track's top edge, in the fill's own colour.
    if (Row.bActive && FillW > 0.0f)
    {
        const float MarkW = S(BreakerUI::HudResourceMarkWidth);
        const float MarkH = S(BreakerUI::HudResourceMarkHeight);
        const float EdgeX = X + FillW;
        DrawTriangle(FVector2D(EdgeX, Y), FVector2D(EdgeX + MarkW * 0.5f, Y + MarkH),
            FVector2D(EdgeX - MarkW * 0.5f, Y + MarkH), Fill);
    }

    // The notch at 70 %: 1×14, taller than the track, in border-high so it
    // reads over fill and ground alike.
    const float NotchH = S(BreakerUI::HudResourceNotchHeight);
    DrawRect(BreakerUI::BorderHigh, X + Width * BreakerUI::HudResourceNotchFraction, Y + (Height - NotchH) * 0.5f,
        FMath::Max(S(1.0f), 1.0f), NotchH);
    // The row's own band edges (Grit's thirds), the same 1×14 mark, so a band
    // reads off the bar without its word.
    for (const float Mark : Row.StepMarks)
    {
        DrawRect(BreakerUI::BorderHigh, X + Width * FMath::Clamp(Mark, 0.0f, 1.0f), Y + (Height - NotchH) * 0.5f,
            FMath::Max(S(1.0f), 1.0f), NotchH);
    }

    // Banked: the gold cell at the right end.
    if (bBanked)
    {
        const float Cell = S(BreakerUI::HudResourceBankedCell);
        DrawRect(BreakerUI::Gold, X + Width - Cell, Y + (Height - Cell) * 0.5f, Cell, Cell);
    }

    DrawBorder(X, Y, Width, Height, BreakerUI::BorderRest, S(BreakerUI::BorderThin));
}

// --------------------------------------------------------------------------
// Top-left: the zone name at (40, 40) and the wave-cell row beneath at
// (40, 76): one 16×8 cell per wave of the run (O120: only where a total is
// authored — a rift run's total is its boss wave; the gym has none and draws
// none), done in text-2, the current wave in bone, the rest in the resting
// border. The mm:ss countdown to the next wave sits right of the cells —
// empty out of combat, empty when nothing is counting.
//
// The current Rift name takes precedence; ordinary prototype regions name
// the nearest authored district and its fixed level. Other maps leave this
// line vacant. Boss phase remains on the enemy nameplate.
// --------------------------------------------------------------------------
void ABreakerPlaytestHUD::DrawZoneLine(const ABreakerCharacter* Character)
{
    if (!Character || !Canvas) return;
    bool bRiftSet = false;
    ZoneName.Reset();
    if (const UBreakerGameInstance* Session = GetWorld() ? GetWorld()->GetGameInstance<UBreakerGameInstance>() : nullptr)
    {
        bRiftSet = Session->PendingRift.IsSet();
        if (bRiftSet) ZoneName = Session->PendingRift.AreaName.ToString().ToUpper();
    }
    if (!bRiftSet)
    {
        if (const auto* Region = BreakerPrototypeDestinations::ForWorld(Character))
        {
            // District labels follow the closest authored centre in the ground plane.
            // This is presentation only: patrol and loot levels remain authored at spawn.
            int32 Closest = INDEX_NONE;
            double ClosestDistance = TNumericLimits<double>::Max();
            for (int32 Index = 0; Index < Region->Districts.Num(); ++Index)
            {
                if (!Region->DistrictNames.IsValidIndex(Index) || !Region->AreaLevels.IsValidIndex(Index)) continue;
                const double Distance = FVector::DistSquared2D(Character->GetActorLocation(), Region->Districts[Index]);
                if (Distance < ClosestDistance)
                {
                    Closest = Index;
                    ClosestDistance = Distance;
                }
            }
            if (Closest != INDEX_NONE)
                ZoneName = FString::Printf(TEXT("%s · LEVEL %d"),
                    *Region->DistrictNames[Closest].ToUpper(), Region->AreaLevels[Closest]);
        }
    }
    if (!ZoneName.IsEmpty())
    {
        const float Pixels = bRiftSet ? BreakerUI::HudZonePixels : FitSpecPixels(ZoneName,
            BreakerUI::HudZonePixels, S(BreakerUI::HudQuestTrackerWidth),
            BreakerUI::HudQuestLinePixels, ESpecFontRole::Display);
        DrawSpecText(ZoneName, S(BreakerUI::HudZoneLeft), S(BreakerUI::HudZoneTop), BreakerUI::System, Pixels, 1.0f, ESpecFontRole::Display);
    }

    const ABreakerGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ABreakerGameMode>() : nullptr;
    float CountdownX = S(BreakerUI::HudZoneLeft);
    const int32 CellTotal = GameMode
        ? BreakerHUDMath::WaveCellTotal(bRiftSet, GameMode->WaveBudget.BossWaveInterval) : 0;
    if (CellTotal > 0)
    {
        // Cell geometry: 16×8 at a 4 gap, the countdown 16 past the last cell.
        // O2 PLACEHOLDER, the sheet's row with no felt number yet.
        const float CellW = S(16.0f);
        const float CellH = S(8.0f);
        const float CellGap = S(4.0f);
        const int32 CurrentWave = GameMode->GetCurrentWave();
        float CellX = S(BreakerUI::HudZoneLeft);
        // The row's vertical centre sits on the countdown numeral's centre.
        const float CellY = S(BreakerUI::HudCountdownTop) + (S(BreakerUI::HudCountdownPixels) - CellH) * 0.5f;
        for (int32 Wave = 1; Wave <= CellTotal; ++Wave)
        {
            const FLinearColor Cell = Wave < CurrentWave ? BreakerUI::TextSecondary
                : Wave == CurrentWave ? BreakerUI::System : BreakerUI::BorderRest;
            DrawRect(Cell, CellX, CellY, CellW, CellH);
            CellX += CellW + CellGap;
        }
        CountdownX = CellX - CellGap + S(16.0f);
    }

    const float Remaining = GameMode ? GameMode->GetWaveAdvanceRemaining() : -1.0f;
    // THE WAVE CLEAR is the countdown's <0 -> >0 edge: the mode starts
    // counting to the next wave the moment the current one is emptied, so the
    // wave that just ended is GetCurrentWave() (a real value, O120). The
    // countdown before wave 1 is not a clear and enqueues nothing. The LAST
    // wave produces no edge — nothing counts after the boss wave — and that
    // is the rift-complete moment, banner'd from OnRiftCompleted instead.
    // The row's own text is the mm:ss countdown and nothing else, so nothing
    // here duplicates the banner and nothing is dropped.
    if (LastWaveAdvanceRemaining < 0.0f && Remaining > 0.0f && GameMode && GameMode->GetCurrentWave() > 0)
    {
        EnqueueBanner(EBreakerBannerKind::WaveClear,
            BreakerStrings::Format(EBreakerStringKey::HudBannerWaveClear, GameMode->GetCurrentWave()), FString());
    }
    LastWaveAdvanceRemaining = Remaining;
    const FString Countdown = BreakerHUDMath::FormatCountdown(Remaining);
    if (!Countdown.IsEmpty())
    {
        DrawSpecText(Countdown, CountdownX, S(BreakerUI::HudCountdownTop),
            BreakerUI::TextSecondary, BreakerUI::HudCountdownPixels, 1.0f, ESpecFontRole::Mono);
    }
}

// --------------------------------------------------------------------------
// The quest line — one right-aligned string at (1880, 40), body 14, text-2,
// on every map: a contract accepted in camp is worked in the field.
//
// DERIVED, never stored: quest state is a pure function of the journal's flag
// set (Save/BreakerQuestContent.h), so this line asks ComputeQuestState and
// the counters and can never disagree with the dialogue system about where a
// quest stands. The strings are the tracker's own, and the two verbs are
// UBreakerMissionLibrary's; no new player-facing words (O195).
//
// THE MISSION FIRST. A mission's current beat (Save/BreakerMissionContent.h)
// is the story's ask, and its TrackerLine outranks the quest's own state: a
// Travel beat says where to go when no quest says anything. A beat whose
// line is empty (Reward, Unlock) falls through to the quest line below. At a
// fresh save the first beat is a Dialogue and is current before its quest is
// Offered, so SPEAK TO THE QUARTERMASTER shows from the first frame.
//
//   Offered       -> SpeakToVerb over the giver
//   Active        -> the first unfinished objective, with its counter
//   ReadyToTurnIn -> ReturnToVerb over the giver
// NotOffered and Complete draw nothing — an empty corner is the truthful
// state, not a placeholder's.
// --------------------------------------------------------------------------
void ABreakerPlaytestHUD::DrawQuestLine(const ABreakerCharacter* Character)
{
    const UBreakerQuestJournal* Journal = Character ? Character->GetQuestJournal() : nullptr;
    if (!Journal || !Canvas) return;

    // Fitted into the 320 rather than trusted to be short; never wrapped.
    const float Right = S(BreakerUI::HudWeaponRight);
    const float Limit = S(BreakerUI::HudQuestTrackerWidth);
    FBreakerLocalMapMarker MapTarget;
    if (Character->GetLocalMap()->GetTrackedMarker(MapTarget))
    {
        const FString Direction = FString::Printf(TEXT("%s · %dm"), *MapTarget.Label.ToString(),
            FMath::RoundToInt(FVector::Dist2D(Character->GetActorLocation(), MapTarget.Location) / 100));
        DrawSpecTextRight(Direction, Right, S(BreakerUI::HudQuestLineTop + 46), BreakerUI::Gold,
            FitSpecPixels(Direction, BreakerUI::HudQuestLinePixels, Limit, 11.0f));
    }

    // These side destinations have a live cache objective, not a campaign
    // dialogue beat. Share the map's actual state without writing quest flags.
    if (BreakerPrototypeDestinations::ForWorld(Character))
    {
        FString Objective=Character->GetLocalMap()->GetCampaignObjective().ToString();
        FString Detail;
        // Reuse the map's two authored sentences in the existing two-line
        // tracker space; retain the tracked-site direction below them.
        const int32 Sentence=Objective.Find(TEXT(". "));
        if (Sentence!=INDEX_NONE)
        {
            Detail=Objective.Mid(Sentence+2);
            Objective=Objective.Left(Sentence+1);
        }
        if (!Objective.IsEmpty())
            DrawSpecTextRight(Objective,Right,S(BreakerUI::HudQuestLineTop),BreakerUI::TextSecondary,
                FitSpecPixels(Objective,BreakerUI::HudQuestLinePixels,Limit,11.0f));
        if (!Detail.IsEmpty())
            DrawSpecTextRight(Detail,Right,S(BreakerUI::HudQuestLineTop+22),BreakerUI::TextSecondary,
                FitSpecPixels(Detail,BreakerUI::HudQuestLinePixels,Limit,11.0f));
        return;
    }

    for (const FBreakerMissionDefinition& Mission : UBreakerMissionLibrary::GetMissions())
    {
        const FBreakerMissionBeat* Beat = UBreakerMissionLibrary::CurrentBeat(Mission, Journal->GetState());
        if (!Beat) continue;
        const FString BeatLine = UBreakerMissionLibrary::TrackerLine(*Beat, Journal->GetState());
        if (BeatLine.IsEmpty()) continue;
        DrawSpecTextRight(BeatLine, Right, S(BreakerUI::HudQuestLineTop), BreakerUI::TextSecondary,
            FitSpecPixels(BeatLine, BreakerUI::HudQuestLinePixels, Limit, 11.0f));
        if (Beat->WorldEncounter == FName(TEXT("earth.survivor_extraction")))
        {
            for (TActorIterator<ABreakerSurvivor> It(GetWorld()); It; ++It)
            {
                FString Detail;
                if (It->IsEscortActive())
                {
                    const int32 Seconds = FMath::CeilToInt(It->GetLucidityRemaining());
                    Detail = FString::Printf(TEXT("LUCIDITY %d:%02d  |  STAY CLOSE"), Seconds / 60, Seconds % 60);
                }
                else Detail = TEXT("RETURN TO THE SHELTER TO RETRY");
                DrawSpecTextRight(Detail, Right, S(BreakerUI::HudQuestLineTop + 22), BreakerUI::TextSecondary,
                    FitSpecPixels(Detail, BreakerUI::HudQuestLinePixels, Limit, 11.0f));
                break;
            }
        }
        return;
    }

    // The first quest that is live in any form is the tracked one. The slice
    // ships one quest; when the campaign ships more, "first live" is still the
    // right minimal policy for one line, and a picker can replace it.
    const FBreakerQuestDefinition* Tracked = nullptr;
    EBreakerQuestState TrackedState = EBreakerQuestState::NotOffered;
    for (const FBreakerQuestDefinition& Quest : UBreakerQuestLibrary::GetFallbackQuests())
    {
        const EBreakerQuestState State = UBreakerQuestLibrary::ComputeQuestState(Quest, Journal->GetState());
        if (State == EBreakerQuestState::Offered || State == EBreakerQuestState::Active
            || State == EBreakerQuestState::ReadyToTurnIn)
        {
            Tracked = &Quest;
            TrackedState = State;
            break;
        }
    }
    if (!Tracked) return;

    FString Line;
    if (TrackedState == EBreakerQuestState::Offered)
    {
        Line = FString::Printf(UBreakerMissionLibrary::SpeakToVerb, *Tracked->Giver.ToUpper());
    }
    else if (TrackedState == EBreakerQuestState::ReadyToTurnIn)
    {
        Line = FString::Printf(UBreakerMissionLibrary::ReturnToVerb, *Tracked->Giver.ToUpper());
    }
    else
    {
        for (const FBreakerQuestObjective& Objective : Tracked->Objectives)
        {
            if (Journal->HasFlag(Objective.CompletionFlag)) continue;
            Line = Objective.Text.ToUpper();
            if (Objective.RequiredCount > 0)
            {
                const int32 Count = FMath::Clamp(Journal->GetCounter(Objective.ProgressCounter), 0, Objective.RequiredCount);
                Line += FString::Printf(TEXT("  %d/%d"), Count, Objective.RequiredCount);
            }
            break;
        }
    }
    if (Line.IsEmpty()) return;

    DrawSpecTextRight(Line, Right, S(BreakerUI::HudQuestLineTop), BreakerUI::TextSecondary,
        FitSpecPixels(Line, BreakerUI::HudQuestLinePixels, Limit, 11.0f));
}

// --------------------------------------------------------------------------
// Floating damage numbers. The timeline is the sheet's (pop, settle, rise,
// fade; a crit holds longer) through BreakerHUDMath::DamageNumberFrame; the
// sizes are the play measurements (O207); crit is weapon orange and gold is
// the weak-point promise (O208). Clusters stack instead of overlapping, and
// the fourth number in one cluster is dropped rather than drawn.
// --------------------------------------------------------------------------
void ABreakerPlaytestHUD::DrawDamageNumbers()
{
    if (DamageNumbers.Num() == 0) return;
    const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;

    // The ring buffer is unordered; resolving oldest-first makes the stacking
    // rule stable frame to frame instead of shuffling with the write cursor.
    TArray<const FBreakerHUDDamageNumber*> Visible;
    Visible.Reserve(DamageNumbers.Num());
    for (const FBreakerHUDDamageNumber& Number : DamageNumbers)
    {
        const float Age = static_cast<float>(Now - Number.Time);
        if (Age < 0.0f || Age >= Number.Lifetime) continue;
        Visible.Add(&Number);
    }
    Visible.Sort([](const FBreakerHUDDamageNumber& A, const FBreakerHUDDamageNumber& B) { return A.Time < B.Time; });

    TArray<FVector2D> Placed;
    Placed.Reserve(Visible.Num());
    TArray<FBox2D> PlacedLabels;
    PlacedLabels.Reserve(Visible.Num() + EnemyPlateBounds.Num());
    PlacedLabels.Append(EnemyPlateBounds);
    const float ClusterRadius = S(BreakerHUD::DamageClusterRadius);

    // WHERE THE PLAYER IS, read once for the whole pass rather than per number.
    const APawn* Viewer = GetOwningPawn();
    const bool bHasViewer = Viewer != nullptr;
    const FVector ViewerAt = bHasViewer ? Viewer->GetActorLocation() : FVector::ZeroVector;

    for (const FBreakerHUDDamageNumber* Number : Visible)
    {
        // OUT OF READING RANGE, so not printed. Owner: "only used when in
        // effective ranges". A yard is 106 m long and a number at the far
        // pocket is a smear over a target too small to attach it to.
        if (bHasViewer && FVector::DistSquared(ViewerAt, Number->World)
            > FMath::Square(BreakerUI::DamageMaxDrawDistanceCm)) continue;
        const FVector Projected = Project(Number->World, false);
        if (Projected.Z <= 0.0f) continue;
        const FVector2D Screen(Projected.X, Projected.Y);

        int32 Neighbours = 0;
        for (const FVector2D& Other : Placed)
        {
            if (FVector2D::Distance(Other, Screen) <= ClusterRadius) ++Neighbours;
        }
        if (Neighbours >= BreakerHUD::DamageClusterMax) continue;


        const float Age = static_cast<float>(Now - Number->Time);

        // --- The hierarchy. A number tells you WHAT you did before you read
        // it: DoT ticks are small and grey and die young; body hits are
        // mid-grey and modest; weak points are gold (the aim-skill lane);
        // crits are orange and big; kills multiply whatever their kind earned
        // and body-shot kills brighten to full white — the heaviest neutral
        // read on the ramp. Secondary (chain/ricochet/AoE spill) draws lighter
        // than its parent. Colour separates KIND, size separates WEIGHT.
        FLinearColor Face = BreakerUI::TextSecondary;
        float SizePixels = BreakerUI::DamageBodyPixels;
        float PopScale = BreakerHUD::DamagePopScale;
        if (Number->bCritical)
        {
            Face = BreakerUI::Orange;
            SizePixels = BreakerUI::DamageCritPixels;
            PopScale = BreakerHUD::DamageCritPopScale;
        }
        else if (Number->bWeakPoint)
        {
            Face = BreakerUI::Gold;
            SizePixels = BreakerUI::DamageWeakPointPixels;
        }
        else if (Number->bFromDoT)
        {
            // A DoT tick that crits or lands a weak point keeps its accent
            // above — those reads outrank the source. Plain ticks stay legible.
            Face = BreakerUI::TextSecondary;
            SizePixels = BreakerUI::DamageDoTPixels;
        }

        // Subtle size-by-magnitude, log not linear: a decade over the
        // reference adds a twentieth, capped before it can cross kinds.
        if (Number->Value > BreakerHUD::DamageMagnitudeReference)
        {
            const float Decades = FMath::LogX(10.0f, Number->Value / BreakerHUD::DamageMagnitudeReference);
            SizePixels *= FMath::Min(1.0f + Decades * BreakerHUD::DamageMagnitudeGainPerDecade,
                BreakerHUD::DamageMagnitudeScaleCap);
        }

        if (Number->bKilled)
        {
            SizePixels *= BreakerUI::DamageKillScale;
            PopScale = FMath::Max(PopScale, BreakerHUD::DamageCritPopScale);
            // A body-shot kill brightens to full white. Crit and weak-point
            // kills keep their accents — the accent is the rarer read.
            if (!Number->bCritical && !Number->bWeakPoint) Face = BreakerUI::TextPrimary;
        }

        if (Number->bSecondary)
        {
            SizePixels *= BreakerHUD::DamageSecondaryScale;
        }
        // The profile's slider, last, over the whole hierarchy: it scales the
        // read, never the ranking between kinds.
        if (Profile) SizePixels *= Profile->DamageNumberScale;
        // ABSORBED. The Warden's whole mechanic is that its FRONT is the wrong
        // place to shoot, and until now the only report of that was the health
        // bar not moving — which reads as a broken game, not as a wrong angle.
        //
        // The number RECEDES rather than changing family: it drops to
        // text/muted, whatever it would otherwise have been. The first pass
        // used OrangeDeep and it was WRONG when looked at — an absorbed crit
        // in OrangeDeep sits one value step from an ordinary crit in Orange,
        // so the two states that most need separating were the two that read
        // most alike. Muted grey is unambiguous against all three of white,
        // gold and orange, and it says the right thing on sight: this one did
        // not land. The mitigation caption underneath carries the accent, so
        // the eye still gets one orange mark to catch.
        //
        // The SIZE hierarchy is untouched. A crit that gets absorbed is still
        // a crit and still 52px — the sizes are the only thing separating a
        // body shot from a weak point from a crit, and losing them here would
        // delete that separation exactly when the player most needs it.
        const bool bAbsorbed = Number->MitigatedFraction >= BreakerUI::DamageAbsorbedThreshold;
        if (bAbsorbed) Face = BreakerUI::TextMuted;

        // The timeline: pop to PopScale, settle to 1, rise easing out, fade
        // over the last 300 ms of whatever lifetime this kind was given. A
        // DoT tick, plain and bookkeeping, does not pop.
        const BreakerHUDMath::FDamageNumberFrame Frame = BreakerHUDMath::DamageNumberFrame(
            Age, Number->Lifetime, Number->bFromDoT && !Number->bCritical && !Number->bWeakPoint ? 1.0f : PopScale);
        SizePixels *= Frame.Scale;
        const float Rise = S(BreakerUI::DamageRisePixels) * Frame.RiseFraction;
        const float Fade = Frame.Alpha;

        // Secondary hits are lighter as well as smaller: the parent owns the
        // full weight of the trigger pull.
        const float DrawAlpha = Number->bSecondary ? Fade * 0.8f : Fade;

        float NumberY = Screen.Y - Rise;
        const bool bRotTick = Number->bFromDoT && Number->Element == EBreakerElement::Entropy
            && Number->DamageTypeTag == FGameplayTag::RequestGameplayTag(TEXT("Status.Rot"));
        const bool bErasedBurst = Number->Element == EBreakerElement::Void
            && Number->DamageTypeTag == FGameplayTag::RequestGameplayTag(TEXT("Status.Erased"));
        const bool bUnstableBurst = Number->Element == EBreakerElement::Rift
            && Number->DamageTypeTag == FGameplayTag::RequestGameplayTag(TEXT("Status.Unstable"));
        const bool bCollapse = Number->DamageTypeTag == FGameplayTag::RequestGameplayTag(TEXT("Reaction.Collapse"));
        const bool bWither = Number->DamageTypeTag == FGameplayTag::RequestGameplayTag(TEXT("Reaction.Wither"));
        const bool bTear = Number->DamageTypeTag == FGameplayTag::RequestGameplayTag(TEXT("Reaction.Tear"));
        const FString DamageText = bCollapse
            ? BreakerStrings::Format(EBreakerStringKey::HudCollapseDamage, *BreakerUI::FormatDamage(Number->Value)) : bWither
            ? BreakerStrings::Format(EBreakerStringKey::HudWitherDamage, *BreakerUI::FormatDamage(Number->Value)) : bTear
            ? BreakerStrings::Format(EBreakerStringKey::HudTearDamage, *BreakerUI::FormatDamage(Number->Value)) : bUnstableBurst
            ? BreakerStrings::Format(EBreakerStringKey::HudUnstableDamage, *BreakerUI::FormatDamage(Number->Value)) : bErasedBurst
            ? BreakerStrings::Format(EBreakerStringKey::HudErasedDamage, *BreakerUI::FormatDamage(Number->Value)) : bRotTick
            ? BreakerStrings::Format(EBreakerStringKey::HudRotDamage, *BreakerUI::FormatDamage(Number->Value))
            : BreakerUI::FormatDamage(Number->Value);
        // Compare the final animated glyph bounds, not the world anchor or an
        // average offset. Different rises and pop scales can otherwise place
        // ROT and ERASED on the same line even inside the old cluster budget.
        const FVector2D MainSize = MeasureSpecText(DamageText, SizePixels, ESpecFontRole::Mono);
        FString CaptionText;
        // The overkill caption goes with it, by the same ruling. The absorbed
        // caption stays: that one says the hit did not land, which is a thing
        // the player has to act on rather than a bigger number.
        if (bAbsorbed)
            CaptionText = BreakerStrings::Format(EBreakerStringKey::HudDamageAbsorbed, Number->MitigatedFraction * 100.0f);
        const FVector2D CaptionSize = CaptionText.IsEmpty() ? FVector2D::ZeroVector
            : MeasureSpecText(CaptionText, 13.0f, ESpecFontRole::Mono);
        const float Outline = FMath::Max(S(FMath::Max(SizePixels, 13.0f) * .05f), 1.0f);
        const float HalfWidth = FMath::Max(MainSize.X, CaptionSize.X) * .5f + Outline;
        const float LabelHeight = MainSize.Y + CaptionSize.Y + 2 * Outline;
        const float LabelGap = S(3.0f); // O2 presentation spacing between actual glyph boxes.
        FBox2D Label(FVector2D(Screen.X - HalfWidth, NumberY - Outline),
            FVector2D(Screen.X + HalfWidth, NumberY - Outline + LabelHeight));
        for (int32 Pass = 0; Pass <= PlacedLabels.Num(); ++Pass)
        {
            bool bMoved = false;
            for (const FBox2D& Other : PlacedLabels)
                if (Label.Intersect(Other))
                {
                    const float Shift = Label.Max.Y - Other.Min.Y + LabelGap;
                    Label.Min.Y -= Shift;
                    Label.Max.Y -= Shift;
                    NumberY -= Shift;
                    bMoved = true;
                }
            if (!bMoved) break;
        }
        if (Label.Min.X < 0 || Label.Max.X > Canvas->ClipX || Label.Min.Y < 0 || Label.Max.Y > Canvas->ClipY) continue;
        Placed.Add(Screen);
        PlacedLabels.Add(Label);
        DrawOutlinedNumber(DamageText,
            Screen.X, NumberY, Face, SizePixels, DrawAlpha);

        // The overkill share of a killing blow, stated as its own mark in the
        // harm accent under the number: the number says how hard the blow
        // was, the caption says how much of it the corpse never felt. Skipped
        // when trivial — a sliver of overkill is trivia, not a read.
        if (Number->bKilled && Number->Overkill >= Number->Value * BreakerHUD::DamageOverkillCaptionFraction)
        {
            const float NumberHeight = MainSize.Y;
            DrawOutlinedNumber(BreakerStrings::Format(EBreakerStringKey::HudDamageOverkill, *BreakerUI::FormatDamage(Number->Overkill)),
                Screen.X, NumberY + NumberHeight, BreakerUI::Harm, 13.0f, DrawAlpha);
        }
        else if (bAbsorbed)
        {
            // Caption under the number, at caption weight so it annotates
            // rather than competes — the same relationship the class-resource
            // state word has to its track. Position is MEASURED off the
            // number's own glyph height, not a fixed nudge, so it holds at
            // every one of the three damage sizes and at every UI scale.
            const FString Caption = BreakerStrings::Format(EBreakerStringKey::HudDamageAbsorbed, Number->MitigatedFraction * 100.0f);
            const float NumberHeight = MainSize.Y;
            DrawOutlinedNumber(Caption, Screen.X, NumberY + NumberHeight,
                BreakerUI::Orange, 13.0f, Fade);
        }
    }
}

void ABreakerPlaytestHUD::DrawInteractPrompt(const ABreakerCharacter* Character, const FVector2D& Center)
{
    // ONE PROMPT, MIRRORING F'S OWN PRECEDENCE (feedstock beats loot beats travel beats
    // talk — InteractWithNearbyNPC's order, restated here so the prompt can
    // never advertise a verb the key would not perform). This used to be an
    // NPC-only prompt, which made "F TALK" the sole key-bearing affordance
    // in the game; travel and loot now speak too.
    if (!Character) return;
    if (const auto* Feedstock = Character->FindNearbyFeedstock())
    {
        FString Label(TEXT("COLLECT FEEDSTOCK"));
        for (const auto& Quest : UBreakerQuestLibrary::GetFallbackQuests())
            if (Quest.QuestId == FName(TEXT("Quest.KessSalvage")))
                for (const auto& Objective : Quest.Objectives)
                    if (Objective.ObjectiveId == FName(TEXT("Feedstock")))
                    {
                        const int32 Count = Character->GetQuestJournal()->GetState().Counters.FindRef(Objective.ProgressCounter);
                        Label += FString::Printf(TEXT("  %d/%d"), FMath::Clamp(Count, 0, Objective.RequiredCount), Objective.RequiredCount);
                    }
        const FVector Projected = Project(Feedstock->GetActorLocation() + FVector(0,0,40), false);
        if (Projected.Z > 0) DrawInteractPlate(Projected.X, Projected.Y, BreakerUI::Gold, 1, Label, true);
        return;
    }
#if BREAKER_HAS_LOOT_PICKUP
    if (Character->FindNearbyPickup())
    {
        // The key rides the plate over the drop (DrawLootPickups puts the
        // tile on the one F would take). What is left to say here is the
        // refusal: O183 made a full backpack REFUSE — TryPickup returns false
        // and the drop stays on the ground — so the plate carries no key over
        // a full bag and this line says why.
        //
        // SAID BEFORE THE PRESS, NOT AFTER IT. The character discards
        // TryPickup's bool, so there is no refusal event to react to without a
        // seam from another lane — but none is needed, because the cap is a
        // public read and the better feedback is preventive anyway: a player
        // who is told the bag is full never spends a dead keypress finding out.
        // The count is stated rather than implied, because "FULL" without a
        // number is a complaint and 25/25 is an instruction.
        const UBreakerEquipmentComponent* Equipment = Character->GetEquipment();
        const int32 Carried = Equipment ? Equipment->GetBackpack().Num() : 0;
        const bool bBackpackFull = Equipment
            && Carried >= UBreakerEquipmentComponent::BackpackCapacity;
        if (bBackpackFull)
        {
            DrawSpecTextCentered(
                BreakerStrings::Format(EBreakerStringKey::HudBackpackFull, Carried, UBreakerEquipmentComponent::BackpackCapacity),
                Center.X, Center.Y + S(90.0f), BreakerUI::Orange, 14.0f, 1.0f, ESpecFontRole::Mono);
        }
        return;
    }
#endif
    // The plate over the travel point or the NPC: rail text-2 (neither is a
    // rarity), one cell, the key tile, and the prompt's own words. Anchored
    // where the over-actor label would have been — that label yields to it.
    if (const ABreakerTravelPoint* Travel = Character->FindNearbyTravelPoint())
    {
        const FVector Projected = Project(Travel->GetActorLocation() + FVector(0.0f, 0.0f, 260.0f), false);
        if (Projected.Z <= 0.0f) return;
        DrawInteractPlate(Projected.X, Projected.Y, BreakerUI::TextSecondary, 1,
            Travel->GetPromptLabel().ToString().ToUpper(), true);
        return;
    }
    if (const ABreakerNPC* NearbyNPC = Character->FindNearbyNPC())
    {
        // Low cache/recorder consoles need a body anchor; the person-head offset leaves the viewport at normal use range.
        const FVector PromptAnchor = (Cast<ABreakerFernhallCache>(NearbyNPC) || Cast<ABreakerBasinRecorder>(NearbyNPC)
            || Cast<ABreakerCoastalUplink>(NearbyNPC) || Cast<ABreakerSupplyChest>(NearbyNPC)) ? NearbyNPC->GetActorLocation()
            : NearbyNPC->GetActorLocation() + FVector(0.0f, 0.0f, 150.0f);
        const FVector Projected = Project(PromptAnchor, false);
        if (Projected.Z <= 0.0f) return;
        const auto* Uplink = Cast<ABreakerCoastalUplink>(NearbyNPC);
        DrawInteractPlate(Projected.X, Projected.Y, BreakerUI::TextSecondary, 1,
            Uplink ? Uplink->GetUplinkPrompt(Character).ToString()
                : Cast<ABreakerBasinRecorder>(NearbyNPC) ? Cast<ABreakerBasinRecorder>(NearbyNPC)->GetRecorderPrompt().ToString()
                : Cast<ABreakerFernhallCache>(NearbyNPC) ? Cast<ABreakerFernhallCache>(NearbyNPC)->GetCachePrompt().ToString()
                : Cast<ABreakerSupplyChest>(NearbyNPC) ? Cast<ABreakerSupplyChest>(NearbyNPC)->GetChestPrompt().ToString()
                : BreakerStrings::Format(EBreakerStringKey::HudPromptTalkNamed, *NearbyNPC->GetDisplayName().ToString().ToUpper()), !Uplink || !Uplink->IsTransmitting());
    }
}

// --------------------------------------------------------------------------
// One plate over a thing F acts on. Geometry is BreakerHUDMath's
// InteractPlateLayout in spec pixels, scaled once here; the name is measured
// before the plate is sized, so nothing reads its own arrangement.
// --------------------------------------------------------------------------
void ABreakerPlaytestHUD::DrawInteractPlate(float CenterX, float BottomY, const FLinearColor& Rail, int32 TallyCells,
    const FString& Name, bool bKeyTile)
{
    if (!Canvas) return;
    const FVector2D NameSize = MeasureSpecText(Name, BreakerHUD::InteractPlateNamePixels, ESpecFontRole::Display);
    const BreakerHUDMath::FInteractPlateLayout Layout = BreakerHUDMath::InteractPlateLayout(
        NameSize.X / FMath::Max(UIScale, UE_KINDA_SMALL_NUMBER), TallyCells, bKeyTile);
    const float W = S(Layout.Width);
    const float H = S(Layout.Height);
    const float X = CenterX - W * 0.5f;
    const float Y = BottomY - H;
    DrawPlate(X, Y, W, H, Rail);

    // The key tile: a bone square with the key struck out of it in void, so
    // the affordance reads as the system's and not the item's.
    if (Layout.bKeyTile)
    {
        const float KeySize = S(Layout.KeySize);
        const float KeyX = X + S(Layout.KeyX);
        const float KeyY = Y + (H - KeySize) * 0.5f;
        DrawRect(BreakerUI::System, KeyX, KeyY, KeySize, KeySize);
        const FVector2D KeyGlyph = MeasureSpecText(TEXT("F"), BreakerUI::HudAbilityKeyPixels, ESpecFontRole::Mono);
        DrawSpecTextCentered(TEXT("F"), KeyX + KeySize * 0.5f, KeyY + (KeySize - KeyGlyph.Y) * 0.5f,
            BreakerUI::BgVoid, BreakerUI::HudAbilityKeyPixels, 1.0f, ESpecFontRole::Mono);
    }

    // The tally, in the rail's colour: the count is the second read of the
    // rarity when the colour is not.
    const float CellW = S(Layout.TallyCellWidth);
    const float CellH = S(Layout.TallyCellHeight);
    const float CellStep = S(Layout.TallyCellWidth + Layout.TallyGap);
    for (int32 Cell = 0; Cell < FMath::Max(TallyCells, 0); ++Cell)
    {
        DrawRect(Rail, X + S(Layout.TallyX) + CellStep * Cell, Y + S(Layout.TallyY), CellW, CellH);
    }

    DrawSpecText(Name, X + S(Layout.NameX), Y + (H - NameSize.Y) * 0.5f, BreakerUI::TextPrimary,
        BreakerHUD::InteractPlateNamePixels, 1.0f, ESpecFontRole::Display);
}

void ABreakerPlaytestHUD::DrawInteractableLabels(const ABreakerCharacter* Character)
{
    UWorld* World = GetWorld();
    if (!World || !Character) return;

    const FVector ViewerLocation = Character->GetActorLocation();
    // Plaza-wide on purpose: the vendors sit ~3.6 km of plaza diagonal apart
    // from the gate, and a label that culls at combat-bar range (50 m) would
    // answer "who is that" only after the walk it was meant to motivate.
    constexpr float LabelMaxDistance = 9000.0f;
    // Warm person accent for names — matches the NPC sash/glow palette, and is
    // deliberately NOT the elite gold or the enemy grey so the populations
    // never share a text colour.
    const FLinearColor PersonWarm(1.0f, 0.78f, 0.45f);

    const auto DistanceScaleFor = [&](float Distance)
    {
        const float Alpha = FMath::Clamp((Distance - 1200.0f) / (LabelMaxDistance - 1200.0f), 0.0f, 1.0f);
        return FMath::Lerp(1.0f, 0.65f, Alpha);
    };
    // The actor the prompt plates this frame keeps its label off: the plate
    // sits on the same anchor and says the same name with the key on it.
    const AActor* Plated = BreakerHUD::BreakerHUDPlatedInteractable(Character);

    for (TActorIterator<ABreakerNPC> It(World); It; ++It)
    {
        const ABreakerNPC* NPC = *It;
        if (!NPC || NPC == Plated) continue;
        const auto* Cache = Cast<ABreakerFernhallCache>(NPC);
        // Cache objectives already have map markers. Only the single focused
        // interaction plate names them in-world, using actual range and LOS.
        if (Cache) continue;
        // A chest is not a person and has no name worth floating: it is found
        // by walking into range, and the focused plate is the whole tell.
        if (Cast<ABreakerSupplyChest>(NPC)) continue;
        const float Distance = FVector::Distance(ViewerLocation, NPC->GetActorLocation());
        if (Distance > LabelMaxDistance) continue;
        // Above the head sphere (rel Z 92 + radius), same idiom as the enemy
        // bars' +120 anchor.
        const FVector Projected = Project(NPC->GetActorLocation() + FVector(0.0f, 0.0f, 150.0f), false);
        if (Projected.Z <= 0.0f) continue;
        const float NameScale = DistanceScaleFor(Distance);
        DrawSpecTextCentered(NPC->GetDisplayName().ToString().ToUpper(),
            Projected.X, Projected.Y, PersonWarm, 12.0f * NameScale, 1.0f, ESpecFontRole::Display);
        // Service names remain useful at a distance. Action keys belong only
        // to the single eligible actor selected by the real interaction path.
    }

    for (TActorIterator<ABreakerTravelPoint> It(World); It; ++It)
    {
        const ABreakerTravelPoint* TravelPoint = *It;
        if (!TravelPoint || TravelPoint == Plated) continue;
        const float Distance = FVector::Distance(ViewerLocation, TravelPoint->GetActorLocation());
        if (Distance > LabelMaxDistance) continue;
        // Anchored at the marker, not the beacon tip: the 14 m column already
        // owns the skyline, and a label at its top would leave the screen the
        // moment the player got close.
        const FVector Projected = Project(TravelPoint->GetActorLocation() + FVector(0.0f, 0.0f, 260.0f), false);
        if (Projected.Z <= 0.0f) continue;
        // Rift-teal, because travel is the rift verb — the one text colour the
        // reserve permits, on the one label describing a rift object.
        //
        // Passive travel/service labels identify the place without promising
        // that F can act at this distance. The focused plate owns that hint.
        const float GateScale = DistanceScaleFor(Distance);
        const FString NounWord = TravelPoint->GetDisplayName().ToString().ToUpper();
        // LIFTED CLEAR OF THE AIM POINT (owner, playtest 2026-09-10: "the rift
        // text is too in the way"). The world anchor is 260 cm up, which is a
        // healthy gap when you are standing at the door and almost nothing at
        // range — so from across the yard the name landed exactly on the
        // crosshair, over the thing you were shooting at.
        //
        // A SCREEN-SPACE lift rather than a taller world anchor, because the
        // world anchor is already the right answer for the near case and the
        // note above says why: a label at the beacon's tip leaves the screen
        // the moment the player walks up to it. This moves the label out of the
        // reticle without moving it off the object.
        float LabelY = Projected.Y - S(BreakerUI::HudTravelLabelLiftPixels);
        if (!NounWord.IsEmpty())
        {
            DrawSpecTextCentered(NounWord, Projected.X, LabelY, BreakerUI::TealUnwritten, 13.0f * GateScale, 1.0f, ESpecFontRole::Display);
            LabelY += S(15.0f) * GateScale;
        }
        // The difficulty gauge, empty on a general gate. GROUND owns the number
        // — "AREA 5" is level-5 content now the required level derives from item
        // level — and this lane owns only how it draws: muted and small, under
        // the name and over the verb, because it qualifies the place rather
        // than announcing it.
        // THE DETAIL LINE IS FOR ARRIVING, NOT FOR SCANNING. "AREA 5"
        // qualifies a door you are walking to; from across the yard it is a
        // second line of text on a thing you are not interacting with, and two
        // stacked labels are most of what made this feel in the way.
        const FString DetailWord = Distance <= BreakerUI::HudTravelDetailCm
            ? TravelPoint->GetDisplayDetail().ToString().ToUpper() : FString();
        if (!DetailWord.IsEmpty())
        {
            DrawSpecTextCentered(DetailWord, Projected.X, LabelY, BreakerUI::TextMuted, 10.0f * GateScale);
            LabelY += S(13.0f) * GateScale;
        }

    }
}

// --------------------------------------------------------------------------
// Loot: one plate per drop in range — rarity on the rail and the tally, the
// name — and the key tile only on the one F would take, and not even that
// over a full backpack (the refusal line in DrawInteractPrompt says why). The
// pickup loop compiles to nothing until Items/BreakerLootPickup.h exists; the
// capture preview's plate is drawn either way.
// --------------------------------------------------------------------------
void ABreakerPlaytestHUD::DrawLootPickups(const ABreakerCharacter* Character)
{
    if (!Character || !Canvas) return;
    const ABreakerFeedstockPickup* NearestFeedstock = Character->FindNearbyFeedstock();
    if (GetWorld() && PlayerOwner && PlayerOwner->PlayerCameraManager)
    {
        const FVector Eye = PlayerOwner->PlayerCameraManager->GetCameraLocation();
        for (TActorIterator<ABreakerFeedstockPickup> It(GetWorld()); It; ++It)
        {
            if (!IsValid(*It) || It->IsActorBeingDestroyed() || It->GetOwner() != Character || *It == NearestFeedstock) continue;
            if (FVector::Dist(Eye, It->GetActorLocation()) > BreakerHUD::PickupChipDistance) continue;
            const FVector Projected = Project(It->GetActorLocation() + FVector(0,0,40), false);
            if (Projected.Z > 0) DrawInteractPlate(Projected.X, Projected.Y, BreakerUI::Gold, 1, TEXT("FEEDSTOCK"), false);
        }
    }
#if BREAKER_HAS_LOOT_PICKUP
    UWorld* World = GetWorld();
    if (World && PlayerOwner && PlayerOwner->PlayerCameraManager)
    {
        const FVector CameraLocation = PlayerOwner->PlayerCameraManager->GetCameraLocation();
        // F's own answer to "which one", so the tile can never sit on a drop
        // the key would not take.
        const ABreakerLootPickup* Nearest = Character->FindNearbyFeedstock() ? nullptr : Character->FindNearbyPickup();
        const UBreakerEquipmentComponent* Equipment = Character->GetEquipment();
        const bool bBackpackFull = Equipment
            && Equipment->GetBackpack().Num() >= UBreakerEquipmentComponent::BackpackCapacity;

        for (TActorIterator<ABreakerLootPickup> It(World); It; ++It)
        {
            const ABreakerLootPickup* Pickup = *It;
            if (!Pickup) continue;
            const float Distance = static_cast<float>(FVector::Dist(Pickup->GetActorLocation(), CameraLocation));
            if (Distance > BreakerHUD::PickupChipDistance) continue;
            const FVector Projected = Project(Pickup->GetActorLocation() + FVector(0.0f, 0.0f, 40.0f), false);
            if (Projected.Z <= 0.0f) continue;

            const FBreakerItemInstance& Item = Pickup->GetItem();
            DrawInteractPlate(Projected.X, Projected.Y, BreakerUI::RarityColor(Item.Rarity),
                BreakerHUDMath::RarityTallyCells(Item.Rarity), Pickup->GetDisplayLabel().ToString().ToUpper(),
                Pickup == Nearest && !bBackpackFull);
        }
    }
#endif

    // -BreakerCaptureHUD: the harness cannot make a drop fall, so the fourth
    // phase plates a fabricated Aberrant six metres ahead. Preview-only; the
    // label is the pickup's own shape (rarity then slot, both enumerator
    // names), so no word is authored here.
    if (IsCapturePreview() && bPreviewLootPlate)
    {
        FBreakerItemInstance Preview;
        Preview.Rarity = EBreakerItemRarity::Aberrant;
        Preview.Slot = EBreakerEquipSlot::BodyArmour;
        const FString Label = FString::Printf(TEXT("%s %s"),
            *StaticEnum<EBreakerItemRarity>()->GetNameStringByValue(static_cast<int64>(Preview.Rarity)).ToUpper(),
            *StaticEnum<EBreakerEquipSlot>()->GetNameStringByValue(static_cast<int64>(Preview.Slot)).ToUpper());
        const FVector Ahead = Character->GetActorLocation() + Character->GetActorForwardVector() * 600.0f;
        const FVector Projected = Project(Ahead + FVector(0.0f, 0.0f, 40.0f), false);
        if (Projected.Z > 0.0f)
        {
            DrawInteractPlate(Projected.X, Projected.Y, BreakerUI::RarityColor(Preview.Rarity),
                BreakerHUDMath::RarityTallyCells(Preview.Rarity), Label, true);
        }
    }
}

void ABreakerPlaytestHUD::EnsureDamageBinding(const ABreakerCharacter* Character)
{
    UBreakerCombatComponent* Combat = Character ? Character->GetCombat() : nullptr;
    if (!Combat || BoundCombat == Combat) return;
    if (BoundCombat)
    {
        BoundCombat->OnDamageReceived.RemoveDynamic(this, &ABreakerPlaytestHUD::HandlePlayerDamageReceived);
        BoundCombat->OnHitDealt.RemoveDynamic(this, &ABreakerPlaytestHUD::HandlePlayerHitDealt);
    }
    Combat->OnDamageReceived.AddDynamic(this, &ABreakerPlaytestHUD::HandlePlayerDamageReceived);
    // EVERY damage the player deals, not just the ones a gun dealt. Owner:
    // "there is no damage indicators for anything but bullet damage" — and
    // that was exactly true, because the floating numbers had a single feed,
    // the weapon's OnShot. Cleave, Rot, every ability, every Bleed tick and
    // every chain detonation applied real damage through the ordinary contract
    // and produced no number at all, so half the damage in the game was
    // invisible. OnHitDealt is the attacker-side event the whole combat layer
    // already raises, so this is one subscription rather than one per source —
    // and a future damage path is numbered the day it is written, without
    // anyone remembering to wire it.
    Combat->OnHitDealt.AddDynamic(this, &ABreakerPlaytestHUD::HandlePlayerHitDealt);
    BoundCombat = Combat;
}

void ABreakerPlaytestHUD::EnsureWeaponBinding(const ABreakerCharacter* Character)
{
    UBreakerWeaponComponent* Weapon = Character ? Character->GetWeapon() : nullptr;
    if (!Weapon || BoundWeapon == Weapon) return;
    if (BoundWeapon) BoundWeapon->OnShot.RemoveDynamic(this, &ABreakerPlaytestHUD::HandlePlayerShot);
    Weapon->OnShot.AddDynamic(this, &ABreakerPlaytestHUD::HandlePlayerShot);
    BoundWeapon = Weapon;
}

void ABreakerPlaytestHUD::EnsureRiftBinding()
{
    UWorld* World = GetWorld();
    ABreakerGameMode* Mode = World ? World->GetAuthGameMode<ABreakerGameMode>() : nullptr;
    if (!Mode || BoundRiftMode.Get() == Mode) return;
    // Weak, and no explicit unbind: the previous mode died with its world, and
    // a lambda bound weakly to this HUD cannot be called into after teardown.
    Mode->OnRiftCompleted.AddWeakLambda(this,
        [this](const FBreakerRiftDefinition& Rift, APawn* Player) { HandleRiftCompleted(Rift, Player); });
    BoundRiftMode = Mode;
}

void ABreakerPlaytestHUD::HandleRiftCompleted(const FBreakerRiftDefinition& Rift, APawn* Player)
{
    // Filter by pawn, the same shape LEDGER's handler uses: the event is
    // broadcast once and every listener sees it, so a HUD must confirm the run
    // was ITS player's before it announces one.
    if (Player && PlayerOwner && PlayerOwner->GetPawn() != Player) return;
    // LATCHED, not looked up later. The session's PendingRift is cleared by
    // the teardown this banner outlives, so reading it at draw time would
    // print an empty name on the frame that matters most.
    //
    // NO REWARD NUMBERS. LEDGER owns what was paid and binds the same seam; a
    // figure here would be a second owner of one question.
    EnqueueBanner(EBreakerBannerKind::RiftComplete,
        Rift.AreaName.IsEmpty() ? BreakerStrings::Get(EBreakerStringKey::HudBannerRiftCleared) : Rift.AreaName.ToString().ToUpper(),
        Rift.AreaLine.ToString());
}

void ABreakerPlaytestHUD::EnqueueBanner(EBreakerBannerKind Kind, const FString& Title, const FString& Line)
{
    FBreakerBanner& Banner = Banners.AddDefaulted_GetRef();
    Banner.Kind = Kind;
    Banner.Title = Title;
    Banner.Line = Line;
    // ArriveAt stays unscheduled: DrawBanners orders every banner enqueued
    // since the last frame by priority before assigning arrivals.
}

// --------------------------------------------------------------------------
// The event banners (04-death-banners, O202). One rectangle per kind, all
// three disjoint, so the co-occurring rift-complete and level-up cannot
// collide at any resolution. Sys ink on the bg-1 plate, a 4 px identity rail
// on the left — system bone, reward gold only for the level. In and out are
// SLIDES along the kind's axis, never a fade: the plate leaves the way it
// came, whole. Arrivals are staggered by the queue, higher priority first.
// --------------------------------------------------------------------------
void ABreakerPlaytestHUD::DrawBanners(const FVector2D& Center)
{
    if (Banners.IsEmpty() || !Canvas) return;
    const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
    BreakerBannerQueue::ScheduleArrival(Banners, Now);
    BreakerBannerQueue::Prune(Banners, Now);

    for (const FBreakerBanner& Banner : Banners)
    {
        const float Age = static_cast<float>(Now - Banner.ArriveAt);
        if (!BreakerBannerQueue::IsShowing(Banner.Kind, Age)) continue;

        const BreakerBannerQueue::FRect Rect = BreakerBannerQueue::RectFor(Banner.Kind);
        const FVector2D Axis = BreakerBannerQueue::SlideAxisFor(Banner.Kind);
        const float Displacement = BreakerBannerQueue::SlideDisplacementFor(Banner.Kind, Age);
        // The sheet's rectangles are 1080p pixels; the rift band is authored
        // full width and follows the canvas edge rather than the 1920.
        const bool bFullWidth = Banner.Kind == EBreakerBannerKind::RiftComplete;
        const float PlateX = (bFullWidth ? 0.0f : S(Rect.X)) - Axis.X * S(Displacement);
        const float PlateY = S(Rect.Y) - Axis.Y * S(Displacement);
        const float PlateW = bFullWidth ? Canvas->ClipX : S(Rect.W);
        const float PlateH = S(Rect.H);
        const float CenterX = PlateX + PlateW * 0.5f;

        const FLinearColor Rail = BreakerBannerQueue::RailIsGold(Banner.Kind) ? BreakerUI::Gold : BreakerUI::System;
        DrawPlate(PlateX, PlateY, PlateW, PlateH, Rail, EBreakerRail::Left);

        // The label names the event in the rail's colour; the title is the
        // thing itself; the line is what it paid or where it was. Stacked and
        // centred vertically inside the fixed plate, never sizing it.
        const FString* Label = Banner.Kind == EBreakerBannerKind::RiftComplete ? &BreakerStrings::Get(EBreakerStringKey::HudBannerRunComplete)
            : Banner.Kind == EBreakerBannerKind::LevelUp ? &BreakerStrings::Get(EBreakerStringKey::HudBannerLevelUp) : nullptr;
        const FVector2D LabelSize = Label ? MeasureSpecText(*Label, BreakerHUD::BannerLabelPixels, ESpecFontRole::Display) : FVector2D::ZeroVector;
        const FVector2D TitleSize = MeasureSpecText(Banner.Title, BreakerHUD::BannerTitlePixels, ESpecFontRole::Display);
        const FVector2D LineSize = Banner.Line.IsEmpty() ? FVector2D::ZeroVector
            : MeasureSpecText(Banner.Line, BreakerHUD::BannerLinePixels);
        const float ContentH = (Label ? LabelSize.Y + S(BreakerUI::Space4) : 0.0f) + TitleSize.Y
            + (Banner.Line.IsEmpty() ? 0.0f : S(BreakerUI::Space8) + LineSize.Y);
        float LineY = PlateY + (PlateH - ContentH) * 0.5f;
        if (Label)
        {
            DrawSpecTextCentered(*Label, CenterX, LineY, Rail, BreakerHUD::BannerLabelPixels, 1.0f, ESpecFontRole::Display);
            LineY += LabelSize.Y + S(BreakerUI::Space4);
        }
        DrawSpecTextCentered(Banner.Title, CenterX, LineY, BreakerUI::System, BreakerHUD::BannerTitlePixels, 1.0f, ESpecFontRole::Display);
        if (!Banner.Line.IsEmpty())
        {
            LineY += TitleSize.Y + S(BreakerUI::Space8);
            DrawSpecTextCentered(Banner.Line, CenterX, LineY, BreakerUI::TextMuted, BreakerHUD::BannerLinePixels);
        }
    }
}

void ABreakerPlaytestHUD::EnsureProgressionBinding(const ABreakerCharacter* Character)
{
    UBreakerProgressionComponent* Progression = Character ? Character->GetProgression() : nullptr;
    if (!Progression || BoundProgression == Progression) return;
    if (BoundProgression) BoundProgression->OnLevelGained.RemoveDynamic(this, &ABreakerPlaytestHUD::HandleLevelGained);
    Progression->OnLevelGained.AddDynamic(this, &ABreakerPlaytestHUD::HandleLevelGained);
    BoundProgression = Progression;
}

void ABreakerPlaytestHUD::HandleLevelGained(int32 NewLevel, int32 LevelsGained)
{
    // The title carries the gain so the banner says "(+2)" rather than lying
    // by one: a single kill can cross more than one level early on, and a
    // tell that says "level 2" when the player reached 4 is worse than none.
    const FString Title = LevelsGained > 1
        ? BreakerStrings::Format(EBreakerStringKey::HudBannerLevelGained, NewLevel, LevelsGained)
        : BreakerStrings::Format(EBreakerStringKey::HudBannerLevel, NewLevel);
    // What this level-up actually PAID, computed the same way the progression
    // component grants it (one point per level up to each currency's cap), so
    // the banner states the grant instead of leaving the player to discover
    // it in a menu. A level past a cap claims nothing; past both, the level
    // still deserves its banner.
    const int32 PrevLevel = NewLevel - LevelsGained;
    const int32 ClassGain = FMath::Max(0,
        FMath::Min(NewLevel, UBreakerProgressionLibrary::ClassPointCapLevel)
        - FMath::Min(PrevLevel, UBreakerProgressionLibrary::ClassPointCapLevel));
    const int32 CoreGain = FMath::Max(0,
        FMath::Min(NewLevel, UBreakerProgressionLibrary::CorePointCapLevel)
        - FMath::Min(PrevLevel, UBreakerProgressionLibrary::CorePointCapLevel));
    FString Grant;
    if (ClassGain > 0) Grant = BreakerStrings::Format(EBreakerStringKey::HudBannerClassPoints, ClassGain);
    if (CoreGain > 0)
    {
        if (!Grant.IsEmpty()) Grant += TEXT("   ");
        Grant += BreakerStrings::Format(EBreakerStringKey::HudBannerCorePoints, CoreGain);
    }
    if (Grant.IsEmpty()) Grant = BreakerStrings::Get(EBreakerStringKey::HudBannerPointCapReached);
    EnqueueBanner(EBreakerBannerKind::LevelUp, Title, Grant);

    // OWNER REPORT: "could we add a sound and a visual for leveling up". The
    // banner above already existed and was not felt — one 400x88 plate in the
    // top-right corner, on a screen that also puts the objective line there —
    // and nothing in the game made a noise when the player grew.
    //
    // Both halves land here rather than at the progression site: WHEN a cue
    // fires is the owning lane's call, and this is the seam that already
    // knows a level was gained. One cue and one ring per EVENT, never per
    // level: two levels from one kill is one banner, and it is one sound.
    if (ABreakerSoundDirector* Sound = GetSoundDirector()) Sound->PlayLevelUp();
    const AActor* Levelled = BoundProgression ? BoundProgression->GetOwner() : nullptr;
    UWorld* World = GetWorld();
    ABreakerEffectRenderer* Effects = Levelled && World ? ABreakerEffectRenderer::FindOrSpawn(World) : nullptr;
    if (Effects)
    {
        // O179's camera law: a self-anchored draw sits at the FEET, because
        // the one camera guaranteed to stand inside a player-centred primitive
        // is the player's own. Gold, because O179 files gold as reward and a
        // level is a payment received.
        const ABreakerCharacter* Grown = Cast<ABreakerCharacter>(Levelled);
        const float HalfHeight = Grown && Grown->GetCapsuleComponent()
            ? Grown->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 88.0f;
        const FVector Feet = Levelled->GetActorLocation() - FVector(0.0f, 0.0f, HalfHeight);
        BreakerFX::FEffectTiming RingTiming;
        RingTiming.DurationSeconds = 0.55f;   // O2 PLACEHOLDER: the cue's own length.
        RingTiming.FadeInSeconds = 0.04f;     // O2 PLACEHOLDER
        RingTiming.FadeOutSeconds = 0.30f;    // O2 PLACEHOLDER
        // Strokes arrive around the ring rather than all at once, which is
        // what makes it read as an expanding mark instead of a decal.
        constexpr float SweepSeconds = 0.14f;   // O2 PLACEHOLDER
        constexpr float RingRadiusCm = 200.0f;  // O2 PLACEHOLDER
        for (int32 Index = 0; Index < BreakerFX::GroundRingStrokes; ++Index)
        {
            FVector A, B;
            BreakerFX::RingStroke(Feet, RingRadiusCm, Index, BreakerFX::GroundRingStrokes, A, B);
            Effects->AddStroke(A, B, 7.0f, BreakerUI::Gold, 3.0f, RingTiming,
                SweepSeconds * Index / BreakerFX::GroundRingStrokes);
        }
    }
}

void ABreakerPlaytestHUD::EnsureAbilityBinding(const ABreakerCharacter* Character)
{
    UBreakerAbilityComponent* Abilities = Character ? Character->GetAbilities() : nullptr;
    if (!Abilities || BoundAbilities == Abilities) return;
    if (BoundAbilities) BoundAbilities->OnAbilityActivated.RemoveDynamic(this, &ABreakerPlaytestHUD::HandleAbilityActivated);
    Abilities->OnAbilityActivated.AddDynamic(this, &ABreakerPlaytestHUD::HandleAbilityActivated);
    BoundAbilities = Abilities;
}

void ABreakerPlaytestHUD::HandleAbilityActivated(EBreakerAbilitySlot Slot)
{
    const int32 Index = static_cast<int32>(Slot);
    if (Index < 0 || Index >= AbilitySlotCount) return;

    const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
    SlotActivationTime[Index] = Now;
    ++SlotActivationCount[Index];
    LastActivatedSlotIndex = Index;

    const UBreakerAbilityComponent* Abilities = BoundAbilities;
    const UBreakerAbilityDefinition* Definition = Abilities ? Abilities->GetDefinitionForSlot(Slot) : nullptr;

    // THE FIFTH VERB (ORDERS ruling 2). Played before the null-definition
    // return below, and with NAME_None when there is no definition: the
    // activation HAPPENED either way, and an ability that fires silently
    // because its definition failed to resolve is the worse failure. NAME_None
    // resolves to the shared default cue.
    if (ABreakerSoundDirector* Sound = GetSoundDirector())
    {
        Sound->PlayAbilityCast(Definition ? Definition->AbilityId : NAME_None);
    }

    if (!Definition) return;

    // Which variant this cast actually resolved to. A keystone rewrite is
    // otherwise completely invisible: the row's authored VariantName was read
    // by nothing in the project, so a player who committed a branch and bought
    // its keystone saw an identical ultimate and had to infer the rewrite from
    // its effects. Resolved from the owner's live tag set, the same input
    // UBreakerGameplayAbility uses, so the HUD can never disagree with the
    // ability about which row ran.
    FString VariantName;
    if (const ABreakerCharacter* Caster = Abilities ? Cast<ABreakerCharacter>(Abilities->GetOwner()) : nullptr)
    {
        if (const UAbilitySystemComponent* ASC = Caster->GetAbilitySystemComponent())
        {
            FGameplayTagContainer OwnerTags;
            ASC->GetOwnedGameplayTags(OwnerTags);
            const FBreakerAbilityVariant Variant = Definition->ResolveVariant(OwnerTags);
            if (Variant.KeystoneTag.IsValid() && !Variant.VariantName.IsEmpty())
            {
                VariantName = Variant.VariantName.ToString();
            }
        }
    }

}

const UBreakerAbilityStateComponent* ABreakerPlaytestHUD::GetAbilityState(const ABreakerCharacter* Character)
{
    // FindComponentByClass, not a character accessor: the state component is
    // added on demand by whichever ability opens the first window, so no
    // character class declares it.
    return Character ? Character->FindComponentByClass<UBreakerAbilityStateComponent>() : nullptr;
}

// --------------------------------------------------------------------------
// Labelled duration bars for every open ability window, e.g. "OVERDRIVE 4.2s".
// --------------------------------------------------------------------------
void ABreakerPlaytestHUD::DrawAbilityWindows(const ABreakerCharacter* Character, float X, float BottomY, float Width)
{
    const UBreakerAbilityStateComponent* State = GetAbilityState(Character);
    if (!State) return;

    const float RowH = S(24.0f);
    float RowBottom = BottomY;
    for (const FName Key : State->GetActiveWindowKeys())
    {
        FString KeyText = Key.ToString();
        if (!KeyText.StartsWith(BreakerHUD::WindowPrefix)) continue;
        const FString ShortKey = KeyText.RightChop(FCString::Strlen(BreakerHUD::WindowPrefix));

        const float Remaining = State->GetWindowRemaining(Key);
        if (Remaining <= 0.0f) continue;
        const FLinearColor Color = BreakerHUD::WindowColor(ShortKey);

        const float RowY = RowBottom - RowH;
        DrawSpecText(ShortKey.ToUpper(), X, RowY, Color, 11.0f, 1.0f, ESpecFontRole::Display);
        DrawSpecTextRight(FString::Printf(TEXT("%.1fs"), Remaining), X + Width, RowY, Color, 11.0f, 1.0f, ESpecFontRole::Mono);

        // The bar has no authored maximum to divide by — GetWindowRemaining is
        // the only reading available — so it is drawn as a decaying 10s scale,
        // clamped full. It communicates "running out", not an exact fraction.
        DrawTrack(X, RowY + S(14.0f), Width, S(5.0f), FMath::Clamp(Remaining / 10.0f, 0.0f, 1.0f), Color, BreakerUI::Panel10);
        RowBottom -= RowH;
    }
}

// --------------------------------------------------------------------------
// Centre-low teaching callout. Fades over its lifetime and never repeats past
// the first few casts of each ability.
// --------------------------------------------------------------------------

// --------------------------------------------------------------------------
// §5 — ultimate treatment. A 3px violet frame inset 8px, 120px violet bands on
// the top and bottom edges only (the sides stay clear so peripheral threat
// reading is untouched), a title plate for the first 1.2s, and a frame that
// steps 3px -> 2px -> 1px over the final 3 seconds so the ending is visible
// without a timer.
// --------------------------------------------------------------------------
void ABreakerPlaytestHUD::DrawUltimateTreatment(const ABreakerCharacter* Character)
{
    const UBreakerAbilityStateComponent* State = GetAbilityState(Character);
    const bool bActive = State && State->IsWindowActive(BreakerHUD::OverdriveWindow);
    const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;

    // Latched on the rising edge: the state component reports remaining time
    // only, and the title plate needs elapsed time.
    if (bActive && !bUltimateWindowWasActive) UltimateWindowStartTime = Now;
    bUltimateWindowWasActive = bActive;
    if (!bActive) return;

    const float Remaining = State->GetWindowRemaining(BreakerHUD::OverdriveWindow);
    const float Elapsed = static_cast<float>(Now - UltimateWindowStartTime);

    // Step down through the final three seconds. No easing: each step is a
    // discrete report that the window is closing.
    float Thickness = 3.0f;
    if (Remaining <= 1.0f) Thickness = 1.0f;
    else if (Remaining <= 2.0f) Thickness = 2.0f;

    const float Inset = S(BreakerUI::UltimateFrameInset);
    const float W = Canvas->ClipX;
    const float H = Canvas->ClipY;

    // IGNITION TINT (ORDERS item 7). The frame and the bands below report that
    // the ultimate is RUNNING; nothing reported that it just STARTED, so
    // ignition could only be read by looking at a bar. One full-bleed violet
    // wash, snapping on at ignition and falling linearly to nothing — the
    // panel-out motion this HUD already uses, not an eased breath.
    //
    // IT CANNOT SURVIVE THE ABILITY, AND THAT IS STRUCTURAL RATHER THAN
    // REMEMBERED. This sits after the `if (!bActive) return` above, so a window
    // that closes — expired, cancelled, or ended by anything else — takes the
    // tint with it on the same frame, without a second condition that could be
    // forgotten or drift. The accidental violet wash that was removed outlived
    // its cause; a deliberate one that could outlive its cause would be the
    // same bug authored on purpose.
    //
    // Drawn FIRST, so the frame, the bands and the title plate all read over
    // the wash rather than under it.
    if (Elapsed < BreakerHUD::UltimateIgnitionSeconds)
    {
        const float Fall = 1.0f - Elapsed / BreakerHUD::UltimateIgnitionSeconds;
        DrawRect(BreakerUI::Alpha(BreakerUI::Violet, BreakerHUD::UltimateIgnitionPeakAlpha * Fall),
            0.0f, 0.0f, W, H);
    }

    DrawBorder(Inset, Inset, W - Inset * 2.0f, H - Inset * 2.0f, BreakerUI::Violet, S(Thickness));

    const FLinearColor Band = BreakerUI::Alpha(BreakerUI::Violet, 0.10f);
    const float BandH = S(BreakerUI::UltimateBandHeight);
    DrawRect(Band, 0.0f, 0.0f, W, BandH);
    DrawRect(Band, 0.0f, H - BandH, W, BandH);

    if (Elapsed >= 0.0f && Elapsed < BreakerUI::UltimateTitleSeconds)
    {
        // Clear of the wave banner band by design: 132px from the top.
        const float PlateW = S(BreakerUI::UltimateTitleWidth);
        const float PlateH = S(44.0f);
        const float PlateX = W * 0.5f - PlateW * 0.5f;
        const float PlateY = S(BreakerUI::UltimateTitleTop);
        DrawPlate(PlateX, PlateY, PlateW, PlateH, BreakerUI::Violet, EBreakerRail::Top);
        DrawSpecTextCentered(BreakerStrings::Get(EBreakerStringKey::HudCalloutOverdriveActive), W * 0.5f, PlateY + S(14.0f), BreakerUI::Violet, 20.0f, 1.0f, ESpecFontRole::Display);
    }
}

// --------------------------------------------------------------------------
// Lead's mark. Projected the same way the enemy bars are, just higher, so the
// diamond sits clear of the health bar on the same target.
// --------------------------------------------------------------------------
void ABreakerPlaytestHUD::DrawMarkedTarget(const ABreakerCharacter* Character)
{
    const UBreakerAbilityStateComponent* State = GetAbilityState(Character);
    if (!State) return;
    for (const AActor* Marked : State->GetMarkedTargets())
    {

    const FVector Projected = Project(Marked->GetActorLocation() + FVector(0.0f, 0.0f, BreakerHUD::MarkHeadroomCm), false);
    if (Projected.Z <= 0.0f) continue;

    // Slow pulse: enough to catch the eye in peripheral vision, not enough to
    // compete with the impact feedback at the crosshair.
    const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
    const float Pulse = 0.75f + 0.25f * FMath::Sin(static_cast<float>(Now) * 4.0f);
    // Lead's mark carries the Swift kit's accent. O179 assigns no colour to a
    // tag verb and the definition carries no verb (plumbing the desk owes),
    // so this stays the class ability accent rather than guessing a hue.
    const FLinearColor Color = BreakerUI::Alpha(BreakerUI::VerbMove, Pulse);
    const float Radius = S(9.0f) * Pulse;

    const float CX = Projected.X;
    const float CY = Projected.Y;
    DrawLine(CX, CY - Radius, CX + Radius, CY, Color, S(1.75f));
    DrawLine(CX + Radius, CY, CX, CY + Radius, Color, S(1.75f));
    DrawLine(CX, CY + Radius, CX - Radius, CY, Color, S(1.75f));
    DrawLine(CX - Radius, CY, CX, CY - Radius, Color, S(1.75f));

    DrawSpecTextCentered(BreakerStrings::Get(EBreakerStringKey::HudCalloutMarked), CX, CY - Radius - S(16.0f), Color, 11.0f, 1.0f, ESpecFontRole::Display);
    }
}

ABreakerTracerRenderer* ABreakerPlaytestHUD::GetTracerRenderer()
{
    if (TracerRenderer) return TracerRenderer;
    UWorld* World = GetWorld();
    if (!World) return nullptr;
    FActorSpawnParameters Params;
    Params.Owner = this;
    Params.ObjectFlags |= RF_Transient;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    TracerRenderer = World->SpawnActor<ABreakerTracerRenderer>(
        ABreakerTracerRenderer::StaticClass(), FTransform::Identity, Params);
    return TracerRenderer;
}

ABreakerSoundDirector* ABreakerPlaytestHUD::GetSoundDirector()
{
    if (IsValid(SoundDirector)) return SoundDirector;
    UWorld* World = GetWorld();
    if (!World) return nullptr;
    for (TActorIterator<ABreakerSoundDirector> It(World); It; ++It)
    {
        if (!IsValid(*It)) continue;
        SoundDirector = *It;
        SoundDirector->SetLifeSpan(0.0f);
        return SoundDirector;
    }
    FActorSpawnParameters Params;
    Params.Owner = this;
    Params.ObjectFlags |= RF_Transient;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    SoundDirector = World->SpawnActor<ABreakerSoundDirector>(
        ABreakerSoundDirector::StaticClass(), FTransform::Identity, Params);
    return SoundDirector;
}

void ABreakerPlaytestHUD::HandlePlayerShot(const FBreakerShotResult& Shot)
{
    if (!Shot.bFired) return;

    // The report, per trigger pull. Before the projectile branch on purpose:
    // a launcher skips the tracer because a real actor already flies, but
    // nothing else makes its sound.
    if (ABreakerSoundDirector* Sound = GetSoundDirector())
    {
        // The archetype decides the clip. BoundWeapon is the same component
        // this handler is bound to, so there is no extra lookup.
        Sound->PlayWeaponFire(BoundWeapon ? BoundWeapon->GetArchetype() : EBreakerWeaponArchetype::Rifle);
    }

    // THE MUZZLE MOMENT (GLASS-1). Every trigger pull, launcher included, at
    // the visual muzzle and down the barrel, in the weapon/heat role. The
    // character's own point light (KIT's) keeps flashing underneath; this is
    // the world flash that becomes NS_Muzzle the day the owner authors it.
    const FVector AimDirection = (Shot.TraceEnd - Shot.TraceStart).GetSafeNormal();
    if (ABreakerEffectRenderer* Effects = ABreakerEffectRenderer::FindOrSpawn(GetWorld()))
    {
        Effects->PlayMoment(EBreakerEffectMoment::Muzzle,
            BoundWeapon ? BoundWeapon->GetVisualMuzzleLocation() : Shot.TraceStart,
            AimDirection, BreakerFX::MomentColor(EBreakerEffectMoment::Muzzle, false));
    }

    // A launcher already puts a real actor in the world; a hitscan streak on
    // top of it drew a second, faster, ghost round every time the rocket fired.
    const UBreakerWeaponDefinition* FiredDefinition = BoundWeapon ? BoundWeapon->GetActiveDefinition() : nullptr;
    const bool bProjectileShot = FiredDefinition && FiredDefinition->bProjectile;
    // A pellet weapon used to get no streak at all, because the shot contract
    // carried ONE impact for a whole spread and drawing one line for eight
    // pellets is a lie about where they went. That gap is CLOSED:
    // FBreakerShotResult now carries a per-pellet record, and the renderer owns
    // the policy for how a spread shares its fixed pool (a budgeted, thinner
    // subsample — see the note in BreakerTracerRenderer.h). The old branch
    // survives only as the fallback for a spread with no per-pellet record,
    // which is what a replicated shot from before this change looks like.
    const bool bPelletShot = FiredDefinition && FiredDefinition->PelletsPerShot > 1;
    const bool bDrawSpread = bPelletShot && Shot.Pellets.Num() > 1;

    const double ShotTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;

    if (!bProjectileShot)
    {
        // VISUAL origin: the gun, not the camera. The trace still starts at
        // the camera and still lands on the crosshair — see
        // UBreakerWeaponComponent::GetVisualMuzzleLocation. The two converge
        // at the impact, which is the only place they have to agree.
        const FVector Start = BoundWeapon ? BoundWeapon->GetVisualMuzzleLocation() : Shot.TraceStart;
        const FVector End = Shot.bHit ? Shot.ImpactPoint : Shot.TraceEnd;
        const float FlightSeconds = BreakerHUD::TracerFlightSeconds(BreakerHUD::LiveTracerFlight(),
            static_cast<float>((End - Start).Size()));

        // Every round counts; only some of them are visible. See
        // BreakerHUD::TracerRoundsPerTracer — fast weapons trace one round in
        // three, slow ones trace all of them.
        const int32 RoundsPerTracer = BreakerHUD::TracerRoundsPerTracer(
            FiredDefinition ? FiredDefinition->RoundsPerMinute : 600.0f);
        const bool bVisibleRound =
            !bPelletShot && BreakerHUD::ShouldTraceRound(RoundsFired, RoundsPerTracer);
        ++RoundsFired;

        // Momentum on the round (KIT-2): the same read the resource row
        // makes, 0 for every owner without a Swift bar, so the streak is
        // brighter exactly when the bar is up and ordinary otherwise.
        const ABreakerCharacter* Shooter = BoundWeapon ? Cast<ABreakerCharacter>(BoundWeapon->GetOwner()) : nullptr;
        const UBreakerMomentumComponent* Momentum = Shooter ? Shooter->GetMomentum() : nullptr;
        const float TracerIntensityScale = BreakerHUD::TracerMomentumIntensityScale(
            (Momentum && Momentum->IsActiveForOwner()) ? Momentum->GetMomentumFraction() : 0.0f);

        if (bDrawSpread)
        {
            // The whole blast in one call: the renderer draws the budgeted
            // subsample of streaks AND a flash on every landed pellet, so the
            // spread is never traced by the single-impact path below. Spreads
            // are exempt from the tracer cadence — a shell is one event, and
            // skipping two shells in three would read as the gun misfiring.
            if (ABreakerTracerRenderer* Renderer = GetTracerRenderer())
            {
                Renderer->AddSpread(Start, Shot.Pellets, TracerIntensityScale);
            }
        }
        else if (bVisibleRound)
        {
            if (ABreakerTracerRenderer* Renderer = GetTracerRenderer())
            {
                Renderer->AddTracer(Start, End, TracerIntensityScale);
            }
        }
        // The flash fires on every hit whether or not the round was traced:
        // hit confirmation is feedback the player acts on, tracer density is
        // decoration. A spread already flashed every landed pellet inside
        // AddSpread, so it must not also flash its last-pellet summary here.
        if (Shot.bHit && !bDrawSpread)
        {
            if (ABreakerTracerRenderer* Renderer = GetTracerRenderer())
            {
                Renderer->AddImpact(Shot.ImpactPoint, Shot.bWeakPoint, FlightSeconds);
            }
        }

        // THE IMPACT MOMENT (GLASS-1), on the same arrival clock as the spark
        // above. The spark IS the pooled fallback, so this draws nothing until
        // NS_Impact exists — then every landed pellet gets one, facing back
        // up the shot, Gold where the pellet found a weak point.
        if (ABreakerEffectRenderer* Effects = ABreakerEffectRenderer::FindOrSpawn(GetWorld()))
        {
            if (bDrawSpread)
            {
                for (const FBreakerPelletImpact& Pellet : Shot.Pellets)
                {
                    if (!Pellet.bHit) continue;
                    Effects->PlayMoment(EBreakerEffectMoment::Impact, Pellet.End, -AimDirection,
                        BreakerFX::MomentColor(EBreakerEffectMoment::Impact, Pellet.bWeakPoint), FlightSeconds);
                }
            }
            else if (Shot.bHit)
            {
                Effects->PlayMoment(EBreakerEffectMoment::Impact, Shot.ImpactPoint, -AimDirection,
                    BreakerFX::MomentColor(EBreakerEffectMoment::Impact, Shot.bWeakPoint), FlightSeconds);
            }
        }

        // Secondary legs — pierce continuations, chain arcs, ricochet bounces.
        // ALWAYS drawn, never subject to the tracer cadence: the channels are
        // the Swift identity (owner ruling 2026-08-16) and a pierce the player
        // cannot see is dead content. Each leg starts when the primary round
        // ARRIVES, plus a beat per leg, so a chain visibly walks from enemy to
        // enemy instead of appearing as one simultaneous web. Capped at the
        // renderer's leg budget so a deep build cannot evict its own streaks.
        if (!Shot.SecondaryImpacts.IsEmpty())
        {
            if (ABreakerTracerRenderer* Renderer = GetTracerRenderer())
            {
                int32 LegsDrawn = 0;
                for (const FBreakerSecondaryImpact& Leg : Shot.SecondaryImpacts)
                {
                    if (LegsDrawn >= ABreakerTracerRenderer::MaxSecondaryLegStreaks) break;
                    Renderer->AddSecondaryLeg(Leg.Start, Leg.End, Leg.bHit,
                        FlightSeconds + 0.02f * LegsDrawn);   // O2 PLACEHOLDER stagger
                    ++LegsDrawn;
                }
            }
        }
    }

    // HOW MUCH DISAPPEARED. FBreakerDamageResult does not carry a mitigation
    // field and inventing one would mean a change in Combat/; it does not need
    // to, because the ratio is already fully determined by two fields it does
    // carry. RawDamage is post-crit, post-weak-point and pre-defence;
    // MitigatedDamage is that same number after armour, after the incoming
    // multiplier and after a block roll. One minus the ratio is exactly the
    // share of the hit the target ate — which for the Warden IS the frontal
    // armour, because nothing else on it moves either factor.
    //
    // Deliberately NOT "armour": the field is honest about being mitigation of
    // any origin, so an Overcast debuff or a future damage-reduction modifier
    // reads through the same channel instead of lying about its cause.
    const float Raw = Shot.DamageResult.RawDamage;
    const float Mitigated = Raw > UE_SMALL_NUMBER
        ? FMath::Clamp(1.0f - Shot.DamageResult.MitigatedDamage / Raw, 0.0f, 1.0f) : 0.0f;
    if (Shot.bHit)
    {
        LastShotMitigatedFraction = Mitigated;
        LastShotHitTime = ShotTime;
    }

    // The floating number is NOT pushed from here any more. It is pushed from
    // HandlePlayerHitDealt, which sees every damage the player deals rather
    // than only the ones a weapon dealt. Pushing from both would double every
    // bullet. This handler keeps the two readouts that are genuinely about the
    // SHOT — the mitigation fraction above and the tracer below — which the
    // hit event cannot provide because it knows nothing about a muzzle.
}

void ABreakerPlaytestHUD::ScheduleArrivalSound(float DelaySeconds, bool bKill)
{
    // NO DEATH SOUND, AND A KILL FALLS THROUGH TO THE HIT-CONFIRM (ruled
    // 2026-08-26, the owner's second playtest). There were TWO death sounds and
    // only one had been ruled on: the player's, silenced by the !bKilled guard
    // in HandlePlayerDamageReceived. This is the other one — the noise an ENEMY
    // makes dying, which the player hears far more often — and it was still
    // there.
    //
    // THE FALL-THROUGH IS THE RULING, not an implementation detail. Deleting
    // the kill branch has two readings and only one was asked for: the kill
    // plays the hit-confirm (no sting, the shot still confirms it connected),
    // NOT the kill plays nothing — which would make the LAST shot on an enemy
    // silent and remove the feedback that it landed at all. Losing hit
    // confirmation is a worse defect than a bad sting.
    //
    // bKill is retained and deliberately unread. The ruling is "for now", and
    // whether a kill should sound different from a graze is a separate question
    // the owner has left open — the caller already knows the answer and
    // throwing it away here is the expensive half to reconstruct.
    (void)bKill;

    // Retrigger-cut semantics survive the delay: each scheduled play calls
    // the same voice, and the newest arrival wins exactly as it does at
    // zero delay. The timer handle is deliberately per-call and discarded —
    // a pending arrival must never be cancelled by the next trigger pull,
    // because its round is still in the air.
    if (DelaySeconds <= 0.0f)
    {
        if (ABreakerSoundDirector* Sound = GetSoundDirector())
        {
            Sound->PlayHitConfirm();
        }
        return;
    }
    UWorld* World = GetWorld();
    if (!World) return;
    TWeakObjectPtr<ABreakerPlaytestHUD> WeakThis(this);
    FTimerHandle Discarded;
    World->GetTimerManager().SetTimer(Discarded, FTimerDelegate::CreateLambda([WeakThis]()
    {
        if (ABreakerPlaytestHUD* HUD = WeakThis.Get())
        {
            if (ABreakerSoundDirector* Sound = HUD->GetSoundDirector())
            {
                Sound->PlayHitConfirm();
            }
        }
    }), DelaySeconds, false);
}

void ABreakerPlaytestHUD::HandlePlayerHitDealt(const FBreakerHitContext& Hit)
{
    // OVERKILL IS NOT PRINTED. Owner-ruled, second playtest: "or overkill damage
    // on displayed numbers". This USED to add it, deliberately — a 900-damage
    // rocket on a 30 HP enemy printed 30, and the argument was that the owner
    // reads these numbers for TTK. He has now played it and ruled the other
    // way: what the number says is what the target LOST. The overkill is still
    // computed and still on the result for anything that wants it.
    const float Applied = Hit.Result.ShieldDamage + Hit.Result.HealthDamage;
    const float Shown = Applied > 0.0f ? Applied : Hit.Result.MitigatedDamage;
    if (Shown <= 0.0f) return;

    UWorld* World = GetWorld();
    const float Now = World ? World->GetTimeSeconds() : 0.0f;
    const float Raw = Hit.Result.RawDamage;
    const float Mitigated = Raw > UE_SMALL_NUMBER
        ? FMath::Clamp(1.0f - Hit.Result.MitigatedDamage / Raw, 0.0f, 1.0f) : 0.0f;
    const bool bWeak = Hit.bWeakPoint || Hit.Result.bWeakPoint;

    // THE ARRIVAL CLOCK (ruled). The DAMAGE above this line already landed:
    // hitscan resolves instantly, that rule is untouched, and nothing below
    // delays a single point of it. What rides the arrival is every SIGNAL
    // that says "the round landed" — this tick's sound, the crosshair mark,
    // the floating number, the kill confirm — computed with the SAME
    // function and the SAME live knobs the tracer and the impact spark
    // already fly on, so all of them land in the one frame the streak does.
    // A weapon hit's flight is muzzle-to-impact; an ability's confirm is
    // instantaneous and stays at zero.
    float ArrivalDelay = 0.0f;
    if (!Hit.bFromDoT && Hit.Delivery == EBreakerDamageDelivery::Weapon && BoundWeapon)
    {
        ArrivalDelay = BreakerHUD::TracerFlightSeconds(BreakerHUD::LiveTracerFlight(),
            static_cast<float>((Hit.WorldLocation - BoundWeapon->GetVisualMuzzleLocation()).Size()));
    }
    const float ArrivalTime = Now + ArrivalDelay;

    // Crosshair confirms, from the same universal feed the numbers ride: an
    // ability's cleave ticks the crosshair exactly as a bullet does. DoT
    // ticks are excluded — a Bleed on three targets is not something the
    // player just did, and it would strobe the crosshair forever. The latch
    // may sit in the FUTURE; every consumer already guards age >= 0.
    if (!Hit.bFromDoT)
    {
        LastHitDealtTime = ArrivalTime;
        bHitDealtWeakPoint = bWeak;
        bHitDealtAbsorbed = Mitigated >= BreakerUI::DamageAbsorbedThreshold;
        // The confirm tick at arrival. On a killing blow it plays UNDER the
        // kill sound below — separate voices, and the kill is authored to
        // read over it.
        ScheduleArrivalSound(ArrivalDelay, /*bKill*/ false);
    }
    if (Hit.Result.bKilled)
    {
        LastKillConfirmTime = ArrivalTime;
        bKillConfirmWeakPoint = bWeak;
        ScheduleArrivalSound(ArrivalDelay, /*bKill*/ true);

        // THE DEATH MOMENT (GLASS-1), where the body stood, when the round
        // lands — the same clock as the crosshair confirm and the number, in
        // the same colour (Harm; Gold for a weak-point kill).
        if (ABreakerEffectRenderer* Effects = ABreakerEffectRenderer::FindOrSpawn(World))
        {
            const FVector Where = Hit.WorldLocation.IsNearlyZero() && Hit.Target
                ? Hit.Target->GetActorLocation() : Hit.WorldLocation;
            Effects->PlayMoment(EBreakerEffectMoment::Death, Where, FVector::UpVector,
                BreakerFX::MomentColor(EBreakerEffectMoment::Death, bWeak), ArrivalDelay);
        }
    }

    // MERGE rather than spawn. A shotgun resolves eight pellets as eight hits,
    // and a Bleed on three targets ticks on its own cadence forever — one
    // number each would bury the screen and push every other number out of the
    // 24-slot ring within a frame. Same target, same kind, inside the merge
    // window: add to the existing number and refresh it, so a spread reads as
    // the one number the player actually wants (what did that shot do) and a
    // DoT reads as a steady accumulating tick.
    //
    // Crit and weak point do NOT merge into a body hit: those are the reads the
    // whole damage-number system exists to make legible, and averaging them
    // into a plain number would be the same as not showing them.
    // The predicate and both windows live in UI/BreakerDamageFeed.h, world-free
    // and unit-tested. It used to be four inline comparisons and one shared
    // constant here, which is why three of the rule's four clauses were wrong
    // and nothing could see it.
    BreakerDamageFeed::FMergeKey IncomingKey;
    IncomingKey.Target = Hit.Target.Get();
    IncomingKey.bCritical = Hit.Result.bCritical;
    IncomingKey.bWeakPoint = bWeak;
    IncomingKey.bFromDoT = Hit.bFromDoT;
    IncomingKey.Element = Hit.Element;
    IncomingKey.DamageTypeTag = Hit.DamageTypeTag;

    for (FBreakerHUDDamageNumber& Existing : DamageNumbers)
    {
        BreakerDamageFeed::FMergeKey ExistingKey;
        ExistingKey.Target = Existing.Target.Get();
        ExistingKey.bCritical = Existing.bCritical;
        ExistingKey.bWeakPoint = Existing.bWeakPoint;
        ExistingKey.bFromDoT = Existing.bFromDoT;
        ExistingKey.Element = Existing.Element;
        ExistingKey.DamageTypeTag = Existing.DamageTypeTag;
        // Merge windows are measured at ARRIVAL: two pellets of one shot share
        // an arrival and merge exactly as they did when both were born at the
        // trigger.
        if (!BreakerDamageFeed::ShouldMerge(ExistingKey, Existing.Time, IncomingKey, ArrivalTime)) continue;

        Existing.Value += Shown;
        // DELIBERATELY NOT `Existing.Time = Now`. That refresh is what made the
        // window slide, so a held trigger accumulated one number for a whole
        // magazine. The stamp is the number's BIRTH and the window is measured
        // from it.
        // A merged pellet that finished the target promotes the whole number
        // to a killing blow — the shot killed, whichever pellet landed last.
        if (Hit.Result.bKilled)
        {
            Existing.bKilled = true;
            Existing.Lifetime = BreakerHUD::DamageKillLifetime;
        }
        Existing.Overkill += Hit.Result.OverkillDamage;
        // Deliberately NOT moving Existing.World: a merged number that chased
        // each pellet's impact point would jitter, and the first impact is as
        // honest a location as any for the sum.
        return;
    }

    FBreakerHUDDamageNumber Number;
    // The hit context carries the world location for every path — an ability's
    // sweep, a DoT tick, a detonation — which is what makes one feed possible.
    // Falls back to the target's own location if a path ever leaves it unset.
    Number.World = Hit.WorldLocation.IsNearlyZero() && Hit.Target
        ? Hit.Target->GetActorLocation()
        : Hit.WorldLocation;
    Number.Target = Hit.Target;
    Number.Value = Shown;
    Number.bCritical = Hit.Result.bCritical;
    Number.bWeakPoint = bWeak;
    Number.bFromDoT = Hit.bFromDoT;
    Number.Element = Hit.Element;
    Number.DamageTypeTag = Hit.DamageTypeTag;
    Number.MitigatedFraction = Mitigated;
    // BORN AT ARRIVAL: the draw skips negative ages, so the number appears
    // the frame the round lands, beside the spark it belongs to.
    Number.Time = ArrivalTime;
    Number.bKilled = Hit.Result.bKilled;
    Number.Overkill = Hit.Result.OverkillDamage;
    Number.Lifetime = Hit.Result.bKilled ? BreakerHUD::DamageKillLifetime
        : Hit.bFromDoT ? BreakerHUD::DamageDoTLifetime
        : BreakerHUDMath::DamageNumberLifetime(Hit.Result.bCritical);

    // SECONDARY: a second non-DoT number born within the sibling window on a
    // DIFFERENT target is the same trigger pull spilling over — a chain jump,
    // a ricochet, an AoE's outer victims. The contract carries no chain flag,
    // so proximity in time is the honest signal available: two deliberate
    // shots at two targets are 100ms+ apart at any human cadence, two chain
    // legs resolve in the same instant. The parent (first spawn) keeps full
    // weight; the spill draws lighter. Kills are never demoted — a kill by
    // ricochet is still a kill.
    if (!Hit.bFromDoT)
    {
        if (!Hit.Result.bKilled
            && Now - LastSiblingSpawnTime < BreakerHUD::DamageSecondaryWindow
            && LastSiblingSpawnTarget != Hit.Target)
        {
            Number.bSecondary = true;
        }
        LastSiblingSpawnTime = Now;
        LastSiblingSpawnTarget = Hit.Target;
    }

    PushDamageNumber(Number);
}

// One door into the ring buffer, shared by the live feed and the capture
// preview so the two can never disagree about how a number enters the pool.
void ABreakerPlaytestHUD::PushDamageNumber(const FBreakerHUDDamageNumber& Number)
{
    if (DamageNumbers.Num() < MaxDamageNumbers)
    {
        DamageNumbers.Add(Number);
        return;
    }

    // CULLS OLDEST-FIRST, which is what the spec says and what the write cursor
    // did not do. The cursor evicted in INSERTION order, and a merge refreshed a
    // number in place without moving its slot — so the busiest number on the
    // player's primary target was the first thing thrown away once the ring
    // filled. It also never reclaimed expired entries, so the array pinned at
    // the cap forever after the first full pass and a quiet moment never
    // cleared it.
    TArray<double> Births;
    TArray<double> Deaths;
    Births.Reserve(DamageNumbers.Num());
    Deaths.Reserve(DamageNumbers.Num());
    for (const FBreakerHUDDamageNumber& Existing : DamageNumbers)
    {
        Births.Add(Existing.Time);
        Deaths.Add(Existing.Time + Existing.Lifetime);
    }
    const int32 Evict = BreakerDamageFeed::IndexToEvict(Births, Deaths, Number.Time);
    if (DamageNumbers.IsValidIndex(Evict)) DamageNumbers[Evict] = Number;
}

// --------------------------------------------------------------------------
// -BreakerCaptureHUD. Dev-only, command-line-gated, and it exists for a
// specific reason: the states this HUD got WRONG are precisely the states a
// headless capture run cannot reach on its own. -BreakerAutoPlay drops the
// player into the gym and then nothing pulls a trigger and nothing presses F4,
// so the wave banner and every damage number were literally unphotographable —
// and both of them shipped broken, which is not a coincidence.
//
// It fabricates nothing about layout: the numbers and the banner go through
// exactly the same drawing paths the real ones do, at values chosen to be the
// worst realistic case (six-figure damage under O29, a two-digit hostile
// count, a heavily absorbed hit). What is faked is only the EVENT.
// --------------------------------------------------------------------------
bool ABreakerPlaytestHUD::IsCapturePreview() const
{
    // Parsed once. FParse over the whole command line every frame would be a
    // string scan per frame for a switch that cannot change.
    static const bool bPreview = FParse::Param(FCommandLine::Get(), TEXT("BreakerCaptureHUD"));
    return bPreview;
}

void ABreakerPlaytestHUD::TickCapturePreview(const ABreakerCharacter* Character)
{
    if (!IsCapturePreview() || !Character || !GetWorld()) return;
    const double Now = GetWorld()->GetTimeSeconds();
    // Re-seeded on the LONGEST lifetime in the set, so the capture catches
    // numbers mid-rise without a seeding overlapping the one before it.
    //
    // It keyed off the default lifetime alone, which is shorter than both the
    // kill and damage-over-time lifetimes — so the two longest-lived numbers
    // were still on screen when the next seeding fired and each was drawn on
    // top of a copy of itself. Found by reading a capture, not by a test: this
    // is a photograph of the instrument, and every fabricated hit carries a
    // null target so the real merge path cannot collapse them the way it would
    // in play. Raising the DoT lifetime to clear the Bleed tick interval made
    // it visibly worse, which is how it was noticed.
    const float LongestLifetime = FMath::Max3(
        BreakerHUDMath::DamageNumberLifetime(true), BreakerHUD::DamageDoTLifetime, BreakerHUD::DamageKillLifetime);

    // THE FORCED STATES, cycling every two seconds so a multi-frame capture
    // photographs each: a reload (the rail ramping and the word), a swap (the
    // name sliding), a cooldown (the drain), a loot plate over a fabricated
    // drop. Set every frame, not on the seeding cadence, because they are
    // states rather than events.
    PreviewPhase = FMath::Abs(FMath::FloorToInt(static_cast<float>(Now) / 2.0f)) % 4;
    bPreviewReload = PreviewPhase == 0;
    PreviewReloadFraction = bPreviewReload ? FMath::Frac(static_cast<float>(Now) / 2.0f) : 0.0f;
    PreviewCooldownFraction = PreviewPhase == 2
        ? 0.25f + 0.5f * FMath::Frac(static_cast<float>(Now) / 2.0f) : 0.0f;
    float ForcedCooldown = 0.0f;
    if (FParse::Value(FCommandLine::Get(), TEXT("BreakerCaptureCooldown="), ForcedCooldown))
        PreviewCooldownFraction = FMath::Clamp(ForcedCooldown, 0.0f, 1.0f);
    bPreviewLootPlate = PreviewPhase == 3;
    if (PreviewPhase == 1 && Now - SwapStartTime >= BreakerUI::HudWeaponSwapSeconds + 0.4f)
    {
        SwapStartTime = Now;
        SwapName = BoundWeapon ? BoundWeapon->GetArchetypeName().ToUpper() : BreakerStrings::Get(EBreakerStringKey::HudWeaponRifle);
    }

    if (Now - LastPreviewSpawnTime < LongestLifetime) return;
    LastPreviewSpawnTime = Now;

    const FVector Eye = Character->GetActorLocation();
    const FVector Forward = Character->GetActorForwardVector();
    const FVector Right = Character->GetActorRightVector();

    struct FPreviewHit
    {
        float Value; bool bCrit; bool bWeak; float Mitigated; float Side; float Up;
        bool bDoT = false; bool bKilled = false; float Overkill = 0.0f; bool bSecondary = false;
    };
    // Every class of hit the hierarchy has to keep distinguishable at a
    // glance: body, weak point, crit, absorbed crit, DoT tick, a killing blow
    // with visible overkill, and a secondary (chain/ricochet) spill.
    static const FPreviewHit Hits[] = {
        { 8420.0f,   false, false, 0.00f, -1.30f,  40.0f },
        { 26800.0f,  false, true,  0.00f, -0.35f,  95.0f },
        { 148200.0f, true,  false, 0.00f,  0.55f, 150.0f },
        { 71500.0f,  true,  false, 0.47f,  1.55f,  60.0f },
        { 1240.0f,   false, false, 0.00f, -0.85f, 150.0f, true },
        { 96400.0f,  false, false, 0.00f,  1.70f, 190.0f, false, true, 31200.0f },
        { 6100.0f,   false, false, 0.00f, -1.75f, 100.0f, false, false, 0.0f, true },
    };
    for (const FPreviewHit& Hit : Hits)
    {
        FBreakerHUDDamageNumber Number;
        Number.World = Eye + Forward * 620.0f + Right * (Hit.Side * 150.0f) + FVector(0.0f, 0.0f, Hit.Up);
        Number.Value = Hit.Value;
        Number.bCritical = Hit.bCrit;
        Number.bWeakPoint = Hit.bWeak;
        Number.MitigatedFraction = Hit.Mitigated;
        Number.Time = Now;
        Number.bFromDoT = Hit.bDoT;
        Number.bKilled = Hit.bKilled;
        Number.Overkill = Hit.Overkill;
        Number.bSecondary = Hit.bSecondary;
        Number.Lifetime = Hit.bKilled ? BreakerHUD::DamageKillLifetime
            : Hit.bDoT ? BreakerHUD::DamageDoTLifetime
            : BreakerHUDMath::DamageNumberLifetime(Hit.bCrit);
        PushDamageNumber(Number);

        // THE DEATH MOMENT, fabricated with the killing blow (GLASS-1). Same
        // renderer, same colour law, same point the number rises from; the
        // only fake is the event, as with everything else here. Without this
        // the fallback glow is unphotographable: -BreakerAutoPlay kills nothing.
        if (Hit.bKilled)
        {
            if (ABreakerEffectRenderer* Effects = ABreakerEffectRenderer::FindOrSpawn(GetWorld()))
            {
                Effects->PlayMoment(EBreakerEffectMoment::Death, Number.World, FVector::UpVector,
                    BreakerFX::MomentColor(EBreakerEffectMoment::Death, Hit.bWeak));
            }
        }
    }

    // THE MUZZLE MOMENT, fabricated at the visual muzzle on the same cadence
    // (the harness pulls no trigger). Its 60 ms life is shorter than a frame
    // interval at capture rate, so it is caught by the tick that seeds it.
    if (BoundWeapon)
    {
        if (ABreakerEffectRenderer* Effects = ABreakerEffectRenderer::FindOrSpawn(GetWorld()))
        {
            Effects->PlayMoment(EBreakerEffectMoment::Muzzle, BoundWeapon->GetVisualMuzzleLocation(),
                Forward, BreakerFX::MomentColor(EBreakerEffectMoment::Muzzle, false));
        }
    }

    // The crosshair kill confirm and the three banners, fabricated on the
    // same cadence so a multi-shot capture run photographs them mid-event.
    // All three at once, so the frame shows the stagger and the disjoint
    // rectangles; re-seeded only once the queue has emptied itself.
    LastKillConfirmTime = Now;
    bKillConfirmWeakPoint = false;
    if (Banners.IsEmpty())
    {
        EnqueueBanner(EBreakerBannerKind::WaveClear, TEXT("WAVE 3 CLEAR"), FString());
        EnqueueBanner(EBreakerBannerKind::LevelUp, TEXT("LEVEL 12"), TEXT("+1 CLASS   +1 CORE"));
        EnqueueBanner(EBreakerBannerKind::RiftComplete, TEXT("FERNHALL"), TEXT("RIFT CLEARED"));
    }
}

void ABreakerPlaytestHUD::HandlePlayerDamageReceived(const FBreakerDamageResult& Result)
{
    const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
    if (Result.bDodged) LastDodgeTime = Now;
    else if (Result.bBlocked) LastBlockTime = Now;
    // THE FOURTH SOUND (ruled: it matters more than the other three).
    // Immediate, never on the arrival clock — being hit has no flight — and
    // only for damage that actually landed: a dodge or a fully blocked hit
    // already has its own readout and earned its silence.
    //
    // NOT ON THE KILLING BLOW: the fatal hit is silent, and the death beat
    // carries the one sound the player's death has (O193) — a low cue at the
    // hard cut to black, scheduled below at the character's own
    // LowerAndDropSeconds so the sound and the black arrive together.
    //
    // THE TAKE-HIT VOCAL IS GONE (owner, playtest 2026-09-10): "i dont need
    // audio of my character groaning when i take damage (its so fucking
    // annoying)". It fired on every landed hit, which in a pack fight is most
    // seconds of the fight. The harm frame still announces the hit and the
    // health bar still moves; neither of them talks.
    if (Result.bKilled)
    {
        // The delay is read from the pawn's authored timeline, not a copy of
        // it: the character's beat and this cue cannot drift apart. A pawn
        // that is not a BreakerCharacter has no death beat and no cut for the
        // cue to land on, so it plays nothing. Discarded handle, as the
        // hit-confirm arrival timer: nothing later has grounds to cancel it.
        const ABreakerCharacter* Character = Cast<ABreakerCharacter>(GetOwningPawn());
        UWorld* World = GetWorld();
        if (!Character || !World) return;
        const float DelaySeconds = Character->DeathBeat.LowerAndDropSeconds;
        TWeakObjectPtr<ABreakerPlaytestHUD> WeakThis(this);
        FTimerHandle Discarded;
        World->GetTimerManager().SetTimer(Discarded, FTimerDelegate::CreateLambda([WeakThis]()
        {
            if (ABreakerPlaytestHUD* HUD = WeakThis.Get())
            {
                if (ABreakerSoundDirector* Sound = HUD->GetSoundDirector())
                {
                    Sound->PlayPlayerDeath();
                }
            }
        }), FMath::Max(DelaySeconds, UE_KINDA_SMALL_NUMBER), false);
    }
}

void ABreakerPlaytestHUD::DrawDefenseFeedback(const FVector2D& Center)
{
    const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
    const double DodgeAge = Now - LastDodgeTime;
    const double BlockAge = Now - LastBlockTime;
    const bool bDodgeFresh = DodgeAge >= 0.0 && DodgeAge < 0.8;
    const bool bBlockFresh = BlockAge >= 0.0 && BlockAge < 0.8;
    if (!bDodgeFresh && !bBlockFresh) return;

    const bool bShowDodge = bDodgeFresh && (!bBlockFresh || DodgeAge <= BlockAge);
    const float Age = static_cast<float>(bShowDodge ? DodgeAge : BlockAge);
    const float Fade = 1.0f - Age / 0.8f;
    // Dodge is a movement verb (O179); block is mitigation, which is the
    // armour/weapon family (orange).
    DrawSpecTextCentered(BreakerStrings::Get(bShowDodge ? EBreakerStringKey::HudCalloutDodged : EBreakerStringKey::HudCalloutBlocked),
        Center.X, Center.Y - S(108.0f), bShowDodge ? BreakerUI::VerbMove : BreakerUI::Orange, 20.0f, Fade, ESpecFontRole::Display);
}

// --------------------------------------------------------------------------
// The crosshair marks, in one place and one priority order. Hit: four
// diagonals 12×2 from the tick corners, weapon orange, 80 ms. Kill: the
// diagonals plus a 6×6 centre square, bone, 200 ms. Weak-point kill:
// diagonals 16×2 plus an 8×8 diamond, gold, 320 ms. A weak-point HIT is gold
// too: O179's promise (gold is the weak-point read) wins over the sheet,
// which colours every hit orange. An absorbed hit keeps its brackets — the
// round stopped at a surface — so the Warden's wrong-angle read survives.
//
// ONE CLOCK. Every latch here is future-dated to the round's ARRIVAL by
// HandlePlayerHitDealt, so the mark lands in the same frame as the spark, the
// number and the sound; the age guard is what holds a still-flying round's
// mark back. The kill outranks the hit at the crosshair.
// --------------------------------------------------------------------------
void ABreakerPlaytestHUD::DrawCrosshairMarks(const FVector2D& Center)
{
    const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
    const float KillAge = static_cast<float>(Now - LastKillConfirmTime);
    const float KillSeconds = bKillConfirmWeakPoint ? BreakerUI::HudWeakPointMarkSeconds : BreakerUI::HudKillMarkSeconds;
    const bool bKill = KillAge >= 0.0f && KillAge < KillSeconds;
    const float HitAge = static_cast<float>(Now - LastHitDealtTime);
    // The preview forces the absorbed hit on: it is the one mark nothing in a
    // headless run can produce, and the one the owner most needed to see.
    const bool bPreviewHit = IsCapturePreview() && !bKill;
    const bool bHit = (HitAge >= 0.0f && HitAge < BreakerUI::HudHitMarkSeconds) || bPreviewHit;
    if (!bKill && !bHit) return;

    const bool bWeakPoint = bKill ? bKillConfirmWeakPoint : bHitDealtWeakPoint;
    const bool bAbsorbed = !bKill && (bPreviewHit || bHitDealtAbsorbed);
    const FLinearColor Color = bWeakPoint ? BreakerUI::Gold
        : bKill ? BreakerUI::System : BreakerUI::Orange;
    const float Stroke = S(BreakerUI::HudCrosshairTickWidth);
    const float Diagonal = 0.7071f;
    // The diagonals start at the tick corners: the box's half-size, out.
    const float Inner = S(BreakerUI::HudCrosshairBox * 0.5f);
    const float Length = S(bWeakPoint && bKill ? BreakerUI::HudWeakPointDiagonal : BreakerUI::HudHitDiagonal);
    for (int32 Index = 0; Index < 4; ++Index)
    {
        const float DX = (Index & 1) ? 1.0f : -1.0f;
        const float DY = (Index & 2) ? 1.0f : -1.0f;
        DrawLine(Center.X + DX * Inner * Diagonal, Center.Y + DY * Inner * Diagonal,
                 Center.X + DX * (Inner + Length) * Diagonal, Center.Y + DY * (Inner + Length) * Diagonal,
                 Color, Stroke);
    }
    if (bKill && bWeakPoint)
    {
        // The diamond: an 8×8 outline at the centre.
        const float R = S(BreakerUI::HudWeakPointDiamond * 0.5f);
        DrawLine(Center.X, Center.Y - R, Center.X + R, Center.Y, Color, Stroke);
        DrawLine(Center.X + R, Center.Y, Center.X, Center.Y + R, Color, Stroke);
        DrawLine(Center.X, Center.Y + R, Center.X - R, Center.Y, Color, Stroke);
        DrawLine(Center.X - R, Center.Y, Center.X, Center.Y - R, Color, Stroke);
    }
    else if (bKill)
    {
        const float Square = S(BreakerUI::HudKillSquare);
        DrawRect(Color, Center.X - Square * 0.5f, Center.Y - Square * 0.5f, Square, Square);
    }
    else if (bAbsorbed)
    {
        // Four short brackets closing the diagonal ends into a box: the
        // round stopped at a surface. The opposite motion to a clean hit.
        const float Corner = S(9.0f);   // O2 PLACEHOLDER
        const float Reach = (Inner + Length) * Diagonal;
        for (int32 Index = 0; Index < 4; ++Index)
        {
            const float DX = (Index & 1) ? 1.0f : -1.0f;
            const float DY = (Index & 2) ? 1.0f : -1.0f;
            DrawLine(Center.X + DX * Reach, Center.Y + DY * Reach,
                     Center.X + DX * (Reach - Corner), Center.Y + DY * Reach, Color, Stroke);
            DrawLine(Center.X + DX * Reach, Center.Y + DY * Reach,
                     Center.X + DX * Reach, Center.Y + DY * (Reach - Corner), Color, Stroke);
        }
    }
}

// --------------------------------------------------------------------------
// The near-death frame: under 20 % health a full-screen harm border pulsing
// 8→16→8 px over 1.6 s, and four 64×64 corner brackets, 4 px L-shapes 24 px
// in from each corner, that do not pulse. Health only: shields regenerate,
// and a full-shield character at 10 % health is still one mistake from dying.
// Sits under the transient damage flash, which draws after it and brighter.
// --------------------------------------------------------------------------
void ABreakerPlaytestHUD::DrawNearDeathFrame(const ABreakerCharacter* Character)
{
    const UBreakerAttributeSet* Attributes = Character ? Character->GetAttributes() : nullptr;
    if (!Attributes || !Canvas) return;
    const float MaxHealth = Attributes->GetMaxHealth();
    // The preview forces the state: nothing in a headless run can lose
    // health, so without this the frame is unphotographable — the exact
    // failure mode the capture harness exists to close.
    const float Fraction = IsCapturePreview() ? 0.12f
        : (MaxHealth > UE_SMALL_NUMBER ? Attributes->GetHealth() / MaxHealth : 1.0f);
    if (!BreakerHUDMath::NearDeathVisible(Fraction)) return;

    const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
    const float W = Canvas->ClipX;
    const float H = Canvas->ClipY;
    DrawBorder(0.0f, 0.0f, W, H, BreakerUI::Harm, S(BreakerHUDMath::NearDeathFrameWidth(Now)));

    const float Inset = S(BreakerUI::HudNearDeathBracketInset);
    const float Size = S(BreakerUI::HudNearDeathBracketSize);
    const float Stroke = S(BreakerUI::HudNearDeathBracketStroke);
    for (int32 Index = 0; Index < 4; ++Index)
    {
        const bool bRight = (Index & 1) != 0;
        const bool bBottom = (Index & 2) != 0;
        const float CornerX = bRight ? W - Inset : Inset;
        const float CornerY = bBottom ? H - Inset : Inset;
        // Each L: a horizontal arm and a vertical arm meeting at the corner.
        DrawRect(BreakerUI::Harm, bRight ? CornerX - Size : CornerX, bBottom ? CornerY - Stroke : CornerY, Size, Stroke);
        DrawRect(BreakerUI::Harm, bRight ? CornerX - Stroke : CornerX, bBottom ? CornerY - Size : CornerY, Stroke, Size);
    }
}

// --------------------------------------------------------------------------
// Status chips, sitting above the vitals plate. BottomY is the row's bottom
// edge so the chips grow upward and never displace the plate.
// --------------------------------------------------------------------------
// --------------------------------------------------------------------------
// HUD v2 — effects read DOWNWARD, one per line, newest at the bottom. A
// vertical column of an 8px dot plus an 11px value can be counted at a glance,
// where the horizontal chip run it replaces had to be READ — and that run was
// also unbounded to the right, walking off toward the wave banner once three
// DoTs were up. A column cannot collide with anything: it grows into empty
// screen above its own anchor. Returns the height consumed.
// --------------------------------------------------------------------------
float ABreakerPlaytestHUD::DrawStatusReadout(const ABreakerCharacter* Character, float X, float BottomY, float Width)
{
    const UBreakerStatusComponent* Status = Character ? Character->FindComponentByClass<UBreakerStatusComponent>() : nullptr;
    if (!Status) return 0.0f;

    const TArray<FBreakerActiveStatus>& Active = Status->GetActiveStatuses();
    const float Threshold = Status->GetEntropyThreshold();
    const float EntropyFraction = Threshold > UE_SMALL_NUMBER ? FMath::Clamp(Status->GetEntropyBuildup() / Threshold, 0.0f, 1.0f) : 0;
    const float VoidThreshold = Status->GetVoidThreshold();
    const float VoidFraction = VoidThreshold > UE_SMALL_NUMBER ? FMath::Clamp(Status->GetVoidBuildup() / VoidThreshold, 0.0f, 1.0f) : 0;
    const float RiftThreshold = Status->GetRiftThreshold();
    const float RiftFraction = RiftThreshold > UE_SMALL_NUMBER ? FMath::Clamp(Status->GetRiftBuildup() / RiftThreshold, 0.0f, 1.0f) : 0;
    if (Active.Num() == 0 && EntropyFraction <= 0 && VoidFraction <= 0 && RiftFraction <= 0) return 0.0f;

    const float Dot = S(BreakerUI::HudV2StatusDot);
    const float Pixels = BreakerUI::HudV2StatusPixels;
    const float RowGap = S(BreakerUI::HudV2StatusRowGap);
    const float RowH = FMath::Max(Dot, MeasureSpecText(TEXT("0"), Pixels, ESpecFontRole::Mono).Y);

    // Newest at the BOTTOM, so the row nearest the momentum track is the one
    // that just landed and the column above it is history.
    float RowBottom = BottomY;
    if (EntropyFraction > 0)
    {
        const float RailH = S(2.0f); // O2 presentation, attached to vitals only.
        const float RowY = RowBottom - RowH - RailH - S(3.0f);
        const FString Text = FString::Printf(TEXT("%s %d%%"), *BreakerStrings::Get(EBreakerStringKey::HudEntropy),
            FMath::Clamp(FMath::RoundToInt(EntropyFraction * 100), 1, 100));
        DrawSpecText(Text, X, RowY, BreakerUI::Gold, Pixels, 1.0f, ESpecFontRole::Mono);
        DrawRect(BreakerUI::BorderRest, X, RowBottom - RailH, Width, RailH);
        DrawRect(BreakerUI::Gold, X, RowBottom - RailH, Width * EntropyFraction, RailH);
        RowBottom = RowY - RowGap;
    }
    if (VoidFraction > 0)
    {
        const float RailH = S(2.0f); // O2 presentation, independent of concurrent Entropy.
        const float RowY = RowBottom - RowH - RailH - S(3.0f);
        const FString Text = FString::Printf(TEXT("%s %d%%"), *BreakerStrings::Get(EBreakerStringKey::HudVoid),
            FMath::Clamp(FMath::RoundToInt(VoidFraction * 100), 1, 100));
        DrawSpecText(Text, X, RowY, BreakerUI::Violet, Pixels, 1.0f, ESpecFontRole::Mono);
        DrawRect(BreakerUI::BorderRest, X, RowBottom - RailH, Width, RailH);
        DrawRect(BreakerUI::Violet, X, RowBottom - RailH, Width * VoidFraction, RailH);
        RowBottom = RowY - RowGap;
    }
    if (RiftFraction > 0)
    {
        const float RailH = S(2.0f); // O2 presentation, independent of concurrent Entropy.
        const float RowY = RowBottom - RowH - RailH - S(3.0f);
        const FString Text = FString::Printf(TEXT("%s %d%%"), *BreakerStrings::Get(EBreakerStringKey::HudRift),
            FMath::Clamp(FMath::RoundToInt(RiftFraction * 100), 1, 100));
        DrawSpecText(Text, X, RowY, BreakerRiftFeedback::Color, Pixels, 1.0f, ESpecFontRole::Mono);
        DrawRect(BreakerUI::BorderRest, X, RowBottom - RailH, Width, RailH);
        DrawRect(BreakerRiftFeedback::Color, X, RowBottom - RailH, Width * RiftFraction, RailH);
        RowBottom = RowY - RowGap;
    }
    for (int32 Index = Active.Num() - 1; Index >= 0; --Index)
    {
        const FBreakerActiveStatus& Entry = Active[Index];
        const bool bRot = Entry.Spec.StatusTag == FGameplayTag::RequestGameplayTag(TEXT("Status.Rot"));
        const bool bUnstable = Entry.Spec.StatusTag == FGameplayTag::RequestGameplayTag(TEXT("Status.Unstable"));
        const bool bErased = Entry.Spec.StatusTag == FGameplayTag::RequestGameplayTag(TEXT("Status.Erased"));
        FString ShortName = Entry.Spec.StatusTag.IsValid()
            ? Entry.Spec.StatusTag.GetTagName().ToString() : BreakerStrings::Get(EBreakerStringKey::HudStatusUnnamed);
        int32 SeparatorIndex = INDEX_NONE;
        if (ShortName.FindLastChar(TEXT('.'), SeparatorIndex)) ShortName = ShortName.RightChop(SeparatorIndex + 1);
        const FString Text = Entry.bPersistentRot ? BreakerStrings::Get(EBreakerStringKey::HudRotPersistent) : bUnstable ? BreakerStrings::Format(EBreakerStringKey::HudUnstableTimer, FMath::Max(Entry.RemainingDuration, 0.0f)) : bErased ? BreakerStrings::Format(EBreakerStringKey::HudErasedTimer, FMath::Max(Entry.RemainingDuration, 0.0f)) : bRot ? BreakerStrings::Format(EBreakerStringKey::HudRotTimer, FMath::Max(Entry.RemainingDuration, 0.0f)) : Entry.Stacks > 1
            ? FString::Printf(TEXT("%s %d  %.1f"), *ShortName.ToUpper(), Entry.Stacks,
                FMath::Max(Entry.RemainingDuration, 0.0f))
            : FString::Printf(TEXT("%s  %.1f"), *ShortName.ToUpper(),
                FMath::Max(Entry.RemainingDuration, 0.0f));

        const float ExtraH = (bRot || bErased || bUnstable) ? S(5.0f) : 0;
        const float RowY = RowBottom - RowH - ExtraH;
        const FLinearColor StatusColor = bUnstable ? BreakerRiftFeedback::Color : bErased ? BreakerUI::Violet : bRot ? BreakerUI::Orange : BreakerUI::Harm;
        DrawRect(StatusColor, X, RowY + (RowH - Dot) * 0.5f, Dot, Dot);
        const FVector2D TextSize = MeasureSpecText(Text, Pixels, ESpecFontRole::Mono);
        DrawSpecText(Text, X + Dot + S(BreakerUI::Space8), RowY + (RowH - TextSize.Y) * 0.5f,
            StatusColor, Pixels, 1.0f, ESpecFontRole::Mono);
        if (bRot || bErased || bUnstable)
        {
            const float Remaining = Entry.bPersistentRot ? 1.0f : FMath::Clamp(Entry.RemainingDuration / FMath::Max(Entry.Spec.Duration, UE_SMALL_NUMBER), 0.0f, 1.0f);
            DrawRect(BreakerUI::BorderRest, X, RowBottom - S(2.0f), Width, S(2.0f));
            DrawRect(StatusColor, X, RowBottom - S(2.0f), Width * Remaining, S(2.0f));
        }
        RowBottom = RowY - RowGap;
    }
    return BottomY - (RowBottom + RowGap);
}

// ==========================================================================
// Drawing primitives
// ==========================================================================

// The crosshair: four 2×12 ticks in bone, each starting GapPx from the
// centre (a 40×40 box at rest). ADS collapses the ticks toward the centre
// into a 2×2 dot and raises a 24 px ring, blended by AdsBlend so the 80 ms
// reads as motion rather than a swap. The ring is a segmented outline: the
// system has no circle primitive and a 1 px polygon at 24 px reads as one.
void ABreakerPlaytestHUD::DrawCrosshair(const FVector2D& Center, float GapPx, float AdsBlend)
{
    const float Blend = FMath::Clamp(AdsBlend, 0.0f, 1.0f);
    const float Stroke = S(BreakerUI::HudCrosshairTickWidth);
    const float Gap = FMath::Lerp(GapPx, 0.0f, Blend);
    const float Length = FMath::Lerp(S(BreakerUI::HudCrosshairTickLength), S(BreakerUI::HudCrosshairAdsDot), Blend);
    const float TickAlpha = 1.0f - Blend;
    if (TickAlpha > 0.0f)
    {
        const FLinearColor Tick = BreakerUI::Alpha(BreakerUI::System, TickAlpha);
        DrawLine(Center.X - Gap, Center.Y, Center.X - Gap - Length, Center.Y, Tick, Stroke);
        DrawLine(Center.X + Gap, Center.Y, Center.X + Gap + Length, Center.Y, Tick, Stroke);
        DrawLine(Center.X, Center.Y - Gap, Center.X, Center.Y - Gap - Length, Tick, Stroke);
        DrawLine(Center.X, Center.Y + Gap, Center.X, Center.Y + Gap + Length, Tick, Stroke);
    }
    if (Blend > 0.0f)
    {
        const FLinearColor Ads = BreakerUI::Alpha(BreakerUI::System, Blend);
        const float Dot = S(BreakerUI::HudCrosshairAdsDot);
        DrawRect(Ads, Center.X - Dot * 0.5f, Center.Y - Dot * 0.5f, Dot, Dot);
        const float Radius = S(BreakerUI::HudCrosshairAdsRing * 0.5f);
        constexpr int32 Segments = 24;
        FVector2D Previous(Center.X + Radius, Center.Y);
        for (int32 Index = 1; Index <= Segments; ++Index)
        {
            const float Angle = 2.0f * UE_PI * static_cast<float>(Index) / Segments;
            const FVector2D Point(Center.X + FMath::Cos(Angle) * Radius, Center.Y + FMath::Sin(Angle) * Radius);
            DrawLine(Previous.X, Previous.Y, Point.X, Point.Y, Ads, FMath::Max(S(BreakerUI::BorderThin), 1.0f));
            Previous = Point;
        }
    }
}

// The engine's small font is a bitmap face at one native size. Asking Canvas
// for a 40px damage number or a 44px magazine meant magnifying that bitmap
// three to seven times, which is why both read as fuzzy. Slate's font info
// rasterises a vector face at whatever pixel size it is handed, so every
// readout below is rendered at its true size instead of scaled up to it.
// FCanvasTextItem refuses to draw anything unless its UFont pointer is set:
// HasValidText() is literally `Font != nullptr`, and the FSlateFontInfo
// constructor fills that in with Cast<UFont>(FontInfo.FontObject). An
// FSlateFontInfo from FCoreStyle carries a raw FCompositeFont and NO UObject,
// so every string silently vanished. The font therefore has to be a real UFont
// asset — and a Runtime-cached one, because GetFontCacheType() dereferences it
// to pick the draw path, and the offline path ignores the size in the font
// info and goes back to magnifying a bitmap.
const UFont* ABreakerPlaytestHUD::GetSpecFont(ESpecFontRole FontRole)
{
    auto& Font = FontRole == ESpecFontRole::Display ? SpecDisplayFont : FontRole == ESpecFontRole::Mono ? SpecMonoFont : SpecFont;
    if (!Font)
    {
        const TCHAR* Path = FontRole == ESpecFontRole::Display ? TEXT("/Game/Breaker/UI/Fonts/F_BreakerDisplay.F_BreakerDisplay")
            : FontRole == ESpecFontRole::Mono ? TEXT("/Game/Breaker/UI/Fonts/F_BreakerMono.F_BreakerMono")
            : TEXT("/Game/Breaker/UI/Fonts/F_BreakerBody.F_BreakerBody");
        Font = LoadObject<UFont>(nullptr, Path);
        if (!Font) Font = LoadObject<UFont>(nullptr, TEXT("/Engine/EngineFonts/Roboto.Roboto"));
    }
    return Font;
}

bool ABreakerPlaytestHUD::CanDrawSpecFont(ESpecFontRole FontRole)
{
    const UFont* Font = GetSpecFont(FontRole);
    return Font && Font->FontCacheType == EFontCacheType::Runtime;
}

FSlateFontInfo ABreakerPlaytestHUD::MakeSpecFont(float SpecPixels, ESpecFontRole FontRole)
{
    // Roles are explicit; a large number and a large heading use different faces.
    const int32 PixelSize = FMath::Max(FMath::RoundToInt(S(SpecPixels)), 6);
    const UFont* Font = GetSpecFont(FontRole);
    const FName Face = Font && Font->GetFName() == TEXT("Roboto")
        ? FName(FontRole == ESpecFontRole::Body ? TEXT("Regular") : TEXT("Bold"))
        : FName(FontRole == ESpecFontRole::Display ? TEXT("SemiBold") : FontRole == ESpecFontRole::Mono ? TEXT("Medium") : TEXT("Regular"));
    return FSlateFontInfo(Font, PixelSize, Face);
}

FVector2D ABreakerPlaytestHUD::MeasureSpecText(const FString& Text, float SpecPixels, ESpecFontRole FontRole)
{
    if (CanDrawSpecFont(FontRole) && FSlateApplication::IsInitialized())
    {
        if (const FSlateRenderer* Renderer = FSlateApplication::Get().GetRenderer())
        {
            return FVector2D(Renderer->GetFontMeasureService()->Measure(Text, MakeSpecFont(SpecPixels, FontRole)));
        }
    }
    // Headless or pre-Slate: fall back to the legacy path so measurement never
    // returns zero and collapses a right-aligned column onto its neighbour.
    float Width = 0.0f;
    float Height = 0.0f;
    GetTextSize(Text, Width, Height, GEngine ? GEngine->GetSmallFont() : nullptr,
        S(BreakerUI::CanvasTextScale(SpecPixels)));
    return FVector2D(Width, Height);
}

void ABreakerPlaytestHUD::DrawSpecText(const FString& Text, float X, float Y, const FLinearColor& Color, float SpecPixels, float TextAlpha, ESpecFontRole FontRole)
{
    if (TextAlpha <= 0.0f || !Canvas) return;
    const FLinearColor Face = BreakerUI::Alpha(Color, Color.A * TextAlpha);
    if (CanDrawSpecFont(FontRole))
    {
        FCanvasTextItem Item(FVector2D(X, Y), FText::FromString(Text), MakeSpecFont(SpecPixels, FontRole), Face);
        // No shadow and no engine outline: this system draws its own outline
        // pass where it wants one (§4), and a default drop shadow would put a
        // soft edge on a spec that says flat fills and hard edges only.
        Item.EnableShadow(FLinearColor::Transparent);
        Item.bOutlined = false;
        Canvas->DrawItem(Item);
        return;
    }
    DrawText(Text, Face, X, Y, GEngine ? GEngine->GetSmallFont() : nullptr,
        S(BreakerUI::CanvasTextScale(SpecPixels)), false);
}

float ABreakerPlaytestHUD::FitSpecPixels(const FString& Text, float DesiredPixels, float MaxWidth, float MinPixels, ESpecFontRole FontRole)
{
    if (Text.IsEmpty() || MaxWidth <= 0.0f) return DesiredPixels;
    // Down one spec pixel at a time. The type scale is small integers and the
    // loop is bounded by (Desired - Min), so this is a handful of measures in
    // the worst case and usually exactly one.
    for (float Pixels = DesiredPixels; Pixels > MinPixels; Pixels -= 1.0f)
    {
        if (MeasureSpecText(Text, Pixels, FontRole).X <= MaxWidth) return Pixels;
    }
    return MinPixels;
}

void ABreakerPlaytestHUD::DrawSpecTextRight(const FString& Text, float RightX, float Y, const FLinearColor& Color, float SpecPixels, float TextAlpha, ESpecFontRole FontRole)
{
    DrawSpecText(Text, RightX - MeasureSpecText(Text, SpecPixels, FontRole).X, Y, Color, SpecPixels, TextAlpha, FontRole);
}

void ABreakerPlaytestHUD::DrawSpecTextCentered(const FString& Text, float CenterX, float Y, const FLinearColor& Color, float SpecPixels, float TextAlpha, ESpecFontRole FontRole)
{
    DrawSpecText(Text, CenterX - MeasureSpecText(Text, SpecPixels, FontRole).X * 0.5f, Y, Color, SpecPixels, TextAlpha, FontRole);
}

// §4: a 2px outline in a near-black tinted toward the number's own hue, so the
// outline never reads as grey mud.
void ABreakerPlaytestHUD::DrawOutlinedNumber(const FString& Text, float CenterX, float Y, const FLinearColor& Face, float SpecPixels, float TextAlpha)
{
    if (TextAlpha <= 0.0f) return;
    const float X = CenterX - MeasureSpecText(Text, SpecPixels, ESpecFontRole::Mono).X * 0.5f;
    const FLinearColor Outline(Face.R * 0.10f, Face.G * 0.10f, Face.B * 0.10f, 0.9f * TextAlpha);
    const float Offset = FMath::Max(S(SpecPixels * 0.05f), 1.0f);
    DrawSpecText(Text, X - Offset, Y, Outline, SpecPixels, 1.0f, ESpecFontRole::Mono);
    DrawSpecText(Text, X + Offset, Y, Outline, SpecPixels, 1.0f, ESpecFontRole::Mono);
    DrawSpecText(Text, X, Y - Offset, Outline, SpecPixels, 1.0f, ESpecFontRole::Mono);
    DrawSpecText(Text, X, Y + Offset, Outline, SpecPixels, 1.0f, ESpecFontRole::Mono);
    DrawSpecText(Text, X, Y, Face, SpecPixels, TextAlpha, ESpecFontRole::Mono);
}

void ABreakerPlaytestHUD::DrawBorder(float X, float Y, float Width, float Height, const FLinearColor& Color, float Thickness)
{
    const float T = FMath::Max(Thickness, 1.0f);
    DrawRect(Color, X, Y, Width, T);
    DrawRect(Color, X, Y + Height - T, Width, T);
    DrawRect(Color, X, Y, T, Height);
    DrawRect(Color, X + Width - T, Y, T, Height);
}

void ABreakerPlaytestHUD::DrawPlate(float X, float Y, float Width, float Height, const FLinearColor& Rail, EBreakerRail RailEdge, const FLinearColor& Face)
{
    const FLinearColor Fill = Face.A > 0.0f ? Face : BreakerHUD::PlateFace;
    DrawRect(Fill, X, Y, Width, Height);
    DrawBorder(X, Y, Width, Height, BreakerUI::BorderEmphasis, S(BreakerUI::BorderThin));

    // The rail is full-bleed to the plate's edge: no inset, no radius. 4px
    // identity on the left, 2px status on the top (01-tokens).
    if (RailEdge == EBreakerRail::Top) DrawRect(Rail, X, Y, Width, S(BreakerUI::HudRailStatus));
    else                               DrawRect(Rail, X, Y, S(BreakerUI::HudRailIdentity), Height);
}

void ABreakerPlaytestHUD::DrawTriangle(const FVector2D& A, const FVector2D& B, const FVector2D& C, const FLinearColor& Color)
{
    // UCanvas::DefaultTexture is the engine's white square. Using it rather
    // than the RenderCore-side GWhiteTexture keeps this module's dependency
    // list unchanged for one flat fill.
    if (!Canvas || !Canvas->DefaultTexture) return;
    const FTexture* WhiteTexture = Canvas->DefaultTexture->GetResource();
    if (!WhiteTexture) return;

    FCanvasTriangleItem Item(A, B, C, WhiteTexture);
    Item.SetColor(Color);
    Item.BlendMode = SE_BLEND_Translucent;
    Canvas->DrawItem(Item);
}

void ABreakerPlaytestHUD::DrawAbilityRecoveryDisc(const FVector2D& Center, float Radius, float Fraction, const FLinearColor& Color)
{
    constexpr int32 Segments = 64; // O2 PLACEHOLDER: smooth at the small HUD icon size.
    const float Fill = FMath::Clamp(Fraction, 0.0f, 1.0f);
    const int32 Count = FMath::CeilToInt(Fill * Segments);
    for (int32 Index = 0; Index < Count; ++Index)
    {
        const float Start = static_cast<float>(Index) / Segments;
        const float End = FMath::Min(static_cast<float>(Index + 1) / Segments, Fill);
        DrawTriangle(Center, Center + BreakerHUDMath::AbilityRadialPoint(Start) * Radius,
            Center + BreakerHUDMath::AbilityRadialPoint(End) * Radius, Color);
    }
}

void ABreakerPlaytestHUD::DrawShearedBlock(float X, float Y, float Width, float Height, float Shear, const FLinearColor& Color)
{
    const FVector2D TopLeft(X + Shear, Y);
    const FVector2D TopRight(X + Width + Shear, Y);
    const FVector2D BottomRight(X + Width, Y + Height);
    const FVector2D BottomLeft(X, Y + Height);
    DrawTriangle(TopLeft, TopRight, BottomRight, Color);
    DrawTriangle(TopLeft, BottomRight, BottomLeft, Color);
}

void ABreakerPlaytestHUD::DrawTrack(float X, float Y, float Width, float Height, float Fraction, const FLinearColor& Fill, const FLinearColor& Track)
{
    DrawRect(Track, X, Y, Width, Height);
    DrawRect(Fill, X, Y, Width * FMath::Clamp(Fraction, 0.0f, 1.0f), Height);
}

// The hatch: a flat fill of A with 135° stripes of B — A for Stripe of every
// Period, B for the rest, measured across the stripes (01-tokens: 6/2 of 8).
// Each stripe is a line of constant
// (x + y) clipped by hand to the rectangle — Canvas has no clip for a line —
// and the first stripe is anchored to the screen (BreakerHUDMath::
// HatchStripeStart) so two adjacent hatches meet without a seam.
void ABreakerPlaytestHUD::DrawHatch(float X, float Y, float Width, float Height, const FLinearColor& A, const FLinearColor& B,
    float Period, float Stripe)
{
    if (Width <= 0.0f || Height <= 0.0f) return;
    DrawRect(A, X, Y, Width, Height);
    const float ScaledPeriod = S(Period);
    const float Step = BreakerHUDMath::HatchDiagonalStep(ScaledPeriod);
    if (Step <= 0.0f) return;
    // The stripe's width across the diagonal is the width DrawLine draws
    // perpendicular to the line, so it is passed through unchanged.
    const float Thickness = FMath::Max(S(Period - Stripe), 1.0f);
    const float Right = X + Width;
    const float Bottom = Y + Height;
    // The line x + y = D crosses the rectangle for x in [max(X, D - Bottom),
    // min(Right, D - Y)]; an empty interval is a stripe past the corner.
    for (float D = BreakerHUDMath::HatchStripeStart(X, Y, ScaledPeriod); D <= Right + Bottom; D += Step)
    {
        const float X0 = FMath::Max(X, D - Bottom);
        const float X1 = FMath::Min(Right, D - Y);
        if (X1 <= X0) continue;
        DrawLine(X0, D - X0, X1, D - X1, B, Thickness);
    }
}

// --------------------------------------------------------------------------
// Ability marks, built to UI-Ability-Icons-Spec.md's construction notes: one
// stroke weight, one hue, side-on, motion rising toward the upper right. These
// are code-drawn stand-ins for the commissioned SVGs, not a substitute for
// them — but they carry the silhouette the spec describes, which a letter in a
// box never did. Coordinates are normalised inside the 36x36 optical box.
// --------------------------------------------------------------------------
void ABreakerPlaytestHUD::DrawAbilityGlyph(const UBreakerAbilityDefinition* Definition, float CenterX, float CenterY, float BoxSize, const FLinearColor& Color)
{
    const float Left = CenterX - BoxSize * 0.5f;
    const float Top = CenterY - BoxSize * 0.5f;
    // 2px at 52px, scaled with the box and never below a hairline.
    const float Stroke = FMath::Max(BoxSize * (2.0f / 36.0f), 1.0f);

    // Normalised helpers: every glyph below reads as coordinates on a unit
    // square, which is how the spec's sketches are dimensioned.
    auto PX = [Left, BoxSize](float U) { return Left + U * BoxSize; };
    auto PY = [Top, BoxSize](float V) { return Top + V * BoxSize; };
    auto Stroke2 = [this, &PX, &PY, Stroke, &Color](float U0, float V0, float U1, float V1)
    {
        DrawLine(PX(U0), PY(V0), PX(U1), PY(V1), Color, Stroke);
    };

    FString Leaf;
    if (Definition)
    {
        Leaf = Definition->AbilityId.ToString();
        int32 Separator = INDEX_NONE;
        if (Leaf.FindLastChar(TEXT('.'), Separator)) Leaf = Leaf.RightChop(Separator + 1);
    }

    if (Leaf.Equals(TEXT("Skim"), ESearchCase::IgnoreCase))
    {
        // A flat velocity line that snaps onto a new upward vector, with the
        // arrowhead at the break. The elbow is the loudest feature: Skim
        // redirects momentum, it does not create it.
        Stroke2(0.04f, 0.74f, 0.46f, 0.74f);
        Stroke2(0.46f, 0.74f, 0.94f, 0.20f);
        Stroke2(0.94f, 0.20f, 0.74f, 0.26f);
        Stroke2(0.94f, 0.20f, 0.86f, 0.44f);
        // Two short speed ticks trailing behind the elbow.
        Stroke2(0.00f, 0.92f, 0.20f, 0.92f);
        Stroke2(0.10f, 0.58f, 0.26f, 0.58f);
        return;
    }
    if (Leaf.Equals(TEXT("Lead"), ESearchCase::IgnoreCase))
    {
        // A dashed sightline climbing to a tagged diamond, dashes lengthening
        // with distance: a tag clamped onto something, not a reticle floating
        // over it.
        Stroke2(0.02f, 0.92f, 0.12f, 0.83f);
        Stroke2(0.20f, 0.76f, 0.34f, 0.63f);
        Stroke2(0.42f, 0.56f, 0.60f, 0.40f);
        const float DiamondU = 0.76f;
        const float DiamondV = 0.24f;
        const float R = 0.16f;
        Stroke2(DiamondU, DiamondV - R, DiamondU + R, DiamondV);
        Stroke2(DiamondU + R, DiamondV, DiamondU, DiamondV + R);
        Stroke2(DiamondU, DiamondV + R, DiamondU - R, DiamondV);
        Stroke2(DiamondU - R, DiamondV, DiamondU, DiamondV - R);
        // Stub ticks on opposing corners.
        Stroke2(DiamondU, DiamondV - R, DiamondU, DiamondV - R - 0.10f);
        Stroke2(DiamondU, DiamondV + R, DiamondU, DiamondV + R + 0.10f);
        return;
    }
    if (Leaf.Equals(TEXT("Overdrive"), ESearchCase::IgnoreCase))
    {
        // A meter whose fill has broken past its own end cap and continues as
        // detached blocks: the container is complete and the contents are not.
        const float BarTop = 0.46f;
        const float BarBottom = 0.72f;
        Stroke2(0.02f, BarTop, 0.56f, BarTop);
        Stroke2(0.02f, BarBottom, 0.56f, BarBottom);
        Stroke2(0.02f, BarTop, 0.02f, BarBottom);
        Stroke2(0.56f, BarTop, 0.56f, BarBottom);
        DrawRect(Color, PX(0.06f), PY(BarTop + 0.05f), BoxSize * 0.46f, BoxSize * (BarBottom - BarTop - 0.10f));
        DrawRect(Color, PX(0.66f), PY(BarTop + 0.05f), BoxSize * 0.12f, BoxSize * (BarBottom - BarTop - 0.10f));
        DrawRect(Color, PX(0.86f), PY(BarTop + 0.05f), BoxSize * 0.10f, BoxSize * (BarBottom - BarTop - 0.10f));
        // The chevron lifting out of the bar.
        Stroke2(0.40f, 0.32f, 0.58f, 0.10f);
        Stroke2(0.58f, 0.10f, 0.76f, 0.32f);
        return;
    }

    if (Leaf.Equals(TEXT("Cleave"), ESearchCase::IgnoreCase))
    {
        // SPELLBLADE STRIKE. A narrow blade angled up to the right, its cutting
        // edge doubled by a second parallel line — the mana edge sitting a hair
        // off the steel — with one clean arc across the lower half as the swing
        // path, cut off before it closes so it reads as a slash, not a ring.
        // Min size is "blade angle + arc": the two are held ~0.15 of the box
        // apart at their closest, so the arc never merges into the blade even
        // when the doubled edge does.
        Stroke2(0.34f, 0.72f, 0.92f, 0.14f);
        // The mana edge, offset perpendicular to the blade. Allowed to merge
        // with the steel below 40px; it is the first thing to go.
        Stroke2(0.42f, 0.79f, 1.00f, 0.21f);
        // Swing path: an arc under the blade, open at both ends.
        {
            const float CU = 0.50f;
            const float CV = 0.46f;
            const float R = 0.44f;
            constexpr int32 Segments = 6;
            const float Start = FMath::DegreesToRadians(30.0f);
            const float End = FMath::DegreesToRadians(150.0f);
            float PrevU = CU + R * FMath::Cos(Start);
            float PrevV = CV + R * FMath::Sin(Start);
            for (int32 Index = 1; Index <= Segments; ++Index)
            {
                const float Angle = FMath::Lerp(Start, End, static_cast<float>(Index) / Segments);
                const float U = CU + R * FMath::Cos(Angle);
                const float V = CV + R * FMath::Sin(Angle);
                Stroke2(PrevU, PrevV, U, V);
                PrevU = U;
                PrevV = V;
            }
        }
        return;
    }
    if (Leaf.Equals(TEXT("Closequarter"), ESearchCase::IgnoreCase))
    {
        // VOID LASH. A single S-curve whipping from the lower-left corner to a
        // two-pronged barb at the far upper right, drawn at full stroke the
        // whole way, with two small dots falling off the underside — the tail
        // coming apart as it travels. Min size is reach: the curve touches two
        // opposite corners of the box, so it is the last thing to shrink.
        {
            const FVector2D P0(0.03f, 0.95f);
            const FVector2D P1(0.58f, 0.86f);
            const FVector2D P2(0.34f, 0.20f);
            const FVector2D P3(0.90f, 0.10f);
            constexpr int32 Segments = 8;
            FVector2D Prev = P0;
            for (int32 Index = 1; Index <= Segments; ++Index)
            {
                const float T = static_cast<float>(Index) / Segments;
                const float IT = 1.0f - T;
                const FVector2D Point =
                    P0 * (IT * IT * IT) + P1 * (3.0f * IT * IT * T) + P2 * (3.0f * IT * T * T) + P3 * (T * T * T);
                Stroke2(Prev.X, Prev.Y, Point.X, Point.Y);
                Prev = Point;
            }
            // The barb: two prongs off the tip, never dropped.
            Stroke2(P3.X, P3.Y, 0.70f, 0.06f);
            Stroke2(P3.X, P3.Y, 0.82f, 0.30f);
        }
        // Two dots off the underside. These go first at small sizes.
        DrawRect(Color, PX(0.24f) - Stroke * 0.5f, PY(0.92f) - Stroke * 0.5f, Stroke, Stroke);
        DrawRect(Color, PX(0.44f) - Stroke * 0.5f, PY(0.78f) - Stroke * 0.5f, Stroke, Stroke);
        return;
    }
    if (Leaf.Equals(TEXT("Unmake"), ESearchCase::IgnoreCase))
    {
        // OVERCAST. A muted baseline across the middle with the bar's outline
        // continuing below it: one channel, half above zero and half beneath,
        // because the cost is the same resource and not a second one. A small
        // cross sits under the dipped section as the debt mark.
        //
        // Unmake carries this mark because Unmake is the ability that rewrites
        // the price of every Caster cast — the set's one statement about the
        // cost channel itself. It is the ultimate, so the caller hands it
        // violet; the baseline stays grey, which is the one place the icon
        // system allows a second value, and the spec names it explicitly.
        const FLinearColor BaselineColor = Color.Equals(BreakerUI::TextDisabled)
            ? BreakerUI::TextDisabled : BreakerUI::TextMuted;
        const float BaselineT = FMath::Max(Stroke * 0.5f, 1.0f);
        DrawRect(BaselineColor, PX(0.02f), PY(0.50f) - BaselineT * 0.5f, BoxSize * 0.96f, BaselineT);

        // The channel: constant height, stepping down across the baseline at
        // the midpoint. Min size is the baseline crossing — keep both.
        const float StepU = 0.52f;
        Stroke2(0.10f, 0.26f, StepU, 0.26f);   // upper channel, top edge
        Stroke2(0.10f, 0.50f, StepU, 0.50f);   // upper channel, bottom edge
        Stroke2(0.10f, 0.26f, 0.10f, 0.50f);   // left cap
        Stroke2(StepU, 0.26f, StepU, 0.52f);   // the step down, top edge
        Stroke2(StepU, 0.50f, StepU, 0.76f);   // the step down, bottom edge
        Stroke2(StepU, 0.52f, 0.90f, 0.52f);   // dipped channel, top edge
        Stroke2(StepU, 0.76f, 0.90f, 0.76f);   // dipped channel, bottom edge
        Stroke2(0.90f, 0.52f, 0.90f, 0.76f);   // right cap

        // The debt mark, under the dipped section.
        Stroke2(0.65f, 0.90f, 0.79f, 0.90f);
        Stroke2(0.72f, 0.83f, 0.72f, 0.97f);
        return;
    }

    // Unbuilt or unknown ability: a hollow diamond, which is the set's
    // "something is here" mark. Still a silhouette, never a letter.
    Stroke2(0.50f, 0.14f, 0.86f, 0.50f);
    Stroke2(0.86f, 0.50f, 0.50f, 0.86f);
    Stroke2(0.50f, 0.86f, 0.14f, 0.50f);
    Stroke2(0.14f, 0.50f, 0.50f, 0.14f);
}

// --------------------------------------------------------------------------
// Placeholder icons keep the ability glyph inside neutral tiles. Cooldowns
// recover their icon color clockwise from twelve and show seconds remaining.
// Locked and unaffordable slots remain gray; the latter keeps its resource mark.
// --------------------------------------------------------------------------
void ABreakerPlaytestHUD::DrawAbilitySlot(const ABreakerCharacter* Character, const UBreakerAbilityComponent* Abilities,
    EBreakerAbilitySlot Slot, const FString& KeyHint, float X, float Y, float Size, float MarkSize, const FLinearColor& Accent)
{
    const bool bUltimate = Slot == EBreakerAbilitySlot::Ultimate;
    const bool bGranted = Abilities && Abilities->IsSlotGranted(Slot);
    float Remaining = bGranted ? Abilities->GetCooldownRemaining(Slot) : 0.0f;
    float Duration = bGranted ? Abilities->GetCooldownDuration(Slot) : 0.0f;
    // The preview forces a cooldown on every granted tile so the drain and
    // the rise are photographable.
    if (IsCapturePreview() && bGranted && PreviewCooldownFraction > 0.0f)
    {
        Duration = 1.0f;
        Remaining = PreviewCooldownFraction;
    }
    const bool bOnCooldown = bGranted && Remaining > 0.0f && Duration > UE_SMALL_NUMBER;
    const bool bAffordable = bGranted && Abilities->CanAffordSlot(Slot);
    const bool bReady = bGranted && !bOnCooldown && bAffordable;
    const UBreakerAbilityDefinition* Definition = bGranted ? Abilities->GetDefinitionForSlot(Slot) : nullptr;

    // Neutral tile, distinct placeholder glyph, and clockwise recovery color.
    // Color belongs to the icon face; no status rail or colored outer border.
    const FLinearColor MarkColor = bGranted ? BreakerUI::TextPrimary : BreakerUI::TextDisabled;
    const FLinearColor KeyColor = bGranted ? BreakerUI::TextSecondary : BreakerUI::TextDisabled;
    DrawRect(BreakerUI::BgBase, X, Y, Size, Size);
    DrawBorder(X, Y, Size, Size, BreakerUI::BorderRest, S(BreakerUI::BorderThin));
    const FVector2D IconCenter(X + Size * 0.5f, Y + Size * 0.43f);
    const float IconRadius = Size * 0.30f; // O2 PLACEHOLDER: leaves key and countdown space.
    DrawAbilityRecoveryDisc(IconCenter, IconRadius, 1.0f, BreakerUI::Panel20);
    if (bGranted && (bReady || bOnCooldown))
    {
        const float Recovery = bOnCooldown ? BreakerHUDMath::AbilityRecoveryFraction(Remaining, Duration) : 1.0f;
        DrawAbilityRecoveryDisc(IconCenter, IconRadius, Recovery, BreakerUI::Alpha(Accent, 0.65f));
    }
    // Activation flash: the only feedback that fires for an ability which
    // changes no visible state, so it runs whatever else the tile shows.
    const int32 SlotIndex = static_cast<int32>(Slot);
    if (SlotIndex >= 0 && SlotIndex < AbilitySlotCount)
    {
        const double FlashAge = (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0) - SlotActivationTime[SlotIndex];
        if (FlashAge >= 0.0 && FlashAge < BreakerHUD::AbilityFlashSeconds)
        {
            const float Fade = 1.0f - static_cast<float>(FlashAge) / BreakerHUD::AbilityFlashSeconds;
            DrawRect(BreakerUI::Alpha(BreakerUI::System, 0.6f * Fade), X, Y, Size, Size);
        }
    }

    // Reuse the ability's distinct glyph as the placeholder icon.
    const float IconMarkSize = FMath::Max(MarkSize, Size * 0.36f); // O2 PLACEHOLDER
    DrawAbilityGlyph(Definition, IconCenter.X, IconCenter.Y, IconMarkSize, MarkColor);
    if (bOnCooldown)
    {
        const FString Countdown = BreakerHUDMath::AbilityCooldownText(Remaining);
        const float Pixels = bUltimate ? 16.0f : 14.0f; // O2 PLACEHOLDER
        const FVector2D TextSize = MeasureSpecText(Countdown, Pixels, ESpecFontRole::Mono);
        DrawSpecText(Countdown, X + (Size - TextSize.X) * 0.5f, Y + Size - TextSize.Y - S(3.0f),
            BreakerUI::TextPrimary, Pixels, 1.0f, ESpecFontRole::Mono);
    }

    // Unaffordable: the struck hex, lower-centre on its own opaque chip. No
    // sweep — nothing is filling, so waiting will not fix it.
    if (bGranted && !bAffordable && !bOnCooldown)
    {
        const float HexR = Size * 0.11f;
        const float HexX = X + Size * 0.5f;
        const float HexY = Y + Size * 0.80f;
        DrawRect(BreakerUI::Alpha(BreakerUI::BgVoid, 0.95f),
            HexX - HexR * 1.3f, HexY - HexR * 1.2f, HexR * 2.6f, HexR * 2.4f);
        FVector2D Previous = FVector2D::ZeroVector;
        for (int32 Index = 0; Index <= 6; ++Index)
        {
            const float Angle = UE_PI / 3.0f * Index - UE_HALF_PI;
            const FVector2D Point(HexX + FMath::Cos(Angle) * HexR, HexY + FMath::Sin(Angle) * HexR);
            if (Index > 0) DrawLine(Previous.X, Previous.Y, Point.X, Point.Y, BreakerUI::Harm, S(1.25f));
            Previous = Point;
        }
        DrawLine(HexX - HexR, HexY + HexR * 0.6f, HexX + HexR, HexY - HexR * 0.6f, BreakerUI::Harm, S(1.25f));
    }

    // The key, top-right: 6/6 in on an ability tile, 8/10 on the ultimate.
    const float KeyInsetX = S(bUltimate ? BreakerUI::HudUltimateKeyInsetX : BreakerUI::HudAbilityKeyInset);
    const float KeyInsetY = S(bUltimate ? BreakerUI::HudUltimateKeyInsetY : BreakerUI::HudAbilityKeyInset);
    DrawSpecTextRight(KeyHint, X + Size - KeyInsetX, Y + KeyInsetY, KeyColor, BreakerUI::HudAbilityKeyPixels, 1.0f, ESpecFontRole::Mono);
}
