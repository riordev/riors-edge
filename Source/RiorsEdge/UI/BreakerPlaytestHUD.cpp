#include "UI/BreakerPlaytestHUD.h"

#include "Abilities/BreakerAbilityComponent.h"
#include "Abilities/BreakerAbilityDefinition.h"
#include "Abilities/BreakerAbilityStateComponent.h"
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
#include "UI/BreakerTracerMath.h"
#include "UI/BreakerTracerRenderer.h"
#include "Weapons/BreakerWeaponComponent.h"
#include "Weapons/BreakerWeaponDefinition.h"
#include "Playtest/BreakerPlaytestComponent.h"
#include "Combat/BreakerTargetDummy.h"
#include "Combat/BreakerEnemy.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerStatusComponent.h"
#include "Interaction/BreakerNPC.h"
#include "Interaction/BreakerTravelPoint.h"
#include "Game/BreakerGameInstance.h"
#include "Game/BreakerGameMode.h"
// Quest tracker: definitions and the pure state helpers (read-only — the HUD
// derives, never writes). The journal type itself comes through
// BreakerQuestContent.h's own include.
#include "Save/BreakerQuestContent.h"
#include "EngineUtils.h"
#include "AbilitySystemComponent.h"
#include "Items/BreakerItemTypes.h"
#include "Items/BreakerAffixLibrary.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
// The cooldown wedge and the momentum track's chevron blocks are triangles;
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
// FIELDPLATE combat HUD. Every colour comes from BreakerUI; every geometry
// value below is authored in the spec's 1920x1080 pixels and scaled once by
// S(). See Docs/Design/UI-HUD-Spec.md — the section numbers in the comments
// are that document's.
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
    static const FTracerFlight TracerFlight;

    // 40ms pop + 520ms rise, FIELDPLATE §04.
    static constexpr float DamageNumberLifetime =
        BreakerUI::MotionDamagePop + BreakerUI::MotionDamageRise;

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

    // Enemy bar visibility rules.
    static constexpr float EnemyBarMaxDistance = 5000.0f;
    // Aim cone for "the enemy I am asking about". Presentation, not balance:
    // it decides which enemy gets a verbose label, never anything about damage
    // or aim. Roughly matches the loot focus cone so the two agree about what
    // the player is pointing at.
    static constexpr float EnemyFocusMinimumDot = 0.985f;
    // How long a damage number stays open to absorb further hits on the same
    // target. Long enough to swallow a shotgun's pellets and a burst, short
    // enough that two deliberate shots read as two numbers. Presentation, not
    // balance — it changes nothing about damage, only how it is counted on
    // screen. O2 PLACEHOLDER.
    static constexpr float DamageNumberMergeWindow = 0.18f;

    // --- Damage-number hierarchy timings/magnitudes. All presentation, all
    // tuned by eye from capture screenshots. O2 PLACEHOLDER, every one.
    // DoT ticks die fast (they recur forever; a long tail is spam), kills hold
    // longest (the one number worth reading after the fight moves on).
    static constexpr float DamageDoTLifetime = 0.35f;
    static constexpr float DamageKillLifetime = 0.85f;
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
    static constexpr float DamageKillPopSeconds = 0.09f;

    // Crosshair confirm timings. Sub-150ms on the tick, per the brief.
    static constexpr float HitTickSeconds = 0.12f;        // O2 PLACEHOLDER
    static constexpr float KillConfirmSeconds = 0.40f;    // O2 PLACEHOLDER

    // Low-health screen-edge cue thresholds. Health only — shields regenerate
    // and a full-shield character at low health is still one mistake from
    // dying, which is exactly what the cue is for. O2 PLACEHOLDER.
    static constexpr float LowHealthFraction = 0.35f;
    static constexpr float LowHealthDireFraction = 0.15f;
    // Loud states blink mechanically between the accent and its deep step —
    // FIELDPLATE has no fades, so urgency is a metronome, not a breath.
    static constexpr float LoudBlinkSeconds = 0.5f;       // O2 PLACEHOLDER

    // How long the level-up banner holds. Presentation, not balance.
    static constexpr float LevelUpBannerSeconds = 3.2f;   // O2 PLACEHOLDER
    // Banner leaves like a panel: 120ms linear out, FIELDPLATE §04.
    static constexpr float LevelUpOutSeconds = 0.12f;
    static constexpr float LevelUpRailBlinkSeconds = 0.25f; // O2 PLACEHOLDER
    static constexpr float EnemyBarAlwaysDistance = 1500.0f;
    static constexpr float EnemyBarRecentDamageSeconds = 6.0f;

    // Ability feedback timings. All cosmetic: nothing here gates a rule.
    static constexpr float AbilityFlashSeconds = 0.3f;
    static constexpr float AbilityCalloutSeconds = 1.4f;
    static constexpr int32 AbilityCalloutMaxShows = 3;
    static constexpr float SkimBurstSeconds = 0.25f;
    static constexpr float MarkHeadroomCm = 160.0f;

    // Every ability state window shares this prefix; the HUD shows the whole
    // family rather than a hard-coded list, so a new Swift ability gets its
    // duration bar for free.
    static const TCHAR* WindowPrefix = TEXT("Window.Swift.");
    static const FName OverdriveWindow(TEXT("Window.Swift.Overdrive"));

    // Ultimates carry violet, class abilities carry cyan (icon spec §Colour).
    static FLinearColor WindowColor(const FString& ShortKey)
    {
        return ShortKey.Equals(TEXT("Overdrive"), ESearchCase::IgnoreCase)
            ? BreakerUI::Violet : BreakerUI::Cyan;
    }

    // "Swift.Skim" -> "Window.Swift.Skim". The ability's own window key is
    // built from the leaf of its id, which is the convention every
    // UBreakerGameplayAbility::GetWindowKey already follows.
    static FName WindowKeyFor(const UBreakerAbilityDefinition* Definition)
    {
        if (!Definition) return NAME_None;
        FString Leaf = Definition->AbilityId.ToString();
        int32 Separator = INDEX_NONE;
        if (Leaf.FindLastChar(TEXT('.'), Separator)) Leaf = Leaf.RightChop(Separator + 1);
        return FName(*(FString(WindowPrefix) + Leaf));
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

    // Loot pickup rules.
    static constexpr float PickupChipDistance = 1500.0f;
    static constexpr float PickupPopupDistance = 800.0f;
    // cos(3 degrees): the popup only opens when the player is genuinely
    // looking at the drop, not merely facing its half of the room.
    static constexpr float PickupPopupCosine = 0.99863f;

#if BREAKER_HAS_LOOT_PICKUP
    // Only compiled alongside the pickup drawing that uses them: unreferenced
    // static functions are a warning, and warnings are errors here.
    static FString TierLabel(int32 Tier)
    {
        return Tier < 0 ? TEXT("T-1") : FString::Printf(TEXT("T%d"), Tier);
    }

    // Same formatting rules as SBreakerMenu::DescribeItem, returned per line
    // so the Canvas popup can lay them out itself.
    static TArray<FString> DescribeItemLines(const FBreakerItemInstance& Item)
    {
        const TArray<FBreakerAffixDefinition>& Pool = UBreakerAffixLibrary::GetSliceAffixPool();
        TArray<FString> Lines;
        for (const FBreakerRolledAffix& Affix : Item.Affixes)
        {
            const FBreakerAffixDefinition* Definition = UBreakerAffixLibrary::FindAffix(Pool, Affix.AffixId);
            const FString Name = Definition ? Definition->DisplayName.ToString() : Affix.AffixId.ToString();
            const bool bPercent = Definition && Definition->StatBucket != EBreakerStatBucket::Flat;
            const bool bPercentStyleFlat = Definition &&
                (Definition->StatTarget == EBreakerStatTarget::CriticalChance || Definition->StatTarget == EBreakerStatTarget::CriticalDamage);
            Lines.Add(FString::Printf(TEXT("%s  +%.1f%s  %s"), *Name, Affix.Value,
                bPercent || bPercentStyleFlat ? TEXT("%") : TEXT(""), *TierLabel(Affix.Tier)));
        }
        return Lines;
    }

    static FString RarityLabel(EBreakerItemRarity Rarity)
    {
        switch (Rarity)
        {
            case EBreakerItemRarity::Uncommon:    return TEXT("UNCOMMON");
            case EBreakerItemRarity::Exceptional: return TEXT("EXCEPTIONAL");
            case EBreakerItemRarity::Aberrant:    return TEXT("ABERRANT");
            case EBreakerItemRarity::Anomalous:   return TEXT("ANOMALOUS");
            default:                              return TEXT("STANDARD");
        }
    }
#endif // BREAKER_HAS_LOOT_PICKUP
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
        DrawCrosshair(Center, BreakerUI::TextPrimary, S(8.0f), S(1.5f));
        return;
    }
    EnsureDamageBinding(Character);
    EnsureWeaponBinding(Character);
    EnsureAbilityBinding(Character);
    EnsureProgressionBinding(Character);
    // Everything below, ability callouts included, is suppressed while the
    // pause/inventory menu owns the screen.
    if (Character->IsMenuOpen()) return;

    // --- ANCHOR TRIM (owner ask, tonight) ---------------------------------
    // The Anchor is a social space: weapons are holstered for the pawn's whole
    // life and there is nothing to shoot, so the combat chrome is not merely
    // idle there — it is a lie about what the place is for. Drawn instead:
    // the minimap, the quest tracker beneath it, the XP rail with its level
    // readout (and the level-up banner, which is the same readout's earned
    // moment), the NPC talk prompt (the Anchor's one verb), and the playtest
    // instrumentation with its working F3 diagnostics toggle. Everything else
    // — health/shield, class resource, ammo, crosshair, wave banner, damage
    // numbers, enemy bars, every combat readout — is deliberately absent.
    if (UBreakerGameInstance::IsAnchorMap(this))
    {
        // No enemy pass runs here, so the blip array is cleared by hand: the
        // minimap consumes whatever the last combat frame left in it.
        EnemyBlips.Reset();
        const float TrackerX = Canvas->ClipX - S(BreakerUI::HudSafeMargin) - S(BreakerUI::HudQuestTrackerWidth);
        DrawMinimap(Character,
            Canvas->ClipX - S(BreakerUI::HudSafeMargin) - S(BreakerUI::HudMinimapWidth),
            S(BreakerUI::HudSafeMargin),
            S(BreakerUI::HudMinimapWidth), S(BreakerUI::HudMinimapHeight));
        DrawQuestTracker(Character, TrackerX,
            S(BreakerUI::HudSafeMargin) + S(BreakerUI::HudMinimapHeight) + S(BreakerUI::HudQuestTrackerGap),
            S(BreakerUI::HudQuestTrackerWidth));
        DrawExperienceRail(Character);
        DrawLevelUpBanner(Center);
        // Who can be talked to and where the way out is, readable from
        // anywhere on the plaza — the Anchor's whole verb set, floating over
        // the actors that own it.
        DrawInteractableLabels(Character);
        if (const ABreakerNPC* NearbyNPC = Character->FindNearbyNPC())
        {
            DrawSpecTextCentered(FString::Printf(TEXT("F  TALK — %s"), *NearbyNPC->GetDisplayName().ToString().ToUpper()),
                Center.X, Center.Y + S(90.0f), BreakerUI::Cyan, 14.0f);
        }
        DrawPlaytestInstrumentation(Character, Center);
        return;
    }

    TickCapturePreview(Character);

    const UBreakerWeaponComponent* Weapon = Character->GetWeapon();
    const bool bRecentShot = Weapon && Weapon->GetSecondsSinceLastShot() < 0.14f;
    const FBreakerShotResult* Shot = Weapon ? &Weapon->GetLastShot() : nullptr;

    // World-anchored layers first: they sit over the world but under every
    // screen-anchored plate, so the HUD frame always wins a collision.
    // Rounds in flight are NOT in this list any more — they are world
    // primitives now (ABreakerTracerRenderer) and the renderer draws them,
    // correctly occluded, before the HUD gets the canvas at all.
    DrawEnemyHealthBars(Character);
    DrawMarkedTarget(Character);
    DrawDamageNumbers();
    DrawLootPickups(Character);
    // The gym camp's Kess/Quartermaster and its travel point get the same
    // over-actor labels the Anchor draws; in a wave they sit far outside the
    // arena, so they cost nothing to the combat read.
    DrawInteractableLabels(Character);

    // Under the crosshair and under every plate: the ultimate frame is
    // ambient, never something the eye has to read past.
    DrawUltimateTreatment(Character);

    // --- Crosshair (§Anchors: 80x80 box, hit ticks gold at 45 degrees) ---
    const bool bAiming = Weapon && Weapon->IsAiming();
    const float RestingCrosshairSize = bAiming ? 4.0f : 8.0f;
    DrawCrosshair(Center, BreakerUI::TextPrimary,
        S(bRecentShot ? 12.0f : RestingCrosshairSize), S(bRecentShot ? 2.5f : 1.5f));
    // The preview forces the hit marker on, because the absorbed tick state is
    // the single most important mark in this pass and nothing in a headless run
    // pulls a trigger to produce one.
    const bool bPreviewHit = IsCapturePreview();
    if ((bRecentShot && Shot && Shot->bHit) || bPreviewHit)
    {
        // ABSORBED is the THIRD tick state, and it exists because the owner
        // shot a Warden in its armoured front and read "the game is broken"
        // rather than "wrong angle": a hit registered, the marker fired, and
        // no health moved. It is told by GEOMETRY as well as colour, per
        // FIELDPLATE 01 — the ticks pull outward and gain a bracket — because
        // colour alone at the crosshair is what the weak-point tick already
        // uses and two colour states at one mark do not read in a fight.
        // Full Orange, not OrangeDeep. The deep step is FIELDPLATE's pressed /
        // track-fill value and it was the first thing tried here; looked at, it
        // is too dark to hold at 2px over the world, which is the one place a
        // hit marker has to survive any background. Orange is the weapon/heat
        // family — the same accent BLOCKED already carries on the player's own
        // side of the same event — and the GEOMETRY is what separates it from
        // the gold weak-point tick, which is the FIELDPLATE-correct division of
        // labour anyway.
        const bool bAbsorbed = bPreviewHit || LastShotMitigatedFraction >= BreakerUI::DamageAbsorbedThreshold;
        const FLinearColor TickColor = bAbsorbed ? BreakerUI::Orange
            : (Shot && Shot->bWeakPoint) ? BreakerUI::Gold : BreakerUI::Harm;
        const float Inner = bAbsorbed ? S(13.0f) : S(6.0f);
        const float Outer = bAbsorbed ? S(23.0f) : S(14.0f);
        const float Diagonal = 0.7071f;
        for (int32 Index = 0; Index < 4; ++Index)
        {
            const float DX = (Index & 1) ? 1.0f : -1.0f;
            const float DY = (Index & 2) ? 1.0f : -1.0f;
            DrawLine(Center.X + DX * Inner * Diagonal, Center.Y + DY * Inner * Diagonal,
                     Center.X + DX * Outer * Diagonal, Center.Y + DY * Outer * Diagonal,
                     TickColor, S(2.0f));
        }
        if (bAbsorbed)
        {
            // Four short brackets closing the tick ends into a box: the round
            // stopped at a surface. Deliberately the OPPOSITE motion to the
            // weak-point tick, which opens outward on a clean hit.
            const float Corner = S(9.0f);
            const float Reach = Outer * Diagonal;
            for (int32 Index = 0; Index < 4; ++Index)
            {
                const float DX = (Index & 1) ? 1.0f : -1.0f;
                const float DY = (Index & 2) ? 1.0f : -1.0f;
                DrawLine(Center.X + DX * Reach, Center.Y + DY * Reach,
                         Center.X + DX * (Reach - Corner), Center.Y + DY * Reach, TickColor, S(2.5f));
                DrawLine(Center.X + DX * Reach, Center.Y + DY * Reach,
                         Center.X + DX * Reach, Center.Y + DY * (Reach - Corner), TickColor, S(2.5f));
            }
        }
    }
    else
    {
        // The universal hit tick: any landed damage the player DEALT — an
        // ability's cleave, a detonation — confirms at the crosshair exactly
        // as a bullet does. Same geometry as the shot tick so "I hit" is one
        // mark everywhere; only reached when the shot path above did not
        // already draw it, so a bullet never ticks twice. DoT ticks are
        // excluded at the latch.
        const double HitAge = (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0) - LastHitDealtTime;
        if (HitAge >= 0.0 && HitAge < BreakerHUD::HitTickSeconds)
        {
            const FLinearColor TickColor = bHitDealtWeakPoint ? BreakerUI::Gold : BreakerUI::Harm;
            const float Inner = S(6.0f);
            const float Outer = S(14.0f);
            const float Diagonal = 0.7071f;
            for (int32 Index = 0; Index < 4; ++Index)
            {
                const float DX = (Index & 1) ? 1.0f : -1.0f;
                const float DY = (Index & 2) ? 1.0f : -1.0f;
                DrawLine(Center.X + DX * Inner * Diagonal, Center.Y + DY * Inner * Diagonal,
                         Center.X + DX * Outer * Diagonal, Center.Y + DY * Outer * Diagonal,
                         TickColor, S(2.0f));
            }
        }
    }
    // The kill confirm draws OVER whichever tick fired: a kill is the one
    // crosshair event that outranks everything else at the crosshair.
    DrawKillConfirm(Center);

    // --- Bottom-left vitals (§Anchors) -----------------------------------
    DrawVitalsPlate(Character, S(BreakerUI::HudSafeMargin), Canvas->ClipY - S(BreakerUI::HudSafeMargin));

    // --- Bottom-right combat cluster: one plate, three readouts (§1) -----
    const float ClusterW = S(BreakerUI::HudClusterWidth);
    const float ClusterH = S(BreakerUI::HudClusterHeight);
    const float ClusterX = Canvas->ClipX - S(BreakerUI::HudSafeMargin) - ClusterW;
    const float ClusterY = Canvas->ClipY - S(BreakerUI::HudSafeMargin) - ClusterH;
    DrawCombatCluster(Character, ClusterX, ClusterY, ClusterW, ClusterH);

    // Duration bars stack upward from just above the cluster, so an expiring
    // window never shifts the cluster itself.
    DrawAbilityWindows(Character, ClusterX, ClusterY - S(BreakerUI::Space8), ClusterW);

    // --- Top centre: wave banner ------------------------------------------
    DrawWaveBanner(Center);

    // --- Top right: the field plate ---------------------------------------
    // Must follow DrawEnemyHealthBars, which fills EnemyBlips from the one
    // enemy iteration the HUD makes.
    DrawMinimap(Character,
        Canvas->ClipX - S(BreakerUI::HudSafeMargin) - S(BreakerUI::HudMinimapWidth),
        S(BreakerUI::HudSafeMargin),
        S(BreakerUI::HudMinimapWidth), S(BreakerUI::HudMinimapHeight));
    // The quest tracker rides directly under the minimap on EVERY map, not
    // only the Anchor: a contract accepted in camp is worked in the field,
    // and objectives that vanish the moment the player travels are objectives
    // the player has to memorise.
    DrawQuestTracker(Character,
        Canvas->ClipX - S(BreakerUI::HudSafeMargin) - S(BreakerUI::HudQuestTrackerWidth),
        S(BreakerUI::HudSafeMargin) + S(BreakerUI::HudMinimapHeight) + S(BreakerUI::HudQuestTrackerGap),
        S(BreakerUI::HudQuestTrackerWidth));

    // --- Centre: feedback only, nothing persistent ------------------------
    DrawSkimBurst(Center);
    DrawExperienceRail(Character);
    DrawLevelUpBanner(Center);
    DrawAbilityCallout(Center);
    DrawDefenseFeedback(Center);
    if (const ABreakerNPC* NearbyNPC = Character->FindNearbyNPC())
    {
        DrawSpecTextCentered(FString::Printf(TEXT("F  TALK — %s"), *NearbyNPC->GetDisplayName().ToString().ToUpper()),
            Center.X, Center.Y + S(90.0f), BreakerUI::Cyan, 14.0f);
    }
    if (const UBreakerCombatComponent* PlayerCombat = Character->GetCombat(); PlayerCombat && PlayerCombat->GetSecondsSinceDamage() < 0.28f)
    {
        // Harm is instant: full-bleed edge lines, no inset, no fade in.
        const FLinearColor DamageColor = BreakerUI::Alpha(BreakerUI::Harm, 0.85f);
        DrawSpecTextCentered(TEXT("DAMAGE"), Center.X, Center.Y - S(80.0f), DamageColor, 16.0f);
        const float T = S(4.0f);
        DrawRect(DamageColor, 0.0f, 0.0f, Canvas->ClipX, T);
        DrawRect(DamageColor, 0.0f, Canvas->ClipY - T, Canvas->ClipX, T);
        DrawRect(DamageColor, 0.0f, 0.0f, T, Canvas->ClipY);
        DrawRect(DamageColor, Canvas->ClipX - T, 0.0f, T, Canvas->ClipY);
    }
    // Persistent low-health edge bands, under the transient damage flash's
    // visual language and after it so the flash always reads over the bands.
    DrawLowHealthCue(Character);
    if (Weapon && Weapon->IsReloading())
    {
        DrawSpecTextCentered(TEXT("RELOADING"), Center.X, Center.Y + S(48.0f), BreakerUI::Orange, 14.0f);
    }

    // The old fixed-position damage readout is gone: floating world-space
    // numbers say the same thing at the impact point. Only the weak-point
    // callout survives, because it is a skill confirmation, not a value.
    if (bRecentShot && Shot && Shot->bHit && Shot->bWeakPoint)
    {
        DrawSpecText(TEXT("WEAK POINT"), Center.X + S(24.0f), Center.Y + S(18.0f), BreakerUI::Gold, 11.0f);
    }

    // Latch elite kills: the shot feedback window is far shorter than the
    // time this callout should stay readable.
    if (bRecentShot && Shot && Shot->bHit && Shot->DamageResult.bKilled)
    {
        if (const ABreakerEnemy* KilledEnemy = Cast<ABreakerEnemy>(Shot->HitActor.Get()); KilledEnemy && KilledEnemy->IsElite())
            LastEliteKillTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
    }
    const double EliteKillAge = (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0) - LastEliteKillTime;
    if (EliteKillAge >= 0.0 && EliteKillAge < 1.2f)
    {
        const float Fade = 1.0f - static_cast<float>(EliteKillAge) / 1.2f;
        DrawSpecTextCentered(TEXT("ELITE DOWN"), Center.X, Center.Y - S(118.0f), BreakerUI::Gold, 20.0f, Fade);
    }

    DrawPlaytestInstrumentation(Character, Center);
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
    {
        const FString KeyLegend(TEXT("F1 RESET   F2 REPORT   F3 DIAGNOSTICS   ESC MENU"));
        const FVector2D LegendSize = MeasureSpecText(KeyLegend, 11.0f);
        const float LegendX = S(BreakerUI::HudSafeMargin);
        const float LegendY = S(BreakerUI::HudSafeMargin);
        const float LegendH = LegendSize.Y + S(BreakerUI::Space8);
        DrawPlate(LegendX, LegendY, LegendSize.X + S(BreakerUI::Space24) + S(BreakerUI::RailThickness), LegendH, BreakerUI::TextMuted);
        DrawSpecText(KeyLegend, LegendX + S(BreakerUI::RailThickness) + S(BreakerUI::Space8), LegendY + S(BreakerUI::Space4),
            BreakerUI::TextSecondary, 11.0f);
    }
    if (Playtest && Playtest->AreDiagnosticsVisible())
    {
        const FBreakerPlaytestStats& Stats = Playtest->GetStats();
        const float FPS = GetWorld() && GetWorld()->GetDeltaSeconds() > UE_SMALL_NUMBER ? 1.0f / GetWorld()->GetDeltaSeconds() : 0.0f;
        const float DiagX = S(BreakerUI::HudSafeMargin);
        // Clear of the key legend above it: the two used to overlap.
        const float DiagY = S(BreakerUI::HudSafeMargin + 32.0f);
        const float DiagW = S(300.0f);
        const float DiagH = S(74.0f);
        DrawPlate(DiagX, DiagY, DiagW, DiagH, BreakerUI::TextMuted);
        const float TextX = DiagX + S(BreakerUI::Space16);
        DrawSpecText(FString::Printf(TEXT("FPS %.0f   FOV %.0f   SENS %.1f"), FPS, Character->GetCurrentFOV(), Character->GetLookSensitivity()),
            TextX, DiagY + S(10.0f), BreakerUI::TextSecondary, 11.0f);
        DrawSpecText(FString::Printf(TEXT("SHOTS %d   ACC %.1f%%   WEAK %.1f%%"), Stats.ShotsFired, Stats.Accuracy(), Stats.WeakPointRate()),
            TextX, DiagY + S(30.0f), BreakerUI::TextSecondary, 11.0f);
        DrawSpecText(FString::Printf(TEXT("DMG %.0f   RELOADS %d"), Stats.DamageDealt, Stats.Reloads),
            TextX, DiagY + S(50.0f), BreakerUI::TextSecondary, 11.0f);

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
                    Screen.X, Screen.Y, BreakerUI::TextMuted, 11.0f, 0.8f);
            }
        }
        for (TActorIterator<ABreakerEnemy> It(GetWorld()); It; ++It)
        {
            if (FVector::DistSquared(Character->GetActorLocation(), It->GetActorLocation()) > FMath::Square(2500.0f)) continue;
            FVector2D Screen;
            if (PlayerOwner && PlayerOwner->ProjectWorldLocationToScreen(It->GetActorLocation() + FVector(0.0f, 0.0f, 130.0f), Screen))
            {
                DrawSpecTextCentered(It->GetEnemyStateLabel(), Screen.X, Screen.Y, BreakerUI::Orange, 11.0f, 0.7f);
            }
        }
    }
    if (Playtest && Playtest->GetSecondsSinceReportCopy() < 2.0f)
    {
        DrawSpecTextCentered(TEXT("PLAYTEST REPORT COPIED"), Center.X, Center.Y + S(72.0f), BreakerUI::Cyan, 14.0f);
    }
}

