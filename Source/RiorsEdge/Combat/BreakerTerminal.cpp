#include "Combat/BreakerStatusComponent.h"
#include "Combat/BreakerCombatComponent.h"
#include "Progression/BreakerProgressionComponent.h"
#include "AbilitySystemComponent.h"
#include "Attributes/BreakerAttributeSet.h"

void UBreakerStatusComponent::RefreshTerminalPersistence(FBreakerActiveStatus& Status)
{
    if (!Status.bTerminalEligible) return;
    const auto* Source = Status.Instigator.Get();
    const auto* SourceCombat = IsValid(Source) ? Source->FindComponentByClass<UBreakerCombatComponent>() : nullptr;
    const auto* Progression = IsValid(Source) ? Source->FindComponentByClass<UBreakerProgressionComponent>() : nullptr;
    const auto* Target = GetOwner() ? GetOwner()->FindComponentByClass<UBreakerCombatComponent>() : nullptr;
    if (!Target || Target->IsDead() || !SourceCombat || SourceCombat->IsDead() || !Progression
        || !Progression->HasNodeTag(FGameplayTag::RequestGameplayTag(TEXT("Progression.Node.Caster.VoidWhisperer.Terminal"))))
    {
        Status.bTerminalPersistent = false;
        Status.bTerminalEligible = false;
        return;
    }
    // Target-facing physical health only; Endurance never changes enemy thresholds.
    const auto* TargetASC = GetOwner()->FindComponentByClass<UAbilitySystemComponent>();
    if (TargetASC && Target->GetMaxHealth() > 0 && TargetASC->GetNumericAttribute(UBreakerAttributeSet::GetHealthAttribute()) < Target->GetMaxHealth() * FMath::Clamp(TerminalHealthFraction, 0.f, 1.f))
        Status.bTerminalPersistent = true;
}
