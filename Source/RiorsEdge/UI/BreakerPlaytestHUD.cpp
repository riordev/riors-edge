#include "UI/BreakerPlaytestHUD.h"

#include "Abilities/BreakerAbilityComponent.h"
#include "Abilities/BreakerAbilityDefinition.h"
#include "Abilities/BreakerAbilityStateComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Classes/BreakerManaComponent.h"
#include "Classes/BreakerMomentumComponent.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "UI/BreakerUIStyle.h"
#include "Weapons/BreakerWeaponComponent.h"
#include "Playtest/BreakerPlaytestComponent.h"
#include "Combat/BreakerTargetDummy.h"
#include "Combat/BreakerEnemy.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerStatusComponent.h"
#include "Interaction/BreakerNPC.h"
#include "Game/BreakerGameMode.h"
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
#include "Styling/CoreStyle.h"
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

    static constexpr float TracerLifetime = 0.12f;
    static constexpr float ImpactLifetime = 0.25f;
    // 40ms pop + 520ms rise, FIELDPLATE §04.
    static constexpr float DamageNumberLifetime =
        BreakerUI::MotionDamagePop + BreakerUI::MotionDamageRise;

    // §4: numbers within 60px of one another stack at 8px offsets, and a
    // fourth simultaneous number in the same cluster is dropped, not drawn.
    static constexpr float DamageClusterRadius = 60.0f;
    static constexpr float DamageClusterOffset = 8.0f;
    static constexpr int32 DamageClusterMax = 3;

    // Enemy bar visibility rules.
    static constexpr float EnemyBarMaxDistance = 5000.0f;
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
    // Everything below, ability callouts included, is suppressed while the
    // pause/inventory menu owns the screen.
    if (Character->IsMenuOpen()) return;

    const UBreakerWeaponComponent* Weapon = Character->GetWeapon();
    const UBreakerPlaytestComponent* Playtest = Character->GetPlaytest();
    const bool bRecentShot = Weapon && Weapon->GetSecondsSinceLastShot() < 0.14f;
    const FBreakerShotResult* Shot = Weapon ? &Weapon->GetLastShot() : nullptr;

    // World-anchored layers first: they sit over the world but under every
    // screen-anchored plate, so the HUD frame always wins a collision.
    DrawTracers();
    DrawEnemyHealthBars(Character);
    DrawMarkedTarget(Character);
    DrawDamageNumbers();
    DrawLootPickups(Character);

    // Under the crosshair and under every plate: the ultimate frame is
    // ambient, never something the eye has to read past.
    DrawUltimateTreatment(Character);

    // --- Crosshair (§Anchors: 80x80 box, hit ticks gold at 45 degrees) ---
    const bool bAiming = Weapon && Weapon->IsAiming();
    const float RestingCrosshairSize = bAiming ? 4.0f : 8.0f;
    DrawCrosshair(Center, BreakerUI::TextPrimary,
        S(bRecentShot ? 12.0f : RestingCrosshairSize), S(bRecentShot ? 2.5f : 1.5f));
    if (bRecentShot && Shot && Shot->bHit)
    {
        // Diagonal ticks, gold on a weak point and harm-red otherwise. The
        // crosshair itself never changes colour, so aim is never re-learned.
        const FLinearColor TickColor = Shot->bWeakPoint ? BreakerUI::Gold : BreakerUI::Harm;
        const float Inner = S(6.0f);
        const float Outer = S(14.0f);
        for (int32 Index = 0; Index < 4; ++Index)
        {
            const float DX = (Index & 1) ? 1.0f : -1.0f;
            const float DY = (Index & 2) ? 1.0f : -1.0f;
            const float Diagonal = 0.7071f;
            DrawLine(Center.X + DX * Inner * Diagonal, Center.Y + DY * Inner * Diagonal,
                     Center.X + DX * Outer * Diagonal, Center.Y + DY * Outer * Diagonal,
                     TickColor, S(2.0f));
        }
    }

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

    // --- Centre: feedback only, nothing persistent ------------------------
    DrawSkimBurst(Center);
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

    // --- Top-left: playtest instrumentation ------------------------------
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
        DrawSpecText(Row.StateWord, InnerX + S(84.0f), Y + Pad - S(3.0f), Row.StateColor, 17.0f);
        DrawSpecTextRight(FString::Printf(TEXT("%.0f M/S"), Character->GetHorizontalSpeed() / 100.0f),
            InnerRight, Y + Pad, BreakerUI::TextMuted, 11.0f);

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
        DrawSpecText(Weapon->GetArchetypeName().ToUpper(), InnerX, WeaponRowY + S(6.0f), BreakerUI::TextPrimary, 18.0f);

        const FString StateText = Weapon->IsReloading() ? TEXT("RELOADING")
            : Weapon->IsSwapping() ? TEXT("SWAPPING")
            : Weapon->IsAiming() ? TEXT("ADS") : FString::Printf(TEXT("SLOT %d"), Slot);
        DrawSpecText(StateText, InnerX, WeaponRowY + S(28.0f),
            Weapon->IsReloading() ? BreakerUI::Orange : BreakerUI::TextMuted, 11.0f);

        // Magazine dominates; reserve is deliberately subordinate (number-large
        // at 44, reserve at 18 in text/muted).
        const FString Reserve = FString::Printf(TEXT("/%s"), *BreakerUI::FormatTicker(Weapon->GetReserveAmmo()));
        const FVector2D ReserveSize = MeasureSpecText(Reserve, 18.0f);
        DrawSpecTextRight(Reserve, InnerRight, WeaponRowY + S(22.0f), BreakerUI::TextMuted, 18.0f);
        DrawSpecTextRight(FString::Printf(TEXT("%d"), Weapon->GetMagazineAmmo()),
            InnerRight - ReserveSize.X - S(BreakerUI::Space4), WeaponRowY + S(2.0f),
            Weapon->GetMagazineAmmo() > 0 ? BreakerUI::TextPrimary : BreakerUI::Harm, 44.0f);
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
    DrawBorder(X, Y, Width, Height, Row.BorderColor, S(Row.BorderPixels));
}