// --------------------------------------------------------------------------
// §Anchors — vitals bottom-left, 420 wide. BottomY is the plate's bottom edge,
// so it grows upward and stays glued to the safe margin at any resolution.
//
// Shield above health: shields are consumed first, so the stack must deplete
// downward on screen (UI-UX-Spec 4.2). Values are right-aligned in a fixed
// 84px column so a four-digit pool never shifts a three-digit one.
// --------------------------------------------------------------------------
void ABreakerPlaytestHUD::DrawVitalsPlate(const ABreakerCharacter* Character, float X, float BottomY)
{
    const float PlateW = S(BreakerUI::HudVitalsWidth);
    const float PlateH = S(100.0f);
    const float Y = BottomY - PlateH;
    const float Pad = S(BreakerUI::HudClusterPad);
    const float ValueColumn = S(BreakerUI::HudValueColumnWidth);
    const float InnerRight = X + PlateW - Pad;
    const float BarX = X + Pad + S(BreakerUI::RailThickness);
    const float BarW = PlateW - Pad * 2.0f - ValueColumn - S(BreakerUI::Space8) - S(BreakerUI::RailThickness);

    DrawPlate(X, Y, PlateW, PlateH, BreakerUI::Cyan);

    // Movement state is a playtest diagnostic, not a headline: caption weight,
    // top of the plate, out of the way of the pools.
    const FString MoveState = Character->IsMantling() ? TEXT("MANTLE")
        : Character->IsWallRiding() ? TEXT("WALL RIDE")
        : Character->IsSliding() ? TEXT("SLIDE")
        : Character->IsSprinting() ? TEXT("SPRINT") : TEXT("MOVE");
    DrawSpecText(MoveState, BarX, Y + Pad, BreakerUI::TextMuted, 11.0f);
    DrawSpecTextRight(FString::Printf(TEXT("%.0f U/S"), Character->GetHorizontalSpeed()),
        InnerRight, Y + Pad, BreakerUI::TextMuted, 11.0f);

    if (const UBreakerAttributeSet* Attributes = Character->GetAttributes())
    {
        // One row = one pool. The bar is vertically centred in its row and the
        // value is centred against the bar in the fixed 84px column, so a
        // four-digit pool never shifts a three-digit one and nothing overlaps.
        auto DrawPoolRow = [this, BarX, BarW, InnerRight](float RowCenterY, float BarHeight, float Value, float Maximum,
            const FLinearColor& Fill, float ValuePixels)
        {
            const bool bHasPool = Maximum > UE_SMALL_NUMBER;
            const float Fraction = bHasPool ? Value / Maximum : 0.0f;
            // An empty pool keeps its geometry but drops to the disabled
            // colour: a stark full-width empty track reads as a broken widget.
            DrawTrack(BarX, RowCenterY - BarHeight * 0.5f, BarW, BarHeight, Fraction,
                bHasPool ? Fill : BreakerUI::Panel20, BreakerUI::Panel10);
            const FString Text = FString::Printf(TEXT("%s/%s"),
                *BreakerUI::FormatTicker(Value), *BreakerUI::FormatTicker(Maximum));
            const FVector2D TextSize = MeasureSpecText(Text, ValuePixels);
            DrawSpecTextRight(Text, InnerRight, RowCenterY - TextSize.Y * 0.5f,
                bHasPool ? Fill : BreakerUI::TextDisabled, ValuePixels);
        };

        const float ShieldRowY = Y + Pad + S(30.0f);
        DrawPoolRow(ShieldRowY, S(BreakerUI::HudShieldBarHeight),
            Attributes->GetShield(), Attributes->GetMaxShield(), BreakerUI::Cyan, 15.0f);

        const float HealthRowY = ShieldRowY + S(22.0f);
        DrawPoolRow(HealthRowY, S(BreakerUI::HudHealthBarHeight),
            Attributes->GetHealth(), Attributes->GetMaxHealth(), BreakerUI::RarityStandard, 13.0f);

        // Armour is a coefficient, not a pool: three chips, never a bar. Each
        // chip is a third of the way to the 80% mitigation ceiling.
        const float ArmorRowY = HealthRowY + S(22.0f);
        const float Armor = Attributes->GetArmor();
        const float Mitigation = Armor > 0.0f ? FMath::Min(Armor / (Armor + 100.0f), 0.8f) : 0.0f;
        const float ChipH = S(BreakerUI::HudArmorChipHeight);
        const FVector2D ArmorLabelSize = MeasureSpecText(TEXT("ARMOR"), 11.0f);
        DrawSpecText(TEXT("ARMOR"), BarX, ArmorRowY - ArmorLabelSize.Y * 0.5f, BreakerUI::TextMuted, 11.0f);
        const float ChipW = S(BreakerUI::HudArmorChipWidth);
        const float ChipsX = BarX + ArmorLabelSize.X + S(BreakerUI::Space8);
        for (int32 Index = 0; Index < 3; ++Index)
        {
            const float Filled = FMath::Clamp((Mitigation / 0.8f) * 3.0f - static_cast<float>(Index), 0.0f, 1.0f);
            const float ChipX = ChipsX + Index * (ChipW + S(BreakerUI::Space4));
            DrawRect(BreakerUI::Panel10, ChipX, ArmorRowY - ChipH * 0.5f, ChipW, ChipH);
            if (Filled > 0.0f) DrawRect(BreakerUI::Gold, ChipX, ArmorRowY - ChipH * 0.5f, ChipW * Filled, ChipH);
        }
        const FVector2D ArmorValueSize = MeasureSpecText(TEXT("000  00%"), 11.0f);
        DrawSpecTextRight(FString::Printf(TEXT("%.0f  %.0f%%"), Armor, Mitigation * 100.0f),
            InnerRight, ArmorRowY - ArmorValueSize.Y * 0.5f,
            Armor > 0.0f ? BreakerUI::TextMuted : BreakerUI::TextDisabled, 11.0f);
    }

    // Status chips run above the plate so an expiring DoT never resizes it.
    DrawStatusReadout(Character, X, Y - S(BreakerUI::Space8));
}

