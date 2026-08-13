#include "UI/BreakerPlaytestHUD.h"

#include "Abilities/BreakerAbilityComponent.h"
#include "Abilities/BreakerAbilityDefinition.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Classes/BreakerMomentumComponent.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
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

namespace BreakerHUD
{
    // The established language: cyan is player/system, orange is weapon,
    // gold is reward/weak point, red is harm.
    static const FLinearColor Cyan(0.12f, 0.78f, 1.0f);
    static const FLinearColor Orange(1.0f, 0.45f, 0.12f);
    static const FLinearColor Gold(1.0f, 0.75f, 0.05f);
    static const FLinearColor Violet(0.72f, 0.38f, 1.0f);
    static const FLinearColor Ink(0.012f, 0.022f, 0.038f, 0.86f);
    static const FLinearColor Bright(0.92f, 0.95f, 0.98f);
    static const FLinearColor Muted(0.58f, 0.66f, 0.75f);
    static const FLinearColor Dim(0.23f, 0.28f, 0.34f);

    // Margin and internal gutter. Everything in the redesign is a multiple of
    // Gutter so the two clusters read as one system.
    static constexpr float Margin = 24.0f;
    static constexpr float Gutter = 8.0f;
    static constexpr float ClusterWidth = 320.0f;
    static constexpr float ClusterHeight = 150.0f;
    static constexpr float TracerLifetime = 0.12f;
    static constexpr float ImpactLifetime = 0.25f;
    static constexpr float DamageNumberLifetime = 0.7f;
    static constexpr float DamageNumberRise = 40.0f;

    // Enemy bar visibility rules.
    static constexpr float EnemyBarMaxDistance = 5000.0f;
    static constexpr float EnemyBarAlwaysDistance = 1500.0f;
    static constexpr float EnemyBarRecentDamageSeconds = 6.0f;

    // Loot pickup rules.
    static constexpr float PickupChipDistance = 1500.0f;
    static constexpr float PickupPopupDistance = 800.0f;
    // cos(3 degrees): the popup only opens when the player is genuinely
    // looking at the drop, not merely facing its half of the room.
    static constexpr float PickupPopupCosine = 0.99863f;

#if BREAKER_HAS_LOOT_PICKUP
    // Only compiled alongside the pickup drawing that uses them: unreferenced
    // static functions are a warning, and warnings are errors here.
    //
    // Mirrors SBreakerMenu's rarity language so an item reads the same on the
    // ground as it does in the backpack.
    static FLinearColor RarityColor(EBreakerItemRarity Rarity)
    {
        switch (Rarity)
        {
            case EBreakerItemRarity::Uncommon: return FLinearColor(0.25f, 0.55f, 1.0f);
            case EBreakerItemRarity::Exceptional: return FLinearColor(0.72f, 0.4f, 1.0f);
            case EBreakerItemRarity::Aberrant: return FLinearColor(1.0f, 0.25f, 0.25f);
            case EBreakerItemRarity::Anomalous: return FLinearColor(0.15f, 0.95f, 0.85f);
            default: return FLinearColor(0.85f, 0.85f, 0.85f);
        }
    }

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
#endif // BREAKER_HAS_LOOT_PICKUP
}

