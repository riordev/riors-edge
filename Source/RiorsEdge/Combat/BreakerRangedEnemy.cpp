#include "Combat/BreakerRangedEnemy.h"

#include "Combat/BreakerBodyPaint.h"

#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerEnemyProjectile.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

namespace
{
    // Mossy grey-green so the ranged archetype is separable from the melee
    // humanoid's grey-violet at a glance, and sits inside the overgrown-Earth
    // read (O24). Deliberately NOT teal: the object-chroma law reserves
    // saturated teal for rift objects and suppression hardware.
    // The same symbol the paint resolver defaults this family to (O128).
    const FLinearColor RangedBodyColor = BreakerBodyPaint::LatticeFamilyPaint;

    void ApplyColor(UStaticMeshComponent* Mesh, const FLinearColor& Color)
    {
        if (!Mesh) return;
        UMaterialInterface* BaseMaterial = LoadObject<UMaterialInterface>(
            nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
        if (!BaseMaterial) return;
        if (UMaterialInstanceDynamic* Dynamic = UMaterialInstanceDynamic::Create(BaseMaterial, Mesh))
        {
            Dynamic->SetVectorParameterValue(TEXT("Color"), Color);
            Mesh->SetMaterial(0, Dynamic);
        }
    }
}

ABreakerRangedEnemy::ABreakerRangedEnemy()
{
    // THE LATTICE STAYS PRIMITIVES BY RULING (owner, 2026-08-29, recorded in
    // Assets/enemies/LICENSE-NOTE.txt): it is composed in-engine, code-
    // animated, BodyPaint material — the one enemy the mech cast does not
    // dress. The base constructor defaults Stan; this clears it.
    BodyMeshAsset = FSoftObjectPath();
    BodyIdleAnimation = FSoftObjectPath();
    // --- Chassis (O2 PLACEHOLDER) ------------------------------------------
    // This archetype used to ship at the base chassis' flat 220 deliberately,
    // because trash and elite health were mid-re-anchor. O27 landed, so
    // Encounter-Design §2.2's 1.6x Lattice ratio is applied — and it is
    // applied through the SAME chassis composition every other monster uses
    // (area level ^ growth, times rank, times archetype) rather than as a
    // second hardcoded health number.
    DetectionRange = 3200.0f;   // it sees you well before you are in its band
    MoveSpeed = 320.0f;
    ArchetypeHealthMultiplier = 1.6f;   // O2 PLACEHOLDER, Encounter-Design §2.2
    // Damage at area level 1; the curve and the rank table take it from here.
    //
    // MOVES WITH THE MELEE BASE, and the ratio is the authored thing. This was
    // 16 against a melee 14, a deliberate 1.143x for the enemy that hits you
    // from cover. The O116 retune raised the melee base to 51.1, and leaving
    // this at 16 would have inverted the pair to 0.31x — the ranged enemy
    // quietly becoming the safe one — without any test noticing, because
    // nothing asserts the relationship. 51.1 x 1.143 = 58.4.
    Chassis.BaseDamage = 58.4f;   // O2 PLACEHOLDER, carried by the projectile
    AttackRange = 0.0f;         // no contact attack — the base melee path is disabled
    AttackCooldown = 0.0f;

    ProjectileClass = ABreakerEnemyProjectile::StaticClass();

    // Layer 1, declared, then painted from the same value.
    FamilyPaint = RangedBodyColor;

    for (UStaticMeshComponent* Part : { BodyVisual.Get(), HeadVisual.Get(), LeftArmVisual.Get(),
        RightArmVisual.Get(), LeftLegVisual.Get(), RightLegVisual.Get() })
    {
        ApplyColor(Part, FamilyPaint);
    }

    EmitterVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("EmitterVisual"));
    EmitterVisual->SetupAttachment(BodyCollision);
    EmitterVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    // Chest height, pushed forward so the orb visibly leaves the body rather
    // than materialising inside it.
    EmitterVisual->SetRelativeLocation(FVector(46.0f, 0.0f, 30.0f));
    EmitterVisual->SetRelativeScale3D(FVector(TelegraphIdleScale));
    EmitterVisual->SetCastShadow(false);
    if (UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere")))
    {
        EmitterVisual->SetStaticMesh(Sphere);
    }

    EmitterLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("EmitterLight"));
    EmitterLight->SetupAttachment(EmitterVisual);
    EmitterLight->SetMobility(EComponentMobility::Movable);
    EmitterLight->SetCastShadows(false);
    EmitterLight->SetIntensity(0.0f);
    EmitterLight->SetAttenuationRadius(900.0f);
}

