#include "Combat/BreakerEnemy.h"

#include "Items/BreakerAffixLibrary.h"

#include "AbilitySystemComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerModifierComponent.h"
#include "Combat/BreakerStatusComponent.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EngineUtils.h"
#include "Game/BreakerGameMode.h"
#include "Items/BreakerEquipmentComponent.h"
#include "Items/BreakerDropTable.h"
#include "Items/BreakerLootLibrary.h"
#include "Items/BreakerLootPickup.h"
#include "Playtest/BreakerPlaytestComponent.h"
#include "Weapons/BreakerWeaponComponent.h"
#include "TimerManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
    // Muted Vestige-ish grey-violet for the humanoid body parts. Local copy of
    // the dressing helper so the enemy never pulls in game mode internals.
    void ApplyEnemyBodyColor(UStaticMeshComponent* Mesh)
    {
        if (!Mesh) return;
        UMaterialInterface* BaseMaterial = LoadObject<UMaterialInterface>(
            nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
        if (!BaseMaterial) return;
        if (UMaterialInstanceDynamic* Dynamic = UMaterialInstanceDynamic::Create(BaseMaterial, Mesh))
        {
            Dynamic->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.35f, 0.32f, 0.42f));
            Mesh->SetMaterial(0, Dynamic);
        }
    }
}

ABreakerEnemy::ABreakerEnemy()
{
    PrimaryActorTick.bCanEverTick = true;
    bReplicates = true;
    BodyCollision = CreateDefaultSubobject<UCapsuleComponent>(TEXT("BodyCollision"));
    SetRootComponent(BodyCollision);
    BodyCollision->InitCapsuleSize(45.0f, 90.0f);
    BodyCollision->SetCollisionResponseToChannel(ECC_GameTraceChannel2, ECR_Ignore);

    // Humanoid silhouette from basic shapes: torso, head, two arms, two legs.
    // Purely cosmetic — every piece is NoCollision and the capsule, hit box
    // and weak point keep doing all the collision work. Elites inherit the
    // actor scale multiplier, so an elite simply reads as a bigger humanoid.
    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> BodySphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));

    // BodyVisual is the torso now (was the single cylinder body).
    BodyVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyVisual"));
    BodyVisual->SetupAttachment(BodyCollision);
    BodyVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    BodyVisual->SetRelativeLocation(FVector(0.0f, 0.0f, 22.0f));
    BodyVisual->SetRelativeScale3D(FVector(0.55f, 0.36f, 0.78f));
    if (CubeMesh.Succeeded()) BodyVisual->SetStaticMesh(CubeMesh.Object);
    ApplyEnemyBodyColor(BodyVisual);

    HeadVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HeadVisual"));
    HeadVisual->SetupAttachment(BodyCollision);
    HeadVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    HeadVisual->SetRelativeLocation(FVector(0.0f, 0.0f, 78.0f));
    HeadVisual->SetRelativeScale3D(FVector(0.34f));
    if (BodySphereMesh.Succeeded()) HeadVisual->SetStaticMesh(BodySphereMesh.Object);
    ApplyEnemyBodyColor(HeadVisual);

    LeftArmVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LeftArmVisual"));
    LeftArmVisual->SetupAttachment(BodyCollision);
    LeftArmVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    LeftArmVisual->SetRelativeLocation(FVector(0.0f, -34.0f, 24.0f));
    LeftArmVisual->SetRelativeRotation(FRotator(0.0f, 0.0f, 12.0f));
    LeftArmVisual->SetRelativeScale3D(FVector(0.18f, 0.18f, 0.62f));
    if (CubeMesh.Succeeded()) LeftArmVisual->SetStaticMesh(CubeMesh.Object);
    ApplyEnemyBodyColor(LeftArmVisual);

    RightArmVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RightArmVisual"));
    RightArmVisual->SetupAttachment(BodyCollision);
    RightArmVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    RightArmVisual->SetRelativeLocation(FVector(0.0f, 34.0f, 24.0f));
    RightArmVisual->SetRelativeRotation(FRotator(0.0f, 0.0f, -12.0f));
    RightArmVisual->SetRelativeScale3D(FVector(0.18f, 0.18f, 0.62f));
    if (CubeMesh.Succeeded()) RightArmVisual->SetStaticMesh(CubeMesh.Object);
    ApplyEnemyBodyColor(RightArmVisual);

    LeftLegVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LeftLegVisual"));
    LeftLegVisual->SetupAttachment(BodyCollision);
    LeftLegVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    LeftLegVisual->SetRelativeLocation(FVector(0.0f, -14.0f, -50.0f));
    LeftLegVisual->SetRelativeScale3D(FVector(0.22f, 0.22f, 0.80f));
    if (CubeMesh.Succeeded()) LeftLegVisual->SetStaticMesh(CubeMesh.Object);
    ApplyEnemyBodyColor(LeftLegVisual);

    RightLegVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RightLegVisual"));
    RightLegVisual->SetupAttachment(BodyCollision);
    RightLegVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    RightLegVisual->SetRelativeLocation(FVector(0.0f, 14.0f, -50.0f));
    RightLegVisual->SetRelativeScale3D(FVector(0.22f, 0.22f, 0.80f));
    if (CubeMesh.Succeeded()) RightLegVisual->SetStaticMesh(CubeMesh.Object);
    ApplyEnemyBodyColor(RightLegVisual);

    BodyHitBox = CreateDefaultSubobject<UBoxComponent>(TEXT("BodyHitBox"));
    BodyHitBox->SetupAttachment(BodyCollision);
    BodyHitBox->SetBoxExtent(FVector(42.0f, 42.0f, 58.0f));
    BodyHitBox->SetRelativeLocation(FVector(0.0f, 0.0f, 4.0f));
    BodyHitBox->SetCollisionResponseToAllChannels(ECR_Ignore);
    BodyHitBox->SetCollisionResponseToChannel(ECC_GameTraceChannel2, ECR_Block);

    WeakPoint = CreateDefaultSubobject<USphereComponent>(TEXT("WeakPoint"));
    WeakPoint->SetupAttachment(BodyCollision);
    WeakPoint->SetRelativeLocation(FVector(0.0f, 0.0f, 78.0f));
    WeakPoint->SetSphereRadius(20.0f);
    WeakPoint->ComponentTags.Add(TEXT("WeakPoint"));
    WeakPoint->SetCollisionResponseToAllChannels(ECR_Ignore);
    WeakPoint->SetCollisionResponseToChannel(ECC_GameTraceChannel2, ECR_Block);

    WeakPointVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WeakPointVisual"));
    WeakPointVisual->SetupAttachment(WeakPoint);
    WeakPointVisual->SetRelativeScale3D(FVector(0.4f));
    WeakPointVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    if (SphereMesh.Succeeded()) WeakPointVisual->SetStaticMesh(SphereMesh.Object);

    AbilitySystem = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystem"));
    AbilitySystem->SetIsReplicated(true);
    Attributes = CreateDefaultSubobject<UBreakerAttributeSet>(TEXT("Attributes"));
    Combat = CreateDefaultSubobject<UBreakerCombatComponent>(TEXT("Combat"));
    Status = CreateDefaultSubobject<UBreakerStatusComponent>(TEXT("Status"));
    // Every enemy carries the modifier component and the overwhelmingly common
    // case is an EMPTY one — no halo, no clocks, no cost beyond the object.
    // Universal rather than opt-in because a modifier must be composable onto
    // ANY enemy (Encounter-Design §1.0: modifiers are a field of the taxonomy,
    // not a subclass).
    ModifierComponent = CreateDefaultSubobject<UBreakerEnemyModifierComponent>(TEXT("Modifiers"));
}