void ABreakerPlaytestHUD::DrawHUD()
{
    Super::DrawHUD();
    if (!Canvas) return;

    const FVector2D Center(Canvas->ClipX * 0.5f, Canvas->ClipY * 0.5f);
    const ABreakerCharacter* Character = Cast<ABreakerCharacter>(GetOwningPawn());
    if (!Character)
    {
        DrawCrosshair(Center, FLinearColor::White, 8.0f, 1.5f);
        return;
    }
    EnsureDamageBinding(Character);
    EnsureWeaponBinding(Character);
    if (Character->IsMenuOpen()) return;

    const UBreakerWeaponComponent* Weapon = Character->GetWeapon();
    const UBreakerPlaytestComponent* Playtest = Character->GetPlaytest();
    const bool bRecentShot = Weapon && Weapon->GetSecondsSinceLastShot() < 0.14f;
    const FBreakerShotResult* Shot = Weapon ? &Weapon->GetLastShot() : nullptr;

    // World-anchored layers first: they sit over the world but under every
    // screen-anchored cluster, so the HUD frame always wins a collision.
    DrawTracers();
    DrawEnemyHealthBars(Character);
    DrawDamageNumbers();
    DrawLootPickups(Character);

    FLinearColor CrosshairColor = FLinearColor::White;
    if (bRecentShot && Shot && Shot->bHit) CrosshairColor = Shot->bWeakPoint ? BreakerHUD::Gold : FLinearColor::Red;
    const bool bAiming = Weapon && Weapon->IsAiming();
    const float RestingCrosshairSize = bAiming ? 4.0f : 8.0f;
    DrawCrosshair(Center, CrosshairColor, bRecentShot ? 12.0f : RestingCrosshairSize, bRecentShot ? 2.5f : 1.5f);

    // --- Bottom-left vitals band ---------------------------------------
    DrawVitalsBand(Character, BreakerHUD::Margin, Canvas->ClipY - BreakerHUD::Margin);

    // --- Bottom-right combat cluster (the anchor) ----------------------
    DrawCombatCluster(Character,
        Canvas->ClipX - BreakerHUD::Margin - BreakerHUD::ClusterWidth,
        Canvas->ClipY - BreakerHUD::Margin - BreakerHUD::ClusterHeight,
        BreakerHUD::ClusterWidth, BreakerHUD::ClusterHeight);

    // --- Centre: feedback only, nothing persistent ---------------------
    DrawDefenseFeedback(Center);
    if (const ABreakerNPC* NearbyNPC = Character->FindNearbyNPC())
    {
        DrawLabel(FString::Printf(TEXT("F  TALK — %s"), *NearbyNPC->GetDisplayName().ToString()), Center.X - 90.0f, Center.Y + 90.0f, BreakerHUD::Cyan, 1.0f);
    }
    if (const ABreakerGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ABreakerGameMode>() : nullptr)
    {
        if (GameMode->IsWaveActive())
        {
            DrawLabel(FString::Printf(TEXT("WAVE %d  —  %d REMAINING"), GameMode->GetCurrentWave(), GameMode->GetWaveEnemiesAlive()), Center.X - 90.0f, 44.0f, BreakerHUD::Gold, 1.1f);
        }
        else if (GameMode->GetCurrentWave() > 0)
        {
            DrawLabel(FString::Printf(TEXT("WAVE %d CLEAR  —  F4 FOR NEXT"), GameMode->GetCurrentWave()), Center.X - 90.0f, 44.0f, BreakerHUD::Cyan, 1.0f);
        }
    }
    if (const UBreakerCombatComponent* PlayerCombat = Character->GetCombat(); PlayerCombat && PlayerCombat->GetSecondsSinceDamage() < 0.28f)
    {
        const FLinearColor DamageColor(1.0f, 0.12f, 0.05f, 0.85f);
        DrawLabel(TEXT("DAMAGE"), Center.X - 32.0f, Center.Y - 80.0f, DamageColor, 1.15f);
        DrawLine(8.0f, 8.0f, Canvas->ClipX - 8.0f, 8.0f, DamageColor, 5.0f);
        DrawLine(8.0f, Canvas->ClipY - 8.0f, Canvas->ClipX - 8.0f, Canvas->ClipY - 8.0f, DamageColor, 5.0f);
        DrawLine(8.0f, 8.0f, 8.0f, Canvas->ClipY - 8.0f, DamageColor, 5.0f);
        DrawLine(Canvas->ClipX - 8.0f, 8.0f, Canvas->ClipX - 8.0f, Canvas->ClipY - 8.0f, DamageColor, 5.0f);
    }
    if (Weapon && Weapon->IsReloading())
        DrawLabel(TEXT("RELOADING"), Center.X - 54.0f, Center.Y + 48.0f, FLinearColor(0.55f, 0.9f, 1.0f), 1.1f);

    // The old fixed-position damage readout is gone: floating world-space
    // numbers say the same thing at the impact point. Only the weak-point
    // callout survives, because it is a skill confirmation, not a value.
    if (bRecentShot && Shot && Shot->bHit && Shot->bWeakPoint)
        DrawLabel(TEXT("WEAK POINT"), Center.X + 24.0f, Center.Y + 18.0f, BreakerHUD::Gold, 0.9f);

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
        DrawLabel(TEXT("ELITE DOWN"), Center.X - 62.0f, Center.Y - 118.0f, FLinearColor(1.0f, 0.75f, 0.05f, Fade), 1.3f);
    }

    // --- Top-left: keys, and the diagnostics panel tucked beneath -------
    DrawLabel(TEXT("F1 RESET   F2 REPORT   F3 DIAGNOSTICS   ESC MENU"), BreakerHUD::Margin, BreakerHUD::Margin, FLinearColor(0.42f, 0.5f, 0.58f), 0.72f);
    if (Playtest && Playtest->AreDiagnosticsVisible())
    {
        const FBreakerPlaytestStats& Stats = Playtest->GetStats();
        const float FPS = GetWorld() && GetWorld()->GetDeltaSeconds() > UE_SMALL_NUMBER ? 1.0f / GetWorld()->GetDeltaSeconds() : 0.0f;
        const float DiagY = BreakerHUD::Margin + 16.0f;
        DrawPanel(BreakerHUD::Margin, DiagY, 300.0f, 58.0f, FLinearColor(0.5f, 1.0f, 0.65f));
        DrawLabel(FString::Printf(TEXT("FPS %.0f   FOV %.0f   SENS %.1f"), FPS, Character->GetCurrentFOV(), Character->GetLookSensitivity()),
            BreakerHUD::Margin + 12.0f, DiagY + 8.0f, FLinearColor(0.5f, 1.0f, 0.65f), 0.8f);
        DrawLabel(FString::Printf(TEXT("SHOTS %d   ACC %.1f%%   WEAK %.1f%%"), Stats.ShotsFired, Stats.Accuracy(), Stats.WeakPointRate()),
            BreakerHUD::Margin + 12.0f, DiagY + 24.0f, FLinearColor(0.5f, 1.0f, 0.65f), 0.78f);
        DrawLabel(FString::Printf(TEXT("DMG %.0f   RELOADS %d"), Stats.DamageDealt, Stats.Reloads),
            BreakerHUD::Margin + 12.0f, DiagY + 40.0f, FLinearColor(0.5f, 1.0f, 0.65f), 0.78f);

        // Diagnostics world labels stay short-range and small: past 25m they
        // were pure screen noise.
        for (TActorIterator<ABreakerTargetDummy> It(GetWorld()); It; ++It)
        {
            const float Distance = FVector::Distance(Character->GetActorLocation(), It->GetActorLocation());
            if (Distance > 2500.0f) continue;
            FVector2D Screen;
            if (PlayerOwner && PlayerOwner->ProjectWorldLocationToScreen(It->GetActorLocation() + FVector(0.0f, 0.0f, 130.0f), Screen))
            {
                DrawLabel(FString::Printf(TEXT("%s  %.0fm"), *It->GetProfileLabel(), Distance / 100.0f), Screen.X - 34.0f, Screen.Y, FLinearColor(0.8f, 0.9f, 1.0f, 0.7f), 0.65f);
            }
        }
        for (TActorIterator<ABreakerEnemy> It(GetWorld()); It; ++It)
        {
            if (FVector::DistSquared(Character->GetActorLocation(), It->GetActorLocation()) > FMath::Square(2500.0f)) continue;
            FVector2D Screen;
            if (PlayerOwner && PlayerOwner->ProjectWorldLocationToScreen(It->GetActorLocation() + FVector(0.0f, 0.0f, 130.0f), Screen))
            {
                DrawLabel(It->GetEnemyStateLabel(), Screen.X - 24.0f, Screen.Y, FLinearColor(1.0f, 0.45f, 0.12f, 0.7f), 0.65f);
            }
        }
    }
    if (Playtest && Playtest->GetSecondsSinceReportCopy() < 2.0f)
        DrawLabel(TEXT("PLAYTEST REPORT COPIED"), Center.X - 100.0f, Center.Y + 72.0f, FLinearColor(0.5f, 1.0f, 0.65f), 1.0f);
}

