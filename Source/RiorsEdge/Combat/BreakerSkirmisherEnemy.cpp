#include "Combat/BreakerSkirmisherEnemy.h"

#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerEnemyProjectile.h"
#include "Combat/BreakerModifierComponent.h"
#include "Combat/BreakerRangedBehavior.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

ABreakerSkirmisherEnemy::ABreakerSkirmisherEnemy()
{
    // EARLY-severance Altered. The stage is not decoration: UsesCoverDiscipline
    // and FlinchesWhenHit are asked of the family/stage pair every frame, so
    // re-authoring this actor as a late-stage Altered genuinely turns both
    // behaviours off (Story-Source §1.5's "stage as art direction", made
    // mechanical).
    Family = EBreakerEnemyFamily::Altered;
    SeveranceStage = EBreakerSeveranceStage::Early;

    // Lighter and faster than the Warden — a soldier, not an anchor. Health
    // sits just above the melee baseline because most of its survivability is
    // supposed to come from the cover, not from the bar.
    ArchetypeHealthMultiplier = 1.15f;   // O2 PLACEHOLDER
    ArchetypeDamageMultiplier = 1.0f;    // O2 PLACEHOLDER
    MoveSpeed = 400.0f;                  // O2 PLACEHOLDER
    DetectionRange = 3400.0f;            // O2 PLACEHOLDER
    // No contact attack and no lunge: it never wants to be close.
    AttackRange = 0.0f;
    LungeRange = 0.0f;
    LungeSpeedMultiplier = 1.0f;
    WeaveStrength = 0.0f;

    ProjectileClass = ABreakerEnemyProjectile::StaticClass();

    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));

    // §1.5: "early stage still wears insignia". In graybox that is a bright
    // plate on the chest, and it is the fastest read in the game for "this used
    // to be someone" — a Vestige has nothing like it.
    InsigniaVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("InsigniaVisual"));
    InsigniaVisual->SetupAttachment(BodyCollision);
    InsigniaVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    InsigniaVisual->SetRelativeLocation(FVector(23.0f, 0.0f, 40.0f));
    InsigniaVisual->SetRelativeScale3D(FVector(0.06f, 0.22f, 0.14f));
    if (CubeMesh.Succeeded()) InsigniaVisual->SetStaticMesh(CubeMesh.Object);

    MuzzleVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MuzzleVisual"));
    MuzzleVisual->SetupAttachment(BodyCollision);
    MuzzleVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    MuzzleVisual->SetRelativeLocation(FVector(58.0f, 26.0f, 30.0f));
    MuzzleVisual->SetRelativeScale3D(FVector(0.13f));
    if (SphereMesh.Succeeded()) MuzzleVisual->SetStaticMesh(SphereMesh.Object);

    MuzzleLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("MuzzleLight"));
    MuzzleLight->SetupAttachment(MuzzleVisual);
    MuzzleLight->SetCastShadows(false);
    MuzzleLight->SetAttenuationRadius(600.0f);
    MuzzleLight->SetIntensity(0.0f);
}