void ABreakerEnemy::BeginPlay()
{
    Super::BeginPlay();
    AbilitySystem->InitAbilityActorInfo(this, this);
    Combat->OnDeath.AddDynamic(this, &ThisClass::HandleDeath);
    Combat->OnDamageReceived.AddDynamic(this, &ThisClass::HandleDamageReceived);
    if (LeashOrigin.IsNearlyZero()) LeashOrigin = GetActorLocation();
    // Captured once, before anything can have scaled them, so Fleetfoot
    // multiplies a base rather than compounding on itself.
    if (BaseMoveSpeed < 0.0f) BaseMoveSpeed = MoveSpeed;
    if (BaseWeaveStrength < 0.0f) BaseWeaveStrength = WeaveStrength;
    if (WeakPointVisual) WeakPointBaseScale = WeakPointVisual->GetRelativeScale3D().X;
    // Health was the literal constant 220 here at every level until O27. It is
    // now a function of the area level this monster belongs to.
    ApplyChassis();
}

void ABreakerEnemy::ApplyChassis()
{
    AreaLevel = UBreakerMonsterChassisLibrary::ClampAreaLevel(AreaLevel);
    // O29. This clamp was 50 and it silently undid the whole endgame ruling:
    // GetDropItemLevel was opened to 120, and then this line put it straight
    // back. EnemyLevel is what GrantLoot hands to the drop pipeline, so NO DROP
    // IN THE SHIPPING GAME carried the deeper ladder - while the automation
    // suite stayed green, because the composition test exercises the library
    // function and never touches an actor.
    //
    // That is the same failure shape as the third jump: a rule that was correct
    // one layer up and dead where the game actually reads it.
    EnemyLevel = UBreakerMonsterChassisLibrary::GetDropItemLevel(AreaLevel)
        + (IsElite() ? FMath::Max(EliteDropItemLevelBonus, 0) : 0);
    EnemyLevel = FMath::Clamp(EnemyLevel, 1, UBreakerAffixLibrary::MaxItemLevel);

    AttackDamage = UBreakerMonsterChassisLibrary::GetMonsterDamage(
        AreaLevel, MonsterRank, Chassis, ArchetypeDamageMultiplier);

    if (Attributes)
    {
        // The modifier count step composes with the archetype ratio and the
        // rank row rather than replacing either: rank says what a
        // ModifierBearing enemy is worth, the archetype says what a Lattice is
        // worth, and the count step is Encounter-Design §1.1's "+0.35x per
        // modifier beyond the first". Three inputs, one product, no second
        // source of truth for any of them.
        // ApplyMaxHealth, not SetMaxHealth: the generated setter ensures with no
        // owning ASC, which made an enemy untestable outside a world. Live
        // behaviour is identical -- both run PreAttributeChange.
        Attributes->ApplyMaxHealth(UBreakerMonsterChassisLibrary::GetMonsterHealth(
            AreaLevel, MonsterRank, Chassis, ArchetypeHealthMultiplier * ModifierCountHealthMultiplier));
        if (Combat) Combat->RestoreVitals();
    }
}