// --------------------------------------------------------------------------
// Bottom-left vitals band. BottomY is the band's bottom edge, so the band
// grows upward and stays glued to the screen bottom at any resolution.
// --------------------------------------------------------------------------
void ABreakerPlaytestHUD::DrawVitalsBand(const ABreakerCharacter* Character, float X, float BottomY)
{
    constexpr float BandW = 300.0f;
    constexpr float BandH = 82.0f;
    const float Y = BottomY - BandH;

    DrawPanel(X, Y, BandW, BandH, BreakerHUD::Cyan);

    // Movement state and speed collapse into one compact line: it is a
    // diagnostic, not a headline, so it sits above the pools at small scale.
    const FString MoveState = Character->IsMantling() ? TEXT("MANTLE")
        : Character->IsWallRiding() ? TEXT("WALL RIDE")
        : Character->IsSliding() ? TEXT("SLIDE")
        : Character->IsSprinting() ? TEXT("SPRINT") : TEXT("MOVE");
    DrawLabel(FString::Printf(TEXT("%s   %.0f u/s"), *MoveState, Character->GetHorizontalSpeed()),
        X + 12.0f, Y + 5.0f, BreakerHUD::Muted, 0.74f);

    if (const UBreakerAttributeSet* Attributes = Character->GetAttributes())
    {
        // Shield above health: shields are consumed first, so the stack must
        // deplete downward on screen (UI-UX-Spec §4.2).
        DrawBar(TEXT("SHIELD"), Attributes->GetShield(), Attributes->GetMaxShield(), X + 12.0f, Y + 24.0f, 190.0f, FLinearColor(0.08f, 0.65f, 1.0f));
        DrawBar(TEXT("HEALTH"), Attributes->GetHealth(), Attributes->GetMaxHealth(), X + 12.0f, Y + 52.0f, 190.0f, FLinearColor(0.9f, 0.18f, 0.14f));

        // Armour is a coefficient, not a pool: a number chip, never a bar.
        const float Armor = Attributes->GetArmor();
        const float Mitigation = Armor > 0.0f ? FMath::Min(Armor / (Armor + 100.0f), 0.8f) * 100.0f : 0.0f;
        constexpr float ChipW = 78.0f;
        const float ChipX = X + BandW - BreakerHUD::Gutter - ChipW;
        DrawRect(FLinearColor(0.03f, 0.05f, 0.08f, 0.95f), ChipX, Y + 24.0f, ChipW, 44.0f);
        DrawRect(FLinearColor(0.35f, 0.45f, 0.55f, 0.9f), ChipX, Y + 24.0f, ChipW, 2.0f);
        DrawLabel(TEXT("ARMOR"), ChipX + 8.0f, Y + 29.0f, BreakerHUD::Muted, 0.66f);
        DrawLabel(FString::Printf(TEXT("%.0f"), Armor), ChipX + 8.0f, Y + 42.0f, BreakerHUD::Bright, 1.25f);
        DrawLabel(FString::Printf(TEXT("%.0f%%"), Mitigation), ChipX + 46.0f, Y + 48.0f, BreakerHUD::Muted, 0.7f);
    }

    // Active statuses run inline just above the band.
    DrawStatusReadout(Character, X + 2.0f, Y - 16.0f);
}

