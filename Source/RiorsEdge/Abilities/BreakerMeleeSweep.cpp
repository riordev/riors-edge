#include "Abilities/BreakerMeleeSweep.h"

#include "Combat/BreakerCombatComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

bool UBreakerMeleeSweep::IsInsideArc(const FVector& Origin, const FVector& Forward, const FVector& TargetLocation, float RangeCm, float ArcDegrees, float TargetRadiusCm)
{
    if (RangeCm <= 0.0f || ArcDegrees <= 0.0f)
    {
        return false;
    }

    FVector ToTarget = TargetLocation - Origin;
    ToTarget.Z = 0.0;
    const double DistanceSquared = ToTarget.SizeSquared();
    // Reach is to the body's SURFACE, not its origin. A negative radius cannot
    // shorten the authored range: forgiveness only ever adds.
    const double Reach = static_cast<double>(RangeCm) + FMath::Max(0.0f, TargetRadiusCm);
    if (DistanceSquared > Reach * Reach)
    {
        return false;
    }
    // A target standing exactly on the caster is always inside the arc: there
    // is no direction to compare against and "the thing on top of me" must not
    // become the one thing Cleave cannot hit.
    if (DistanceSquared <= KINDA_SMALL_NUMBER)
    {
        return true;
    }

    FVector Facing = Forward;
    Facing.Z = 0.0;
    if (!Facing.Normalize())
    {
        return false;
    }
    const double CosineToTarget = FVector::DotProduct(Facing, ToTarget / FMath::Sqrt(DistanceSquared));
    // ArcDegrees is the full included angle, so the half-angle is the gate.
    const double HalfArcCosine = FMath::Cos(FMath::DegreesToRadians(FMath::Min(ArcDegrees, 360.0f) * 0.5));
    return CosineToTarget >= HalfArcCosine - KINDA_SMALL_NUMBER;
}

float UBreakerMeleeSweep::SortKey(const FVector& Origin, const FVector& Forward, const FVector& TargetLocation)
{
    FVector ToTarget = TargetLocation - Origin;
    ToTarget.Z = 0.0;
    const float Distance = static_cast<float>(ToTarget.Size());

    FVector Facing = Forward;
    Facing.Z = 0.0;
    float AngleDegrees = 0.0f;
    if (Facing.Normalize() && Distance > KINDA_SMALL_NUMBER)
    {
        const double Cosine = FMath::Clamp(FVector::DotProduct(Facing, ToTarget / Distance), -1.0, 1.0);
        AngleDegrees = static_cast<float>(FMath::RadiansToDegrees(FMath::Acos(Cosine)));
    }
    // Angle dominates, distance breaks ties. Scaling the angle by a large
    // constant keeps the ordering total rather than approximate.
    return AngleDegrees * 10000.0f + Distance;
}

TArray<AActor*> UBreakerMeleeSweep::SweepTargets(const UWorld* World, AActor* Instigator, const FBreakerMeleeSweepParams& Params)
{
    TArray<AActor*> Targets;
    if (!World || Params.RangeCm <= 0.0f)
    {
        return Targets;
    }

    // Overlap on the same channel the weapon traces on, so anything shootable
    // is also cleavable and no actor needs a second collision setup.
    TArray<FOverlapResult> Overlaps;
    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(BreakerMeleeSweep), false, Instigator);
    World->OverlapMultiByChannel(
        Overlaps,
        Params.Origin,
        FQuat::Identity,
        ECC_GameTraceChannel2,
        FCollisionShape::MakeSphere(Params.RangeCm),
        QueryParams);

    TSet<AActor*> Seen;
    for (const FOverlapResult& Overlap : Overlaps)
    {
        AActor* Candidate = Overlap.GetActor();
        if (!Candidate || Candidate == Instigator || Seen.Contains(Candidate))
        {
            continue;
        }
        // Only actors that can actually take damage: a decorative prop inside
        // the arc must not eat a MaxTargets slot.
        if (!Candidate->FindComponentByClass<UBreakerCombatComponent>())
        {
            continue;
        }
        // The forgiveness comes from the very component this overlap admitted
        // the candidate by, so the arc can never disagree with the query that
        // fed it. Overlap.Component is that component by construction.
        float TargetRadius = 0.0f;
        if (const UPrimitiveComponent* Touched = Overlap.GetComponent())
        {
            const FVector Extent = Touched->Bounds.BoxExtent;
            TargetRadius = HorizontalReach(Extent);
        }
        if (!IsInsideArc(Params.Origin, Params.Forward, Candidate->GetActorLocation(), Params.RangeCm, Params.ArcDegrees, TargetRadius))
        {
            continue;
        }
        // Never swing through a wall.
        FHitResult Occlusion;
        FCollisionQueryParams OcclusionParams = QueryParams;
        OcclusionParams.AddIgnoredActor(Candidate);
        if (World->LineTraceSingleByObjectType(Occlusion, Params.Origin, Candidate->GetActorLocation(),
            FCollisionObjectQueryParams(ECC_WorldStatic), OcclusionParams))
        {
            continue;
        }
        Seen.Add(Candidate);
        Targets.Add(Candidate);
    }

    const FVector Origin = Params.Origin;
    const FVector Forward = Params.Forward;
    Targets.Sort([&Origin, &Forward](const AActor& A, const AActor& B)
    {
        return SortKey(Origin, Forward, A.GetActorLocation()) < SortKey(Origin, Forward, B.GetActorLocation());
    });

    if (Params.MaxTargets > 0 && Targets.Num() > Params.MaxTargets)
    {
        Targets.SetNum(Params.MaxTargets);
    }
    return Targets;
}
