#include "Audio/BreakerFootstepComponent.h"
#include "Audio/BreakerSoundDirector.h"
#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerCombatComponent.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"

UBreakerFootstepComponent::UBreakerFootstepComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

void UBreakerFootstepComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    const ABreakerCharacter* Player = Cast<ABreakerCharacter>(GetOwner());
    if (!Player) return;
    const auto* Movement = Player->GetBreakerMovement();
    const auto* PC = Cast<APlayerController>(Player->GetController());
    const auto* Combat = Player->GetCombat();
    const uint32 Continuity = Movement ? Movement->GetTraversalInvalidationSerial() : 0;
    const bool bAllowed = PC && PC->IsLocalController() && !PC->IsMoveInputIgnored()
        && !Player->IsMenuOpen() && Combat && !Combat->IsDead()
        && Movement && Movement->IsMovingOnGround() && !Movement->IsSliding()
        && !Movement->IsTraversingLedge() && Continuity == LastContinuity;
    LastContinuity = Continuity;
    if (Cadence.Advance(Player->GetActorLocation(), Player->GetVelocity().Size2D(), DeltaTime, bAllowed))
        ABreakerSoundDirector::PlayFootstep(GetWorld());
}
