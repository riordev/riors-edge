#include "Playtest/BreakerKillTelemetryComponent.h"

#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerEnemy.h"
#include "Combat/BreakerModifierComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Playtest/BreakerPlaytestComponent.h"

UBreakerKillTelemetryComponent::UBreakerKillTelemetryComponent()
{
    // Purely reactive: it does nothing between damage events, so it must not
    // cost a tick on every enemy in a 24-strong wave.
    PrimaryComponentTick.bCanEverTick = false;
}

UBreakerKillTelemetryComponent* UBreakerKillTelemetryComponent::AttachTo(ABreakerEnemy* Enemy)
{
    if (!Enemy || Enemy->FindComponentByClass<UBreakerKillTelemetryComponent>()) return nullptr;
    UBreakerKillTelemetryComponent* Component = NewObject<UBreakerKillTelemetryComponent>(Enemy);
    if (!Component) return nullptr;
    Component->RegisterComponent();
    return Component;
}

void UBreakerKillTelemetryComponent::BeginPlay()
{
    Super::BeginPlay();
    if (UBreakerCombatComponent* Combat = GetOwner() ? GetOwner()->FindComponentByClass<UBreakerCombatComponent>() : nullptr)
    {
        Combat->OnDamageReceived.AddDynamic(this, &ThisClass::HandleOwnerDamaged);
    }
}

void UBreakerKillTelemetryComponent::HandleOwnerDamaged(const FBreakerDamageResult& Result)
{
    if (!Result.bKilled) return;
    const ABreakerEnemy* Enemy = Cast<ABreakerEnemy>(GetOwner());
    if (!Enemy) return;

    const UBreakerEnemyModifierComponent* Modifiers = Enemy->GetModifierComponent();
    const EBreakerKillBucket Bucket = UBreakerKillBucketLibrary::ClassifyKill(
        Enemy->GetMonsterRank(),
        Enemy->IsRangedForTelemetry(),
        Modifiers ? Modifiers->GetModifierCount() : 0);

    // Posted onto the PLAYER's instrument, which is where every other playtest
    // number already lives. A kill with no local player is a headless run, and
    // there is nothing to report to.
    const UWorld* World = GetWorld();
    APawn* PlayerPawn = World && World->GetFirstPlayerController() ? World->GetFirstPlayerController()->GetPawn() : nullptr;
    if (UBreakerPlaytestComponent* Playtest = PlayerPawn ? PlayerPawn->FindComponentByClass<UBreakerPlaytestComponent>() : nullptr)
    {
        Playtest->NotePendingKillBucket(Bucket);
    }
}
