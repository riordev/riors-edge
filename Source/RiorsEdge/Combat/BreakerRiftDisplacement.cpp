#include "Combat/BreakerRiftDisplacement.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

float BreakerRiftDisplacement::Apply(AActor* Target, const FVector& SourceLocation, float DistanceCm)
{
    const auto* Progression = Target ? Target->FindComponentByClass<UBreakerProgressionComponent>() : nullptr;
    if (Progression && Progression->HasNodeTag(FGameplayTag::RequestGameplayTag(TEXT("Progression.Node.Core.Immovable")))) return 0;
    auto* Pawn = Cast<APawn>(Target);
    auto* Capsule = Pawn ? Cast<UCapsuleComponent>(Pawn->GetRootComponent()) : nullptr;
    UWorld* World = Pawn ? Pawn->GetWorld() : nullptr;
    if (!World || !Capsule || !Pawn->HasAuthority() || Pawn->IsActorBeingDestroyed()
        || SourceLocation.ContainsNaN() || !FMath::IsFinite(DistanceCm) || DistanceCm <= 0) return 0;
    const FVector Start = Pawn->GetActorLocation();
    const FVector Away = (Start - SourceLocation).GetSafeNormal2D();
    if (Away.IsNearlyZero()) return 0;
    const float Radius = Capsule->GetScaledCapsuleRadius();
    const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
    float WalkableZ = .7f;
    if (const auto* Character = Cast<ACharacter>(Pawn))
        if (const auto* Movement = Character->GetCharacterMovement()) WalkableZ = Movement->GetWalkableFloorZ();
    FCollisionQueryParams Query(SCENE_QUERY_STAT(BreakerRiftDisplacement), false, Pawn);
    // Match native pawn obstruction queries even for an enemy capsule whose
    // profile delegates locomotion collision to its mover.
    FHitResult Block;
    float Allowed = DistanceCm;
    if (World->SweepSingleByChannel(Block, Start, Start + Away * DistanceCm, Capsule->GetComponentQuat(),
        ECC_Pawn, FCollisionShape::MakeCapsule(Radius, HalfHeight), Query))
    {
        if (Block.bStartPenetrating) return 0;
        Allowed = FMath::Max(0.0f, DistanceCm * Block.Time - 2.0f);
    }
    // Check the footprint along the route, not just its destination. A short
    // displacement must not throw a grounded pawn across an unbacked ledge.
    const auto Supported = [&](const FVector& At)
    {
        for (int32 Probe = -1; Probe < 8; ++Probe)
        {
            const FVector Offset = Probe < 0 ? FVector::ZeroVector
                : FVector::ForwardVector.RotateAngleAxis(Probe * 45.0f, FVector::UpVector) * FMath::Max(0.0f, Radius - 2.0f);
            const FVector Feet = At + Offset - FVector(0, 0, HalfHeight);
            FHitResult Floor;
            if (!World->LineTraceSingleByChannel(Floor, Feet + FVector(0, 0, 10), Feet - FVector(0, 0, 30), ECC_Pawn, Query)
                || Floor.ImpactNormal.Z < WalkableZ || Cast<APawn>(Floor.GetActor())) return false;
        }
        return true;
    };
    if (!Supported(Start)) return 0;
    float SafeDistance = 0;
    const int32 Steps = FMath::Max(1, FMath::CeilToInt(Allowed / 10.0f));
    for (int32 Step = 1; Step <= Steps; ++Step)
    {
        const float Candidate = Allowed * Step / Steps;
        if (!Supported(Start + Away * Candidate)) break;
        SafeDistance = Candidate;
    }
    if (SafeDistance <= 0) return 0;
    FHitResult ActualHit;
    Pawn->SetActorLocation(Start + Away * SafeDistance, true, &ActualHit, ETeleportType::None);
    return FVector::Dist2D(Start, Pawn->GetActorLocation());
}