// --------------------------------------------------------------------------
// Bottom-right cluster: resource strip on top, then weapon block on the left
// and the three ability squares on the right. Everything inside X..X+Width
// and Y..Y+Height with a uniform 8px gutter.
// --------------------------------------------------------------------------
void ABreakerPlaytestHUD::DrawCombatCluster(const ABreakerCharacter* Character, float X, float Y, float Width, float Height)
{
    constexpr float ResourceH = 20.0f;
    const float BodyY = Y + ResourceH + BreakerHUD::Gutter;
    const float BodyH = Height - ResourceH - BreakerHUD::Gutter;

    // --- Class resource strip, sitting directly above the cluster body ---
    {
        FString StateName = TEXT("NO RESOURCE");
        float Fraction = 0.0f;
        FLinearColor ResourceColor = BreakerHUD::Dim;
        if (const UBreakerMomentumComponent* Momentum = Character->GetMomentum(); Momentum && Momentum->IsActiveForOwner())
        {
            Fraction = FMath::Clamp(Momentum->GetMomentumFraction(), 0.0f, 1.0f);
            switch (Momentum->GetMomentumState())
            {
            case EBreakerMomentumState::Redline: StateName = TEXT("REDLINE"); ResourceColor = BreakerHUD::Orange; break;
            case EBreakerMomentumState::Running: StateName = TEXT("RUNNING"); ResourceColor = BreakerHUD::Gold; break;
            default:                            StateName = TEXT("SETTLED"); ResourceColor = BreakerHUD::Cyan; break;
            }
            StateName = FString::Printf(TEXT("MOMENTUM  %s"), *StateName);
        }
        DrawLabel(StateName, X, Y, ResourceColor, 0.74f);
        const float BarY = Y + 13.0f;
        DrawRect(FLinearColor(0.06f, 0.09f, 0.13f, 0.92f), X, BarY, Width, 6.0f);
        DrawRect(ResourceColor, X, BarY, Width * Fraction, 6.0f);
    }

    DrawPanel(X, BodyY, Width, BodyH, BreakerHUD::Orange);

    // --- Weapon block (left of the cluster body) ------------------------
    if (const UBreakerWeaponComponent* Weapon = Character->GetWeapon())
    {
        const int32 Slot = Weapon->GetCurrentSlot();
        DrawLabel(Weapon->GetArchetypeName().ToUpper(), X + 12.0f, BodyY + 8.0f, BreakerHUD::Bright, 0.8f);
        DrawChip(TEXT("1"), X + Width - BreakerHUD::Gutter - 38.0f, BodyY + 6.0f, 16.0f, 14.0f, BreakerHUD::Orange, Slot == 1);
        DrawChip(TEXT("2"), X + Width - BreakerHUD::Gutter - 18.0f, BodyY + 6.0f, 16.0f, 14.0f, BreakerHUD::Orange, Slot == 2);

        // Magazine dominates; reserve is deliberately subordinate.
        DrawLabel(FString::Printf(TEXT("%02d"), Weapon->GetMagazineAmmo()), X + 12.0f, BodyY + 24.0f, BreakerHUD::Bright, 2.4f);
        DrawLabel(FString::Printf(TEXT("/ %d"), Weapon->GetReserveAmmo()), X + 70.0f, BodyY + 44.0f, BreakerHUD::Muted, 0.95f);

        const FString StateText = Weapon->IsReloading() ? TEXT("RELOADING")
            : Weapon->IsSwapping() ? TEXT("SWAPPING")
            : Weapon->IsAiming() ? TEXT("ADS") : FString();
        if (!StateText.IsEmpty())
            DrawLabel(StateText, X + 12.0f, BodyY + 58.0f, BreakerHUD::Cyan, 0.78f);
    }

    // --- Three ability squares (right of the cluster body) ---------------
    const UBreakerAbilityComponent* Abilities = Character->GetAbilities();
    constexpr float SlotSize = 52.0f;
    const float SlotY = Y + Height - SlotSize - BreakerHUD::Gutter;
    const float SlotsX = X + Width - BreakerHUD::Gutter - (SlotSize * 3.0f + BreakerHUD::Gutter * 2.0f);
    DrawAbilitySlot(Abilities, EBreakerAbilitySlot::ClassAbilityOne, TEXT("E"), SlotsX, SlotY, SlotSize, BreakerHUD::Cyan);
    DrawAbilitySlot(Abilities, EBreakerAbilitySlot::ClassAbilityTwo, TEXT("T"), SlotsX + SlotSize + BreakerHUD::Gutter, SlotY, SlotSize, BreakerHUD::Cyan);
    DrawAbilitySlot(Abilities, EBreakerAbilitySlot::Ultimate, TEXT("G"), SlotsX + (SlotSize + BreakerHUD::Gutter) * 2.0f, SlotY, SlotSize, BreakerHUD::Violet);
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
                const FLinearColor TracerColor(1.0f, 0.95f, 0.82f, Fade);
                DrawLine(StartProj.X, StartProj.Y, EndProj.X, EndProj.Y, TracerColor, 1.25f);
            }
        }

        if (Tracer.bHit && Age < BreakerHUD::ImpactLifetime)
        {
            const FVector ImpactProj = Project(Tracer.Impact, false);
            if (ImpactProj.Z > 0.0f)
            {
                const float Fade = 1.0f - Age / BreakerHUD::ImpactLifetime;
                const FLinearColor Base = Tracer.bWeakPoint ? BreakerHUD::Gold : FLinearColor(0.55f, 0.88f, 1.0f);
                const FLinearColor CrossColor(Base.R, Base.G, Base.B, Fade);
                constexpr float Arm = 4.0f;
                DrawLine(ImpactProj.X - Arm, ImpactProj.Y - Arm, ImpactProj.X + Arm, ImpactProj.Y + Arm, CrossColor, 1.5f);
                DrawLine(ImpactProj.X - Arm, ImpactProj.Y + Arm, ImpactProj.X + Arm, ImpactProj.Y - Arm, CrossColor, 1.5f);
            }
        }
    }
}