float ABreakerEnemy::GetMonsterMaxHealth() const
{
    return Attributes ? Attributes->GetMaxHealth() : 0.0f;
}

bool ABreakerEnemy::UsesCoverDiscipline() const
{
    return UBreakerEnemyFamilyLibrary::StageUsesCover(Family, SeveranceStage);
}

bool ABreakerEnemy::FlinchesWhenHit() const
{
    return UBreakerEnemyFamilyLibrary::StageFlinches(Family, SeveranceStage);
}

int32 ABreakerEnemy::ConfigureWithModifiers(int32 Seed)
{
    if (!ModifierComponent) return 0;
    // The family gate is passed down, so a Vestige never rolls a tactical
    // modifier and an Altered never rolls an alien-body one.
    const TArray<EBreakerEnemyModifier> Granted = ModifierComponent->RollAndApplyModifiers(Seed, Family);
    if (!Granted.IsEmpty())
    {
        MonsterRank = UBreakerEnemyModifierLibrary::GetRankForModifierCount(Granted.Num());
        ModifierCountHealthMultiplier = UBreakerEnemyModifierLibrary::GetModifierCountHealthMultiplier(
            Granted.Num(), ModifierComponent->Params);
        ApplyChassis();
        // The ward is sized off max health, so it has to be re-derived AFTER
        // the chassis rebuild that the rank promotion just caused.
        ModifierComponent->SetModifiers(Granted);
        StateLabel = TEXT("PATROL");
    }
    return Granted.Num();
}

bool ABreakerEnemy::ConfigureWithExactModifiers(const TArray<EBreakerEnemyModifier>& InModifiers)
{
    if (!ModifierComponent || !ModifierComponent->SetModifiers(InModifiers)) return false;
    MonsterRank = UBreakerEnemyModifierLibrary::GetRankForModifierCount(InModifiers.Num());
    ModifierCountHealthMultiplier = UBreakerEnemyModifierLibrary::GetModifierCountHealthMultiplier(
        InModifiers.Num(), ModifierComponent->Params);
    ApplyChassis();
    ModifierComponent->SetModifiers(InModifiers);
    return true;
}

void ABreakerEnemy::SetModifierShield(float Amount)
{
    if (!Attributes) return;
    const float Clamped = FMath::Max(0.0f, Amount);
    Attributes->SetMaxShield(Clamped);
    Attributes->SetShield(Clamped);
}

void ABreakerEnemy::AddModifierShield(float Amount)
{
    if (!Attributes || Amount <= 0.0f) return;
    Attributes->SetShield(FMath::Min(Attributes->GetMaxShield(), Attributes->GetShield() + Amount));
}

void ABreakerEnemy::ApplyModifierMovementProfile(float SpeedMultiplier, float WeaveStrengthOverride)
{
    if (BaseMoveSpeed < 0.0f) BaseMoveSpeed = MoveSpeed;
    if (BaseWeaveStrength < 0.0f) BaseWeaveStrength = WeaveStrength;
    MoveSpeed = BaseMoveSpeed * FMath::Max(0.0f, SpeedMultiplier);
    WeaveStrength = WeaveStrengthOverride >= 0.0f ? WeaveStrengthOverride : BaseWeaveStrength;
}

void ABreakerEnemy::ApplyModifierSlowToTarget(float SpeedMultiplier, float Duration)
{
    AActor* Target = ModifierTrackedTarget.Get();
    if (!Target || Duration <= 0.0f) return;
    // Through the movement layer's own keyed push/pop, so the slow composes
    // with everything else that touches speed and expires on its own clock
    // instead of needing this enemy to survive long enough to remove it.
    if (UBreakerCharacterMovementComponent* Movement =
        Target->FindComponentByClass<UBreakerCharacterMovementComponent>())
    {
        Movement->PushSpeedMultiplier(TEXT("Modifier.Anchored"), SpeedMultiplier, Duration);
    }
}

void ABreakerEnemy::SetModifierUntargetable(bool bUntargetable)
{
    const ECollisionEnabled::Type Mode = bUntargetable
        ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryOnly;
    if (BodyHitBox) BodyHitBox->SetCollisionEnabled(Mode);
    if (WeakPoint) WeakPoint->SetCollisionEnabled(Mode);
    SetBodyVisible(!bUntargetable);
}

