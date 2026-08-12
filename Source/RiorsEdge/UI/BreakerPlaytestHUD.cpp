#include "UI/BreakerPlaytestHUD.h"

#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Weapons/BreakerWeaponComponent.h"

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
    DrawLabel(TEXT("WASD Move  |  Shift Sprint  |  Q Dash  |  C/Ctrl Slide  |  Space Jump/Wall Jump"), 24.0f, 24.0f, FLinearColor(0.75f, 0.8f, 0.85f), 0.85f);
    DrawLabel(TEXT("LMB Fire  |  RMB Aim  |  R Reload   -   Red hit / Gold weak point"), 24.0f, 47.0f, FLinearColor(0.75f, 0.8f, 0.85f), 0.85f);

    const float AppliedDamage = Shot ? Shot->DamageResult.ShieldDamage + Shot->DamageResult.HealthDamage : 0.0f;
    if (bRecentShot && Shot && Shot->bHit && AppliedDamage > 0.0f)
    {
        const FString DamageText = Shot->bWeakPoint
            ? FString::Printf(TEXT("WEAK POINT  %.0f"), AppliedDamage)
            : FString::Printf(TEXT("%.0f"), AppliedDamage);
        DrawLabel(DamageText, Center.X + 24.0f, Center.Y + 18.0f, CrosshairColor, 1.1f);
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
