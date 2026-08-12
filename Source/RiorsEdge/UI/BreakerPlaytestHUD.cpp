#include "UI/BreakerPlaytestHUD.h"

#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Weapons/BreakerWeaponComponent.h"
#include "Playtest/BreakerPlaytestComponent.h"
#include "Combat/BreakerTargetDummy.h"
#include "Combat/BreakerEnemy.h"
#include "Combat/BreakerCombatComponent.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"

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
    if (Character->IsMenuOpen()) return;

    const UBreakerWeaponComponent* Weapon = Character->GetWeapon();
    const UBreakerAttributeSet* Attributes = Character->GetAttributes();
    const UBreakerPlaytestComponent* Playtest = Character->GetPlaytest();
    const bool bRecentShot = Weapon && Weapon->GetSecondsSinceLastShot() < 0.14f;
    const FBreakerShotResult* Shot = Weapon ? &Weapon->GetLastShot() : nullptr;
    FLinearColor CrosshairColor = FLinearColor::White;
    if (bRecentShot && Shot && Shot->bHit) CrosshairColor = Shot->bWeakPoint ? FLinearColor(1.0f, 0.75f, 0.05f) : FLinearColor::Red;
    const bool bAiming = Weapon && Weapon->IsAiming();
    const float RestingCrosshairSize = bAiming ? 4.0f : 8.0f;
    DrawCrosshair(Center, CrosshairColor, bRecentShot ? 12.0f : RestingCrosshairSize, bRecentShot ? 2.5f : 1.5f);

    const FString MoveState = Character->IsMantling() ? TEXT("MANTLE") : Character->IsWallRiding() ? TEXT("WALL RIDE") : Character->IsSliding() ? TEXT("SLIDE") : Character->IsSprinting() ? TEXT("SPRINT") : TEXT("MOVE");
    const float LeftPanelX = 24.0f;
    const float PanelY = Canvas->ClipY - 126.0f;
    DrawPanel(LeftPanelX, PanelY, 340.0f, 102.0f, FLinearColor(0.1f, 0.75f, 1.0f));
    DrawLabel(FString::Printf(TEXT("%s   SPEED %.0f"), *MoveState, Character->GetHorizontalSpeed()), LeftPanelX + 16.0f, PanelY + 10.0f, FLinearColor::White, 0.9f);
    if (Attributes)
    {
        DrawBar(TEXT("HEALTH"), Attributes->GetHealth(), Attributes->GetMaxHealth(), LeftPanelX + 16.0f, PanelY + 38.0f, 205.0f, FLinearColor(0.9f, 0.18f, 0.14f));
        DrawBar(TEXT("SHIELD"), Attributes->GetShield(), Attributes->GetMaxShield(), LeftPanelX + 16.0f, PanelY + 68.0f, 205.0f, FLinearColor(0.08f, 0.65f, 1.0f));
        DrawLabel(FString::Printf(TEXT("ARMOR\n%.0f"), Attributes->GetArmor()), LeftPanelX + 258.0f, PanelY + 42.0f, FLinearColor(0.85f, 0.9f, 0.95f), 0.92f);
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
    if (Weapon)
    {
        const FString WeaponState = Weapon->IsReloading() ? TEXT("  RELOADING") : bAiming ? TEXT("  AIM") : TEXT("");
        const float WeaponPanelX = LeftPanelX + 352.0f;
        DrawPanel(WeaponPanelX, PanelY, 270.0f, 102.0f, FLinearColor(1.0f, 0.45f, 0.12f));
        DrawLabel(FString::Printf(TEXT("SLOT %d   %s%s"), Weapon->GetCurrentSlot(), *Weapon->GetArchetypeName(), *WeaponState), WeaponPanelX + 18.0f, PanelY + 12.0f, FLinearColor(0.65f, 0.82f, 0.92f), 0.9f);
        DrawLabel(FString::Printf(TEXT("%02d"), Weapon->GetMagazineAmmo()), WeaponPanelX + 18.0f, PanelY + 38.0f, FLinearColor::White, 2.0f);
        DrawLabel(FString::Printf(TEXT("/ %03d"), Weapon->GetReserveAmmo()), WeaponPanelX + 88.0f, PanelY + 58.0f, FLinearColor(0.65f, 0.72f, 0.8f), 1.0f);
        DrawLabel(TEXT("1  PRIMARY\n2  SECONDARY"), WeaponPanelX + 158.0f, PanelY + 47.0f, FLinearColor(0.65f, 0.72f, 0.8f), 0.72f);
        if (Weapon->IsReloading())
            DrawLabel(TEXT("RELOADING"), Center.X - 54.0f, Center.Y + 48.0f, FLinearColor(0.55f, 0.9f, 1.0f), 1.1f);
    }
    const float AbilityPanelX = LeftPanelX + 634.0f;
    DrawPanel(AbilityPanelX, PanelY, 390.0f, 102.0f, FLinearColor(0.68f, 0.38f, 1.0f));
    DrawLabel(TEXT("CLASS ABILITIES"), AbilityPanelX + 14.0f, PanelY + 10.0f, FLinearColor(0.72f, 0.66f, 0.92f), 0.78f);
    DrawAbilitySlot(TEXT("ABILITY 1"), AbilityPanelX + 14.0f, PanelY + 34.0f, FLinearColor(0.18f, 0.7f, 1.0f));
    DrawAbilitySlot(TEXT("ABILITY 2"), AbilityPanelX + 136.0f, PanelY + 34.0f, FLinearColor(0.18f, 0.7f, 1.0f));
    DrawAbilitySlot(TEXT("ULTIMATE"), AbilityPanelX + 258.0f, PanelY + 34.0f, FLinearColor(0.75f, 0.35f, 1.0f));
    DrawLabel(TEXT("F1 RESET     F2 REPORT     F3 DIAGNOSTICS     ESC MENU"), 24.0f, 24.0f, FLinearColor(0.58f, 0.68f, 0.76f), 0.76f);

    const float AppliedDamage = Shot ? Shot->DamageResult.ShieldDamage + Shot->DamageResult.HealthDamage : 0.0f;
    if (bRecentShot && Shot && Shot->bHit && AppliedDamage > 0.0f)
    {
        const FString DamageText = Shot->bWeakPoint
            ? FString::Printf(TEXT("WEAK POINT  %.0f"), AppliedDamage)
            : FString::Printf(TEXT("%.0f"), AppliedDamage);
        DrawLabel(DamageText, Center.X + 24.0f, Center.Y + 18.0f, CrosshairColor, 1.1f);
    }

    if (Playtest && Playtest->AreDiagnosticsVisible())
    {
        const FBreakerPlaytestStats& Stats = Playtest->GetStats();
        const float FPS = GetWorld() && GetWorld()->GetDeltaSeconds() > UE_SMALL_NUMBER ? 1.0f / GetWorld()->GetDeltaSeconds() : 0.0f;
        DrawLabel(FString::Printf(TEXT("FPS %.0f | FOV %.0f | SENS %.1f"), FPS, Character->GetCurrentFOV(), Character->GetLookSensitivity()), Canvas->ClipX - 250.0f, 24.0f, FLinearColor(0.5f, 1.0f, 0.65f), 0.9f);
        DrawLabel(FString::Printf(TEXT("SHOTS %d | ACC %.1f%% | WEAK %.1f%% | DMG %.0f | RELOADS %d"), Stats.ShotsFired, Stats.Accuracy(), Stats.WeakPointRate(), Stats.DamageDealt, Stats.Reloads), Canvas->ClipX - 500.0f, 48.0f, FLinearColor(0.5f, 1.0f, 0.65f), 0.82f);

        if (Weapon && bRecentShot && Shot && Shot->bHit)
        {
            FVector2D ImpactScreen;
            if (PlayerOwner && PlayerOwner->ProjectWorldLocationToScreen(Shot->TraceEnd, ImpactScreen))
            {
                const FLinearColor ImpactColor = Shot->bWeakPoint ? FLinearColor(1.0f, 0.75f, 0.05f) : FLinearColor(0.3f, 0.8f, 1.0f);
                DrawLine(ImpactScreen.X - 5.0f, ImpactScreen.Y - 5.0f, ImpactScreen.X + 5.0f, ImpactScreen.Y + 5.0f, ImpactColor, 1.5f);
                DrawLine(ImpactScreen.X - 5.0f, ImpactScreen.Y + 5.0f, ImpactScreen.X + 5.0f, ImpactScreen.Y - 5.0f, ImpactColor, 1.5f);
            }
        }

        for (TActorIterator<ABreakerTargetDummy> It(GetWorld()); It; ++It)
        {
            FVector2D Screen;
            if (PlayerOwner && PlayerOwner->ProjectWorldLocationToScreen(It->GetActorLocation() + FVector(0.0f, 0.0f, 130.0f), Screen))
            {
                const float DistanceMeters = FVector::Distance(Character->GetActorLocation(), It->GetActorLocation()) / 100.0f;
                DrawLabel(FString::Printf(TEXT("%s  %.0fm"), *It->GetProfileLabel(), DistanceMeters), Screen.X - 34.0f, Screen.Y, FLinearColor(0.8f, 0.9f, 1.0f), 0.75f);
            }
        }
        for (TActorIterator<ABreakerEnemy> It(GetWorld()); It; ++It)
        {
            FVector2D Screen;
            if (PlayerOwner && PlayerOwner->ProjectWorldLocationToScreen(It->GetActorLocation() + FVector(0.0f, 0.0f, 130.0f), Screen))
            {
                DrawLabel(FString::Printf(TEXT("ENEMY  %s"), *It->GetEnemyStateLabel()), Screen.X - 48.0f, Screen.Y, FLinearColor(1.0f, 0.45f, 0.2f), 0.75f);
            }
        }
    }
    if (Playtest && Playtest->GetSecondsSinceReportCopy() < 2.0f)
        DrawLabel(TEXT("PLAYTEST REPORT COPIED"), Center.X - 100.0f, Center.Y + 72.0f, FLinearColor(0.5f, 1.0f, 0.65f), 1.0f);
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

void ABreakerPlaytestHUD::DrawPanel(float X, float Y, float Width, float Height, const FLinearColor& Accent)
{
    DrawRect(FLinearColor(0.012f, 0.022f, 0.038f, 0.86f), X, Y, Width, Height);
    DrawRect(Accent, X, Y, 4.0f, Height);
    DrawRect(FLinearColor(0.16f, 0.24f, 0.32f, 0.65f), X + 4.0f, Y, Width - 4.0f, 1.0f);
}

void ABreakerPlaytestHUD::DrawBar(const FString& Label, float Value, float Maximum, float X, float Y, float Width, const FLinearColor& Color)
{
    const float Ratio = Maximum > UE_SMALL_NUMBER ? FMath::Clamp(Value / Maximum, 0.0f, 1.0f) : 0.0f;
    DrawLabel(FString::Printf(TEXT("%s  %.0f / %.0f"), *Label, Value, Maximum), X, Y - 2.0f, FLinearColor(0.82f, 0.88f, 0.94f), 0.72f);
    DrawRect(FLinearColor(0.08f, 0.11f, 0.15f, 0.95f), X, Y + 15.0f, Width, 7.0f);
    DrawRect(Color, X, Y + 15.0f, Width * Ratio, 7.0f);
}

void ABreakerPlaytestHUD::DrawAbilitySlot(const FString& Label, float X, float Y, const FLinearColor& Accent)
{
    DrawRect(FLinearColor(0.025f, 0.04f, 0.065f, 0.98f), X, Y, 112.0f, 54.0f);
    DrawRect(Accent, X, Y, 112.0f, 3.0f);
    DrawLabel(Label, X + 9.0f, Y + 10.0f, FLinearColor(0.78f, 0.84f, 0.9f), 0.68f);
    DrawLabel(TEXT("--"), X + 48.0f, Y + 28.0f, FLinearColor(0.42f, 0.5f, 0.6f), 0.9f);
}