void ABreakerEnemy::ConfigureAsSplitCopy(int32 InAreaLevel, float HealthFraction)
{
    bDropsLoot = false;
    bRespawns = false;
    // A copy that chain-detonates would make Splitting a pack-clearing gift.
    bExplodesOnDeath = false;
    SetActorScale3D(GetActorScale3D() * 0.7f);
    // Rank Trash, no modifiers: this is what stops a split from splitting.
    MonsterRank = EBreakerMonsterRank::Trash;
    ModifierCountHealthMultiplier = 1.0f;
    SetAreaLevel(InAreaLevel);
    if (Attributes)
    {
        Attributes->SetHealth(Attributes->GetMaxHealth() * FMath::Clamp(HealthFraction, 0.01f, 1.0f));
    }
    StateLabel = TEXT("SPLIT");
}

void ABreakerEnemy::SetAreaLevel(int32 NewAreaLevel)
{
    AreaLevel = UBreakerMonsterChassisLibrary::ClampAreaLevel(NewAreaLevel);
    ApplyChassis();
}

void ABreakerEnemy::SetMonsterRank(EBreakerMonsterRank NewRank)
{
    MonsterRank = NewRank;
    ApplyChassis();
}

void ABreakerEnemy::ConfigureWave(int32 NewAreaLevel)
{
    bRespawns = false;
    SetAreaLevel(NewAreaLevel);
}

UAbilitySystemComponent* ABreakerEnemy::GetAbilitySystemComponent() const { return AbilitySystem; }

void ABreakerEnemy::ConfigureEncounter(const FVector& NewLeashOrigin, float NewPatrolPhase)
{
    LeashOrigin = NewLeashOrigin;
    PatrolPhase = NewPatrolPhase;
}

void ABreakerEnemy::ConfigureElite()
{
    // The elite's health and damage numbers used to live right here — a
    // hardcoded 440 health and a *= 1.5f damage, a second source of truth
    // sitting alongside the base chassis' hardcoded 220. Both are gone: rank
    // is now a row in the chassis rank table, and ApplyChassis composes it.
    // What stays here is what is genuinely elite PRESENTATION and BEHAVIOUR:
    // the bigger silhouette, the slower implacable advance, the loot floor.
    MonsterRank = EBreakerMonsterRank::Elite;
    SetActorScale3D(GetActorScale3D() * 1.25f);
    MoveSpeed *= 0.85f;
    ApplyChassis();
    StateLabel = TEXT("ELITE PATROL");
}

FString ABreakerEnemy::GetEnemyStateLabel() const
{
    // The modifier banner rides on the state label the HUD already prints over
    // every enemy's head. That is deliberate: Encounter-Design §1.2's first
    // acceptance test is that a modifier is identifiable within 1.5s of the
    // enemy entering view, and an unannounced modifier is an unfair death
    // rather than a challenge. Reusing the existing readout means the
    // announcement cannot be forgotten by a UI pass that does not know about
    // modifiers, and an unmodified enemy's label is byte-identical to before.
    FString Label = StateLabel;
    if (ModifierComponent)
    {
        const FString Banner = ModifierComponent->GetBanner();
        if (!Banner.IsEmpty()) Label = Banner + TEXT("\n") + Label;
    }
    // The family line is printed only for the ALTERED. A Vestige is the
    // baseline and labelling every trash mob "VESTIGE" would be noise; an
    // Altered is the exception, and Story-Source §1.5's severance stage is
    // meant to be READABLE, so the stage rides where the player is already
    // looking. This is also the only place the militia's engage-on-sight
    // tragedy is visible in gameplay: the readout says what stage it is, and
    // the player still has to kill it.
    if (Family == EBreakerEnemyFamily::Altered)
    {
        Label = UBreakerEnemyFamilyLibrary::GetFamilyBanner(Family, SeveranceStage) + TEXT("\n") + Label;
    }
    return Label;
}