void ABreakerSkirmisherEnemy::BeginPlay()
{
    Super::BeginPlay();
    BodyRestZ = BodyVisual ? BodyVisual->GetRelativeLocation().Z : 0.0f;

    if (UMaterialInterface* Base = LoadObject<UMaterialInterface>(
        nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
    {
        if (InsigniaVisual)
        {
            if (UMaterialInstanceDynamic* Plate = UMaterialInstanceDynamic::Create(Base, InsigniaVisual))
            {
                Plate->SetVectorParameterValue(TEXT("Color"), InsigniaColor);
                InsigniaVisual->SetMaterial(0, Plate);
            }
        }
        if (MuzzleVisual)
        {
            MuzzleMaterial = UMaterialInstanceDynamic::Create(Base, MuzzleVisual);
            if (MuzzleMaterial)
            {
                MuzzleMaterial->SetVectorParameterValue(TEXT("Color"), MuzzleIdleColor);
                MuzzleVisual->SetMaterial(0, MuzzleMaterial);
            }
        }
    }

    // The flinch needs to know it was HIT, which OnDamageReceived cannot say
    // (it carries a result, not an attacker or a family). OnDamageTaken is the
    // victim-side context broadcast added for the Reflective modifier and it is
    // exactly the right shape here too.
    if (Combat) Combat->OnDamageTaken.AddDynamic(this, &ThisClass::HandleFlinchSource);

    CoverState = EBreakerCoverState::Relocating;
    StateElapsed = 0.0f;
}

void ABreakerSkirmisherEnemy::SetBodyVisible(bool bVisible)
{
    Super::SetBodyVisible(bVisible);
    if (InsigniaVisual) InsigniaVisual->SetVisibility(bVisible, true);
    if (MuzzleVisual) MuzzleVisual->SetVisibility(bVisible, true);
    if (MuzzleLight && !bVisible) MuzzleLight->SetIntensity(0.0f);
}

void ABreakerSkirmisherEnemy::HandleFlinchSource(const FBreakerHitContext& Hit)
{
    if (!FlinchesWhenHit() || !GetWorld()) return;
    // A DoT tick is not a flinch. Being on fire does not make a soldier duck;
    // being shot at does, and letting a bleed chain-interrupt would let a
    // status build suppress the archetype out of the fight entirely.
    if (Hit.bFromDoT) return;
    if (Hit.Result.HealthDamage <= 0.0f && Hit.Result.ShieldDamage <= 0.0f) return;
    // Only while it is up. Flinching behind cover would be invisible and would
    // burn the cooldown for nothing.
    if (CoverState != EBreakerCoverState::Exposed) return;
    if (GetWorld()->GetTimeSeconds() - LastFlinchTime < FlinchCooldownSeconds) return;

    // Recorded, not applied: this runs inside a damage broadcast, and moving
    // the enemy or spawning from in here re-enters the combat pipeline. The
    // behaviour tick consumes it on the next frame.
    bFlinchRequested = true;
}

bool ABreakerSkirmisherEnemy::IsBlockedFrom(const FVector& ThreatLocation, const FVector& Point) const
{
    if (!GetWorld()) return false;
    // Eye height on both ends: a point whose ANKLES are hidden behind a kerb is
    // not cover, and testing the floor would say it was.
    const FVector From = ThreatLocation + FVector(0.0f, 0.0f, 80.0f);
    const FVector To = Point + FVector(0.0f, 0.0f, 60.0f);
    FCollisionQueryParams Query(SCENE_QUERY_STAT(BreakerSkirmisherCover), false, this);
    FHitResult Hit;
    // WorldStatic only: another enemy standing in the way is not cover, and
    // treating it as cover would make a pack shuffle behind each other.
    return GetWorld()->LineTraceSingleByChannel(Hit, From, To, ECC_WorldStatic, Query);
}

bool ABreakerSkirmisherEnemy::FindCoverPoint(const FVector& ThreatLocation, FVector& OutPoint)
{
    const int32 Seed = HashCombine(GetTypeHash(GetActorLocation()), GetUniqueID());
    const TArray<FVector> Candidates = UBreakerCoverLibrary::GenerateCoverCandidates(
        GetActorLocation(), Cover, Seed);

    TArray<FVector> Blocked;
    Blocked.Reserve(Candidates.Num());
    for (const FVector& Candidate : Candidates)
    {
        if (IsBlockedFrom(ThreatLocation, Candidate)) Blocked.Add(Candidate);
    }
    // The scoring and the band rejection are pure and live in the library; the
    // only thing that needed a world was the trace above.
    return UBreakerCoverLibrary::ChooseCoverPoint(Blocked, GetActorLocation(), ThreatLocation, Cover, OutPoint);
}

void ABreakerSkirmisherEnemy::SetCrouched(bool bInCrouched)
{
    if (bCrouched == bInCrouched) return;
    bCrouched = bInCrouched;
    // The crouch is the only thing that makes "in cover" legible when the cover
    // is an untextured grey box: the silhouette drops behind the edge.
    if (BodyVisual)
    {
        FVector Location = BodyVisual->GetRelativeLocation();
        Location.Z = BodyRestZ - (bCrouched ? CrouchDropCm : 0.0f);
        BodyVisual->SetRelativeLocation(Location);
    }
}

void ABreakerSkirmisherEnemy::TickEngagedBehaviour(ABreakerCharacter* Player, float Distance, float DeltaSeconds,
    FVector& OutDirection, float& OutSpeedScale)
{
    if (!Player || !GetWorld()) return;
    const FVector ThreatLocation = Player->GetActorLocation();
    const FVector ToPlayer = (ThreatLocation - GetActorLocation()).GetSafeNormal2D();
    DesiredFacing = ToPlayer;
    OutDirection = FVector::ZeroVector;
    OutSpeedScale = 1.0f;
    StateElapsed += DeltaSeconds;

    // A late-stage Altered, or this actor re-authored as a Vestige, has no
    // cover discipline: it degrades to an open-ground shooter that walks into
    // its band and fires. That is the spectrum working — the same class, two
    // stages, two fights.
    const bool bUsesCover = UsesCoverDiscipline();

    if (bFlinchRequested)
    {
        bFlinchRequested = false;
        LastFlinchTime = GetWorld()->GetTimeSeconds();
        CoverState = EBreakerCoverState::Flinched;
        StateElapsed = 0.0f;
        // The burst is CANCELLED, not paused. Landing a hit has to buy the
        // player the whole remainder of the burst or the flinch is not a
        // reward, it is a rounding error.
        RoundsLeftInBurst = 0;
        AimElapsed = 0.0f;
        UpdateMuzzle(0.0f);
    }

    switch (CoverState)
    {
    case EBreakerCoverState::Flinched:
    {
        SetCrouched(true);
        StateLabel = UBreakerCoverLibrary::GetCoverStateName(CoverState);
        if (StateElapsed < FlinchSeconds) break;
        CoverState = bUsesCover ? EBreakerCoverState::Relocating : EBreakerCoverState::Exposed;
        StateElapsed = 0.0f;
        bHasCoverPoint = false;
        break;
    }

    case EBreakerCoverState::Relocating:
    {
        SetCrouched(true);
        if (!bUsesCover)
        {
            // No cover discipline: hold a loose range and shoot. Deliberately
            // simple — the interesting version of this archetype is the one
            // with the discipline, and the degraded one should read as worse
            // at fighting, not as a second design.
            CoverState = EBreakerCoverState::Exposed;
            StateElapsed = 0.0f;
            break;
        }
        if (!bHasCoverPoint)
        {
            bHasCoverPoint = FindCoverPoint(ThreatLocation, CoverPoint);
            if (!bHasCoverPoint)
            {
                // Genuinely no cover in range — an open field. It stands and
                // fights rather than freezing, and the state label says so, so
                // a playtester can tell "the map has no cover" from "the cover
                // logic is broken".
                StateLabel = TEXT("NO COVER");
                CoverState = EBreakerCoverState::Exposed;
                StateElapsed = 0.0f;
                break;
            }
        }

        OutDirection = (CoverPoint - GetActorLocation()).GetSafeNormal2D();
        OutSpeedScale = RelocateSpeedMultiplier;
        // It runs facing where it is going, not at the player. The exposed back
        // during a relocation is the punish the archetype is teaching.
        DesiredFacing = OutDirection;
        StateLabel = UBreakerCoverLibrary::GetCoverStateName(CoverState);

        const bool bArrived = FVector::Dist2D(GetActorLocation(), CoverPoint) <= CoverArrivalToleranceCm;
        if (bArrived || StateElapsed >= RelocateTimeoutSeconds)
        {
            CoverState = EBreakerCoverState::InCover;
            StateElapsed = 0.0f;
            // A timed-out relocation drops the point so the next cycle picks a
            // fresh one instead of walking into the same wall forever.
            if (!bArrived) bHasCoverPoint = false;
        }
        break;
    }

    case EBreakerCoverState::InCover:
    {
        SetCrouched(true);
        StateLabel = UBreakerCoverLibrary::GetCoverStateName(CoverState);
        // It will not sit behind cover the player has already walked around:
        // if the line is open it stands up early, which is what stops a push
        // from being answered by hiding.
        const bool bStillCovered = bUsesCover && IsBlockedFrom(ThreatLocation, GetActorLocation());
        if (StateElapsed >= PeekDelaySeconds || !bStillCovered)
        {
            CoverState = EBreakerCoverState::Exposed;
            StateElapsed = 0.0f;
            AimElapsed = 0.0f;
            RoundsLeftInBurst = 0;
            RoundTimer = 0.0f;
        }
        break;
    }

    case EBreakerCoverState::Exposed:
    default:
    {
        SetCrouched(false);
        StateLabel = UBreakerCoverLibrary::GetCoverStateName(EBreakerCoverState::Exposed);

        if (RoundsLeftInBurst > 0)
        {
            RoundTimer -= DeltaSeconds;
            if (RoundTimer <= 0.0f)
            {
                FireRound(Player);
                --RoundsLeftInBurst;
                RoundTimer = RoundIntervalSeconds;
                UpdateMuzzle(1.0f);
            }
            else
            {
                // Decay between rounds so the burst reads as three flashes
                // rather than one continuous glow.
                UpdateMuzzle(FMath::Max(0.0f, RoundTimer / FMath::Max(RoundIntervalSeconds, 0.01f)));
            }
        }
        else
        {
            AimElapsed += DeltaSeconds;
            UpdateMuzzle(UBreakerRangedBehaviorLibrary::GetTelegraphAlpha(AimElapsed, AimSeconds) * 0.6f);
            if (AimElapsed >= AimSeconds + BurstCooldownSeconds)
            {
                AimElapsed = 0.0f;
                RoundsLeftInBurst = FMath::Max(1, RoundsPerBurst);
                RoundTimer = 0.0f;
            }
        }

        // Back down on its own clock even if never hit, so a player who cannot
        // land a shot still learns the loop has a rhythm.
        if (bUsesCover && StateElapsed >= MaximumExposureSeconds && RoundsLeftInBurst <= 0)
        {
            CoverState = EBreakerCoverState::Relocating;
            StateElapsed = 0.0f;
            bHasCoverPoint = false;
            UpdateMuzzle(0.0f);
        }
        // Without cover discipline it still respects the band, so it does not
        // stand in the player's face.
        else if (!bUsesCover)
        {
            if (Distance > Cover.PreferredMaxRangeCm) OutDirection = ToPlayer;
            else if (Distance < Cover.PreferredMinRangeCm) OutDirection = -ToPlayer;
        }
        break;
    }
    }
}

void ABreakerSkirmisherEnemy::FireRound(const AActor* Target)
{
    UWorld* World = GetWorld();
    if (!World || !HasAuthority() || !Target || !ProjectileClass) return;

    const FVector Muzzle = MuzzleVisual ? MuzzleVisual->GetComponentLocation()
        : GetActorLocation() + GetActorForwardVector() * 60.0f;
    // NO LEAD. A soldier's round is fast and flat, so leading it would make it
    // unavoidable at this speed; LATTICE's partial lead exists precisely
    // because its orb is slow. The two ranged archetypes therefore fail to
    // different kinds of movement, which is the point of having both.
    const FVector Base = (Target->GetActorLocation() - Muzzle).GetSafeNormal();
    if (Base.IsNearlyZero()) return;

    const FVector Direction = FMath::VRandCone(Base, FMath::DegreesToRadians(FMath::Max(0.0f, SpreadDegrees)));

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    SpawnParams.Owner = this;
    SpawnParams.Instigator = this;
    ABreakerEnemyProjectile* Round = World->SpawnActor<ABreakerEnemyProjectile>(
        ProjectileClass, Muzzle, Direction.Rotation(), SpawnParams);
    if (!Round) return;

    // Smaller and paler than the Lattice orb, so the player can tell which
    // family is shooting at them from the round alone.
    Round->VisualScale = 0.30f;
    Round->OrbColor = MuzzleHotColor;
    Round->CollisionRadiusCm = 14.0f;

    FBreakerDamageRequest Shot;
    // Per-round damage is a FRACTION of the chassis attack, so the whole burst
    // is roughly one attack and the archetype is not secretly three times as
    // dangerous as its area level claims.
    Shot.BaseDamage = GetAttackDamage() * FMath::Max(0.0f, DamagePerRoundFraction);
    Shot.DamageFamily = EBreakerDamageFamily::Physical;
    // Enemies do not crit; crit is the player's multiplier (§0).
    Shot.bCanCritical = false;
    Shot.SetInstigator(this);
    Round->InitializeProjectile(Shot, Direction, ProjectileSpeed);
}

void ABreakerSkirmisherEnemy::UpdateMuzzle(float Alpha)
{
    const float Clamped = FMath::Clamp(Alpha, 0.0f, 1.0f);
    if (MuzzleMaterial)
    {
        MuzzleMaterial->SetVectorParameterValue(TEXT("Color"),
            FMath::Lerp(MuzzleIdleColor, MuzzleHotColor, Clamped));
    }
    if (MuzzleLight) MuzzleLight->SetIntensity(MuzzleLightIntensity * Clamped * Clamped);
}