// --------------------------------------------------------------------------
// Wave banner, centred at 48px from the top. Top rail, because a wave is a
// transient status and not a system that owns the plate.
// --------------------------------------------------------------------------
void ABreakerPlaytestHUD::DrawWaveBanner(const FVector2D& Center)
{
    const ABreakerGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ABreakerGameMode>() : nullptr;
    if (!GameMode || GameMode->GetCurrentWave() <= 0) return;

    const bool bActive = GameMode->IsWaveActive();
    const FString Title = FString::Printf(TEXT("WAVE %02d"), GameMode->GetCurrentWave());
    const FString Status = bActive
        ? FString::Printf(TEXT("%d HOSTILE"), GameMode->GetWaveEnemiesAlive())
        : FString(TEXT("CLEAR — F4"));

    const float PlateW = S(260.0f);
    const float PlateH = S(56.0f);
    const float PlateX = Center.X - PlateW * 0.5f;
    const float PlateY = S(BreakerUI::HudSafeMargin);
    DrawPlate(PlateX, PlateY, PlateW, PlateH, bActive ? BreakerUI::Orange : BreakerUI::Cyan, EBreakerRail::Top);

    DrawSpecText(Title, PlateX + S(BreakerUI::Space16), PlateY + S(14.0f), BreakerUI::TextPrimary, 28.0f);
    // Vertical divider, then the count: two facts, one plate.
    DrawRect(BreakerUI::BorderEmphasis, PlateX + PlateW * 0.55f, PlateY + S(14.0f), S(1.0f), PlateH - S(26.0f));
    DrawSpecTextRight(Status, PlateX + PlateW - S(BreakerUI::Space16), PlateY + S(20.0f),
        bActive ? BreakerUI::Orange : BreakerUI::Cyan, 16.0f);
}