void ABreakerEnemy::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!HasAuthority() || !GetWorld()) return;
    if (bDead)
    {
        // A Volatile corpse still has a fuse to run, and its strobe is the only
        // warning the player gets. Everything else about a dead enemy stops.
        if (ModifierComponent) ModifierComponent->AdvanceModifiers(DeltaSeconds);
        return;
    }

    ABreakerCharacter* NearestPlayer = nullptr;
    float NearestDistanceSq = TNumericLimits<float>::Max();
    for (TActorIterator<ABreakerCharacter> It(GetWorld()); It; ++It)
    {
        const float DistanceSq = FVector::DistSquared2D(GetActorLocation(), It->GetActorLocation());
        if (DistanceSq < NearestDistanceSq)
        {
            NearestDistanceSq = DistanceSq;
            NearestPlayer = *It;
        }
    }

    // Players standing in the safe zone are off-limits, and enemies stop at
    // its edge rather than following them in.
    const ABreakerGameMode* GameMode = GetWorld()->GetAuthGameMode<ABreakerGameMode>();
    if (NearestPlayer && GameMode && GameMode->IsInSafeZone(NearestPlayer->GetActorLocation()))
    {
        NearestPlayer = nullptr;
    }

    // The modifier layer's only view of the world: a bare AActor* whose
    // POSITION it may read. It never learns what class this is, let alone what
    // level or gear it carries (O27).
    ModifierTrackedTarget = NearestPlayer;
    if (ModifierComponent)
    {
        ModifierComponent->SetTrackedTarget(NearestPlayer);
        ModifierComponent->AdvanceModifiers(DeltaSeconds);
    }

    const float Distance = FMath::Sqrt(NearestDistanceSq);
    FVector DesiredDirection = FVector::ZeroVector;
    // Speed multiplier for this frame. 1.0 = the old constant walk.
    float SpeedScale = 1.0f;
    DesiredFacing = FVector::ZeroVector;
    if (NearestPlayer && Distance <= DetectionRange)
    {
        TickEngagedBehaviour(NearestPlayer, Distance, DeltaSeconds, DesiredDirection, SpeedScale);
    }
    else
    {
        PatrolPhase += DeltaSeconds * 0.7f;
        const FVector PatrolTarget = LeashOrigin + FVector(0.0f, FMath::Sin(PatrolPhase) * 350.0f, 0.0f);
        DesiredDirection = (PatrolTarget - GetActorLocation()).GetSafeNormal2D();
        StateLabel = TEXT("PATROL");
    }

    // Facing is decided before movement so an archetype that strafes sideways
    // while aiming at the player still points its muzzle at the player.
    const FVector Facing = DesiredFacing.IsNearlyZero() ? DesiredDirection : DesiredFacing;
    if (!Facing.IsNearlyZero()) SetActorRotation(Facing.Rotation());

    if (!DesiredDirection.IsNearlyZero())
    {
        const FVector Step = DesiredDirection * MoveSpeed * SpeedScale * DeltaSeconds;
        const FVector NextLocation = GetActorLocation() + Step;
        if (GameMode && GameMode->IsInSafeZone(NextLocation))
        {
            StateLabel = TEXT("HELD");
            return;
        }
        FHitResult MoveHit;
        AddActorWorldOffset(Step, true, &MoveHit);
    }

    // Ground snap: enemies move by offset with no gravity, so without this
    // they hover over the apron slabs or float where spawn height was off.
    // Trace down, plant the capsule base on whatever is below.
    {
        const float HalfHeight = BodyCollision ? BodyCollision->GetScaledCapsuleHalfHeight() : 88.0f;
        const FVector TraceStart = GetActorLocation() + FVector(0, 0, 60.0f);
        const FVector TraceEnd = TraceStart - FVector(0, 0, 4000.0f);
        FCollisionQueryParams GroundParams(SCENE_QUERY_STAT(BreakerEnemyGround), false, this);
        FHitResult Ground;
        if (GetWorld()->LineTraceSingleByChannel(Ground, TraceStart, TraceEnd, ECC_WorldStatic, GroundParams))
        {
            const float TargetZ = Ground.ImpactPoint.Z + HalfHeight;
            const float CurrentZ = GetActorLocation().Z;
            // Snap down instantly, step up smoothly, so slabs read as steps
            // rather than teleports.
            const float NewZ = CurrentZ > TargetZ
                ? FMath::Max(TargetZ, CurrentZ - 1200.0f * DeltaSeconds)
                : FMath::Min(TargetZ, CurrentZ + 600.0f * DeltaSeconds);
            SetActorLocation(FVector(GetActorLocation().X, GetActorLocation().Y, NewZ), false);
        }
    }
}