// --------------------------------------------------------------------------
// §1 — one cluster, one plate. 440x184, 3px orange rail on the left edge,
// 12px interior padding, 10px between rows. Momentum on top, weapon name and
// magazine as one baseline-aligned row, three ability squares as the base.
// --------------------------------------------------------------------------
void ABreakerPlaytestHUD::DrawCombatCluster(const ABreakerCharacter* Character, float X, float Y, float Width, float Height)
{
    DrawPlate(X, Y, Width, Height, BreakerUI::Orange);

    const float Pad = S(BreakerUI::HudClusterPad);
    const float RowGap = S(BreakerUI::HudClusterRowGap);
    const float InnerX = X + Pad + S(BreakerUI::RailThickness);
    const float InnerRight = X + Width - Pad;
    const float InnerW = InnerRight - InnerX;

    // --- Row 1: class resource (§2) ---------------------------------------
    // Generic by construction: the row asks for a resolved description and
    // paints that. It does not know which classes exist.
    {
        const BreakerHUD::FResourceRow Row = ResolveResourceRow(Character);

        DrawSpecText(Row.Label, InnerX, Y + Pad, Row.StateColor, 11.0f);
        // The state word is a confirmation, never the carrier: colour, fill
        // height and block texture do the work below.
        //
        // Offset MEASURED from the label rather than a fixed gutter. The fixed
        // 84px this used to be was narrower than "MOMENTUM" renders at 11px, so
        // Swift's HUD read "MOMENTUMSETTLED" with the two words touching --
        // found by looking at a screenshot, because no test can see a
        // collision. Measuring also keeps Caster's shorter "MANA" from leaving
        // a hole, which a wider fixed gutter would have.
        //
        // 13px, down from the spec's 17. Owner, looking at it on a screen:
        // the state word "seems a little too big and disjointed". It is a
        // CONFIRMATION of what the track already says in colour, fill height
        // and block texture -- at 17 it was competing with the track for the
        // row instead of annotating it.
        //
        // Both strings now share a BASELINE derived from their measured
        // heights rather than from a hand-tuned vertical nudge. The old
        // -3px offset was eyeballed against 17px text and would have gone
        // wrong the moment either size moved, which is exactly what happened.
        const FVector2D ResourceLabelSize = MeasureSpecText(Row.Label, 11.0f);
        const FVector2D StateWordSize = MeasureSpecText(Row.StateWord, BreakerUI::HudResourceStatePixels);
        const float ResourceBaseline = Y + Pad + ResourceLabelSize.Y;
        const float StateWordRight = InnerX + ResourceLabelSize.X + S(BreakerUI::Space8) + StateWordSize.X;
        DrawSpecText(Row.StateWord, InnerX + ResourceLabelSize.X + S(BreakerUI::Space8),
            ResourceBaseline - StateWordSize.Y, Row.StateColor, BreakerUI::HudResourceStatePixels);

        // AUDIT (2026-08-14): the third thing on this row is right-aligned to
        // the plate edge and knew nothing about the two left-aligned strings
        // beside it — the same shape as the wave banner's collision, one row
        // down. It has slack today with MOMENTUM/SETTLED, and none of that
        // slack is guaranteed: a longer class label or state word closes it
        // silently. The speed readout is the least important of the three and
        // is playtest chrome, so it YIELDS when it does not fit rather than
        // printing through the state word.
        const FString SpeedText = FString::Printf(TEXT("%.0f M/S"), Character->GetHorizontalSpeed() / 100.0f);
        const FVector2D SpeedSize = MeasureSpecText(SpeedText, 11.0f);
        if (InnerRight - SpeedSize.X >= StateWordRight + S(BreakerUI::Space8))
        {
            DrawSpecTextRight(SpeedText, InnerRight, Y + Pad, BreakerUI::TextMuted, 11.0f);
        }

        const float TrackY = Y + Pad + S(20.0f);
        const float TrackH = S(BreakerUI::HudMomentumTrackHeight);
        DrawResourceTrack(Row, InnerX, TrackY, InnerW, TrackH);
    }

    // --- Row 2: weapon name and magazine on one baseline ------------------
    const float WeaponRowY = Y + Pad + S(20.0f) + S(BreakerUI::HudMomentumTrackHeight) + RowGap;
    if (const UBreakerWeaponComponent* Weapon = Character->GetWeapon())
    {
        DrawRect(BreakerUI::BorderEmphasis, InnerX, WeaponRowY, InnerW, S(1.0f));

        const int32 Slot = Weapon->GetCurrentSlot();

        const FString StateText = Weapon->IsReloading() ? TEXT("RELOADING")
            : Weapon->IsSwapping() ? TEXT("SWAPPING")
            : Weapon->IsAiming() ? TEXT("ADS") : FString::Printf(TEXT("SLOT %d"), Slot);
        DrawSpecText(StateText, InnerX, WeaponRowY + S(28.0f),
            Weapon->IsReloading() ? BreakerUI::Orange : BreakerUI::TextMuted, 11.0f);

        // Magazine dominates; reserve is deliberately subordinate. 32/15, down
        // from the spec's 44/18 -- owner, looking at it: "the gun ammo size
        // seems a little too big and disjointed on both ends". At 44 the
        // magazine was taller than the weapon name and the state line stacked
        // together and pulled the eye to the corner of the screen, which is
        // the opposite of what a subordinate readout should do.
        //
        // "Disjointed" was a real geometry bug, not only a size complaint: the
        // two numbers were positioned by two hand-tuned vertical offsets (+2
        // and +22) that only coincidentally lined up at 44/18 and would drift
        // apart at any other pair. They now share one BASELINE computed from
        // the measured glyph heights, so the reserve sits ON the magazine's
        // bottom edge at every size and at every UI scale.
        const FString MagazineText = FString::Printf(TEXT("%d"), Weapon->GetMagazineAmmo());
        const FString Reserve = FString::Printf(TEXT("/%s"), *BreakerUI::FormatTicker(Weapon->GetReserveAmmo()));
        const FVector2D MagazineSize = MeasureSpecText(MagazineText, BreakerUI::HudMagazinePixels);
        const FVector2D ReserveSize = MeasureSpecText(Reserve, BreakerUI::HudReservePixels);
        const float AmmoTop = WeaponRowY + S(6.0f);
        const float AmmoBaseline = AmmoTop + MagazineSize.Y;
        DrawSpecTextRight(Reserve, InnerRight, AmmoBaseline - ReserveSize.Y,
            BreakerUI::TextMuted, BreakerUI::HudReservePixels);
        DrawSpecTextRight(MagazineText, InnerRight - ReserveSize.X - S(BreakerUI::Space4), AmmoTop,
            Weapon->GetMagazineAmmo() > 0 ? BreakerUI::TextPrimary : BreakerUI::Harm,
            BreakerUI::HudMagazinePixels);

        // AUDIT (2026-08-14): the weapon name is left-aligned and the ammo pair
        // is right-aligned on the SAME row, and until now neither knew about
        // the other — a third instance of the wave banner's shape. The ammo
        // block is the one that must never move (it is a fixed-column readout
        // by design), so it is measured FIRST and the name is fitted into what
        // is left. Stepping the name's size down is measured too, so nothing
        // here depends on a layout pass.
        const float NameLimit = (InnerRight - ReserveSize.X - S(BreakerUI::Space4) - MagazineSize.X)
            - S(BreakerUI::Space16) - InnerX;
        const FString WeaponName = Weapon->GetArchetypeName().ToUpper();
        DrawSpecText(WeaponName, InnerX, WeaponRowY + S(6.0f), BreakerUI::TextPrimary,
            FitSpecPixels(WeaponName, 18.0f, NameLimit, 11.0f));
    }

    // --- Row 3: three ability squares, anchored to the bottom pad (§3) ----
    const UBreakerAbilityComponent* Abilities = Character->GetAbilities();
    const float SlotSize = S(BreakerUI::HudAbilitySquare);
    const float SlotGap = S(BreakerUI::HudAbilityGap);
    const float SlotY = Y + Height - Pad - SlotSize;
    const float SlotsX = InnerRight - (SlotSize * 3.0f + SlotGap * 2.0f);
    DrawAbilitySlot(Character, Abilities, EBreakerAbilitySlot::ClassAbilityOne, TEXT("E"), SlotsX, SlotY, SlotSize, BreakerUI::Cyan);
    DrawAbilitySlot(Character, Abilities, EBreakerAbilitySlot::ClassAbilityTwo, TEXT("T"), SlotsX + SlotSize + SlotGap, SlotY, SlotSize, BreakerUI::Cyan);
    DrawAbilitySlot(Character, Abilities, EBreakerAbilitySlot::Ultimate, TEXT("G"), SlotsX + (SlotSize + SlotGap) * 2.0f, SlotY, SlotSize, BreakerUI::Violet);

    // Silent-dead-keys guard: not every class has an implemented kit yet
    // (Swift and Caster do). If nothing is granted, say so instead of letting
    // E/T/G feel broken.
    if (Abilities && Abilities->GetGrantedCount() == 0)
    {
        DrawSpecTextRight(TEXT("NO ABILITY KIT FOR THIS CLASS YET"),
            InnerRight, SlotY - S(14.0f), BreakerUI::Orange, 11.0f);
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

    if (const UBreakerMomentumComponent* Momentum = Character->GetMomentum(); Momentum && Momentum->IsActiveForOwner())
    {
        return BreakerHUD::ResolveMomentumRow(Momentum->GetMomentumFraction(), Momentum->GetMomentumState());
    }
    if (const UBreakerManaComponent* Mana = Character->GetMana(); Mana && Mana->IsActiveForOwner())
    {
        // GetManaFraction() clamps to [0,1] and so cannot express the debt;
        // the raw bank and the floor can, and both are already public.
        const UBreakerAttributeSet* Attributes = Character->GetAttributes();
        const float MaxMana = Attributes ? Attributes->GetMaxClassResource() : 0.0f;
        return BreakerHUD::ResolveManaRow(Mana->GetMana(), MaxMana, Mana->GetOvercastFloor());
    }
    if (const UBreakerScrapComponent* Scrap = Character->GetScrap(); Scrap && Scrap->IsActiveForOwner())
    {
        return BreakerHUD::ResolveScrapRow(Scrap->GetScrapFraction(), Scrap->GetScrapState());
    }
    if (const UBreakerGritComponent* Grit = Character->GetGrit(); Grit && Grit->IsActiveForOwner())
    {
        return BreakerHUD::ResolveGritRow(Grit->GetGritFraction(), Grit->GetGritBand());
    }
    if (const UBreakerChargeComponent* Charge = Character->GetCharge(); Charge && Charge->IsActiveForOwner())
    {
        return BreakerHUD::ResolveChargeRow(Charge->GetChargeFraction(), Charge->GetChargeBand());
    }
    return BreakerHUD::ResolveEmptyResourceRow();
}

// --------------------------------------------------------------------------
// §2 — the track itself. 12px tall with two 2px notches at 33% and 66%, fixed
// by the spec and identical for every class; only the fill's texture changes.
//
// The Signed treatment is the one that needed designing. Overcast is a
// NEGATIVE bank, and a left-anchored horizontal fill has no room to the left
// of zero — stealing track width for a debt zone would move the two notches,
// which the spec fixes. So the axis that inverts is the vertical one: a zero
// baseline across the middle of the track, credit growing rightward in the
// upper half, debt growing rightward in the LOWER half in the harm colour.
// Length still reads magnitude, direction now reads sign, and it is exactly
// what the Overcast icon does ("the same channel, half above zero and half
// beneath"). The baseline is drawn last and always, credit or debt: without
// it the bar is a short fill, not a deficit.
// --------------------------------------------------------------------------
void ABreakerPlaytestHUD::DrawResourceTrack(const BreakerHUD::FResourceRow& Row, float X, float Y, float Width, float Height)
{
    DrawRect(BreakerUI::Panel10, X, Y, Width, Height);

    const float Magnitude = FMath::Clamp(FMath::Abs(Row.Fraction), 0.0f, 1.0f);
    const float FillW = Width * Magnitude;

    switch (Row.Track)
    {
    case BreakerHUD::EResourceTrack::Continuous:
        if (FillW > 0.0f) DrawRect(Row.StateColor, X, Y + Height * 0.25f, FillW, Height * 0.5f);
        break;

    case BreakerHUD::EResourceTrack::Blocks:
    case BreakerHUD::EResourceTrack::WideBlocks:
        if (FillW > 0.0f)
        {
            // Chevron-cut blocks: the texture itself changes with state, so
            // peripheral vision reads the tier without the word.
            const bool bWide = Row.Track == BreakerHUD::EResourceTrack::WideBlocks;
            const float BlockW = S(bWide ? 14.0f : 8.0f);
            const float BlockGap = S(3.0f);
            const float Shear = S(3.0f);
            const float BlockH = bWide ? Height : Height * 0.75f;
            const float BlockY = Y + (Height - BlockH);
            for (float BX = X; BX < X + FillW - BlockW * 0.5f; BX += BlockW + BlockGap)
            {
                DrawShearedBlock(BX, BlockY, FMath::Min(BlockW, X + FillW - BX), BlockH, Shear, Row.StateColor);
            }
        }
        break;

    case BreakerHUD::EResourceTrack::Signed:
    {
        const float LineT = FMath::Max(S(1.0f), 1.0f);
        const float BaseY = Y + Height * 0.5f;
        const float HalfFill = Height * 0.5f - LineT;
        if (FillW > 0.0f && HalfFill > 0.0f)
        {
            if (Row.Fraction < 0.0f) DrawRect(Row.StateColor, X, BaseY + LineT, FillW, HalfFill);
            else                     DrawRect(Row.StateColor, X, BaseY - LineT - HalfFill, FillW, HalfFill);
        }
        // Zero, stated: the reference line survives at 1px whatever the fill
        // is doing, and is never coloured by the state.
        DrawRect(BreakerUI::TextMuted, X, BaseY - LineT * 0.5f, Width, LineT);
        break;
    }

    case BreakerHUD::EResourceTrack::Empty:
    default:
        break;
    }

    // Two 2px notches at 33% and 66%: the state thresholds, cut into the track
    // rather than painted on it. Fixed by the spec for every class.
    const float NotchW = S(2.0f);
    DrawRect(BreakerUI::BgVoid, X + Width * 0.33f, Y, NotchW, Height);
    DrawRect(BreakerUI::BgVoid, X + Width * 0.66f, Y, NotchW, Height);

    // A LOUD state (Redline, Surplus, Ironclad, Resonant, Overcast — anything
    // that widened its own border to 2px) pulses that border between the
    // accent and its deep step on a metronome. A blink, never a fade: the
    // deep colours are FIELDPLATE's own pressed/track-fill steps, so both
    // phases are palette values and the plate never lowers opacity. The
    // resting 1px border never pulses — quiet states stay quiet.
    FLinearColor BorderColor = Row.BorderColor;
    if (Row.BorderPixels >= 2.0f && GetWorld())
    {
        const bool bBlinkOn = FMath::Fmod(static_cast<float>(GetWorld()->GetTimeSeconds()),
            BreakerHUD::LoudBlinkSeconds) < BreakerHUD::LoudBlinkSeconds * 0.5f;
        if (!bBlinkOn)
        {
            if (BorderColor == BreakerUI::Orange) BorderColor = BreakerUI::OrangeDeep;
            else if (BorderColor == BreakerUI::Harm) BorderColor = BreakerUI::HarmDeep;
            else if (BorderColor == BreakerUI::Gold) BorderColor = BreakerUI::GoldDeep;
        }
    }
    DrawBorder(X, Y, Width, Height, BorderColor, S(Row.BorderPixels));
}

// --------------------------------------------------------------------------
// Wave banner, centred at 48px from the top. Top rail, because a wave is a
// transient status and not a system that owns the plate.
//
// THE COLLISION, and why it is fixed by measuring. The owner's screenshot shows
// "WAVE 01" and "4 HOSTILE" printed on top of one another. The cause is exactly
// the MOMENTUMSETTLED defect: a plate at a fixed 260px with its divider at a
// fixed 55% of that, and a 28px title that renders wider than the 143px gutter
// left of the divider. The title starts at a fixed inset and runs as far as it
// runs; the count is right-aligned and runs backwards as far as IT runs; the
// two meet in the middle with nothing in the code that knows they are on the
// same row. A two-digit hostile count widens the right half and closes what
// little slack was left.
//
// Nudging the divider or widening the plate would only move the magnitude of
// the same bug — the next long string re-opens it. So both strings are
// MEASURED, the content is laid out left-to-right from those measurements, and
// the PLATE is sized from the content rather than the content being trusted to
// fit the plate. 260px survives as a MINIMUM so the banner does not visibly
// breathe between "9 HOSTILE" and "12 HOSTILE"; past that it grows.
//
// Both strings also share ONE BASELINE derived from their measured glyph
// heights, the same fix the ammo pair and the resource row already carry, so a
// change to either size cannot drift them apart again.
// --------------------------------------------------------------------------
void ABreakerPlaytestHUD::DrawWaveBanner(const FVector2D& Center)
{
    const ABreakerGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ABreakerGameMode>() : nullptr;
    const bool bPreview = IsCapturePreview() && (!GameMode || GameMode->GetCurrentWave() <= 0);
    if (!bPreview && (!GameMode || GameMode->GetCurrentWave() <= 0)) return;

    // The preview CYCLES the three shapes this row can take — the owner's own
    // screenshot, a two-digit count (the widest active case), and the cleared
    // state with its em dash — so a four-shot capture run photographs all of
    // them instead of proving one string fits.
    const int32 PreviewCase = bPreview
        ? FMath::Abs(FMath::FloorToInt(static_cast<float>(GetWorld()->GetTimeSeconds()) / 2.0f)) % 3 : 0;
    const bool bActive = bPreview ? PreviewCase != 2 : GameMode->IsWaveActive();
    const FString Title = !bPreview ? FString::Printf(TEXT("WAVE %02d"), GameMode->GetCurrentWave())
        : PreviewCase == 0 ? FString(TEXT("WAVE 01"))
        : PreviewCase == 1 ? FString(TEXT("WAVE 12")) : FString(TEXT("WAVE 07"));
    const FString Status = !bPreview
        ? (bActive ? FString::Printf(TEXT("%d HOSTILE"), GameMode->GetWaveEnemiesAlive())
                   : FString(TEXT("CLEAR — F4")))
        : PreviewCase == 0 ? FString(TEXT("4 HOSTILE"))
        : PreviewCase == 1 ? FString(TEXT("24 HOSTILE")) : FString(TEXT("CLEAR — F4"));

    constexpr float TitlePixels = 28.0f;
    constexpr float StatusPixels = 16.0f;
    const FVector2D TitleSize = MeasureSpecText(Title, TitlePixels);
    const FVector2D StatusSize = MeasureSpecText(Status, StatusPixels);

    const float Pad = S(BreakerUI::Space16);
    const float Gap = S(BreakerUI::Space16);
    const float DividerW = S(BreakerUI::BorderThin);
    const float ContentW = TitleSize.X + Gap + DividerW + Gap + StatusSize.X;

    const float PlateW = FMath::Max(S(260.0f), ContentW + Pad * 2.0f);
    const float PlateH = S(56.0f);
    const float PlateX = Center.X - PlateW * 0.5f;
    const float PlateY = S(BreakerUI::HudSafeMargin);
    DrawPlate(PlateX, PlateY, PlateW, PlateH, bActive ? BreakerUI::Orange : BreakerUI::Cyan, EBreakerRail::Top);

    // Centred as a block, so at the minimum width the two halves sit either
    // side of the plate's centre rather than pinned to its edges.
    const float ContentX = PlateX + (PlateW - ContentW) * 0.5f;
    const float Baseline = PlateY + (PlateH + TitleSize.Y) * 0.5f;

    DrawSpecText(Title, ContentX, Baseline - TitleSize.Y, BreakerUI::TextPrimary, TitlePixels);
    // Vertical divider, then the count: two facts, one plate. Its X now comes
    // out of the title's measured width instead of a percentage of the plate.
    const float DividerX = ContentX + TitleSize.X + Gap;
    DrawRect(BreakerUI::BorderEmphasis, DividerX, PlateY + Pad, DividerW, PlateH - Pad * 2.0f);
    DrawSpecText(Status, DividerX + DividerW + Gap, Baseline - StatusSize.Y,
        bActive ? BreakerUI::Orange : BreakerUI::Cyan, StatusPixels);
}

// --------------------------------------------------------------------------
// UI-HUD-Spec section 6 — the minimap.
//
// LANDSCAPE AND FIELD-ALIGNED. Level-Design section 5 strings every station
// along one forward axis across a 25000 cm long axis; the occupied width is
// roughly a third of that. A square window over that field spends most of its
// area on empty flank, so the plate is 320x176 with the field's forward axis
// running along its LONG side, and the map does not rotate with the player.
// A rotating map would throw the alignment away on every turn, and the one
// thing a player needs from this field is "how far along am I".
//
// COST. It iterates nothing. DrawEnemyHealthBars already walks every enemy
// once per frame and now fills EnemyBlips as it goes; this reads that array.
// The array is a member, so after the first few frames it never allocates.
// --------------------------------------------------------------------------
void ABreakerPlaytestHUD::DrawMinimap(const ABreakerCharacter* Character, float X, float Y, float Width, float Height)
{
    if (!Character) return;

    // Cyan rail: FIELDPLATE 01 names the player/system accent as "the only
    // accent allowed on chrome", and a minimap is a player-system readout.
    // TEAL IS FORBIDDEN HERE by the same section and by O19 — teal is a noun
    // for rift geometry and suppression hardware, so the day a rift lands on
    // this map it can be teal precisely because nothing else on the plate is.
    DrawPlate(X, Y, Width, Height, BreakerUI::Cyan);

    const float Rail = S(BreakerUI::RailThickness);
    const float InnerX = X + Rail;
    const float InnerY = Y + S(BreakerUI::BorderThin);
    const float InnerW = Width - Rail - S(BreakerUI::BorderThin);
    const float InnerH = Height - S(BreakerUI::BorderThin) * 2.0f;
    const float CenterX = InnerX + InnerW * 0.5f;
    const float CenterY = InnerY + InnerH * 0.5f;

    // World cm -> plate pixels. Scaled by S() like every other geometry value,
    // so the map covers the same amount of WORLD at every resolution instead
    // of showing more field on a bigger monitor.
    const float PixelsPerCm = S(1.0f) / BreakerUI::HudMinimapCmPerPixel;
    const FVector Origin = Character->GetActorLocation();

    // World +X is the field's forward axis and maps to plate +X (right); world
    // +Y maps to plate +Y. Both are simple scales because the map does not
    // rotate — that is the whole point of aligning it to the field.
    auto ToPlate = [Origin, PixelsPerCm, CenterX, CenterY](const FVector& World)
    {
        return FVector2D(CenterX + static_cast<float>(World.X - Origin.X) * PixelsPerCm,
                         CenterY + static_cast<float>(World.Y - Origin.Y) * PixelsPerCm);
    };
    auto Inside = [InnerX, InnerY, InnerW, InnerH](const FVector2D& P)
    {
        return P.X >= InnerX && P.X <= InnerX + InnerW && P.Y >= InnerY && P.Y <= InnerY + InnerH;
    };

    // --- Graticule: one line per combat-pocket radius of world ------------
    // Anchored to WORLD coordinates, not to the plate, so the grid slides past
    // as the player moves. A grid pinned to the plate would be decoration; a
    // grid pinned to the world is the thing that says you are travelling.
    {
        const float GridPx = BreakerUI::HudMinimapGridCm * PixelsPerCm;
        if (GridPx >= S(8.0f))
        {
            const float FirstX = CenterX - FMath::Fmod(static_cast<float>(Origin.X), BreakerUI::HudMinimapGridCm) * PixelsPerCm;
            for (float GX = FirstX - FMath::CeilToFloat(InnerW * 0.5f / GridPx) * GridPx; GX <= InnerX + InnerW; GX += GridPx)
            {
                if (GX < InnerX) continue;
                DrawRect(BreakerUI::Panel20, GX, InnerY, FMath::Max(S(1.0f), 1.0f), InnerH);
            }
            const float FirstY = CenterY - FMath::Fmod(static_cast<float>(Origin.Y), BreakerUI::HudMinimapGridCm) * PixelsPerCm;
            for (float GY = FirstY - FMath::CeilToFloat(InnerH * 0.5f / GridPx) * GridPx; GY <= InnerY + InnerH; GY += GridPx)
            {
                if (GY < InnerY) continue;
                DrawRect(BreakerUI::Panel20, InnerX, GY, InnerW, FMath::Max(S(1.0f), 1.0f));
            }
        }
    }

    // --- The safe ring ----------------------------------------------------
    // Drawn as a segmented outline rather than a fill: the ring is a boundary,
    // and a filled disc would compete with the blips sitting on top of it.
    // Segments outside the plate are simply not drawn, which is the cheapest
    // correct clip Canvas offers for a shape it has no primitive for.
    if (const ABreakerGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ABreakerGameMode>() : nullptr)
    {
        const FVector RingCenter = GameMode->GetSafeZoneCenter();
        const float RingRadius = GameMode->GetSafeZoneRadius();
        if (RingRadius > 0.0f)
        {
            constexpr int32 Segments = 40;
            const FLinearColor RingColor = BreakerUI::Alpha(BreakerUI::Cyan, 0.65f);
            FVector2D Previous = FVector2D::ZeroVector;
            for (int32 Index = 0; Index <= Segments; ++Index)
            {
                const float Angle = 2.0f * UE_PI * static_cast<float>(Index) / Segments;
                const FVector2D Point = ToPlate(RingCenter +
                    FVector(FMath::Cos(Angle) * RingRadius, FMath::Sin(Angle) * RingRadius, 0.0f));
                if (Index > 0 && Inside(Previous) && Inside(Point))
                {
                    DrawLine(Previous.X, Previous.Y, Point.X, Point.Y, RingColor, FMath::Max(S(1.5f), 1.0f));
                }
                Previous = Point;
            }
        }
    }

    // --- Hostiles ---------------------------------------------------------
    // Harm red, square, and never a dot: a square survives at 5px where a
    // circle turns into a smudge, and the system has no radius above 4px
    // anyway. Off-map hostiles are CLAMPED to the rim at half size rather than
    // dropped — direction of threat is the single most useful thing a minimap
    // reports, and a field this long puts most of a wave off the window.
    const float Blip = S(BreakerUI::HudMinimapBlipSize);
    for (const FBreakerHUDMapBlip& Enemy : EnemyBlips)
    {
        const FVector2D Point = ToPlate(Enemy.World);
        const bool bOnMap = Inside(Point);
        const float Size = bOnMap ? (Enemy.bBoss ? Blip * 1.8f : Blip) : Blip * 0.6f;
        const FVector2D Drawn(
            FMath::Clamp(Point.X, InnerX + Size * 0.5f, InnerX + InnerW - Size * 0.5f),
            FMath::Clamp(Point.Y, InnerY + Size * 0.5f, InnerY + InnerH - Size * 0.5f));

        DrawRect(bOnMap ? BreakerUI::Harm : BreakerUI::Alpha(BreakerUI::Harm, 0.55f),
            Drawn.X - Size * 0.5f, Drawn.Y - Size * 0.5f, Size, Size);
        // Rank is an EDGE, never a fill: the harm colour has to keep meaning
        // "hostile" whatever the rank does, exactly as the enemy bars do it.
        if (bOnMap && (Enemy.bElite || Enemy.bBoss))
        {
            DrawBorder(Drawn.X - Size * 0.5f - S(2.0f), Drawn.Y - Size * 0.5f - S(2.0f),
                Size + S(4.0f), Size + S(4.0f), BreakerUI::Gold, FMath::Max(S(1.0f), 1.0f));
        }
    }

    // --- The player -------------------------------------------------------
    // A triangle, because it is the only mark here that has to report a
    // DIRECTION as well as a position, and the map does not rotate so the
    // triangle is the entire facing readout.
    {
        const FVector Forward = Character->GetActorForwardVector().GetSafeNormal2D();
        const FVector2D Facing(Forward.X, Forward.Y);
        const FVector2D Side(-Facing.Y, Facing.X);
        const float R = S(BreakerUI::HudMinimapPlayerSize);
        const FVector2D Nose(CenterX + Facing.X * R, CenterY + Facing.Y * R);
        const FVector2D Left(CenterX - Facing.X * R * 0.55f + Side.X * R * 0.62f,
                             CenterY - Facing.Y * R * 0.55f + Side.Y * R * 0.62f);
        const FVector2D Right(CenterX - Facing.X * R * 0.55f - Side.X * R * 0.62f,
                              CenterY - Facing.Y * R * 0.55f - Side.Y * R * 0.62f);
        DrawTriangle(Nose, Left, Right, BreakerUI::Cyan);
    }

    // Scale statement. A map with no unit is a picture, and the whole reason
    // the grid pitch is the combat pocket radius is so this line can say what
    // one cell buys.
    DrawSpecText(FString::Printf(TEXT("GRID %.0fM"), BreakerUI::HudMinimapGridCm / 100.0f),
        InnerX + S(BreakerUI::Space8), InnerY + InnerH - S(16.0f), BreakerUI::TextMuted, 11.0f);
}

// --------------------------------------------------------------------------
// Quest tracker — a compact panel directly below the minimap, on all maps.
//
// DERIVED, never stored: quest state is a pure function of the journal's flag
// set (Save/BreakerQuestContent.h), so this panel asks ComputeQuestState and
// the counters and can never disagree with the dialogue system about where a
// quest stands. Gold rail: a contract is the reward family's system, the same
// accent its payout already carries.
//
// State-aware by design:
//   Offered       -> "SPEAK TO THE <GIVER>"  (the player has not accepted yet)
//   Active        -> objectives with live counters ("Thin the spill... 4/5")
//   ReadyToTurnIn -> "RETURN TO THE <GIVER>"
// NotOffered and Complete draw nothing — an empty tracker is the truthful
// state, not a placeholder's.
// --------------------------------------------------------------------------
void ABreakerPlaytestHUD::DrawQuestTracker(const ABreakerCharacter* Character, float X, float Y, float Width)
{
    const UBreakerQuestJournal* Journal = Character ? Character->GetQuestJournal() : nullptr;
    if (!Journal) return;

    // The first quest that is live in any form is the tracked one. The slice
    // ships one quest; when the campaign ships more, "first live" is still the
    // right minimal policy for a panel this size, and a picker can replace it.
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

    // Content rows resolved BEFORE the plate, so the plate is sized from its
    // content rather than the content being trusted to fit the plate — the
    // wave banner's lesson, applied from the start instead of after an audit.
    struct FTrackerRow
    {
        FString Text;
        FString Counter;    // right-aligned, empty for directive rows
        FLinearColor Color = BreakerUI::TextSecondary;
    };
    TArray<FTrackerRow> Rows;
    if (TrackedState == EBreakerQuestState::Offered)
    {
        Rows.Add({ FString::Printf(TEXT("SPEAK TO THE %s"), *Tracked->Giver.ToUpper()), FString(), BreakerUI::Cyan });
    }
    else if (TrackedState == EBreakerQuestState::ReadyToTurnIn)
    {
        Rows.Add({ FString::Printf(TEXT("RETURN TO THE %s"), *Tracked->Giver.ToUpper()), FString(), BreakerUI::Gold });
    }
    else
    {
        for (const FBreakerQuestObjective& Objective : Tracked->Objectives)
        {
            FTrackerRow& Row = Rows.AddDefaulted_GetRef();
            Row.Text = Objective.Text.ToUpper();
            const bool bComplete = Journal->HasFlag(Objective.CompletionFlag);
            if (Objective.RequiredCount > 0)
            {
                // A completed counted objective reads full whatever the raw
                // counter says: the FLAG is the truth, the counter is how it
                // got there.
                const int32 Count = bComplete ? Objective.RequiredCount
                    : FMath::Clamp(Journal->GetCounter(Objective.ProgressCounter), 0, Objective.RequiredCount);
                Row.Counter = FString::Printf(TEXT("%d/%d"), Count, Objective.RequiredCount);
            }
            else if (bComplete)
            {
                Row.Counter = TEXT("DONE");
            }
            // A finished objective recedes rather than disappearing, so the
            // list keeps saying what the contract was.
            Row.Color = bComplete ? BreakerUI::TextMuted : BreakerUI::TextSecondary;
        }
    }

    const float Pad = S(BreakerUI::HudQuestTrackerPad);
    const float RowH = S(BreakerUI::HudQuestTrackerRowHeight);
    const FVector2D TitleSize = MeasureSpecText(Tracked->Title, BreakerUI::HudQuestTitlePixels);
    const float PlateH = Pad + TitleSize.Y + S(BreakerUI::Space4) + Rows.Num() * RowH + Pad;

    DrawPlate(X, Y, Width, PlateH, BreakerUI::Gold);

    const float InnerX = X + S(BreakerUI::RailThickness) + Pad;
    const float InnerRight = X + Width - Pad;
    DrawSpecText(Tracked->Title, InnerX, Y + Pad, BreakerUI::TextPrimary,
        FitSpecPixels(Tracked->Title, BreakerUI::HudQuestTitlePixels, InnerRight - InnerX, 11.0f));

    float RowY = Y + Pad + TitleSize.Y + S(BreakerUI::Space4);
    for (const FTrackerRow& Row : Rows)
    {
        // The counter column is reserved from the token, not measured per
        // frame, so the objective text has a stable fit limit; the text is
        // then FITTED into what is left rather than trusted to be short.
        const float CounterColumn = Row.Counter.IsEmpty() ? 0.0f : S(BreakerUI::HudQuestCounterColumn);
        const float TextLimit = (InnerRight - InnerX) - CounterColumn - (CounterColumn > 0.0f ? S(BreakerUI::Space8) : 0.0f);
        DrawSpecText(Row.Text, InnerX, RowY, Row.Color,
            FitSpecPixels(Row.Text, BreakerUI::HudQuestLinePixels, TextLimit, 9.0f));
        if (!Row.Counter.IsEmpty())
        {
            DrawSpecTextRight(Row.Counter, InnerRight, RowY,
                Row.Counter == TEXT("DONE") || Row.Color == BreakerUI::TextMuted ? BreakerUI::TextMuted : BreakerUI::Gold,
                BreakerUI::HudQuestLinePixels);
        }
        RowY += RowH;
    }
}


// --------------------------------------------------------------------------
// §4 — floating damage numbers. Body 40, weak point 64 gold, crit 80 orange
// spawning at 140% for 60ms. Clusters stack instead of overlapping, and the
// fourth number in one cluster is dropped rather than drawn.
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
    const float ClusterRadius = S(BreakerHUD::DamageClusterRadius);

    for (const FBreakerHUDDamageNumber* Number : Visible)
    {
        const FVector Projected = Project(Number->World, false);
        if (Projected.Z <= 0.0f) continue;
        const FVector2D Screen(Projected.X, Projected.Y);

        int32 Neighbours = 0;
        for (const FVector2D& Other : Placed)
        {
            if (FVector2D::Distance(Other, Screen) <= ClusterRadius) ++Neighbours;
        }
        if (Neighbours >= BreakerHUD::DamageClusterMax) continue;
        Placed.Add(Screen);

        const float Age = static_cast<float>(Now - Number->Time);
        const float Alpha01 = Age / Number->Lifetime;
        // Ease-out rise: fast off the impact, settling as it fades. The last
        // 200ms carry the fade, matching the motion spec. Rise distance rides
        // the number's own lifetime so a short DoT tick travels a short way
        // instead of streaking at three times the speed of everything else.
        const float Rise = S(BreakerUI::DamageRisePixels)
            * (Number->Lifetime / BreakerHUD::DamageNumberLifetime)
            * (1.0f - FMath::Square(1.0f - Alpha01));
        const float FadeStart = 1.0f - FMath::Min(0.2f / Number->Lifetime, 0.6f);
        const float Fade = Alpha01 <= FadeStart ? 1.0f : 1.0f - (Alpha01 - FadeStart) / (1.0f - FadeStart);

        // --- The hierarchy. A number tells you WHAT you did before you read
        // it: DoT ticks are small and grey and die young; body hits are
        // mid-grey and modest; weak points are gold (the aim-skill lane);
        // crits are orange and big; kills multiply whatever their kind earned
        // and body-shot kills brighten to full white — the heaviest neutral
        // read on the ramp. Secondary (chain/ricochet/AoE spill) draws lighter
        // than its parent. Colour separates KIND, size separates WEIGHT.
        FLinearColor Face = BreakerUI::TextSecondary;
        float SizePixels = BreakerUI::DamageBodyPixels;
        float PopScale = 1.15f;
        float PopSeconds = BreakerUI::MotionDamagePop;
        if (Number->bCritical)
        {
            Face = BreakerUI::Orange;
            SizePixels = BreakerUI::DamageCritPixels;
            PopScale = 1.4f;
            PopSeconds = BreakerUI::MotionCritHold;
        }
        else if (Number->bWeakPoint)
        {
            Face = BreakerUI::Gold;
            SizePixels = BreakerUI::DamageWeakPointPixels;
        }
        else if (Number->bFromDoT)
        {
            // A DoT tick that crits or lands a weak point keeps its accent
            // above — those reads outrank the source. A plain tick recedes.
            Face = BreakerUI::TextMuted;
            SizePixels = BreakerUI::DamageDoTPixels;
            PopScale = 1.0f;   // bookkeeping does not pop
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
            PopScale = FMath::Max(PopScale, 1.4f);
            PopSeconds = BreakerHUD::DamageKillPopSeconds;
            // A body-shot kill brightens to full white. Crit and weak-point
            // kills keep their accents — the accent is the rarer read.
            if (!Number->bCritical && !Number->bWeakPoint) Face = BreakerUI::RarityStandard;
        }

        if (Number->bSecondary)
        {
            SizePixels *= BreakerHUD::DamageSecondaryScale;
        }
        // Stack offset scales with the number's OWN size, so a 52px crit and a
        // 26px body hit in the same cluster separate by proportionate amounts
        // instead of both by a flat 8px. Taken before the pop, or a number
        // would jump sideways in the stack as it settled.
        const float StackOffset = S(SizePixels * BreakerHUD::DamageClusterOffsetRatio);

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

        // Spawn oversized, settle to 100%: the pop is the hit confirmation.
        if (Age < PopSeconds) SizePixels *= PopScale;

        // Secondary hits are lighter as well as smaller: the parent owns the
        // full weight of the trigger pull.
        const float DrawAlpha = Number->bSecondary ? Fade * 0.8f : Fade;

        const float NumberY = Screen.Y - Rise - Neighbours * StackOffset;
        DrawOutlinedNumber(BreakerUI::FormatDamage(Number->Value),
            Screen.X, NumberY, Face, SizePixels, DrawAlpha);

        // The overkill share of a killing blow, stated as its own mark in the
        // harm accent under the number: the number says how hard the blow
        // was, the caption says how much of it the corpse never felt. Skipped
        // when trivial — a sliver of overkill is trivia, not a read.
        if (Number->bKilled && Number->Overkill >= Number->Value * BreakerHUD::DamageOverkillCaptionFraction)
        {
            const float NumberHeight = MeasureSpecText(TEXT("0"), SizePixels).Y;
            DrawOutlinedNumber(FString::Printf(TEXT("+%s OVER"), *BreakerUI::FormatDamage(Number->Overkill)),
                Screen.X, NumberY + NumberHeight, BreakerUI::Harm, 13.0f, DrawAlpha);
        }
        else if (bAbsorbed)
        {
            // Caption under the number, at caption weight so it annotates
            // rather than competes — the same relationship the class-resource
            // state word has to its track. Position is MEASURED off the
            // number's own glyph height, not a fixed nudge, so it holds at
            // every one of the three damage sizes and at every UI scale.
            const FString Caption = FString::Printf(TEXT("ABSORBED -%.0f%%"), Number->MitigatedFraction * 100.0f);
            const float NumberHeight = MeasureSpecText(TEXT("0"), SizePixels).Y;
            DrawOutlinedNumber(Caption, Screen.X, NumberY + NumberHeight,
                BreakerUI::Orange, 13.0f, Fade);
        }
    }
}