// --------------------------------------------------------------------------
// Floating damage numbers. Rise and fade at the world impact point.
// --------------------------------------------------------------------------
void ABreakerPlaytestHUD::DrawDamageNumbers()
{
    if (DamageNumbers.Num() == 0) return;
    const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;

    for (const FBreakerHUDDamageNumber& Number : DamageNumbers)
    {
        const float Age = static_cast<float>(Now - Number.Time);
        if (Age < 0.0f || Age >= BreakerHUD::DamageNumberLifetime) continue;

        const FVector Projected = Project(Number.World, false);
        if (Projected.Z <= 0.0f) continue;

        const float Alpha01 = Age / BreakerHUD::DamageNumberLifetime;
        // Ease-out rise: fast off the impact, settling as it fades.
        const float Rise = BreakerHUD::DamageNumberRise * (1.0f - FMath::Square(1.0f - Alpha01));
        const float Fade = 1.0f - FMath::Square(Alpha01);

        FLinearColor Face = FLinearColor(0.94f, 0.96f, 0.98f);
        float Scale = 0.95f;
        if (Number.bCritical) { Face = FLinearColor(1.0f, 0.52f, 0.16f); Scale = 1.35f; }
        else if (Number.bWeakPoint) { Face = BreakerHUD::Gold; Scale = 1.12f; }

        DrawBoldLabel(FString::Printf(TEXT("%.0f"), Number.Value),
            Projected.X - 8.0f, Projected.Y - Rise, Face, Scale, Fade);
    }
}