void ABreakerEnemy::TickEngagedBehaviour(ABreakerCharacter* Player, float Distance, float DeltaSeconds,
    FVector& OutDirection, float& OutSpeedScale)
{
    // The melee chase, in three gears. Extracted verbatim from Tick so a
    // ranged archetype can replace the whole decision without forking the
    // shared target-selection, safe-zone, and ground-snap code around it.
    if (!Player || !GetWorld()) return;
    const double Now = GetWorld()->GetTimeSeconds();

    const FVector ToPlayer = (Player->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
    OutDirection = ToPlayer;
    StateLabel = Distance <= AttackRange ? TEXT("ATTACK") : TEXT("CHASE");
    if (Distance <= AttackRange) PerformAttack(Player);

    // (a) Closing sprint: far away, they commit to closing the gap
    // instead of ambling. Inside SprintRange they drop to normal so the
    // player still gets readable spacing at knife range.
    if (Distance > SprintRange)
    {
        OutSpeedScale = SprintSpeedMultiplier;
        StateLabel = TEXT("CLOSING");
    }

    // (b) Strafe weave: a lateral sinusoid folded into the chase vector.
    // Elites are exempt — the identity is that an elite advances
    // implacably and does not juke (Encounter-Design §1.1 chassis).
    if (!IsElite() && Distance > AttackRange)
    {
        WeaveTime += DeltaSeconds;
        const FVector Lateral = FVector::CrossProduct(FVector::UpVector, ToPlayer).GetSafeNormal2D();
        const float Weave = FMath::Sin((WeaveTime + PatrolPhase) * WeaveFrequency) * WeaveStrength;
        OutDirection = (ToPlayer + Lateral * Weave).GetSafeNormal2D();
    }

    // (c) SKITTER's committed leap (Encounter-Design §2.1). Three stages:
    // wind-up, committed burst, cooldown.
    //
    // What changed from the shipping version, and why: the lunge had no
    // wind-up and re-solved its direction every frame, so it TRACKED the player
    // through the whole burst. That made it unanswerable by movement — there
    // was nothing to step out of — and O1 leaves the player no other defensive
    // input. §2.1 is explicit that "the leap direction is locked at wind-up —
    // it cannot track", and that the wind-up EXPOSES the weak point, so the
    // correct answer becomes "step sideways and shoot the thing it just showed
    // you". Both halves are now real.
    const bool bLungeActive = !bLungeWindingUp && (Now - LungeStartTime) < LungeDuration;

    if (bLungeWindingUp)
    {
        // Crouched and slow. It has already chosen where it is going.
        OutSpeedScale = LungeWindupMoveScale;
        OutDirection = ToPlayer;
        StateLabel = TEXT("WIND-UP");
        if (WeakPointVisual)
        {
            const float Alpha = LungeWindupSeconds > 0.0f
                ? FMath::Clamp(static_cast<float>(Now - LungeWindupStartTime) / LungeWindupSeconds, 0.0f, 1.0f)
                : 1.0f;
            WeakPointVisual->SetRelativeScale3D(FVector(
                FMath::Lerp(WeakPointBaseScale, WeakPointBaseScale * LungeWeakPointSwell, Alpha)));
        }
        if ((Now - LungeWindupStartTime) >= LungeWindupSeconds)
        {
            bLungeWindingUp = false;
            LungeStartTime = Now;
            // THE COMMITMENT. Locked here, once, and never touched again for
            // the duration of the burst.
            LungeLockedDirection = ToPlayer;
            if (WeakPointVisual) WeakPointVisual->SetRelativeScale3D(FVector(WeakPointBaseScale));
        }
    }
    else if (bLungeActive)
    {
        OutSpeedScale = LungeSpeedMultiplier;
        OutDirection = LungeLockedDirection.IsNearlyZero() ? ToPlayer : LungeLockedDirection;
        StateLabel = TEXT("LUNGE");
    }
    else if (Distance <= LungeRange && Distance > AttackRange
        && (Now - LungeStartTime) >= (LungeDuration + LungeCooldown))
    {
        bLungeWindingUp = true;
        LungeWindupStartTime = Now;
        OutSpeedScale = LungeWindupMoveScale;
        StateLabel = TEXT("WIND-UP");
    }
}

void ABreakerEnemy::PerformAttack(APawn* TargetPawn)
{
    if (!TargetPawn || !GetWorld() || GetWorld()->GetTimeSeconds() - LastAttackTime < AttackCooldown) return;
    UBreakerCombatComponent* TargetCombat = TargetPawn->FindComponentByClass<UBreakerCombatComponent>();
    if (!TargetCombat) return;
    FBreakerDamageRequest Damage;
    Damage.BaseDamage = AttackDamage;
    Damage.DamageFamily = EBreakerDamageFamily::Physical;
    Damage.bCanCritical = false;
    Damage.SetInstigator(this);
    TargetCombat->ReceiveDamage(Damage);
    LastAttackTime = GetWorld()->GetTimeSeconds();
    // Anchored's slow and Cascading's hazard both hang off a LANDED hit rather
    // than a swing, so a whiff costs the player nothing.
    if (ModifierComponent) ModifierComponent->NotifyAttackLanded(TargetPawn->GetActorLocation());
}

void ABreakerEnemy::SetBodyVisible(bool bVisible)
{
    for (UStaticMeshComponent* Part : { BodyVisual.Get(), HeadVisual.Get(), LeftArmVisual.Get(),
        RightArmVisual.Get(), LeftLegVisual.Get(), RightLegVisual.Get(), WeakPointVisual.Get() })
    {
        if (Part) Part->SetVisibility(bVisible, true);
    }
}

void ABreakerEnemy::HandleDeath()
{
    // WAKEFUL runs first, and it runs by an explicit call rather than by
    // binding OnDeath alongside this handler. Delegate broadcast order is
    // registration order, which is an accident of component initialisation and
    // not a contract — and a modifier that SUPPRESSES a death cannot be allowed
    // to run after the death has already dropped loot and fed the TTK sample.
    if (ModifierComponent && ModifierComponent->TryConsumeWakefulRevive(bLastHitWasWeakPoint))
    {
        EnterWakefulDowned();
        return;
    }

    bDead = true;
    StateLabel = TEXT("DEAD");
    BodyCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    BodyHitBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    WeakPoint->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    SetBodyVisible(false);
    if (HasAuthority() && bDropsLoot) GrantLoot();
    if (HasAuthority()) GrantAmmo();

    // On-death chain detonation: hurts other enemies only, so packed
    // spawns cascade without turning the player's own kills against them.
    if (HasAuthority() && bExplodesOnDeath && Attributes)
    {
        const float ExplosionDamage = Attributes->GetMaxHealth() * DeathExplosionHealthFraction;
        for (TActorIterator<ABreakerEnemy> It(GetWorld()); It; ++It)
        {
            if (*It == this || It->bDead) continue;
            if (FVector::DistSquared(It->GetActorLocation(), GetActorLocation()) > FMath::Square(DeathExplosionRadius)) continue;
            FBreakerDamageRequest ChainDamage;
            ChainDamage.BaseDamage = ExplosionDamage;
            ChainDamage.DamageFamily = EBreakerDamageFamily::Physical;
            ChainDamage.bCanCritical = false;
            ChainDamage.SourceLocation = GetActorLocation();
            ChainDamage.bHasSourceLocation = true;
            ChainDamage.SetInstigator(this);
            It->Combat->ReceiveDamage(ChainDamage);
        }
    }

    // Feed the time-to-kill instrument (Decisions.md O2).
    if (FirstDamageTime >= 0.0 && GetWorld())
    {
        APawn* PlayerPawn = GetWorld()->GetFirstPlayerController() ? GetWorld()->GetFirstPlayerController()->GetPawn() : nullptr;
        if (UBreakerPlaytestComponent* Playtest = PlayerPawn ? PlayerPawn->FindComponentByClass<UBreakerPlaytestComponent>() : nullptr)
        {
            // Engagement-gapped TTK: idle stretches between damage events are
            // capped, so target-switching doesn't inflate the sample the way
            // wall-clock first-damage-to-death did (session 3 finding).
            Playtest->AddTimeToKillSample(FMath::Max(EngagedSeconds, 0.05f), IsElite(), IsRangedForTelemetry());
        }
        FirstDamageTime = -1.0;
        LastDamageEventTime = -1.0;
        EngagedSeconds = 0.0f;
    }

    // Volatile's fuse and Splitting's copies. After the loot and the TTK
    // sample, because the kill is real — these are what the corpse does next.
    if (HasAuthority() && ModifierComponent) ModifierComponent->NotifyOwnerDied();

    if (bRespawns) GetWorldTimerManager().SetTimerForNextTick(this, &ThisClass::RespawnEnemy);
    // Long enough for a Volatile fuse to finish before the actor goes away. The
    // old 2.0s was already comfortably past the 1.2s placeholder fuse; this
    // makes the dependency explicit instead of a coincidence.
    else SetLifeSpan(FMath::Max(2.0f,
        ModifierComponent && ModifierComponent->HasModifier(EBreakerEnemyModifier::Volatile)
            ? ModifierComponent->Params.VolatileFuseSeconds + 1.0f : 0.0f));
}

void ABreakerEnemy::EnterWakefulDowned()
{
    // Down, not dead: no loot, no ammo, no chain detonation, no TTK sample, and
    // the enemy is NOT marked bDead — the kill has not happened yet.
    StateLabel = TEXT("DOWNED");
    if (BodyCollision) BodyCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    if (BodyHitBox) BodyHitBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    if (WeakPoint) WeakPoint->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    SetBodyVisible(false);

    const float Delay = ModifierComponent ? ModifierComponent->GetWakefulReviveDelay() : 4.0f;
    FTimerHandle ReviveTimer;
    GetWorldTimerManager().SetTimer(ReviveTimer, this, &ThisClass::FinishWakefulRevive,
        FMath::Max(0.01f, Delay), false);
}

void ABreakerEnemy::FinishWakefulRevive()
{
    const float Fraction = ModifierComponent ? ModifierComponent->GetWakefulReviveHealthFraction() : 0.35f;
    if (BodyCollision) BodyCollision->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    if (BodyHitBox) BodyHitBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    if (WeakPoint) WeakPoint->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    SetBodyVisible(true);
    if (Combat) Combat->RestoreVitals();
    if (Attributes) Attributes->SetHealth(Attributes->GetMaxHealth() * Fraction);
    // The ward does NOT come back with it: a Wakeful Warded enemy would be two
    // full health bars twice, which is exactly the durability stacking §1.3's
    // three-modifier rule exists to prevent.
    if (Attributes && ModifierComponent && ModifierComponent->HasModifier(EBreakerEnemyModifier::Warded))
    {
        Attributes->SetShield(0.0f);
    }
    StateLabel = TEXT("RISEN");
}

void ABreakerEnemy::HandleDamageReceived(const FBreakerDamageResult& Result)
{
    if (!GetWorld() || (Result.HealthDamage <= 0.0f && Result.ShieldDamage <= 0.0f)) return;
    // Wakeful denies its revive to a weak-point killing blow, so the LAST hit's
    // weak-point flag has to survive until HandleDeath reads it.
    bLastHitWasWeakPoint = Result.bWeakPoint;
    const double Now = GetWorld()->GetTimeSeconds();
    if (FirstDamageTime < 0.0) FirstDamageTime = Now;
    if (LastDamageEventTime >= 0.0)
    {
        // Gaps longer than 1.5s are disengagement, not fighting.
        EngagedSeconds += static_cast<float>(FMath::Min(Now - LastDamageEventTime, 1.5));
    }
    LastDamageEventTime = Now;
}

void ABreakerEnemy::GrantLoot()
{
    APawn* PlayerPawn = GetWorld() ? GetWorld()->GetFirstPlayerController() ? GetWorld()->GetFirstPlayerController()->GetPawn() : nullptr : nullptr;
    UBreakerEquipmentComponent* Equipment = PlayerPawn ? PlayerPawn->FindComponentByClass<UBreakerEquipmentComponent>() : nullptr;
    if (!Equipment) return;

    ++KillCount;
    const int32 Seed = HashCombine(GetTypeHash(GetActorLocation()), KillCount);

    // THE DROP PIPELINE (Items/BreakerDropTable.h). This used to be a bare
    // RollRarity call, which meant every death produced an item and the flat
    // rarity table was the whole system — the owner's playtest report from both
    // ends. Now: a per-rank DROP CHANCE step runs first (most trash kills drop
    // nothing at all), then the rarity is rolled against gates on drop item
    // level and monster rank, so a low-level trash kill is structurally
    // incapable of producing an Aberrant.
    EBreakerItemRarity Rarity = EBreakerItemRarity::Standard;
    if (!UBreakerDropTableLibrary::RollDrop(Seed, EnemyLevel, MonsterRank,
        Equipment->GetStats().DropChancePercent, DropTable, Rarity))
    {
        return;
    }

    // The elite floor survives the rewrite. It is a FLOOR on an elite that has
    // already decided to drop, not a second drop chance, so it composes with
    // the gates rather than competing with them: an elite in a level-3 area
    // still cannot exceed what its item level unlocks.
    if (IsElite() && Rarity < EBreakerItemRarity::Exceptional
        && UBreakerDropTableLibrary::IsRarityUnlocked(EBreakerItemRarity::Exceptional, EnemyLevel, MonsterRank, DropTable))
    {
        Rarity = EBreakerItemRarity::Exceptional;
    }

    const EBreakerEquipSlot Slot = static_cast<EBreakerEquipSlot>(FRandomStream(Seed).RandRange(0, static_cast<int32>(EBreakerEquipSlot::Count) - 1));
    const FBreakerItemInstance Item = UBreakerLootLibrary::RollItem(TEXT("GymDrop"), Slot, Rarity, EnemyLevel, Seed);

    // Drops land on the ground now instead of teleporting into the backpack:
    // the player walks over, reads the popup, and presses F. Scatter keeps a
    // stack of kills on one spot from overlapping into a single column.
    FRandomStream ScatterStream(Seed ^ 0x5EED);
    const FVector Scatter(ScatterStream.FRandRange(-80.0f, 80.0f), ScatterStream.FRandRange(-80.0f, 80.0f), 0.0f);
    const FVector DropLocation = GetActorLocation() + Scatter + FVector(0.0f, 0.0f, 40.0f);
    if (ABreakerLootPickup* Pickup = GetWorld()->SpawnActor<ABreakerLootPickup>(ABreakerLootPickup::StaticClass(), DropLocation, FRotator::ZeroRotator))
    {
        Pickup->SetItem(Item);
    }
}

void ABreakerEnemy::GrantAmmo()
{
    // Owner feedback: "ran out of ammo after 3 waves — no way to regain
    // ammo". Kills now feed the gun. O2 placeholders: a normal kill returns
    // 15% of a magazine-weapon's starting reserve, an elite half of it —
    // roughly, sustained accurate play is ammo-neutral and sloppy play still
    // runs dry. Uses the first player pawn, same as GrantLoot.
    const float NormalKillFraction = 0.15f;
    const float EliteKillFraction = 0.50f;

    APawn* PlayerPawn = GetWorld() && GetWorld()->GetFirstPlayerController()
        ? GetWorld()->GetFirstPlayerController()->GetPawn() : nullptr;
    if (UBreakerWeaponComponent* Weapon = PlayerPawn ? PlayerPawn->FindComponentByClass<UBreakerWeaponComponent>() : nullptr)
    {
        Weapon->AddReserveAmmoFraction(IsElite() ? EliteKillFraction : NormalKillFraction);
    }
}

void ABreakerEnemy::RespawnEnemy()
{
    FTimerHandle RespawnTimer;
    GetWorldTimerManager().SetTimer(RespawnTimer, [this]()
    {
        SetActorLocation(LeashOrigin);
        BodyCollision->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        BodyHitBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        WeakPoint->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        SetBodyVisible(true);
        bDead = false;
        FirstDamageTime = -1.0;
        LastDamageEventTime = -1.0;
        EngagedSeconds = 0.0f;
        Combat->RestoreVitals();
    }, RespawnDelay, false);
}