// --------------------------------------------------------------------------
// Hitscan tracers. Ring buffer of world-space lines, projected fresh every
// frame so they track the camera correctly while they fade.
// --------------------------------------------------------------------------
void ABreakerPlaytestHUD::DrawTracers()
{
    if (Tracers.Num() == 0) return;
    const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;

    for (const FBreakerHUDTracer& Tracer : Tracers)
    {
        const float Age = static_cast<float>(Now - Tracer.Time);
        if (Age < 0.0f) continue;

        if (Age < BreakerHUD::TracerLifetime)
        {
            // bClampToZeroPlane=false so a point behind the camera keeps its
            // negative depth and can be rejected instead of folding forward.
            const FVector StartProj = Project(Tracer.Start, false);
            const FVector EndProj = Project(Tracer.End, false);
            if (StartProj.Z > 0.0f && EndProj.Z > 0.0f)
            {
                const float Fade = 1.0f - Age / BreakerHUD::TracerLifetime;
                DrawLine(StartProj.X, StartProj.Y, EndProj.X, EndProj.Y,
                    BreakerUI::Alpha(BreakerUI::Gold, Fade), S(1.25f));
            }
        }

        if (Tracer.bHit && Age < BreakerHUD::ImpactLifetime)
        {
            const FVector ImpactProj = Project(Tracer.Impact, false);
            if (ImpactProj.Z > 0.0f)
            {
                const float Fade = 1.0f - Age / BreakerHUD::ImpactLifetime;
                const FLinearColor CrossColor = BreakerUI::Alpha(
                    Tracer.bWeakPoint ? BreakerUI::Gold : BreakerUI::Cyan, Fade);
                const float Arm = S(4.0f);
                DrawLine(ImpactProj.X - Arm, ImpactProj.Y - Arm, ImpactProj.X + Arm, ImpactProj.Y + Arm, CrossColor, S(1.5f));
                DrawLine(ImpactProj.X - Arm, ImpactProj.Y + Arm, ImpactProj.X + Arm, ImpactProj.Y - Arm, CrossColor, S(1.5f));
            }
        }
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
        if (Age < 0.0f || Age >= BreakerHUD::DamageNumberLifetime) continue;
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
        const float Alpha01 = Age / BreakerHUD::DamageNumberLifetime;
        // Ease-out rise: fast off the impact, settling as it fades. The last
        // 200ms carry the fade, matching the motion spec.
        const float Rise = S(BreakerUI::DamageRisePixels) * (1.0f - FMath::Square(1.0f - Alpha01));
        const float FadeStart = 1.0f - 0.2f / BreakerHUD::DamageNumberLifetime;
        const float Fade = Alpha01 <= FadeStart ? 1.0f : 1.0f - (Alpha01 - FadeStart) / (1.0f - FadeStart);

        FLinearColor Face = BreakerUI::RarityStandard;
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
        // Spawn oversized, settle to 100%: the pop is the hit confirmation.
        if (Age < PopSeconds) SizePixels *= PopScale;

        DrawOutlinedNumber(BreakerUI::FormatTicker(Number->Value),
            Screen.X, Screen.Y - Rise - Neighbours * S(BreakerHUD::DamageClusterOffset),
            Face, SizePixels, Fade);
    }
}

