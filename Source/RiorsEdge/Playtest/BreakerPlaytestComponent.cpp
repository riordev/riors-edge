#include "Playtest/BreakerPlaytestComponent.h"

#include "Characters/BreakerCharacter.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Weapons/BreakerWeaponComponent.h"

UBreakerPlaytestComponent::UBreakerPlaytestComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
}

void UBreakerPlaytestComponent::BeginPlay()
{
    Super::BeginPlay();
    if (const ABreakerCharacter* Character = Cast<ABreakerCharacter>(GetOwner()))
    {
        if (UBreakerWeaponComponent* Weapon = Character->GetWeapon())
        {
            Weapon->OnShot.AddDynamic(this, &ThisClass::HandleShot);
            Weapon->OnReloadChanged.AddDynamic(this, &ThisClass::HandleReload);
        }
    }
}

void UBreakerPlaytestComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    Stats.SessionSeconds += DeltaTime;
}

void UBreakerPlaytestComponent::HandleShot(const FBreakerShotResult& Shot)
{
    if (!Shot.bFired) return;
    ++Stats.ShotsFired;
    if (Shot.bHit) ++Stats.Hits;
    if (Shot.bWeakPoint) ++Stats.WeakPointHits;
    Stats.DamageDealt += Shot.DamageResult.ShieldDamage + Shot.DamageResult.HealthDamage;
}

void UBreakerPlaytestComponent::HandleReload(bool bReloading)
{
    if (bReloading) ++Stats.Reloads;
}

void UBreakerPlaytestComponent::ResetStats()
{
    Stats = FBreakerPlaytestStats();
}

FString UBreakerPlaytestComponent::BuildReport() const
{
    return FString::Printf(
        TEXT("Rior's Edge Playtest Report\nDuration: %.1f minutes\nShots: %d\nHits: %d\nAccuracy: %.1f%%\nWeak-point hits: %d\nWeak-point rate: %.1f%%\nDamage dealt: %.0f\nReloads: %d\n\nMovement notes:\n- Walk/stopping:\n- Sprint:\n- Dash:\n- Slide:\n- Wall ride/jump:\n\nWeapon notes:\n- Hip fire / aim:\n- Cadence / reload:\n- Weak points / falloff:\n\nDefects or discomfort:\n- "),
        Stats.SessionSeconds / 60.0f, Stats.ShotsFired, Stats.Hits, Stats.Accuracy(), Stats.WeakPointHits,
        Stats.WeakPointRate(), Stats.DamageDealt, Stats.Reloads);
}

void UBreakerPlaytestComponent::CopyReportToClipboard() const
{
    FPlatformApplicationMisc::ClipboardCopy(*BuildReport());
    LastReportCopyTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
}

float UBreakerPlaytestComponent::GetSecondsSinceReportCopy() const
{
    return GetWorld() ? static_cast<float>(GetWorld()->GetTimeSeconds() - LastReportCopyTime) : BIG_NUMBER;
}
