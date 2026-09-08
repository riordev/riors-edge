#include "Classes/BreakerCasterStatusRules.h"
#include "Combat/BreakerCombatTypes.h"
#include "Classes/BreakerManaComponent.h"
#include "Combat/BreakerStatusRules.h"
#include "Combat/BreakerZoneActor.h"
#include "Combat/BreakerZoneMath.h"
#include "Progression/BreakerProgressionComponent.h"
#include "GameFramework/Actor.h"

void BreakerCasterStatusRules::SnapshotPhysicalApplication(FBreakerStatusApplicationSpec& Spec, EBreakerDamageFamily Family, AActor* Source)
{
    if (Spec.bHasCasterApplicationSnapshot) return;
    Spec.bHasCasterApplicationSnapshot = true;
    const auto* Rule = BreakerStatusRules::FindRule(Spec.StatusTag);
    if (Family != EBreakerDamageFamily::Physical || !Rule || !Rule->bDealsPeriodicDamage
        || !FMath::IsFinite(Spec.BaseDamagePerTick) || Spec.BaseDamagePerTick <= 0
        || !FMath::IsFinite(Spec.TickInterval) || Spec.TickInterval <= 0 || !IsValid(Source)) return;
    const auto* Mana = Source->FindComponentByClass<UBreakerManaComponent>();
    const auto* Progression = Source->FindComponentByClass<UBreakerProgressionComponent>();
    if (Mana && Mana->IsActiveForOwner() && Progression && Spec.Snapshot.bHasCriticalRollSample
        && FMath::IsFinite(Spec.Snapshot.CriticalRollSample)
        && Progression->HasNodeTag(FGameplayTag::RequestGameplayTag(TEXT("Progression.Node.Caster.VoidWhisperer.SnapshotDiscipline"))))
    {
        for (const auto& Held : ABreakerZoneActor::GetLiveZones())
        {
            const auto* Zone = Held.Get();
            if (!IsValid(Zone) || Zone->IsActorBeingDestroyed() || Zone->IsReleased()
                || Zone->GetWorld() != Source->GetWorld() || Zone->GetZoneInstigator() != Source
                || (Zone->GetRemainingDuration() <= 0 && !Zone->IsExpiryPaused())) continue;
            const auto& ZoneSpec = Zone->GetSpec();
            if (!UBreakerZoneMath::IsInsideZone(Zone->GetCurrentFootprintCenter(), ZoneSpec.RadiusCm,
                ZoneSpec.HalfHeightCm, Source->GetActorLocation())) continue;
            Spec.Snapshot.CriticalChance = FMath::Clamp(Spec.Snapshot.CriticalChance + .25f, 0.0f, 1.0f); // O2 PLACEHOLDER, authored percentage points.
            Spec.Snapshot.bRolledCritical = Spec.Snapshot.CriticalRollSample < Spec.Snapshot.CriticalChance;
            break;
        }
    }
    if (Mana && Mana->IsOvercast() && Progression
        && Progression->HasNodeTag(FGameplayTag::RequestGameplayTag(TEXT("Progression.Node.Caster.VoidWhisperer.LongDebt"))))
        Spec.TickInterval *= .5f; // O2 PLACEHOLDER, authored twice-frequency application snapshot.
}

float BreakerCasterStatusRules::RotLifetimeMultiplier(AActor* Source)
{
    const auto* Mana = IsValid(Source) ? Source->FindComponentByClass<UBreakerManaComponent>() : nullptr;
    const auto* Progression = IsValid(Source) ? Source->FindComponentByClass<UBreakerProgressionComponent>() : nullptr;
    return Mana && Mana->IsOvercast() && Progression
        && Progression->HasNodeTag(FGameplayTag::RequestGameplayTag(TEXT("Progression.Node.Caster.VoidWhisperer.LongDebt")))
        ? 2.f : 1.f; // O2 PLACEHOLDER, longer lifetime redistributes the same finite budget.
}

float BreakerCasterStatusRules::RotCriticalBudgetMultiplier(const FBreakerDamageRequest& Request, const FBreakerDamageResult& Result)
{
    // The applying hit already paid its critical multiplier; never apply it twice.
    if (Result.bCritical || !Request.bCanCritical || !Result.bHasCriticalRollSample
        || !FMath::IsFinite(Result.CriticalRollSample)) return 1;
    AActor* Source = Request.Instigator.Get();
    const auto* Mana = IsValid(Source) ? Source->FindComponentByClass<UBreakerManaComponent>() : nullptr;
    const auto* Progression = IsValid(Source) ? Source->FindComponentByClass<UBreakerProgressionComponent>() : nullptr;
    if (!Mana || !Mana->IsActiveForOwner() || !Progression
        || !Progression->HasNodeTag(FGameplayTag::RequestGameplayTag(TEXT("Progression.Node.Caster.VoidWhisperer.SnapshotDiscipline")))) return 1;
    const float Chance = FMath::Clamp(Request.CriticalChance + .25f, 0.f, 1.f); // O2 PLACEHOLDER, authored percentage points.
    if (Result.CriticalRollSample >= Chance) return 1;
    for (const auto& Held : ABreakerZoneActor::GetLiveZones())
    {
        const auto* Zone = Held.Get();
        if (!IsValid(Zone) || Zone->IsActorBeingDestroyed() || Zone->IsReleased()
            || Zone->GetWorld() != Source->GetWorld() || Zone->GetZoneInstigator() != Source
            || (Zone->GetRemainingDuration() <= 0 && !Zone->IsExpiryPaused())) continue;
        const auto& Spec = Zone->GetSpec();
        if (UBreakerZoneMath::IsInsideZone(Zone->GetCurrentFootprintCenter(),Spec.RadiusCm,Spec.HalfHeightCm,Source->GetActorLocation()))
            return FMath::Max(1.f,Request.CriticalMultiplier);
    }
    return 1;
}