// --------------------------------------------------------------------------
// Enemy health bars. Shown near, or for six seconds after they were hit, so
// a distant sniped target still confirms the hit landed.
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
        const float DistanceScale = FMath::Lerp(1.0f, 0.6f, DistanceAlpha);
        const bool bElite = Enemy->IsElite();
        const float BarW = (bElite ? 76.0f : 60.0f) * DistanceScale;
        const float BarH = 6.0f * DistanceScale;
        const float BarX = Projected.X - BarW * 0.5f;
        const float BarY = Projected.Y;

        const float Shield = EnemyAttributes->GetShield();
        const float MaxShield = EnemyAttributes->GetMaxShield();
        if (Shield > 0.0f && MaxShield > UE_SMALL_NUMBER)
        {
            const float ShieldH = FMath::Max(BarH * 0.45f, 2.0f);
            const float ShieldY = BarY - ShieldH - 1.0f;
            DrawRect(FLinearColor(0.02f, 0.04f, 0.07f, 0.8f), BarX, ShieldY, BarW, ShieldH);
            DrawRect(FLinearColor(0.08f, 0.65f, 1.0f, 0.95f), BarX, ShieldY, BarW * FMath::Clamp(Shield / MaxShield, 0.0f, 1.0f), ShieldH);
        }

        DrawRect(FLinearColor(0.02f, 0.03f, 0.05f, 0.8f), BarX, BarY, BarW, BarH);
        DrawRect(FLinearColor(0.88f, 0.16f, 0.13f, 0.95f), BarX, BarY, BarW * FMath::Clamp(Health / MaxHealth, 0.0f, 1.0f), BarH);
        if (bElite)
        {
            // Gold edge, not a gold fill: the health colour must stay readable.
            const FLinearColor EliteEdge(BreakerHUD::Gold.R, BreakerHUD::Gold.G, BreakerHUD::Gold.B, 0.95f);
            DrawRect(EliteEdge, BarX, BarY - 1.0f, BarW, 1.0f);
            DrawRect(EliteEdge, BarX, BarY + BarH, BarW, 1.0f);
        }
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
        const FLinearColor Accent = BreakerHUD::RarityColor(Item.Rarity);

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

        const FString Label = Pickup->GetDisplayLabel().ToString();
        const float ChipW = 16.0f + Label.Len() * 5.5f;
        DrawRect(FLinearColor(0.02f, 0.03f, 0.05f, 0.78f), Projected.X - ChipW * 0.5f, Projected.Y - 8.0f, ChipW, 16.0f);
        DrawRect(FLinearColor(Accent.R, Accent.G, Accent.B, 0.9f), Projected.X - ChipW * 0.5f, Projected.Y - 8.0f, 2.0f, 16.0f);
        DrawLabel(Label, Projected.X - ChipW * 0.5f + 8.0f, Projected.Y - 6.0f, Accent, 0.76f);
    }

    if (Focused)
    {
        const FVector Projected = Project(Focused->GetActorLocation() + FVector(0.0f, 0.0f, 50.0f), false);
        if (Projected.Z <= 0.0f) return;

        const FBreakerItemInstance& Item = Focused->GetItem();
        const FLinearColor Accent = BreakerHUD::RarityColor(Item.Rarity);
        const TArray<FString> AffixLines = BreakerHUD::DescribeItemLines(Item);

        constexpr float PanelW = 260.0f;
        const float PanelH = 52.0f + AffixLines.Num() * 14.0f + 20.0f;
        const float PanelX = FMath::Clamp(Projected.X - PanelW * 0.5f, 8.0f, Canvas->ClipX - PanelW - 8.0f);
        const float PanelY = FMath::Clamp(Projected.Y - PanelH - 12.0f, 8.0f, Canvas->ClipY - PanelH - 8.0f);

        DrawPanel(PanelX, PanelY, PanelW, PanelH, Accent);
        DrawLabel(Focused->GetDisplayLabel().ToString().ToUpper(), PanelX + 12.0f, PanelY + 8.0f, Accent, 1.0f);
        DrawLabel(FString::Printf(TEXT("ITEM LEVEL %d"), Item.ItemLevel), PanelX + 12.0f, PanelY + 26.0f, BreakerHUD::Muted, 0.74f);

        float LineY = PanelY + 44.0f;
        for (const FString& Line : AffixLines)
        {
            DrawLabel(Line, PanelX + 12.0f, LineY, BreakerHUD::Bright, 0.72f);
            LineY += 14.0f;
        }
        DrawLabel(TEXT("F  PICK UP"), PanelX + 12.0f, LineY + 4.0f, BreakerHUD::Cyan, 0.8f);
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
    const FLinearColor Color = bShowDodge ? FLinearColor(0.35f, 0.95f, 1.0f, Fade) : FLinearColor(1.0f, 0.55f, 0.12f, Fade);
    DrawLabel(bShowDodge ? TEXT("DODGED") : TEXT("BLOCKED"), Center.X - 44.0f, Center.Y - 108.0f, Color, 1.25f);
}

