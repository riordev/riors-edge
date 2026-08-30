#include "Combat/BreakerWardenEnemy.h"

#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerModifierComponent.h"
#include "Combat/BreakerRangedBehavior.h"
#include "Combat/BreakerZoneActor.h"
#include "Components/CapsuleComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

namespace BreakerWardenDetail
{
    // Distinctively prefixed for the unity build.
    static UMaterialInstanceDynamic* BreakerMakeWardenMaterial(UStaticMeshComponent* Mesh, const FLinearColor& Color)
    {
        if (!Mesh) return nullptr;
        UMaterialInterface* Base = LoadObject<UMaterialInterface>(
            nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
        if (!Base) return nullptr;
        UMaterialInstanceDynamic* Dynamic = UMaterialInstanceDynamic::Create(Base, Mesh);
        if (!Dynamic) return nullptr;
        Dynamic->SetVectorParameterValue(TEXT("Color"), Color);
        Mesh->SetMaterial(0, Dynamic);
        return Dynamic;
    }
}

ABreakerWardenEnemy::ABreakerWardenEnemy()
{
    // The heavy wears George (owner's mech-cast ruling, O2 mapping); the boss
    // inherits it, which reads correctly at boss scale until it earns its own
    // body. The shield stays the puzzle — it never depended on the torso.
    BodyMeshAsset = FSoftObjectPath(TEXT("/Game/Breaker/Meshes/enemies/mechs/George/George.George"));
    BodyIdleAnimation = FSoftObjectPath(TEXT("/Game/Breaker/Meshes/enemies/mechs/George/GeorgeRobotArmature_Walk.GeorgeRobotArmature_Walk"));
    BodyDeathAnimation = FSoftObjectPath(TEXT("/Game/Breaker/Meshes/enemies/mechs/George/GeorgeRobotArmature_Death.GeorgeRobotArmature_Death"));
    // §2.3's stat block, expressed as chassis inputs rather than as literals:
    // health 320 against the Skitter's 100 is 3.2x, damage 26 against the melee
    // baseline's 14 is ~1.86x. Both compose through GetMonsterHealth /
    // GetMonsterDamage, so the Warden climbs the area-level curve like
    // everything else and nothing here is a second source of truth.
    //
    // DIVERGENCE [O18], recorded rather than fixed: 3.2x health on a Trash-rank
    // enemy is a long fight against the "trash a little under 1 second" target,
    // even before its frontal armour roughly doubles effective health from the
    // front. §2.3's own answer is that the Warden is not meant to be fought
    // frontally at all and a flanking player does "triple the damage" — whether
    // that closes the gap is a measurement, not an assertion, and O2 freezes it
    // until wave mode reports.
    ArchetypeHealthMultiplier = 3.2f;   // O2 PLACEHOLDER (§2.3)
    ArchetypeDamageMultiplier = 1.86f;  // O2 PLACEHOLDER (§2.3: 26 sweep damage)

    // FAMILY (Assets/story-source.md §1.5). The Warden is Altered, and it is authored at
    // MID severance rather than early. Encounter-Design §2.3 describes an early
    // stage — "still wears insignia and still uses cover" — but the BEHAVIOUR
    // implemented here does not use cover and does not flinch: it advances,
    // holds ground and swings. That is a mid-stage read, and calling it early
    // while it fights like this would make the stage label a lie. The early
    // stage is ABreakerSkirmisherEnemy's, and it is a genuinely different
    // fight, which is the whole value of the axis.
    Family = EBreakerEnemyFamily::Altered;
    SeveranceStage = EBreakerSeveranceStage::Mid;

    MoveSpeed = 260.0f;         // O2 PLACEHOLDER (§2.3)
    DetectionRange = 2600.0f;   // O2 PLACEHOLDER (§2.3)
    AttackRange = SweepRangeCm;
    // Implacable: it advances and never jukes. The weave is the Skitter's
    // identity and giving it to the anchor would blur both.
    WeaveStrength = 0.0f;
    // No lunge either. A Warden that leaps is a fast melee enemy with armour,
    // and the whole point is that it is slow enough to walk around.
    LungeRange = 0.0f;
    LungeSpeedMultiplier = 1.0f;

    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));

    // §2.3: "give it a distinct rectangular shield primitive". It must read as
    // SOMEONE in a way the Vestiges do not, and in graybox that is silhouette.
    ShieldVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ShieldVisual"));
    ShieldVisual->SetupAttachment(BodyCollision);
    // The shield the player SEES is the shield the game SIMULATES. This was
    // NoCollision, so the rectangular slab occupying X 47..57, Y +-47,
    // Z -47..87 stopped nothing — a round aimed square at it sailed through
    // to whatever was behind, which read as broken hit recognition. It now
    // blocks the player weapon trace channel exactly the way BodyHitBox does
    // (ignore everything, block ECC_GameTraceChannel2, query only): a shot
    // into the shield face registers as a frontal hit on the Warden and pays
    // the frontal armour, which is the archetype's whole lesson. QueryOnly so
    // it never pushes physics; movement and enemy projectiles are untouched.
    ShieldVisual->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    ShieldVisual->SetCollisionResponseToAllChannels(ECR_Ignore);
    ShieldVisual->SetCollisionResponseToChannel(ECC_GameTraceChannel2, ECR_Block);
    ShieldVisual->SetRelativeLocation(FVector(52.0f, 0.0f, 20.0f));
    ShieldVisual->SetRelativeScale3D(FVector(0.10f, 0.95f, 1.35f));
    if (CubeMesh.Succeeded()) ShieldVisual->SetStaticMesh(CubeMesh.Object);

    // The slam preview. A flat disc that grows to the real radius, so what the
    // player sees is exactly what the attack will hit.
    SlamRingVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SlamRingVisual"));
    SlamRingVisual->SetupAttachment(BodyCollision);
    SlamRingVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    SlamRingVisual->SetRelativeLocation(FVector(0.0f, 0.0f, -88.0f));
    SlamRingVisual->SetVisibility(false, true);
    if (CylinderMesh.Succeeded()) SlamRingVisual->SetStaticMesh(CylinderMesh.Object);
}

