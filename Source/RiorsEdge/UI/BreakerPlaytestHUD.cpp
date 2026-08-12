#include "UI/BreakerPlaytestHUD.h"

#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Weapons/BreakerWeaponComponent.h"
#include "Playtest/BreakerPlaytestComponent.h"
#include "Combat/BreakerTargetDummy.h"
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

    const UBreakerWeaponComponent* Weapon = Character->GetWeapon();
    const UBreakerAttributeSet* Attributes = Character->GetAttributes();
    const UBreakerPlaytestComponent* Playtest = Character->GetPlaytest();
    const bool bRecentShot = Weapon && Weapon->GetSecondsSinceLastShot() < 0.14f;
    const FBreakerShotResult* Shot = Weapon ? &Weapon->GetLastShot() : nullptr;
    FLinearColor CrosshairColor = FLinearColor::White;
    if (bRecentShot && Shot && Shot->bHit) CrosshairColor = Shot->bWeakPoint ? FLinearColor(1.0f, 0.75f, 0.05f) : FLinearColor::Red;
    DrawCrosshair(Center, CrosshairColor, bRecentShot ? 12.0f : 8.0f, bRecentShot ? 2.5f : 1.5f);

    const FString MoveState = Character->IsWallRiding() ? TEXT("WALL RIDE") : Character->IsSliding() ? TEXT("SLIDE") : Character->IsSprinting() ? TEXT("SPRINT") : TEXT("MOVE");
    DrawLabel(FString::Printf(TEXT("%s  |  SPEED %.0f"), *MoveState, Character->GetHorizontalSpeed()), 32.0f, Canvas->ClipY - 72.0f, FLinearColor::White);
    if (Attributes)
    {
        DrawLabel(FString::Printf(TEXT("HEALTH %.0f / %.0f   SHIELD %.0f / %.0f"), Attributes->GetHealth(), Attributes->GetMaxHealth(), Attributes->GetShield(), Attributes->GetMaxShield()), 32.0f, Canvas->ClipY - 42.0f, FLinearColor(0.55f, 0.9f, 1.0f));
    }
    if (Weapon)
    {
        const FString Reload = Weapon->IsReloading() ? TEXT("  RELOADING") : TEXT("");
        DrawLabel(FString::Printf(TEXT("%d / %d%s"), Weapon->GetMagazineAmmo(), Weapon->GetReserveAmmo(), *Reload), Canvas->ClipX - 190.0f, Canvas->ClipY - 50.0f, FLinearColor::White, 1.2f);
    }
    DrawLabel(TEXT("WASD Move | Shift Sprint | Q Dash | C/Ctrl Slide | Space Jump/Wall Jump"), 24.0f, 24.0f, FLinearColor(0.75f, 0.8f, 0.85f), 0.82f);
    DrawLabel(TEXT("LMB Fire | RMB Aim | R Reload | F1 Reset | F2 Copy Report | F3 Diagnostics"), 24.0f, 46.0f, FLinearColor(0.75f, 0.8f, 0.85f), 0.82f);
    DrawLabel(TEXT("[ / ] FOV | - / = Sensitivity"), 24.0f, 68.0f, FLinearColor(0.75f, 0.8f, 0.85f), 0.82f);

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

        if (Weapon && bRecentShot)
        {
            FVector2D StartScreen;
            FVector2D EndScreen;
            if (PlayerOwner && PlayerOwner->ProjectWorldLocationToScreen(Shot->TraceStart, StartScreen) && PlayerOwner->ProjectWorldLocationToScreen(Shot->TraceEnd, EndScreen))
                DrawLine(StartScreen.X, StartScreen.Y, EndScreen.X, EndScreen.Y, FLinearColor(0.3f, 0.8f, 1.0f, 0.65f), 1.0f);
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