void ABreakerPlaytestHUD::DrawStatusReadout(const ABreakerCharacter* Character, float X, float Y)
{
    const UBreakerStatusComponent* Status = Character ? Character->FindComponentByClass<UBreakerStatusComponent>() : nullptr;
    if (!Status) return;

    // Statuses run inline left-to-right so they never push the vitals band up.
    const TArray<FBreakerActiveStatus>& ActiveStatuses = Status->GetActiveStatuses();
    float ChipX = X;
    for (const FBreakerActiveStatus& Active : ActiveStatuses)
    {
        FString ShortName = Active.Spec.StatusTag.IsValid() ? Active.Spec.StatusTag.GetTagName().ToString() : TEXT("STATUS");
        int32 SeparatorIndex = INDEX_NONE;
        if (ShortName.FindLastChar(TEXT('.'), SeparatorIndex)) ShortName = ShortName.RightChop(SeparatorIndex + 1);
        const FString Text = FString::Printf(TEXT("%s x%d %.1fs"), *ShortName.ToUpper(), Active.Stacks, FMath::Max(Active.RemainingDuration, 0.0f));
        const float ChipW = 14.0f + Text.Len() * 5.0f;
        DrawRect(FLinearColor(0.05f, 0.03f, 0.05f, 0.85f), ChipX, Y - 1.0f, ChipW, 14.0f);
        DrawLabel(Text, ChipX + 6.0f, Y + 1.0f, FLinearColor(1.0f, 0.45f, 0.55f), 0.7f);
        ChipX += ChipW + 4.0f;
    }
}

void ABreakerPlaytestHUD::DrawCrosshair(const FVector2D& Center, const FLinearColor& Color, float Size, float Thickness)
{
    DrawLine(Center.X - Size, Center.Y, Center.X + Size, Center.Y, Color, Thickness);
    DrawLine(Center.X, Center.Y - Size, Center.X, Center.Y + Size, Color, Thickness);
}

void ABreakerPlaytestHUD::DrawLabel(const FString& Text, float X, float Y, const FLinearColor& Color, float Scale)
{
    DrawText(Text, Color, X, Y, GEngine ? GEngine->GetSmallFont() : nullptr, Scale, false);
}

void ABreakerPlaytestHUD::DrawBoldLabel(const FString& Text, float X, float Y, const FLinearColor& Face, float Scale, float Alpha)
{
    if (Alpha <= 0.0f) return;
    // Four 1px offsets in near-black at low alpha read as a hairline outline;
    // one heavier +1/+1 pass gives the glyph weight without a bold typeface.
    const FLinearColor Outline(0.02f, 0.02f, 0.03f, 0.45f * Alpha);
    DrawLabel(Text, X - 1.0f, Y, Outline, Scale);
    DrawLabel(Text, X + 1.0f, Y, Outline, Scale);
    DrawLabel(Text, X, Y - 1.0f, Outline, Scale);
    DrawLabel(Text, X, Y + 1.0f, Outline, Scale);
    DrawLabel(Text, X + 1.0f, Y + 1.0f, FLinearColor(0.01f, 0.01f, 0.02f, 0.75f * Alpha), Scale);
    DrawLabel(Text, X, Y, FLinearColor(Face.R, Face.G, Face.B, Alpha), Scale);
}

void ABreakerPlaytestHUD::DrawPanel(float X, float Y, float Width, float Height, const FLinearColor& Accent)
{
    DrawRect(BreakerHUD::Ink, X, Y, Width, Height);
    DrawRect(Accent, X, Y, 4.0f, Height);
    DrawRect(FLinearColor(0.16f, 0.24f, 0.32f, 0.65f), X + 4.0f, Y, Width - 4.0f, 1.0f);
}