// --------------------------------------------------------------------------
// §Anchors — enemy bars 180x8 with the name at 11px beneath. Shown near, or
// for six seconds after they were hit, so a distant sniped target still
// confirms the hit landed.
// --------------------------------------------------------------------------
void ABreakerPlaytestHUD::DrawEnemyHealthBars(const ABreakerCharacter* Character)
{
    UWorld* World = GetWorld();
    if (!World || !Character) return;
    const FVector ViewerLocation = Character->GetActorLocation();

    for (TActorIterator<ABreakerEnemy> It(World); It; ++It)
    {
        const ABreakerEnemy* Enemy = *It;
        if (!Enemy) continue;

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

        // The name line the spec asks for. Enemies carry no display name or
        // level yet, so it states the two facts that do exist.
        DrawSpecTextCentered(FString::Printf(TEXT("%s · %s"),
            bElite ? TEXT("ELITE") : TEXT("HOSTILE"), *Enemy->GetEnemyStateLabel()),
            Projected.X, BarY + BarH + S(3.0f),
            bElite ? BreakerUI::Gold : BreakerUI::TextMuted, 11.0f * DistanceScale);
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

        const float PanelW = S(300.0f);
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
        DrawSpecText(Focused->GetDisplayLabel().ToString(), TextX, PanelY + S(BreakerUI::Space16), Accent, 20.0f);
        DrawSpecText(FString::Printf(TEXT("%s · i%d · F TAKE"),
            *BreakerHUD::RarityLabel(Item.Rarity), Item.ItemLevel),
            TextX, PanelY + S(44.0f), BreakerUI::TextMuted, 11.0f);

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
    if (BoundCombat) BoundCombat->OnDamageReceived.RemoveDynamic(this, &ABreakerPlaytestHUD::HandlePlayerDamageReceived);
    Combat->OnDamageReceived.AddDynamic(this, &ABreakerPlaytestHUD::HandlePlayerDamageReceived);
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

    // The teaching callout retires itself. After three casts the player knows
    // what the key does, and a permanent banner would be noise.
    if (!Definition || SlotActivationCount[Index] > BreakerHUD::AbilityCalloutMaxShows) return;

    const FString Name = (Definition->DisplayName.IsEmpty() ? FText::FromName(Definition->AbilityId) : Definition->DisplayName).ToString();
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

void ABreakerPlaytestHUD::HandlePlayerShot(const FBreakerShotResult& Shot)
{
    if (!Shot.bFired) return;

    FBreakerHUDTracer Entry;
    Entry.Start = Shot.TraceStart;
    // Draw to the impact when there is one, otherwise out to the trace end.
    Entry.End = Shot.bHit ? Shot.ImpactPoint : Shot.TraceEnd;
    Entry.Impact = Shot.ImpactPoint;
    Entry.bHit = Shot.bHit;
    Entry.bWeakPoint = Shot.bWeakPoint;
    Entry.Time = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;

    if (Tracers.Num() < MaxTracers)
    {
        Tracers.Add(Entry);
        NextTracerIndex = Tracers.Num() % MaxTracers;
    }
    else
    {
        Tracers[NextTracerIndex] = Entry;
        NextTracerIndex = (NextTracerIndex + 1) % MaxTracers;
    }

    // Same event feeds the floating numbers: one subscription, two readouts.
    const float Applied = Shot.DamageResult.ShieldDamage + Shot.DamageResult.HealthDamage;
    const float Shown = Applied > 0.0f ? Applied : Shot.DamageResult.MitigatedDamage;
    if (!Shot.bHit || Shown <= 0.0f) return;

    FBreakerHUDDamageNumber Number;
    Number.World = Shot.ImpactPoint;
    Number.Value = Shown;
    Number.bCritical = Shot.DamageResult.bCritical;
    Number.bWeakPoint = Shot.bWeakPoint || Shot.DamageResult.bWeakPoint;
    Number.Time = Entry.Time;

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
    for (const FBreakerActiveStatus& Active : Status->GetActiveStatuses())
    {
        FString ShortName = Active.Spec.StatusTag.IsValid() ? Active.Spec.StatusTag.GetTagName().ToString() : TEXT("STATUS");
        int32 SeparatorIndex = INDEX_NONE;
        if (ShortName.FindLastChar(TEXT('.'), SeparatorIndex)) ShortName = ShortName.RightChop(SeparatorIndex + 1);
        const FString Text = FString::Printf(TEXT("%s %d · %.1f"), *ShortName.ToUpper(), Active.Stacks, FMath::Max(Active.RemainingDuration, 0.0f));

        const FVector2D TextSize = MeasureSpecText(Text, 11.0f);
        const float ChipW = TextSize.X + S(BreakerUI::Space16) + S(BreakerUI::Space4);
        DrawRect(BreakerHUD::PlateFace, ChipX, ChipY, ChipW, ChipH);
        DrawBorder(ChipX, ChipY, ChipW, ChipH, BreakerUI::BorderEmphasis, S(1.0f));
        // Statuses are incoming harm: the chip's marker is the harm accent.
        DrawRect(BreakerUI::Harm, ChipX, ChipY, S(BreakerUI::Space4), ChipH);
        DrawSpecText(Text, ChipX + S(BreakerUI::Space8) + S(BreakerUI::Space4), ChipY + S(BreakerUI::Space4), BreakerUI::Harm, 11.0f);
        ChipX += ChipW + S(BreakerUI::Space8);
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
FSlateFontInfo ABreakerPlaytestHUD::MakeSpecFont(float SpecPixels) const
{
    // The type scale carries its own weight rule: display and number tokens are
    // 600-700, body and caption are 400-500. 17px is the boundary between them,
    // so weight follows size rather than needing a flag at every call site.
    const int32 PixelSize = FMath::Max(FMath::RoundToInt(S(SpecPixels)), 6);
    return FCoreStyle::GetDefaultFontStyle(SpecPixels >= 17.0f ? "Bold" : "Regular", PixelSize);
}

FVector2D ABreakerPlaytestHUD::MeasureSpecText(const FString& Text, float SpecPixels) const
{
    if (FSlateApplication::IsInitialized())
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
    if (FSlateApplication::IsInitialized())
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
    DrawSpecTextRight(KeyHint, X + Size - S(BreakerUI::Space4), Y + Size - S(16.0f), GlyphColor, 11.0f);
}