void ABreakerRangedEnemy::BeginPlay()
{
    Super::BeginPlay();

    if (UMaterialInterface* BaseMaterial = LoadObject<UMaterialInterface>(
        nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
    {
        EmitterMaterial = UMaterialInstanceDynamic::Create(BaseMaterial, EmitterVisual);
        if (EmitterMaterial) EmitterVisual->SetMaterial(0, EmitterMaterial);
    }
    if (EmitterLight) EmitterLight->SetLightColor(TelegraphHotColor);
    // PatrolPhase is the per-enemy desync seed the base class already uses;
    // reusing it means a pack of these never strafes in lockstep.
    StrafeSign = FMath::Fmod(FMath::Abs(PatrolPhase), 2.0f) < 1.0f ? 1.0f : -1.0f;
    StrafeTimer = FMath::Fmod(FMath::Abs(PatrolPhase), StrafeReverseSeconds);
    UpdateTelegraph(0.0f);
    StateLabel = TEXT("LATTICE PATROL");
}

void ABreakerRangedEnemy::TickEngagedBehaviour(ABreakerCharacter* Player, float Distance, float DeltaSeconds,
    FVector& OutDirection, float& OutSpeedScale)
{
    UWorld* World = GetWorld();
    if (!Player || !World) return;
    const double Now = World->GetTimeSeconds();

    const FVector ToPlayer = (Player->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
    const FVector Lateral = FVector::CrossProduct(FVector::UpVector, ToPlayer).GetSafeNormal2D();
    // Always face the player, whatever direction the feet are going. The base
    // class honours this instead of facing the movement vector.
    DesiredFacing = ToPlayer;

    const bool bLineOfSight = HasLineOfSightTo(Player);

    // --- Band: advance / hold / retreat ------------------------------------
    if (!bLineOfSight)
    {
        // Something is between us. Closing is the reliable way to recover an
        // angle; strafing alone can hug a pillar forever.
        Band = EBreakerRangedBand::Advance;
        OutDirection = ToPlayer;
        OutSpeedScale = AdvanceSpeedMultiplier;
        StateLabel = TEXT("REPOSITION");
    }
    else
    {
        Band = UBreakerRangedBehaviorLibrary::ClassifyBand(
            Distance, MinEngagementDistance, MaxEngagementDistance, BandHysteresis, Band);
        OutSpeedScale = UBreakerRangedBehaviorLibrary::GetBandSpeedScale(
            Band, AdvanceSpeedMultiplier, RetreatSpeedMultiplier, StrafeSpeedMultiplier);

        switch (Band)
        {
        case EBreakerRangedBand::Advance:
            OutDirection = ToPlayer;
            StateLabel = TEXT("CLOSING");
            break;
        case EBreakerRangedBand::Retreat:
            OutDirection = -ToPlayer;
            StateLabel = TEXT("FALLING BACK");
            break;
        default:
            // Holding station is NOT standing still. It strafes, reversing on
            // a cadence so its path is never a straight line to pre-aim at.
            StrafeTimer += DeltaSeconds;
            if (StrafeTimer >= StrafeReverseSeconds)
            {
                StrafeTimer = 0.0f;
                StrafeSign = -StrafeSign;
            }
            OutDirection = Lateral * StrafeSign;
            StateLabel = TEXT("HOLDING");
            break;
        }
    }

    // --- Fire cycle: wind-up (the tell), then the shot ---------------------
    if (bWindingUp)
    {
        // Through the keyed telegraph seam, sampled per frame: unkeyed this is
        // exactly the authored WindupSeconds; Interdiction stretches the tell
        // (and the emitter ramp with it — the bloom slows to match, so the
        // telegraph never finishes before the shot it warns about).
        const float EffectiveWindup = WindupSeconds * GetComposedWindupDurationMultiplier();
        const float Elapsed = static_cast<float>(Now - WindupStartTime);
        UpdateTelegraph(UBreakerRangedBehaviorLibrary::GetTelegraphAlpha(Elapsed, EffectiveWindup));
        // Committing to a shot costs mobility, so the player can also read the
        // tell from the enemy's feet, not only from the emitter.
        OutSpeedScale *= WindupMoveScale;
        StateLabel = TEXT("AIMING");

        if (Elapsed >= EffectiveWindup)
        {
            bWindingUp = false;
            LastAttackTime = Now;
            UpdateTelegraph(0.0f);
            // Losing sight mid-wind-up aborts the shot and still pays the
            // cooldown. A telegraph that finishes with nothing behind it is
            // survivable; one that fires through a wall is not.
            if (bLineOfSight) FireVolley(Player);
            StateLabel = TEXT("FIRED");
        }
    }
    else if (bLineOfSight
        && Distance <= MaxEngagementDistance + BandHysteresis
        && (Now - LastAttackTime) >= ShotCooldown)
    {
        bWindingUp = true;
        WindupStartTime = Now;
        StateLabel = TEXT("AIMING");
    }
}

bool ABreakerRangedEnemy::CommandVolley()
{
    if (!GetWorld() || !HasAuthority() || IsDeadEnemy()) return false;
    // Already committed: a doubled order must not restart the wind-up, because
    // restarting it would RESET the tell the player is already reading and
    // effectively shorten the warning to nothing.
    if (bWindingUp) return false;
    bWindingUp = true;
    WindupStartTime = GetWorld()->GetTimeSeconds();
    // The ordinary cadence gate is bypassed — that is the point of the order —
    // but the telegraph is not. StateLabel says ORDERED rather than AIMING so
    // the player can tell a commanded volley from a routine one.
    StateLabel = TEXT("ORDERED");
    return true;
}

void ABreakerRangedEnemy::FireVolley(const AActor* Target)
{
    UWorld* World = GetWorld();
    if (!World || !HasAuthority() || !Target || !ProjectileClass) return;

    const FVector Muzzle = GetMuzzleLocation();
    // Aim at the torso, not the feet: an orb that arrives at ankle height is
    // both harder to read and unfairly easy to jump.
    const FVector TargetPoint = Target->GetActorLocation();
    const FVector AimPoint = UBreakerRangedBehaviorLibrary::ComputeAimPoint(
        Muzzle, TargetPoint, Target->GetVelocity(), ProjectileSpeed, LeadFraction);

    FVector BaseDirection = (AimPoint - Muzzle).GetSafeNormal();
    if (BaseDirection.IsNearlyZero()) return;

    // The aim-error seam. LATTICE's authored aim is EXACT (spread 0), so only
    // the keyed excess over 1.0 opens a cone — Suppress's accuracy cut is the
    // consumer. Guarded so an unkeyed volley never touches the RNG stream and
    // stays bit-identical to the authored aim solve.
    const float ErrorConeDegrees = GetEffectiveSpreadDegrees(0.0f, GetComposedAimErrorMultiplier(), AimErrorUnitDegrees);
    if (ErrorConeDegrees > 0.0f)
    {
        BaseDirection = FMath::VRandCone(BaseDirection, FMath::DegreesToRadians(ErrorConeDegrees));
    }

    const int32 Count = FMath::Max(ProjectilesPerVolley, 1);
    for (int32 Index = 0; Index < Count; ++Index)
    {
        // Symmetric fan around the aim line; a single-projectile volley gets
        // no spread at all, which is the readable default.
        const float Offset = Count > 1
            ? (static_cast<float>(Index) / (Count - 1) - 0.5f) * 2.0f * VolleySpreadDegrees
            : 0.0f;
        const FVector Direction = BaseDirection.RotateAngleAxis(Offset, FVector::UpVector);

        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        Params.Owner = this;
        Params.Instigator = this;
        ABreakerEnemyProjectile* Projectile = World->SpawnActor<ABreakerEnemyProjectile>(
            ProjectileClass, Muzzle, Direction.Rotation(), Params);
        if (!Projectile) continue;

        FBreakerDamageRequest Shot;
        // Through the outgoing seam (WA6's softening lands here); unkeyed it
        // IS the chassis AttackDamage exactly.
        Shot.BaseDamage = GetEffectiveAttackDamage();
        Shot.DamageFamily = EBreakerDamageFamily::Physical;
        // Enemies do not crit; crit is the player's multiplier
        // (Encounter-Design §0).
        Shot.bCanCritical = false;
        Shot.SetInstigator(this);
        Projectile->InitializeProjectile(Shot, Direction, ProjectileSpeed);
    }
}

void ABreakerRangedEnemy::UpdateTelegraph(float Alpha)
{
    const float Clamped = FMath::Clamp(Alpha, 0.0f, 1.0f);
    if (EmitterMaterial)
    {
        EmitterMaterial->SetVectorParameterValue(TEXT("Color"),
            FMath::Lerp(TelegraphIdleColor, TelegraphHotColor, Clamped));
    }
    if (EmitterVisual)
    {
        EmitterVisual->SetRelativeScale3D(FVector(FMath::Lerp(TelegraphIdleScale, TelegraphHotScale, Clamped)));
    }
    if (EmitterLight)
    {
        // Squared so the last third of the wind-up is where it really blooms —
        // "now" is more legible than "soon".
        EmitterLight->SetIntensity(TelegraphLightIntensity * Clamped * Clamped);
    }
}

bool ABreakerRangedEnemy::HasLineOfSightTo(const AActor* Target) const
{
    const UWorld* World = GetWorld();
    if (!World || !Target) return false;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(BreakerRangedLineOfSight), false, this);
    Params.AddIgnoredActor(Target);
    FHitResult Blocked;
    // World statics only: the level's cover, ruins and pillars break the shot;
    // other enemies never do.
    return !World->LineTraceSingleByChannel(Blocked, GetMuzzleLocation(),
        Target->GetActorLocation(), ECC_WorldStatic, Params);
}

FVector ABreakerRangedEnemy::GetMuzzleLocation() const
{
    return EmitterVisual ? EmitterVisual->GetComponentLocation() : GetActorLocation();
}

void ABreakerRangedEnemy::SetBodyVisible(bool bVisible)
{
    Super::SetBodyVisible(bVisible);
    // Death and respawn both route through here, so this is the one place the
    // charge state has to be cleared — a corpse must not keep glowing, and a
    // respawn must not resume mid-wind-up.
    bWindingUp = false;
    UpdateTelegraph(0.0f);
    if (EmitterVisual) EmitterVisual->SetVisibility(bVisible, true);
    if (EmitterLight) EmitterLight->SetVisibility(bVisible, true);
}
