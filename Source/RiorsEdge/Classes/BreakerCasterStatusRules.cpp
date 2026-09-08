#include "Classes/BreakerCasterStatusRules.h"
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