void ABreakerWardenEnemy::BeginPlay()
{
    Super::BeginPlay();
    AttackRange = SweepRangeCm;
    ShieldRestLocation = ShieldVisual ? ShieldVisual->GetRelativeLocation() : FVector::ZeroVector;
    ShieldMaterial = BreakerWardenDetail::BreakerMakeWardenMaterial(ShieldVisual, ShieldColor);
    SlamRingMaterial = BreakerWardenDetail::BreakerMakeWardenMaterial(SlamRingVisual, SlamRingColor);

    if (Attributes) Attributes->SetArmor(FMath::Max(0.0f, FrontalArmor));
    // The facing rule, published onto the shared combat component. The armour
    // number and the arc live together on the archetype; the PIPELINE just
    // applies whatever it is told, which is what keeps the rule out of six
    // different damage call sites.
    if (Combat) Combat->RearArcArmorMultiplier = FMath::Clamp(RearArmorFraction, 0.0f, 1.0f);

    // §2.3 gives it high stagger resistance. There is no stagger system yet
    // (Encounter-Design OPEN QUESTION 9), so this is recorded and not faked.
}

float ABreakerWardenEnemy::GetSweepDamage() const
{
    // AttackDamage is the chassis output for this archetype at this area level
    // and rank. The sweep IS the baseline attack, so it is that number
    // unmodified — the archetype multiplier in the constructor is where §2.3's
    // 26 lives.
    return AttackDamage;
}

float ABreakerWardenEnemy::GetSlamDamage() const
{
    return AttackDamage * FMath::Max(0.0f, SlamDamageRelativeToSweep);
}