void ABreakerPlaytestHUD::DrawBar(const FString& Label, float Value, float Maximum, float X, float Y, float Width, const FLinearColor& Color)
{
    const float Ratio = Maximum > UE_SMALL_NUMBER ? FMath::Clamp(Value / Maximum, 0.0f, 1.0f) : 0.0f;
    DrawLabel(FString::Printf(TEXT("%s  %.0f / %.0f"), *Label, Value, Maximum), X, Y - 2.0f, FLinearColor(0.82f, 0.88f, 0.94f), 0.72f);
    DrawRect(FLinearColor(0.08f, 0.11f, 0.15f, 0.95f), X, Y + 13.0f, Width, 7.0f);
    DrawRect(Color, X, Y + 13.0f, Width * Ratio, 7.0f);
}

void ABreakerPlaytestHUD::DrawChip(const FString& Text, float X, float Y, float Width, float Height, const FLinearColor& Accent, bool bFilled)
{
    DrawRect(bFilled ? Accent : FLinearColor(0.06f, 0.09f, 0.13f, 0.9f), X, Y, Width, Height);
    if (!bFilled) DrawRect(FLinearColor(Accent.R, Accent.G, Accent.B, 0.45f), X, Y, Width, 1.0f);
    DrawLabel(Text, X + Width * 0.5f - 3.0f, Y + 2.0f, bFilled ? FLinearColor(0.05f, 0.05f, 0.06f) : BreakerHUD::Muted, 0.7f);
}

void ABreakerPlaytestHUD::DrawAbilitySlot(const UBreakerAbilityComponent* Abilities, EBreakerAbilitySlot Slot, const FString& KeyHint, float X, float Y, float Size, const FLinearColor& Accent)
{
    const bool bGranted = Abilities && Abilities->IsSlotGranted(Slot);
    const float Remaining = bGranted ? Abilities->GetCooldownRemaining(Slot) : 0.0f;
    const float Duration = bGranted ? Abilities->GetCooldownDuration(Slot) : 0.0f;
    const bool bOnCooldown = bGranted && Remaining > 0.0f && Duration > UE_SMALL_NUMBER;
    const bool bAffordable = bGranted && Abilities->CanAffordSlot(Slot);
    // Cost gating dims the same way a cooldown desaturates, so "cannot use"
    // reads identically whatever the reason.
    const float Brightness = !bGranted ? 0.35f : (bOnCooldown || !bAffordable) ? 0.55f : 1.0f;

    DrawRect(FLinearColor(0.025f, 0.04f, 0.065f, 0.96f), X, Y, Size, Size);

    // Cooldown sweep fills left-to-right as the ability comes back.
    if (bOnCooldown)
    {
        const float Ready = FMath::Clamp(1.0f - Remaining / Duration, 0.0f, 1.0f);
        DrawRect(FLinearColor(Accent.R * 0.35f, Accent.G * 0.35f, Accent.B * 0.35f, 0.85f), X, Y, Size * Ready, Size);
    }
    DrawRect(FLinearColor(Accent.R * Brightness, Accent.G * Brightness, Accent.B * Brightness, 1.0f), X, Y, Size, 3.0f);

    DrawLabel(KeyHint, X + 5.0f, Y + 5.0f, FLinearColor(Accent.R, Accent.G, Accent.B, bGranted ? 1.0f : 0.5f), 0.9f);

    // Short name from the definition where one exists; the id otherwise, so a
    // designed-but-unbuilt slot still labels itself.
    FString ShortName = TEXT("--");
    if (bGranted)
    {
        if (const UBreakerAbilityDefinition* Definition = Abilities->GetDefinitionForSlot(Slot))
        {
            ShortName = Definition->DisplayName.IsEmpty() ? Definition->AbilityId.ToString() : Definition->DisplayName.ToString();
        }
        ShortName = ShortName.Left(6).ToUpper();
    }
    DrawLabel(ShortName, X + 5.0f, Y + Size - 14.0f, FLinearColor(0.7f, 0.78f, 0.86f, Brightness), 0.62f);

    if (bOnCooldown)
        DrawLabel(FString::Printf(TEXT("%.1f"), Remaining), X + Size - 22.0f, Y + Size * 0.5f - 6.0f, BreakerHUD::Bright, 0.85f);
    else if (bGranted && !bAffordable)
        DrawLabel(FString::Printf(TEXT("%.0f"), Abilities->GetCost(Slot)), X + Size - 20.0f, Y + Size * 0.5f - 6.0f, FLinearColor(1.0f, 0.35f, 0.3f), 0.8f);
}