// --------------------------------------------------------------------------
// §Anchors — enemy bars 180x8 with the name at 11px beneath. Shown near, or
// for six seconds after they were hit, so a distant sniped target still
// confirms the hit landed.
// --------------------------------------------------------------------------
void ABreakerPlaytestHUD::DrawEnemyHealthBars(const ABreakerCharacter* Character)
{
    // ORDERING CONTRACT: this runs before DrawMinimap, and it is the ONE enemy
    // iteration the HUD makes. Reset (not Empty) keeps the capacity, so the
    // array stops allocating after the first busy frame — DrawHUD runs every
    // frame and a container built inside it is a per-frame allocation.
    EnemyBlips.Reset();

    UWorld* World = GetWorld();
    if (!World || !Character) return;
    const FVector ViewerLocation = Character->GetActorLocation();

    // Which enemy the player is actually asking about. Only this one gets the
    // verbose state line; see the label block below for the split. Same
    // aim-cone shape DrawLootPickups already uses to pick its focused pickup,
    // so "what am I pointing at" means one thing across the whole HUD.
    const ABreakerEnemy* FocusedEnemy = nullptr;
    if (PlayerOwner && PlayerOwner->PlayerCameraManager)
    {
        const FVector CameraLocation = PlayerOwner->PlayerCameraManager->GetCameraLocation();
        const FVector CameraForward = PlayerOwner->PlayerCameraManager->GetCameraRotation().Vector();
        float BestDot = BreakerHUD::EnemyFocusMinimumDot;
        for (TActorIterator<ABreakerEnemy> It(World); It; ++It)
        {
            const ABreakerEnemy* Candidate = *It;
            if (!Candidate || Candidate->IsDeadEnemy()) continue;
            const FVector ToEnemy = (Candidate->GetActorLocation() - CameraLocation);
            if (ToEnemy.IsNearlyZero()) continue;
            const float Dot = FVector::DotProduct(CameraForward, ToEnemy.GetSafeNormal());
            if (Dot > BestDot)
            {
                BestDot = Dot;
                FocusedEnemy = Candidate;
            }
        }
    }

    // Reset per frame: these are screen-space rectangles, and last frame's are
    // meaningless the moment the camera moves.
    DrawnLabelBounds.Reset();

    for (TActorIterator<ABreakerEnemy> It(World); It; ++It)
    {
        const ABreakerEnemy* Enemy = *It;
        if (!Enemy) continue;

        // Collected BEFORE the health-bar culls, because the two readouts want
        // different ranges: a bar is pointless past 50 m, and a minimap is
        // mostly useful for the hostiles that are further away than that.
        if (!Enemy->IsDeadEnemy())
        {
            FBreakerHUDMapBlip& Blip = EnemyBlips.AddDefaulted_GetRef();
            Blip.World = Enemy->GetActorLocation();
            Blip.bElite = Enemy->GetMonsterRank() == EBreakerMonsterRank::Elite;
            Blip.bBoss = Enemy->GetMonsterRank() == EBreakerMonsterRank::Boss;
        }

        const float Distance = FVector::Distance(ViewerLocation, Enemy->GetActorLocation());
        if (Distance > BreakerHUD::EnemyBarMaxDistance) continue;

        const UAbilitySystemComponent* EnemyAbilitySystem = Enemy->GetAbilitySystemComponent();
        const UBreakerAttributeSet* EnemyAttributes = EnemyAbilitySystem ? EnemyAbilitySystem->GetSet<UBreakerAttributeSet>() : nullptr;
        if (!EnemyAttributes) continue;

        const float Health = EnemyAttributes->GetHealth();
        const float MaxHealth = EnemyAttributes->GetMaxHealth();
        if (Health <= 0.0f || MaxHealth <= UE_SMALL_NUMBER) continue;

        // Recently damaged is read off the enemy's own combat component: the
        // enemy's Attributes/Combat members are protected, this is not.
        const UBreakerCombatComponent* EnemyCombat = Enemy->FindComponentByClass<UBreakerCombatComponent>();
        const bool bRecentlyDamaged = EnemyCombat && EnemyCombat->GetSecondsSinceDamage() < BreakerHUD::EnemyBarRecentDamageSeconds;
        if (!bRecentlyDamaged && Distance > BreakerHUD::EnemyBarAlwaysDistance) continue;

        const FVector Projected = Project(Enemy->GetActorLocation() + FVector(0.0f, 0.0f, 120.0f), false);
        if (Projected.Z <= 0.0f) continue;

        // Gentle distance scaling: readable up close, unobtrusive far away.
        const float DistanceAlpha = FMath::Clamp((Distance - 500.0f) / (BreakerHUD::EnemyBarMaxDistance - 500.0f), 0.0f, 1.0f);
        const float DistanceScale = FMath::Lerp(1.0f, 0.55f, DistanceAlpha);
        const bool bElite = Enemy->IsElite();
        const float BarW = S(BreakerUI::HudEnemyBarWidth) * DistanceScale;
        const float BarH = S(BreakerUI::HudEnemyBarHeight) * DistanceScale;
        const float BarX = Projected.X - BarW * 0.5f;
        const float BarY = Projected.Y;

        const float Shield = EnemyAttributes->GetShield();
        const float MaxShield = EnemyAttributes->GetMaxShield();
        if (Shield > 0.0f && MaxShield > UE_SMALL_NUMBER)
        {
            const float ShieldH = FMath::Max(BarH * 0.45f, S(2.0f));
            const float ShieldY = BarY - ShieldH - S(1.0f);
            DrawRect(BreakerUI::Panel10, BarX, ShieldY, BarW, ShieldH);
            DrawRect(BreakerUI::Cyan, BarX, ShieldY, BarW * FMath::Clamp(Shield / MaxShield, 0.0f, 1.0f), ShieldH);
        }

        DrawRect(BreakerUI::Panel10, BarX, BarY, BarW, BarH);
        DrawRect(BreakerUI::Harm, BarX, BarY, BarW * FMath::Clamp(Health / MaxHealth, 0.0f, 1.0f), BarH);
        if (bElite)
        {
            // Gold edge, not a gold fill: the health colour must stay readable.
            DrawBorder(BarX, BarY, BarW, BarH, BreakerUI::Gold, S(1.0f) * DistanceScale);
        }

        // ---- The label, and how much of it -----------------------------
        // Owner: "theres a lot of text bloat on enemies". Every enemy printed
        // ELITE/HOSTILE plus the whole of GetEnemyStateLabel(), which is up to
        // THREE stacked lines (family banner, modifier banner, state) — and
        // nothing checked whether two enemies' labels landed on the same
        // pixels. Six enemies in a pocket produced the overlapping mush in the
        // report: "WARDED | VOLATILE" printed through "HOSTILE · WIND-UP"
        // printed through "CHASE".
        //
        // What survives, and why that split:
        //  - The MODIFIER banner is load-bearing and always prints.
        //    Encounter-Design §1.2's first acceptance test is that a modifier
        //    is identifiable within 1.5s of the enemy entering view, and an
        //    unannounced modifier is an unfair death rather than a challenge.
        //    Culling it to declutter would trade legibility for legibility.
        //  - The STATE line (CHASE / CLOSING / WIND-UP) prints only for the
        //    enemy under the crosshair. It was the loudest line and the least
        //    informative: the enemy's own telegraph already shows a wind-up as
        //    a scaling, brightening emitter, so the text was restating in
        //    words, six times over, something the world was already saying.
        //  - ELITE prints; HOSTILE does not. "HOSTILE" on every hostile is not
        //    information — the health bar already says it is an enemy.
        const bool bFocused = (Enemy == FocusedEnemy);
        const FString ModifierBanner = Enemy->GetEnemyModifierBanner();
        TArray<FString> Lines;
        if (bElite) Lines.Add(TEXT("ELITE"));
        if (!ModifierBanner.IsEmpty()) Lines.Add(ModifierBanner);
        if (bFocused) Lines.Add(Enemy->GetEnemyStateLabel());

        if (Lines.Num() > 0)
        {
            const FString Label = FString::Join(Lines, TEXT("\n"));
            const float LabelY = BarY + BarH + S(3.0f);
            // Screen-space overlap suppression. Two enemies standing in line
            // with the camera project to nearly the same point, and the second
            // label lands on top of the first — unreadable, and worse than
            // showing one. The focused enemy is drawn regardless, because it is
            // the one the player is deliberately asking about.
            const float LineCount = static_cast<float>(Lines.Num());
            const float LabelH = S(13.0f) * DistanceScale * LineCount;
            const float LabelW = BarW;
            bool bOccluded = false;
            if (!bFocused)
            {
                for (const FVector4& Taken : DrawnLabelBounds)
                {
                    if (FMath::Abs(Projected.X - Taken.X) < (LabelW + Taken.Z) * 0.5f
                        && FMath::Abs(LabelY - Taken.Y) < (LabelH + Taken.W) * 0.5f)
                    {
                        bOccluded = true;
                        break;
                    }
                }
            }
            if (!bOccluded)
            {
                DrawnLabelBounds.Emplace(Projected.X, LabelY, LabelW, LabelH);
                DrawSpecTextCentered(Label, Projected.X, LabelY,
                    bElite ? BreakerUI::Gold : BreakerUI::TextMuted, 11.0f * DistanceScale);
            }
        }
    }
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

    for (TActorIterator<ABreakerNPC> It(World); It; ++It)
    {
        const ABreakerNPC* NPC = *It;
        if (!NPC) continue;
        const float Distance = FVector::Distance(ViewerLocation, NPC->GetActorLocation());
        if (Distance > LabelMaxDistance) continue;
        // Above the head sphere (rel Z 92 + radius), same idiom as the enemy
        // bars' +120 anchor.
        const FVector Projected = Project(NPC->GetActorLocation() + FVector(0.0f, 0.0f, 150.0f), false);
        if (Projected.Z <= 0.0f) continue;
        DrawSpecTextCentered(NPC->GetDisplayName().ToString().ToUpper(),
            Projected.X, Projected.Y, PersonWarm, 12.0f * DistanceScaleFor(Distance));
    }

    for (TActorIterator<ABreakerTravelPoint> It(World); It; ++It)
    {
        const ABreakerTravelPoint* TravelPoint = *It;
        if (!TravelPoint) continue;
        const float Distance = FVector::Distance(ViewerLocation, TravelPoint->GetActorLocation());
        if (Distance > LabelMaxDistance) continue;
        // Anchored at the marker, not the beacon tip: the 14 m column already
        // owns the skyline, and a label at its top would leave the screen the
        // moment the player got close.
        const FVector Projected = Project(TravelPoint->GetActorLocation() + FVector(0.0f, 0.0f, 260.0f), false);
        if (Projected.Z <= 0.0f) continue;
        // Rift-teal, because travel is the rift verb — the one text colour the
        // reserve permits, on the one label describing a rift object.
        DrawSpecTextCentered(TEXT("TRAVEL"),
            Projected.X, Projected.Y, BreakerUI::TealAnomalous, 13.0f * DistanceScaleFor(Distance));
    }
}