void ABreakerWardenEnemy::SetBodyVisible(bool bVisible)
{
    Super::SetBodyVisible(bVisible);
    if (ShieldVisual)
    {
        ShieldVisual->SetVisibility(bVisible, true);
        // Collision tracks visibility: every path that hides the body (death,
        // Wakeful downed, a Phase modifier's untargetable window) also stops
        // the shield blocking, and every path that shows it re-arms it. An
        // invisible slab that still ate bullets would be the same lie this
        // component just stopped telling, mirrored.
        ShieldVisual->SetCollisionEnabled(bVisible ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
    }
    // The ring is driven by the slam and must never be left on by a death or a
    // respawn, so it goes away regardless of which way bVisible points.
    if (SlamRingVisual) SlamRingVisual->SetVisibility(false, true);
}

FVector ABreakerWardenEnemy::ComputeCappedFacing(const FVector& CurrentForward, const FVector& DesiredDirection,
    float MaxDegreesPerSecond, float DeltaSeconds)
{
    const FVector Current = CurrentForward.GetSafeNormal2D();
    const FVector Desired = DesiredDirection.GetSafeNormal2D();
    if (Desired.IsNearlyZero()) return Current;
    if (Current.IsNearlyZero()) return Desired;

    const float DotClamped = FMath::Clamp(FVector::DotProduct(Current, Desired), -1.0f, 1.0f);
    const float AngleDegrees = FMath::RadiansToDegrees(FMath::Acos(DotClamped));
    const float MaxStepDegrees = FMath::Max(0.0f, MaxDegreesPerSecond) * FMath::Max(0.0f, DeltaSeconds);
    if (AngleDegrees <= MaxStepDegrees || AngleDegrees <= KINDA_SMALL_NUMBER)
    {
        return Desired;
    }

    // Signed by the Z component of the cross product so the Warden turns the
    // SHORT way toward the player rather than always pivoting one direction.
    const float Cross = Current.X * Desired.Y - Current.Y * Desired.X;
    const float SignedStepDegrees = (Cross >= 0.0f ? 1.0f : -1.0f) * MaxStepDegrees;
    return Current.RotateAngleAxis(SignedStepDegrees, FVector::UpVector);
}

void ABreakerWardenEnemy::TickEngagedBehaviour(ABreakerCharacter* Player, float Distance, float DeltaSeconds,
    FVector& OutDirection, float& OutSpeedScale)
{
    if (!Player || !GetWorld()) return;
    const double Now = GetWorld()->GetTimeSeconds();
    const FVector ToPlayer = (Player->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();

    // §2.3: "advances slowly and ALWAYS faces the player" — but "always faces"
    // cannot mean "faces instantly", or the frontal-approach punishment this
    // archetype exists for has no counterplay (owner playtest: "the shield
    // guys instantly turn to you so its hard to hit their weakspot"). The base
    // Tick (BreakerEnemy.cpp) applies DesiredFacing with an unconditional
    // SetActorRotation snap every frame; ComputeCappedFacing is what turns
    // that into a rate-limited turn by feeding it an already-clamped step
    // instead of the raw direction to the player. This is what makes circling
    // it a real contest rather than a free win.
    DesiredFacing = ComputeCappedFacing(GetActorForwardVector(), ToPlayer, MaxTurnRateDegreesPerSecond, DeltaSeconds);
    OutDirection = ToPlayer;
    OutSpeedScale = 1.0f;
    StateLabel = TEXT("ADVANCE");

    // --- Slam: resolves first, because it is the attack that ignores facing
    // and therefore the one a player standing behind the Warden still has to
    // answer. Without it the flank is a completely safe place to stand and the
    // archetype teaches "get behind it and hold the trigger".
    if (bSlamWindup)
    {
        const float Alpha = UBreakerRangedBehaviorLibrary::GetTelegraphAlpha(
            static_cast<float>(Now - SlamWindupStart), SlamWindupSeconds);
        UpdateSlamTelegraph(Alpha, true);
        OutDirection = FVector::ZeroVector;   // it plants
        StateLabel = TEXT("SLAM");
        if (Alpha >= 1.0f)
        {
            bSlamWindup = false;
            LastSlamTime = Now;
            UpdateSlamTelegraph(0.0f, false);
            ResolveSlam();
        }
        return;
    }

    if (Distance <= SlamRadiusCm && (Now - LastSlamTime) >= SlamCooldownSeconds)
    {
        bSlamWindup = true;
        SlamWindupStart = Now;
        OutDirection = FVector::ZeroVector;
        StateLabel = TEXT("SLAM");
        UpdateSlamTelegraph(0.0f, true);
        return;
    }

    // --- Sweep: the frontal arc.
    if (bSweepWindup)
    {
        const float Alpha = UBreakerRangedBehaviorLibrary::GetTelegraphAlpha(
            static_cast<float>(Now - SweepWindupStart), SweepWindupSeconds);
        UpdateSweepTelegraph(Alpha);
        OutSpeedScale = SweepWindupMoveScale;
        StateLabel = TEXT("SWEEP");
        if (Alpha >= 1.0f)
        {
            bSweepWindup = false;
            LastSweepTime = Now;
            UpdateSweepTelegraph(0.0f);
            ResolveSweep(Player);
        }
        return;
    }

    if (Distance <= SweepRangeCm && (Now - LastSweepTime) >= SweepCooldownSeconds)
    {
        bSweepWindup = true;
        SweepWindupStart = Now;
        OutSpeedScale = SweepWindupMoveScale;
        StateLabel = TEXT("SWEEP");
    }
}

void ABreakerWardenEnemy::UpdateSweepTelegraph(float Alpha)
{
    if (!ShieldVisual) return;
    // The arm draws BACK and the shield heats. Two channels for one tell, so it
    // survives being read from an angle where the motion is foreshortened.
    ShieldVisual->SetRelativeLocation(ShieldRestLocation - FVector(ShieldDrawBackCm * Alpha, 0.0f, 0.0f));
    if (ShieldMaterial)
    {
        ShieldMaterial->SetVectorParameterValue(TEXT("Color"),
            FMath::Lerp(ShieldColor, SweepHotColor, Alpha));
    }
}

void ABreakerWardenEnemy::UpdateSlamTelegraph(float Alpha, bool bVisible)
{
    if (!SlamRingVisual) return;
    SlamRingVisual->SetVisibility(bVisible, true);
    if (!bVisible) return;
    // The engine cylinder is 100 cm across, so the XY scale is radius/50. The
    // ring grows to EXACTLY SlamRadiusCm and stops: the preview is the hitbox,
    // and a preview that lies is worse than no preview at all.
    const float Scale = SlamRadiusCm / 50.0f * Alpha;
    SlamRingVisual->SetRelativeScale3D(FVector(FMath::Max(Scale, 0.01f), FMath::Max(Scale, 0.01f), 0.02f));
    if (SlamRingMaterial)
    {
        SlamRingMaterial->SetVectorParameterValue(TEXT("Color"), SlamRingColor * (0.35f + 0.65f * Alpha));
    }
}

void ABreakerWardenEnemy::ResolveSweep(AActor* Target)
{
    if (!Target) return;
    // The arc is re-checked at RESOLUTION, not at wind-up. That is the
    // counterplay: a player who walks out of the frontal cone during the 0.5s
    // draw-back is not hit, which is what makes "circle it" a real answer under
    // O1's passive defence.
    const FVector ToTarget = (Target->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
    if (FVector::Dist2D(Target->GetActorLocation(), GetActorLocation()) > SweepRangeCm * 1.15f) return;
    const float Cosine = FVector::DotProduct(GetActorForwardVector().GetSafeNormal2D(), ToTarget);
    if (Cosine < FMath::Cos(FMath::DegreesToRadians(FMath::Clamp(SweepHalfAngleDegrees, 0.0f, 180.0f)))) return;

    UBreakerCombatComponent* TargetCombat = Target->FindComponentByClass<UBreakerCombatComponent>();
    if (!TargetCombat) return;

    FBreakerDamageRequest Damage;
    Damage.BaseDamage = GetSweepDamage();
    Damage.DamageFamily = EBreakerDamageFamily::Physical;
    Damage.bCanCritical = false;
    Damage.SourceLocation = GetActorLocation();
    Damage.bHasSourceLocation = true;
    Damage.SetInstigator(this);
    TargetCombat->ReceiveDamage(Damage);

    // Modifiers hang off a LANDED hit, exactly as they do on the base melee
    // attack, so a Warden carrying Anchored or Cascading behaves consistently
    // with a Skitter carrying them.
    if (UBreakerEnemyModifierComponent* Mods = GetModifierComponent())
    {
        Mods->NotifyAttackLanded(Target->GetActorLocation());
    }
}

void ABreakerWardenEnemy::ResolveSlam()
{
    if (!GetWorld() || !HasAuthority()) return;
    const FVector Center = GetActorLocation();
    const float RadiusSq = FMath::Square(FMath::Max(0.0f, SlamRadiusCm));

    // Pawns only, and never another enemy: same friendly-fire rule the death
    // chain, the enemy projectile and the Volatile detonation already use.
    for (TActorIterator<APawn> It(GetWorld()); It; ++It)
    {
        APawn* Candidate = *It;
        if (!Candidate || Candidate == this || Candidate->IsA<ABreakerEnemy>()) continue;
        if (FVector::DistSquared(Candidate->GetActorLocation(), Center) > RadiusSq) continue;
        UBreakerCombatComponent* TargetCombat = Candidate->FindComponentByClass<UBreakerCombatComponent>();
        if (!TargetCombat) continue;

        FBreakerDamageRequest Damage;
        Damage.BaseDamage = GetSlamDamage();
        Damage.DamageFamily = EBreakerDamageFamily::Physical;
        Damage.bCanCritical = false;
        Damage.SourceLocation = Center;
        Damage.bHasSourceLocation = true;
        Damage.SetInstigator(this);
        TargetCombat->ReceiveDamage(Damage);

        if (UBreakerEnemyModifierComponent* Mods = GetModifierComponent())
        {
            Mods->NotifyAttackLanded(Candidate->GetActorLocation());
        }
    }

    if (!bSlamLeavesHazard) return;

    SlamHazards.RemoveAll([](const TWeakObjectPtr<ABreakerZoneActor>& Zone) { return !Zone.IsValid(); });
    if (SlamHazards.Num() >= FMath::Max(1, MaximumLiveSlamHazards))
    {
        if (ABreakerZoneActor* Oldest = SlamHazards[0].Get()) Oldest->Destroy();
        SlamHazards.RemoveAt(0);
    }

    FBreakerZoneSpec Spec;
    Spec.RadiusCm = SlamRadiusCm;
    Spec.HalfHeightCm = 250.0f;
    Spec.Duration = FMath::Max(0.0f, SlamHazardSeconds);
    Spec.TickInterval = FMath::Max(0.05f, SlamHazardTickInterval);
    Spec.ZoneColor = SlamRingColor;
    Spec.TickDamage.BaseDamage = GetSlamDamage() * FMath::Max(0.0f, SlamHazardTickFractionOfSlam);
    Spec.TickDamage.DamageFamily = EBreakerDamageFamily::Physical;
    Spec.TickDamage.bCanCritical = false;

    if (ABreakerZoneActor* Zone = GetWorld()->SpawnActor<ABreakerZoneActor>(
        ABreakerZoneActor::StaticClass(), Center, FRotator::ZeroRotator))
    {
        Zone->ConfigureZone(Spec, this);
        SlamHazards.Add(Zone);
    }
}