// --------------------------------------------------------------------------
// INTEGRATION: loot pickup chips and hover popup.
// Compiles to nothing until Items/BreakerLootPickup.h exists.
// --------------------------------------------------------------------------
void ABreakerPlaytestHUD::DrawLootPickups(const ABreakerCharacter* Character)
{
#if BREAKER_HAS_LOOT_PICKUP
    UWorld* World = GetWorld();
    if (!World || !Character || !PlayerOwner || !PlayerOwner->PlayerCameraManager) return;

    const FVector CameraLocation = PlayerOwner->PlayerCameraManager->GetCameraLocation();
    const FVector CameraForward = PlayerOwner->PlayerCameraManager->GetCameraRotation().Vector();

    const ABreakerLootPickup* Focused = nullptr;
    float FocusedDot = -1.0f;

    for (TActorIterator<ABreakerLootPickup> It(World); It; ++It)
    {
        const ABreakerLootPickup* Pickup = *It;
        if (!Pickup) continue;

        const FVector ToPickup = Pickup->GetActorLocation() - CameraLocation;
        const float Distance = ToPickup.Size();
        if (Distance > BreakerHUD::PickupChipDistance || Distance <= UE_SMALL_NUMBER) continue;

        const FBreakerItemInstance& Item = Pickup->GetItem();
        const FLinearColor Accent = BreakerUI::RarityColor(Item.Rarity);

        // The single best-aligned pickup inside the popup range wins the
        // panel; everything else stays a chip. Only one popup, ever.
        const float Dot = static_cast<float>(FVector::DotProduct(ToPickup / Distance, CameraForward));
        if (Distance <= BreakerHUD::PickupPopupDistance && Dot >= BreakerHUD::PickupPopupCosine && Dot > FocusedDot)
        {
            Focused = Pickup;
            FocusedDot = Dot;
            continue;
        }

        const FVector Projected = Project(Pickup->GetActorLocation() + FVector(0.0f, 0.0f, 40.0f), false);
        if (Projected.Z <= 0.0f) continue;

        // Chip anatomy: panel face, rarity on the 3px rail and the name only.
        const FString Label = Pickup->GetDisplayLabel().ToString().ToUpper();
        const FVector2D LabelSize = MeasureSpecText(Label, 11.0f);
        const float ChipW = LabelSize.X + S(BreakerUI::Space16) + S(BreakerUI::RailThickness);
        const float ChipH = LabelSize.Y + S(BreakerUI::Space8);
        const float ChipX = Projected.X - ChipW * 0.5f;
        const float ChipY = Projected.Y - ChipH * 0.5f;
        DrawRect(BreakerHUD::PlateFace, ChipX, ChipY, ChipW, ChipH);
        DrawRect(Accent, ChipX, ChipY, S(BreakerUI::RailThickness), ChipH);
        DrawSpecText(Label, ChipX + S(BreakerUI::RailThickness) + S(BreakerUI::Space8), ChipY + S(BreakerUI::Space4), Accent, 11.0f);
    }

    if (Focused)
    {
        const FVector Projected = Project(Focused->GetActorLocation() + FVector(0.0f, 0.0f, 50.0f), false);
        if (Projected.Z <= 0.0f) return;

        const FBreakerItemInstance& Item = Focused->GetItem();
        const FLinearColor Accent = BreakerUI::RarityColor(Item.Rarity);
        const TArray<FString> AffixLines = BreakerHUD::DescribeItemLines(Item);

        // AUDIT (2026-08-14): the panel was a fixed 300px and every line inside
        // it was drawn at its own measured width with nothing checking the two
        // agreed. Item names and affix lines are generated content — an
        // Anomalous name plus a tier suffix is not bounded by anything — so
        // this is the same fixed-gutter defect with the collision against the
        // panel edge instead of against a sibling. Measured: the panel is sized
        // from its widest line, with 300 as a MINIMUM so a two-affix Standard
        // drop does not draw a narrow sliver.
        const FString TitleLine = Focused->GetDisplayLabel().ToString();
        const FString MetaLine = FString::Printf(TEXT("%s · i%d · F TAKE"),
            *BreakerHUD::RarityLabel(Item.Rarity), Item.ItemLevel);
        float WidestLine = FMath::Max(MeasureSpecText(TitleLine, 20.0f).X, MeasureSpecText(MetaLine, 11.0f).X);
        for (const FString& Line : AffixLines)
        {
            WidestLine = FMath::Max(WidestLine, MeasureSpecText(Line, 13.0f).X);
        }
        const float PanelW = FMath::Max(S(300.0f), WidestLine + S(BreakerUI::Space16) * 2.0f);
        const float PanelH = S(64.0f) + AffixLines.Num() * S(16.0f) + S(24.0f);
        const float PanelX = FMath::Clamp(Projected.X - PanelW * 0.5f, S(8.0f), Canvas->ClipX - PanelW - S(8.0f));
        const float PanelY = FMath::Clamp(Projected.Y - PanelH - S(12.0f), S(8.0f), Canvas->ClipY - PanelH - S(8.0f));

        DrawPlate(PanelX, PanelY, PanelW, PanelH, Accent);
        // Anomalous is the one rarity that also gets a full border, because it
        // is the only tier that is also a world object class.
        if (BreakerUI::RarityGetsFullBorder(Item.Rarity))
        {
            DrawBorder(PanelX, PanelY, PanelW, PanelH, Accent, S(1.0f));
        }

        const float TextX = PanelX + S(BreakerUI::Space16);
        DrawSpecText(TitleLine, TextX, PanelY + S(BreakerUI::Space16), Accent, 20.0f);
        DrawSpecText(MetaLine, TextX, PanelY + S(44.0f), BreakerUI::TextMuted, 11.0f);

        float LineY = PanelY + S(64.0f);
        for (const FString& Line : AffixLines)
        {
            DrawSpecText(Line, TextX, LineY, BreakerUI::TextSecondary, 13.0f);
            LineY += S(16.0f);
        }
    }
#endif
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

void ABreakerPlaytestHUD::DrawExperienceRail(const ABreakerCharacter* Character)
{
    const UBreakerProgressionComponent* Progression = Character ? Character->GetProgression() : nullptr;
    if (!Progression || !Canvas) return;

    const int32 Level = Progression->GetCharacterLevel();
    const float Fraction = Progression->GetLevelProgressFraction();
    const int32 ToNext = Progression->GetXpToNextLevel();

    // Bottom-centre, above the ability cluster's baseline and clear of it: the
    // XP rail is glanceable, not a thing to read mid-fight, so it gets width
    // and almost no height.
    const float RailW = S(420.0f);
    const float RailH = S(6.0f);
    const float RailX = Canvas->ClipX * 0.5f - RailW * 0.5f;
    const float RailY = Canvas->ClipY - S(46.0f);

    // While the level-up banner is live the rail joins the event: the fill
    // blinks between gold and cyan on a metronome (a blink, never a fade —
    // FIELDPLATE motion is mechanical) and the whole track takes a 1px gold
    // frame. Same geometry, same position: the pulse is colour, so nothing
    // shifts.
    const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
    const float BannerAge = static_cast<float>(Now - LevelUpTime);
    const bool bCelebrating = LevelUpShownLevel > 0
        && BannerAge >= 0.0f && BannerAge < BreakerHUD::LevelUpBannerSeconds;
    const bool bBlinkOn = bCelebrating
        && FMath::Fmod(BannerAge, BreakerHUD::LevelUpRailBlinkSeconds) < BreakerHUD::LevelUpRailBlinkSeconds * 0.5f;

    DrawRect(BreakerUI::Panel10, RailX, RailY, RailW, RailH);
    DrawRect(bBlinkOn ? BreakerUI::Gold : BreakerUI::Cyan,
        RailX, RailY, RailW * FMath::Clamp(Fraction, 0.0f, 1.0f), RailH);
    if (bCelebrating) DrawBorder(RailX, RailY, RailW, RailH, BreakerUI::Gold, S(1.0f));

    // At the cap the bar reads full and the caption says so, rather than
    // showing a full bar next to a number that will never move again.
    const FString Caption = ToNext > 0
        ? FString::Printf(TEXT("LEVEL %d   %s XP TO NEXT"), Level,
            *BreakerUI::FormatDamage(static_cast<float>(FMath::Max(0,
                ToNext - FMath::RoundToInt(Fraction * static_cast<float>(ToNext))))))
        : FString::Printf(TEXT("LEVEL %d   MAX"), Level);
    DrawSpecTextCentered(Caption, Canvas->ClipX * 0.5f, RailY - S(16.0f),
        bCelebrating ? BreakerUI::Gold : BreakerUI::TextMuted, 11.0f);
}

void ABreakerPlaytestHUD::DrawLevelUpBanner(const FVector2D& Center)
{
    if (LevelUpShownLevel <= 0 || !Canvas) return;
    const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
    const float Age = static_cast<float>(Now - LevelUpTime);
    if (Age < 0.0f || Age >= BreakerHUD::LevelUpBannerSeconds) return;

    // The EVENT treatment. A level is the single most earned moment in the
    // loop and it gets a real plate: gold top rail (transient status, reward
    // family), the level stated large, and the point grant stated explicitly
    // so the player learns what they were just paid without opening a menu.
    // In is a snap — frame one is the full banner, purchase-confirm style,
    // because the commit must feel mechanical. Out is a 120ms linear fade,
    // the panel-out motion, so leaving costs less than arriving. No layout
    // shift: the plate is sized from its measured content and nothing else
    // on the screen moves for it.
    const float OutStart = BreakerHUD::LevelUpBannerSeconds - BreakerHUD::LevelUpOutSeconds;
    const float Fade = Age <= OutStart ? 1.0f
        : 1.0f - (Age - OutStart) / BreakerHUD::LevelUpOutSeconds;

    const FString Title = LevelUpShownGain > 1
        ? FString::Printf(TEXT("LEVEL %d  (+%d)"), LevelUpShownLevel, LevelUpShownGain)
        : FString::Printf(TEXT("LEVEL %d"), LevelUpShownLevel);
    // The grant line: "+1 CLASS   +1 CORE". Both halves are optional past
    // their caps; past both, the level still deserves its banner.
    FString Grant;
    if (LevelUpClassGain > 0) Grant = FString::Printf(TEXT("+%d CLASS"), LevelUpClassGain);
    if (LevelUpCoreGain > 0)
    {
        if (!Grant.IsEmpty()) Grant += TEXT("   ");
        Grant += FString::Printf(TEXT("+%d CORE"), LevelUpCoreGain);
    }
    if (Grant.IsEmpty()) Grant = TEXT("POINT CAP REACHED");

    constexpr float TitlePixels = 24.0f;   // O2 PLACEHOLDER
    constexpr float GrantPixels = 13.0f;   // O2 PLACEHOLDER
    const FVector2D LabelSize = MeasureSpecText(TEXT("LEVEL UP"), 11.0f);
    const FVector2D TitleSize = MeasureSpecText(Title, TitlePixels);
    const FVector2D GrantSize = MeasureSpecText(Grant, GrantPixels);

    const float Pad = S(BreakerUI::Space16);
    const float ContentW = FMath::Max3(LabelSize.X, TitleSize.X, GrantSize.X);
    const float PlateW = FMath::Max(S(260.0f), ContentW + Pad * 2.0f);
    const float PlateH = S(BreakerUI::RailThickness) + Pad
        + LabelSize.Y + S(BreakerUI::Space4) + TitleSize.Y + S(BreakerUI::Space8) + GrantSize.Y + Pad;
    const float PlateX = Center.X - PlateW * 0.5f;
    const float PlateY = Center.Y - S(190.0f);

    // The plate never fades — FIELDPLATE plates do not lower opacity — so the
    // out is carried by the text and the banner's plate leaves on its last
    // frame whole, like a plate being unbolted rather than dissolving.
    DrawPlate(PlateX, PlateY, PlateW, PlateH, BreakerUI::Gold, EBreakerRail::Top);
    // Purchase-confirm language: the border snaps to gold on frame one, no
    // ease in, and decays back to the resting border over 260ms.
    if (Age < 0.26f) DrawBorder(PlateX, PlateY, PlateW, PlateH, BreakerUI::Gold, S(1.0f));

    float LineY = PlateY + S(BreakerUI::RailThickness) + Pad;
    DrawSpecTextCentered(TEXT("LEVEL UP"), Center.X, LineY, BreakerUI::Gold, 11.0f, Fade);
    LineY += LabelSize.Y + S(BreakerUI::Space4);
    DrawSpecTextCentered(Title, Center.X, LineY, BreakerUI::TextPrimary, TitlePixels, Fade);
    LineY += TitleSize.Y + S(BreakerUI::Space8);
    DrawSpecTextCentered(Grant, Center.X, LineY, BreakerUI::Gold, GrantPixels, Fade);
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
    LevelUpTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
    LevelUpShownLevel = NewLevel;
    // Carried so the banner can say "+2 LEVELS" rather than lying by one: a
    // single kill can cross more than one level early on, and a tell that says
    // "level 2" when the player reached 4 is worse than no tell.
    LevelUpShownGain = LevelsGained;
    // What this level-up actually PAID, computed the same way the progression
    // component grants it (one point per level up to each currency's cap), so
    // the banner states the grant instead of leaving the player to discover
    // it in a menu. A level past a cap claims nothing.
    const int32 PrevLevel = NewLevel - LevelsGained;
    LevelUpClassGain = FMath::Max(0,
        FMath::Min(NewLevel, UBreakerProgressionLibrary::ClassPointCapLevel)
        - FMath::Min(PrevLevel, UBreakerProgressionLibrary::ClassPointCapLevel));
    LevelUpCoreGain = FMath::Max(0,
        FMath::Min(NewLevel, UBreakerProgressionLibrary::CorePointCapLevel)
        - FMath::Min(PrevLevel, UBreakerProgressionLibrary::CorePointCapLevel));
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

    // Skim's crosshair burst is keyed off the ability's identity, not its slot:
    // the loadout is free to move it between the two class slots.
    if (Definition && Definition->AbilityId == FName(TEXT("Swift.Skim")))
    {
        SkimBurstTime = Now;
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

    // The teaching callout retires itself. After three casts the player knows
    // what the key does, and a permanent banner would be noise. The ONE
    // exception is a rewrite the player has not been told about yet: a keystone
    // is bought long after the third cast, so gating it on the show count alone
    // would mean the ultimate silently becomes a different ability. A newly
    // resolved variant name re-opens the callout exactly once.
    const bool bVariantIsNew = !VariantName.IsEmpty() && VariantName != SlotLastVariantName[Index];
    if (!bVariantIsNew)
    {
        SlotLastVariantName[Index] = VariantName;
        if (SlotActivationCount[Index] > BreakerHUD::AbilityCalloutMaxShows) return;
    }
    SlotLastVariantName[Index] = VariantName;

    // The variant name already reads as "Overdrive — Terminal Velocity", so it
    // replaces the plain display name rather than appending to it.
    const FString Name = !VariantName.IsEmpty()
        ? VariantName
        : (Definition->DisplayName.IsEmpty() ? FText::FromName(Definition->AbilityId) : Definition->DisplayName).ToString();
    const FString Blurb = BreakerHUD::FirstSentence(Definition->Description.ToString()).TrimStartAndEnd();
    CalloutText = Blurb.IsEmpty() ? Name.ToUpper() : FString::Printf(TEXT("%s — %s"), *Name.ToUpper(), *Blurb);
    CalloutTime = Now;
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
        DrawSpecText(ShortKey.ToUpper(), X, RowY, Color, 11.0f);
        DrawSpecTextRight(FString::Printf(TEXT("%.1fs"), Remaining), X + Width, RowY, Color, 11.0f);

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
void ABreakerPlaytestHUD::DrawAbilityCallout(const FVector2D& Center)
{
    if (CalloutText.IsEmpty()) return;
    const double Age = (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0) - CalloutTime;
    if (Age < 0.0 || Age >= BreakerHUD::AbilityCalloutSeconds) return;

    const float Fade = 1.0f - static_cast<float>(Age) / BreakerHUD::AbilityCalloutSeconds;
    DrawSpecTextCentered(CalloutText, Center.X, Center.Y + S(132.0f), BreakerUI::TextPrimary, 14.0f, Fade);
}

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
        DrawSpecTextCentered(TEXT("OVERDRIVE ACTIVE"), W * 0.5f, PlateY + S(14.0f), BreakerUI::Violet, 20.0f);
    }
}

// --------------------------------------------------------------------------
// Skim's confirmation: a radial speed-line burst at the crosshair. Short and
// centre-screen because the redirect is felt, not seen.
// --------------------------------------------------------------------------
void ABreakerPlaytestHUD::DrawSkimBurst(const FVector2D& Center)
{
    const double Age = (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0) - SkimBurstTime;
    if (Age < 0.0 || Age >= BreakerHUD::SkimBurstSeconds) return;

    const float Alpha01 = static_cast<float>(Age) / BreakerHUD::SkimBurstSeconds;
    const float Fade = 1.0f - Alpha01;
    // Lines travel outward as they fade, which reads as speed rather than as a
    // flashing ring.
    const float Inner = FMath::Lerp(S(16.0f), S(40.0f), Alpha01);
    const float Outer = Inner + FMath::Lerp(S(14.0f), S(6.0f), Alpha01);
    const FLinearColor Color = BreakerUI::Alpha(BreakerUI::Cyan, Fade);

    constexpr int32 LineCount = 8;
    for (int32 Index = 0; Index < LineCount; ++Index)
    {
        const float Angle = (2.0f * UE_PI) * static_cast<float>(Index) / static_cast<float>(LineCount);
        const float Cos = FMath::Cos(Angle);
        const float Sin = FMath::Sin(Angle);
        DrawLine(Center.X + Cos * Inner, Center.Y + Sin * Inner,
                 Center.X + Cos * Outer, Center.Y + Sin * Outer, Color, S(1.75f));
    }
}

// --------------------------------------------------------------------------
// Lead's mark. Projected the same way the enemy bars are, just higher, so the
// diamond sits clear of the health bar on the same target.
// --------------------------------------------------------------------------
void ABreakerPlaytestHUD::DrawMarkedTarget(const ABreakerCharacter* Character)
{
    const UBreakerAbilityStateComponent* State = GetAbilityState(Character);
    const AActor* Marked = State ? State->GetMarkedTarget() : nullptr;
    if (!Marked) return;

    const FVector Projected = Project(Marked->GetActorLocation() + FVector(0.0f, 0.0f, BreakerHUD::MarkHeadroomCm), false);
    if (Projected.Z <= 0.0f) return;

    // Slow pulse: enough to catch the eye in peripheral vision, not enough to
    // compete with the impact feedback at the crosshair.
    const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
    const float Pulse = 0.75f + 0.25f * FMath::Sin(static_cast<float>(Now) * 4.0f);
    const FLinearColor Color = BreakerUI::Alpha(BreakerUI::Cyan, Pulse);
    const float Radius = S(9.0f) * Pulse;

    const float CX = Projected.X;
    const float CY = Projected.Y;
    DrawLine(CX, CY - Radius, CX + Radius, CY, Color, S(1.75f));
    DrawLine(CX + Radius, CY, CX, CY + Radius, Color, S(1.75f));
    DrawLine(CX, CY + Radius, CX - Radius, CY, Color, S(1.75f));
    DrawLine(CX - Radius, CY, CX, CY - Radius, Color, S(1.75f));

    DrawSpecTextCentered(TEXT("MARKED"), CX, CY - Radius - S(16.0f), Color, 11.0f);
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

void ABreakerPlaytestHUD::HandlePlayerShot(const FBreakerShotResult& Shot)
{
    if (!Shot.bFired) return;

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
        const float FlightSeconds = BreakerHUD::TracerFlightSeconds(BreakerHUD::TracerFlight,
            static_cast<float>((End - Start).Size()));

        // Every round counts; only some of them are visible. See
        // BreakerHUD::TracerRoundsPerTracer — fast weapons trace one round in
        // three, slow ones trace all of them.
        const int32 RoundsPerTracer = BreakerHUD::TracerRoundsPerTracer(
            FiredDefinition ? FiredDefinition->RoundsPerMinute : 600.0f);
        const bool bVisibleRound =
            !bPelletShot && BreakerHUD::ShouldTraceRound(RoundsFired, RoundsPerTracer);
        ++RoundsFired;

        if (bDrawSpread)
        {
            // The whole blast in one call: the renderer draws the budgeted
            // subsample of streaks AND a flash on every landed pellet, so the
            // spread is never traced by the single-impact path below. Spreads
            // are exempt from the tracer cadence — a shell is one event, and
            // skipping two shells in three would read as the gun misfiring.
            if (ABreakerTracerRenderer* Renderer = GetTracerRenderer())
            {
                Renderer->AddSpread(Start, Shot.Pellets);
            }
        }
        else if (bVisibleRound)
        {
            if (ABreakerTracerRenderer* Renderer = GetTracerRenderer())
            {
                Renderer->AddTracer(Start, End);
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

void ABreakerPlaytestHUD::HandlePlayerHitDealt(const FBreakerHitContext& Hit)
{
    // OVERKILL-INCLUSIVE, display only. Applied damage is clamped to what the
    // target could still lose — correct for the vitals write, and a lie as a
    // number: a 900-damage rocket on a 30 HP enemy printed 30, and the owner
    // reads these numbers for TTK/balance, so a killing blow under-reporting
    // by 30x poisons exactly the read they exist for. The clamped value still
    // drives everything mechanical; only what is PRINTED adds the overkill.
    const float Applied = Hit.Result.ShieldDamage + Hit.Result.HealthDamage;
    const float Shown = Applied > 0.0f ? Applied + Hit.Result.OverkillDamage : Hit.Result.MitigatedDamage;
    if (Shown <= 0.0f) return;

    const UWorld* World = GetWorld();
    const float Now = World ? World->GetTimeSeconds() : 0.0f;
    const float Raw = Hit.Result.RawDamage;
    const float Mitigated = Raw > UE_SMALL_NUMBER
        ? FMath::Clamp(1.0f - Hit.Result.MitigatedDamage / Raw, 0.0f, 1.0f) : 0.0f;
    const bool bWeak = Hit.bWeakPoint || Hit.Result.bWeakPoint;

    // Crosshair confirms, from the same universal feed the numbers ride: an
    // ability's cleave ticks the crosshair exactly as a bullet does. DoT
    // ticks are excluded — a Bleed on three targets is not something the
    // player just did, and it would strobe the crosshair forever.
    if (!Hit.bFromDoT)
    {
        LastHitDealtTime = Now;
        bHitDealtWeakPoint = bWeak;
    }
    if (Hit.Result.bKilled)
    {
        LastKillConfirmTime = Now;
        bKillConfirmWeakPoint = bWeak;
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
    for (FBreakerHUDDamageNumber& Existing : DamageNumbers)
    {
        if (Existing.Target != Hit.Target) continue;
        if (Existing.bCritical != Hit.Result.bCritical) continue;
        if (Existing.bWeakPoint != bWeak) continue;
        if (Existing.bFromDoT != Hit.bFromDoT) continue;
        if (Now - Existing.Time > BreakerHUD::DamageNumberMergeWindow) continue;

        Existing.Value += Shown;
        Existing.Time = Now;
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
    Number.MitigatedFraction = Mitigated;
    Number.Time = Now;
    Number.bKilled = Hit.Result.bKilled;
    Number.Overkill = Hit.Result.OverkillDamage;
    Number.Lifetime = Hit.Result.bKilled ? BreakerHUD::DamageKillLifetime
        : Hit.bFromDoT ? BreakerHUD::DamageDoTLifetime
        : BreakerHUD::DamageNumberLifetime;

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
        NextDamageNumberIndex = DamageNumbers.Num() % MaxDamageNumbers;
    }
    else
    {
        DamageNumbers[NextDamageNumberIndex] = Number;
        NextDamageNumberIndex = (NextDamageNumberIndex + 1) % MaxDamageNumbers;
    }
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
    // Re-seeded on the damage numbers' own lifetime, so the capture always
    // catches them mid-rise rather than after they have expired.
    if (Now - LastPreviewSpawnTime < BreakerHUD::DamageNumberLifetime * 0.6) return;
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
            : BreakerHUD::DamageNumberLifetime;
        PushDamageNumber(Number);
    }

    // The crosshair kill confirm and the level-up moment, fabricated on the
    // same cadence so a multi-shot capture run photographs both mid-event.
    LastKillConfirmTime = Now;
    bKillConfirmWeakPoint = false;
    const float BannerAge = static_cast<float>(Now - LevelUpTime);
    if (BannerAge < 0.0f || BannerAge > BreakerHUD::LevelUpBannerSeconds + 2.0f)
    {
        LevelUpTime = Now;
        LevelUpShownLevel = 12;
        LevelUpShownGain = 1;
        LevelUpClassGain = 1;
        LevelUpCoreGain = 1;
    }
}

void ABreakerPlaytestHUD::HandlePlayerDamageReceived(const FBreakerDamageResult& Result)
{
    const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
    if (Result.bDodged) LastDodgeTime = Now;
    else if (Result.bBlocked) LastBlockTime = Now;
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
    // Dodge is a player-system success (cyan); block is mitigation, which is
    // the armour/weapon family (orange).
    DrawSpecTextCentered(bShowDodge ? TEXT("DODGED") : TEXT("BLOCKED"),
        Center.X, Center.Y - S(108.0f), bShowDodge ? BreakerUI::Cyan : BreakerUI::Orange, 20.0f, Fade);
}

// --------------------------------------------------------------------------
// Crosshair kill confirm. Distinct from the hit tick by GEOMETRY, per the
// same argument the absorbed tick already makes: two states told only by
// colour at one mark do not read in a fight. The hit tick is four short
// diagonal strokes OUTSIDE the centre; the kill confirm is an eight-point
// burst whose strokes push outward as it ages — the mark physically opens,
// the way the target just did. Harm red for an ordinary kill, gold when the
// killing blow was a weak-point hit, so the aim-skill lane keeps its colour
// through the loudest confirm it can earn.
// --------------------------------------------------------------------------
void ABreakerPlaytestHUD::DrawKillConfirm(const FVector2D& Center)
{
    const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
    const float Age = static_cast<float>(Now - LastKillConfirmTime);
    if (Age < 0.0f || Age >= BreakerHUD::KillConfirmSeconds) return;

    const float T = Age / BreakerHUD::KillConfirmSeconds;
    // Fast open, easing out: most of the travel in the first third.
    const float Open = 1.0f - FMath::Square(1.0f - T);
    // Line alpha fades over the back half. Lines over the world, not a plate.
    const float Alpha = T < 0.5f ? 1.0f : 1.0f - (T - 0.5f) * 2.0f;
    const FLinearColor Color = BreakerUI::Alpha(
        bKillConfirmWeakPoint ? BreakerUI::Gold : BreakerUI::Harm, Alpha);

    const float Inner = S(10.0f) + S(8.0f) * Open;
    const float Length = S(11.0f);
    const float Diagonal = 0.7071f;
    for (int32 Index = 0; Index < 4; ++Index)
    {
        const float DX = (Index & 1) ? 1.0f : -1.0f;
        const float DY = (Index & 2) ? 1.0f : -1.0f;
        // Diagonal strokes: the X of the burst.
        DrawLine(Center.X + DX * Inner * Diagonal, Center.Y + DY * Inner * Diagonal,
                 Center.X + DX * (Inner + Length) * Diagonal, Center.Y + DY * (Inner + Length) * Diagonal,
                 Color, S(3.5f));
    }
    // Axis strokes, shorter: the + that makes it a burst rather than a
    // second tick.
    const float AxisInner = Inner * 0.9f;
    const float AxisLength = Length * 0.6f;
    DrawLine(Center.X - AxisInner - AxisLength, Center.Y, Center.X - AxisInner, Center.Y, Color, S(2.0f));
    DrawLine(Center.X + AxisInner, Center.Y, Center.X + AxisInner + AxisLength, Center.Y, Color, S(2.0f));
    DrawLine(Center.X, Center.Y - AxisInner - AxisLength, Center.X, Center.Y - AxisInner, Color, S(2.0f));
    DrawLine(Center.X, Center.Y + AxisInner, Center.X, Center.Y + AxisInner + AxisLength, Color, S(2.0f));
}

// --------------------------------------------------------------------------
// Low-health screen-edge cue. FIELDPLATE has no gradients, so this is not a
// true vignette: two nested full-bleed frames in the harm accent, solid
// fills, stepping in as health falls. Below the dire threshold the outer
// band blinks on a metronome — a blink, never a fade, because urgency in
// this system is mechanical. Health only: shields regenerate, and a
// full-shield character at 10% health is still one mistake from dying.
// Sits under the transient damage flash, which draws after it and brighter.
// --------------------------------------------------------------------------
void ABreakerPlaytestHUD::DrawLowHealthCue(const ABreakerCharacter* Character)
{
    const UBreakerAttributeSet* Attributes = Character ? Character->GetAttributes() : nullptr;
    if (!Attributes || !Canvas) return;
    const float MaxHealth = Attributes->GetMaxHealth();
    // The preview forces the dire state: nothing in a headless run can lose
    // health, so without this the cue is unphotographable — the exact failure
    // mode the capture harness exists to close.
    const float Fraction = IsCapturePreview() ? 0.12f
        : (MaxHealth > UE_SMALL_NUMBER ? Attributes->GetHealth() / MaxHealth : 1.0f);
    if (Fraction >= BreakerHUD::LowHealthFraction) return;

    const bool bDire = Fraction < BreakerHUD::LowHealthDireFraction;
    const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
    const bool bBlinkOn = !bDire
        || FMath::Fmod(static_cast<float>(Now), BreakerHUD::LoudBlinkSeconds) < BreakerHUD::LoudBlinkSeconds * 0.5f;

    // Outer band: always on while low; steps thicker as the state worsens.
    // 0.75/0.45, up from a first pass at 0.6/0.35: over a bright sky the
    // lower pair read as a polite pink picture frame, which is the opposite
    // of what a near-death scream is for. Looked at, not guessed.
    const float OuterT = S(bDire ? 5.0f : 3.0f);
    const FLinearColor Outer = BreakerUI::Alpha(BreakerUI::Harm, bBlinkOn ? 0.75f : 0.45f);
    DrawRect(Outer, 0.0f, 0.0f, Canvas->ClipX, OuterT);
    DrawRect(Outer, 0.0f, Canvas->ClipY - OuterT, Canvas->ClipX, OuterT);
    DrawRect(Outer, 0.0f, 0.0f, OuterT, Canvas->ClipY);
    DrawRect(Outer, Canvas->ClipX - OuterT, 0.0f, OuterT, Canvas->ClipY);

    if (bDire)
    {
        // Inner band, deep step, inset by the outer band: the second ring of
        // the stepped vignette, only in the dire state.
        const float InnerT = S(2.0f);
        const FLinearColor Inner = BreakerUI::Alpha(BreakerUI::HarmDeep, 0.45f);
        DrawRect(Inner, OuterT, OuterT, Canvas->ClipX - OuterT * 2.0f, InnerT);
        DrawRect(Inner, OuterT, Canvas->ClipY - OuterT - InnerT, Canvas->ClipX - OuterT * 2.0f, InnerT);
        DrawRect(Inner, OuterT, OuterT, InnerT, Canvas->ClipY - OuterT * 2.0f);
        DrawRect(Inner, Canvas->ClipX - OuterT - InnerT, OuterT, InnerT, Canvas->ClipY - OuterT * 2.0f);
    }
}

// --------------------------------------------------------------------------
// Status chips, sitting above the vitals plate. BottomY is the row's bottom
// edge so the chips grow upward and never displace the plate.
// --------------------------------------------------------------------------
void ABreakerPlaytestHUD::DrawStatusReadout(const ABreakerCharacter* Character, float X, float BottomY)
{
    const UBreakerStatusComponent* Status = Character ? Character->FindComponentByClass<UBreakerStatusComponent>() : nullptr;
    if (!Status) return;

    const float ChipH = S(20.0f);
    const float ChipY = BottomY - ChipH;
    float ChipX = X;
    // AUDIT (2026-08-14): this row ran rightward with no bound at all. Three
    // stacked DoTs and it walks out from under the vitals plate and off toward
    // the wave banner; enough of them and it leaves the screen. It is the same
    // defect the others are — a fixed start with no knowledge of what shares
    // the row — just with the collision at the far end instead of the near one.
    // The row is bounded to the vitals plate it annotates, and the overflow is
    // COUNTED rather than silently dropped.
    const float RowRight = X + S(BreakerUI::HudVitalsWidth);
    int32 Dropped = 0;
    for (const FBreakerActiveStatus& Active : Status->GetActiveStatuses())
    {
        FString ShortName = Active.Spec.StatusTag.IsValid() ? Active.Spec.StatusTag.GetTagName().ToString() : TEXT("STATUS");
        int32 SeparatorIndex = INDEX_NONE;
        if (ShortName.FindLastChar(TEXT('.'), SeparatorIndex)) ShortName = ShortName.RightChop(SeparatorIndex + 1);
        const FString Text = FString::Printf(TEXT("%s %d · %.1f"), *ShortName.ToUpper(), Active.Stacks, FMath::Max(Active.RemainingDuration, 0.0f));

        const FVector2D TextSize = MeasureSpecText(Text, 11.0f);
        const float ChipW = TextSize.X + S(BreakerUI::Space16) + S(BreakerUI::Space4);
        // Reserve room for a "+N" overflow chip while anything is still to come.
        if (ChipX + ChipW > RowRight - S(36.0f))
        {
            ++Dropped;
            continue;
        }
        DrawRect(BreakerHUD::PlateFace, ChipX, ChipY, ChipW, ChipH);
        DrawBorder(ChipX, ChipY, ChipW, ChipH, BreakerUI::BorderEmphasis, S(1.0f));
        // Statuses are incoming harm: the chip's marker is the harm accent.
        DrawRect(BreakerUI::Harm, ChipX, ChipY, S(BreakerUI::Space4), ChipH);
        DrawSpecText(Text, ChipX + S(BreakerUI::Space8) + S(BreakerUI::Space4), ChipY + S(BreakerUI::Space4), BreakerUI::Harm, 11.0f);
        ChipX += ChipW + S(BreakerUI::Space8);
    }

    if (Dropped > 0)
    {
        const FString More = FString::Printf(TEXT("+%d"), Dropped);
        const FVector2D MoreSize = MeasureSpecText(More, 11.0f);
        const float MoreW = MoreSize.X + S(BreakerUI::Space16);
        DrawRect(BreakerHUD::PlateFace, ChipX, ChipY, MoreW, ChipH);
        DrawBorder(ChipX, ChipY, MoreW, ChipH, BreakerUI::BorderEmphasis, S(1.0f));
        DrawSpecText(More, ChipX + S(BreakerUI::Space8), ChipY + S(BreakerUI::Space4), BreakerUI::Harm, 11.0f);
    }
}

// ==========================================================================
// FIELDPLATE primitives
// ==========================================================================

void ABreakerPlaytestHUD::DrawCrosshair(const FVector2D& Center, const FLinearColor& Color, float Size, float Thickness)
{
    DrawLine(Center.X - Size, Center.Y, Center.X + Size, Center.Y, Color, Thickness);
    DrawLine(Center.X, Center.Y - Size, Center.X, Center.Y + Size, Color, Thickness);
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
const UFont* ABreakerPlaytestHUD::GetSpecFont()
{
    if (!SpecFont)
    {
        // UMG's default face. Vector, Runtime-cached, and the same Roboto the
        // Slate menus draw with, so the HUD and the front end agree.
        SpecFont = LoadObject<UFont>(nullptr, TEXT("/Engine/EngineFonts/Roboto.Roboto"));
    }
    return SpecFont;
}

bool ABreakerPlaytestHUD::CanDrawSpecFont()
{
    const UFont* Font = GetSpecFont();
    return Font && Font->FontCacheType == EFontCacheType::Runtime;
}

FSlateFontInfo ABreakerPlaytestHUD::MakeSpecFont(float SpecPixels)
{
    // The type scale carries its own weight rule: display and number tokens are
    // 600-700, body and caption are 400-500. 17px is the boundary between them,
    // so weight follows size rather than needing a flag at every call site.
    const int32 PixelSize = FMath::Max(FMath::RoundToInt(S(SpecPixels)), 6);
    return FSlateFontInfo(GetSpecFont(), PixelSize, SpecPixels >= 17.0f ? TEXT("Bold") : TEXT("Regular"));
}

FVector2D ABreakerPlaytestHUD::MeasureSpecText(const FString& Text, float SpecPixels)
{
    if (CanDrawSpecFont() && FSlateApplication::IsInitialized())
    {
        if (const FSlateRenderer* Renderer = FSlateApplication::Get().GetRenderer())
        {
            return FVector2D(Renderer->GetFontMeasureService()->Measure(Text, MakeSpecFont(SpecPixels)));
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

void ABreakerPlaytestHUD::DrawSpecText(const FString& Text, float X, float Y, const FLinearColor& Color, float SpecPixels, float TextAlpha)
{
    if (TextAlpha <= 0.0f || !Canvas) return;
    const FLinearColor Face = BreakerUI::Alpha(Color, Color.A * TextAlpha);
    if (CanDrawSpecFont())
    {
        FCanvasTextItem Item(FVector2D(X, Y), FText::FromString(Text), MakeSpecFont(SpecPixels), Face);
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

float ABreakerPlaytestHUD::FitSpecPixels(const FString& Text, float DesiredPixels, float MaxWidth, float MinPixels)
{
    if (Text.IsEmpty() || MaxWidth <= 0.0f) return DesiredPixels;
    // Down one spec pixel at a time. The type scale is small integers and the
    // loop is bounded by (Desired - Min), so this is a handful of measures in
    // the worst case and usually exactly one.
    for (float Pixels = DesiredPixels; Pixels > MinPixels; Pixels -= 1.0f)
    {
        if (MeasureSpecText(Text, Pixels).X <= MaxWidth) return Pixels;
    }
    return MinPixels;
}

void ABreakerPlaytestHUD::DrawSpecTextRight(const FString& Text, float RightX, float Y, const FLinearColor& Color, float SpecPixels, float TextAlpha)
{
    DrawSpecText(Text, RightX - MeasureSpecText(Text, SpecPixels).X, Y, Color, SpecPixels, TextAlpha);
}

void ABreakerPlaytestHUD::DrawSpecTextCentered(const FString& Text, float CenterX, float Y, const FLinearColor& Color, float SpecPixels, float TextAlpha)
{
    DrawSpecText(Text, CenterX - MeasureSpecText(Text, SpecPixels).X * 0.5f, Y, Color, SpecPixels, TextAlpha);
}

// §4: a 2px outline in a near-black tinted toward the number's own hue, so the
// outline never reads as grey mud.
void ABreakerPlaytestHUD::DrawOutlinedNumber(const FString& Text, float CenterX, float Y, const FLinearColor& Face, float SpecPixels, float TextAlpha)
{
    if (TextAlpha <= 0.0f) return;
    const float X = CenterX - MeasureSpecText(Text, SpecPixels).X * 0.5f;
    const FLinearColor Outline(Face.R * 0.10f, Face.G * 0.10f, Face.B * 0.10f, 0.9f * TextAlpha);
    const float Offset = FMath::Max(S(SpecPixels * 0.05f), 1.0f);
    DrawSpecText(Text, X - Offset, Y, Outline, SpecPixels);
    DrawSpecText(Text, X + Offset, Y, Outline, SpecPixels);
    DrawSpecText(Text, X, Y - Offset, Outline, SpecPixels);
    DrawSpecText(Text, X, Y + Offset, Outline, SpecPixels);
    DrawSpecText(Text, X, Y, Face, SpecPixels, TextAlpha);
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

    // The rail is full-bleed to the plate's edge: no inset, no radius.
    const float RailSize = S(BreakerUI::RailThickness);
    if (RailEdge == EBreakerRail::Top) DrawRect(Rail, X, Y, Width, RailSize);
    else                               DrawRect(Rail, X, Y, RailSize, Height);
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

// Icon spec: a flat dark wedge sweeping clockwise from 12 o'clock, uncovering
// the plate as it empties. Hard-edged, no feather. Each fan vertex is placed
// on the square's own boundary rather than on a circle, so the sweep is
// exactly the square's sector and never spills past the plate edge.
void ABreakerPlaytestHUD::DrawCooldownWedge(float X, float Y, float Size, float CoveredFraction, const FLinearColor& Color)
{
    const float Covered = FMath::Clamp(CoveredFraction, 0.0f, 1.0f);
    if (Covered <= 0.0f) return;

    const FVector2D Center(X + Size * 0.5f, Y + Size * 0.5f);
    const float Half = Size * 0.5f;
    constexpr int32 Segments = 32;
    const int32 Used = FMath::Max(1, FMath::CeilToInt(Segments * Covered));

    auto OnSquare = [&Center, Half](float Angle)
    {
        const float Cos = FMath::Cos(Angle);
        const float Sin = FMath::Sin(Angle);
        const float Reach = Half / FMath::Max(FMath::Max(FMath::Abs(Cos), FMath::Abs(Sin)), UE_SMALL_NUMBER);
        return FVector2D(Center.X + Cos * Reach, Center.Y + Sin * Reach);
    };

    // -90 degrees is 12 o'clock; increasing angle is clockwise in screen space.
    const float Start = -UE_HALF_PI;
    const float Sweep = 2.0f * UE_PI * Covered;
    for (int32 Index = 0; Index < Used; ++Index)
    {
        const float A0 = Start + Sweep * (static_cast<float>(Index) / Used);
        const float A1 = Start + Sweep * (static_cast<float>(Index + 1) / Used);
        DrawTriangle(Center, OnSquare(A0), OnSquare(A1), Color);
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
// §3 — ability squares. 56x56, one plate, four states carried by geometry and
// border rather than by brightness.
// --------------------------------------------------------------------------
void ABreakerPlaytestHUD::DrawAbilitySlot(const ABreakerCharacter* Character, const UBreakerAbilityComponent* Abilities,
    EBreakerAbilitySlot Slot, const FString& KeyHint, float X, float Y, float Size, const FLinearColor& Accent)
{
    const bool bGranted = Abilities && Abilities->IsSlotGranted(Slot);
    const float Remaining = bGranted ? Abilities->GetCooldownRemaining(Slot) : 0.0f;
    const float Duration = bGranted ? Abilities->GetCooldownDuration(Slot) : 0.0f;
    const bool bOnCooldown = bGranted && Remaining > 0.0f && Duration > UE_SMALL_NUMBER;
    const bool bAffordable = bGranted && Abilities->CanAffordSlot(Slot);

    const UBreakerAbilityDefinition* Definition = bGranted ? Abilities->GetDefinitionForSlot(Slot) : nullptr;
    const UBreakerAbilityStateComponent* State = GetAbilityState(Character);
    const FName WindowKey = BreakerHUD::WindowKeyFor(Definition);
    const bool bWindowActive = State && !WindowKey.IsNone() && State->IsWindowActive(WindowKey);

    // Window active lifts the plate fill a panel step. Everything else keeps
    // the plate at panel/10 — states are told by border and geometry.
    DrawRect(bWindowActive ? BreakerUI::Panel20 : BreakerUI::Panel10, X, Y, Size, Size);

    // Cooldown: hard dark wedge over the plate, covering what is left to wait.
    if (bOnCooldown)
    {
        DrawCooldownWedge(X, Y, Size, Remaining / Duration, BreakerUI::Alpha(BreakerUI::BgVoid, 0.85f));
    }

    // Activation flash: the only feedback that fires for an ability which
    // changes no visible state, so it runs whatever else the square shows.
    const int32 SlotIndex = static_cast<int32>(Slot);
    if (SlotIndex >= 0 && SlotIndex < AbilitySlotCount)
    {
        const double FlashAge = (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0) - SlotActivationTime[SlotIndex];
        if (FlashAge >= 0.0 && FlashAge < BreakerHUD::AbilityFlashSeconds)
        {
            const float Fade = 1.0f - static_cast<float>(FlashAge) / BreakerHUD::AbilityFlashSeconds;
            DrawRect(BreakerUI::Alpha(Accent, 0.6f * Fade), X, Y, Size, Size);
        }
    }

    // Border states, in priority order: unaffordable reads deep red because
    // waiting will not fix it; cooldown drops to the neutral rest border;
    // ready and window-active hold the 2px accent.
    FLinearColor GlyphColor = Accent;
    if (!bGranted)
    {
        DrawBorder(X, Y, Size, Size, BreakerUI::BorderRest, S(BreakerUI::BorderThin));
        GlyphColor = BreakerUI::TextDisabled;
    }
    else if (!bAffordable)
    {
        DrawBorder(X, Y, Size, Size, BreakerUI::HarmDeep, S(BreakerUI::BorderSelected));
        GlyphColor = BreakerUI::TextDisabled;
    }
    else if (bOnCooldown)
    {
        DrawBorder(X, Y, Size, Size, BreakerUI::BorderEmphasis, S(BreakerUI::BorderThin));
        GlyphColor = BreakerUI::TextDisabled;
    }
    else
    {
        DrawBorder(X, Y, Size, Size, Accent, S(BreakerUI::BorderSelected));
    }

    // Window active adds two 8px corner ticks — geometry, not brightness.
    if (bWindowActive)
    {
        const float Tick = S(BreakerUI::Space8);
        const float T = S(2.0f);
        DrawRect(Accent, X, Y, Tick, T);
        DrawRect(Accent, X, Y, T, Tick);
        DrawRect(Accent, X + Size - Tick, Y + Size - T, Tick, T);
        DrawRect(Accent, X + Size - T, Y + Size - Tick, T, Tick);
    }

    // The mark itself, drawn to the icon spec's construction notes inside the
    // 36x36 optical box. Never a word: a letter in a 56px square is what made
    // the first pass unreadable.
    DrawAbilityGlyph(Definition, X + Size * 0.5f, Y + Size * 0.44f, Size * (36.0f / 56.0f), GlyphColor);

    // Cooldown timer over the wedge, one decimal below 3s.
    if (bOnCooldown)
    {
        const FString Timer = Remaining < 3.0f ? FString::Printf(TEXT("%.1f"), Remaining) : FString::Printf(TEXT("%.0f"), Remaining);
        const FVector2D TimerSize = MeasureSpecText(Timer, 18.0f);
        // On its own opaque chip: the number has to win against the glyph and
        // the wedge edge underneath it.
        DrawRect(BreakerUI::Alpha(BreakerUI::BgVoid, 0.9f),
            X + Size * 0.5f - TimerSize.X * 0.5f - S(4.0f), Y + Size * 0.5f - TimerSize.Y * 0.5f,
            TimerSize.X + S(8.0f), TimerSize.Y);
        DrawSpecTextCentered(Timer, X + Size * 0.5f, Y + Size * 0.5f - TimerSize.Y * 0.5f, BreakerUI::TextPrimary, 18.0f);
    }
    // Unaffordable: a struck hex chip lower-centre on its own opaque plate, so
    // it never tangles with the mark behind it. No sweep, by design — nothing
    // is filling, so waiting will not fix it.
    else if (bGranted && !bAffordable)
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

    // Key hint bottom-right, inheriting the state colour: a glance at the
    // letter also reports the state.
    // Inset from the square's inner edge, not its outer one: at Space4 the
    // hint sat ON the 2px ready border and read as a rendering fault. The
    // vertical inset carries the text height plus the border so the glyph
    // clears it at every scale.
    DrawSpecTextRight(KeyHint, X + Size - S(BreakerUI::Space8), Y + Size - S(19.0f), GlyphColor, 11.0f);
}
