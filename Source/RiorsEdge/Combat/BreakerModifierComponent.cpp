#include "Combat/BreakerModifierComponent.h"

#include "Attributes/BreakerAttributeSet.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerEnemy.h"
#include "Combat/BreakerZoneActor.h"
#include "Components/CapsuleComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UI/BreakerUIStyle.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"

namespace BreakerModifierRuntime
{
    // Distinctively prefixed for the unity build: a bare `AuraKey` would
    // collide with whatever the next translation unit in the blob declares.
    static const FName BreakerAuraModifierKey(TEXT("Modifier.WardingAura"));
    static const FName BreakerHazardZoneTagName(TEXT("Modifier.Cascading.Hazard"));
}

UBreakerEnemyModifierComponent::UBreakerEnemyModifierComponent()
{
    PrimaryComponentTick.bCanEverTick = false;   // the owner drives AdvanceModifiers
    SetIsReplicatedByDefault(true);
}

void UBreakerEnemyModifierComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(UBreakerEnemyModifierComponent, Modifiers);
    DOREPLIFETIME(UBreakerEnemyModifierComponent, FuseRemaining);
}

void UBreakerEnemyModifierComponent::BeginPlay()
{
    Super::BeginPlay();
    if (UBreakerCombatComponent* Combat = OwnerCombat())
    {
        Combat->OnDamageTaken.AddDynamic(this, &ThisClass::HandleOwnerDamaged);
    }
    // A set granted before BeginPlay (the spawner's normal order) still has to
    // land its persistent half once the components exist.
    if (!Modifiers.IsEmpty()) ApplyPersistentModifiers();
}

void UBreakerEnemyModifierComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    ReleaseExternalEffects();
    Super::EndPlay(Reason);
}

UBreakerCombatComponent* UBreakerEnemyModifierComponent::OwnerCombat() const
{
    return GetOwner() ? GetOwner()->FindComponentByClass<UBreakerCombatComponent>() : nullptr;
}

float UBreakerEnemyModifierComponent::OwnerMaxHealth() const
{
    const ABreakerEnemy* Enemy = Cast<ABreakerEnemy>(GetOwner());
    return Enemy ? Enemy->GetMonsterMaxHealth() : 0.0f;
}

float UBreakerEnemyModifierComponent::OwnerAttackDamage() const
{
    const ABreakerEnemy* Enemy = Cast<ABreakerEnemy>(GetOwner());
    return Enemy ? Enemy->GetAttackDamage() : 0.0f;
}

bool UBreakerEnemyModifierComponent::SetModifiers(const TArray<EBreakerEnemyModifier>& NewModifiers)
{
    FString Reason;
    if (!UBreakerEnemyModifierLibrary::IsLegalModifierSet(NewModifiers, Reason))
    {
        // Loud but not fatal: an illegal set is an authoring bug in the caller,
        // and a spawner that thinks it made a Champion needs to be told it did
        // not rather than quietly getting a Veteran.
        UE_LOG(LogTemp, Warning, TEXT("Rejected an illegal enemy modifier set: %s"), *Reason);
        return false;
    }

    ReleaseExternalEffects();
    bExternalEffectsReleased = false;
    Modifiers = NewModifiers;
    bWakefulSpent = false;
    // A pooled body is re-dressed through here (ReviveFromPool's checklist),
    // so the previous life's killer must not survive into the next one.
    VolatileCreditTo = nullptr;
    FuseRemaining = -1.0f;
    FuseTotal = 0.0f;
    ApplyPersistentModifiers();
    OnModifiersChanged.Broadcast();
    return true;
}

TArray<EBreakerEnemyModifier> UBreakerEnemyModifierComponent::RollAndApplyModifiers(int32 Seed, EBreakerEnemyFamily Family)
{
    const TArray<EBreakerEnemyModifier> Rolled = UBreakerEnemyModifierLibrary::RollModifiers(Seed, Params, Family);
    SetModifiers(Rolled);
    return Rolled;
}

FString UBreakerEnemyModifierComponent::GetBanner() const
{
    return UBreakerEnemyModifierLibrary::GetModifierBanner(Modifiers);
}

void UBreakerEnemyModifierComponent::OnRep_Modifiers()
{
    // Clients read the tell off the replicated list every frame (the
    // nameplate's marks); nothing is rebuilt here. They never run membership,
    // damage or spawning.
    OnModifiersChanged.Broadcast();
}

void UBreakerEnemyModifierComponent::ApplyPersistentModifiers()
{
    ABreakerEnemy* Enemy = Cast<ABreakerEnemy>(GetOwner());

    // WARDED. A real shield on the attribute set, so it routes through the
    // existing shield step in ReceiveDamage — which is also what makes the
    // "DoT ignores the ward" rule free: every status tick already sets
    // bBypassShield, so a bleed build walks straight past this modifier while
    // a burst build has to out-race the recharge.
    if (Enemy && HasModifier(EBreakerEnemyModifier::Warded))
    {
        const float Ward = UBreakerEnemyModifierLibrary::GetWardShieldAmount(OwnerMaxHealth(), Params);
        Enemy->SetModifierShield(Ward);
    }

    // FLEETFOOT. Speed and circling, both applied through the enemy's own
    // mutator rather than by reaching into its protected tuning.
    if (Enemy)
    {
        const bool bFleet = HasModifier(EBreakerEnemyModifier::Fleetfoot);
        Enemy->ApplyModifierMovementProfile(
            bFleet ? FMath::Max(1.0f, Params.FleetfootSpeedMultiplier) : 1.0f,
            bFleet ? FMath::Clamp(Params.FleetfootWeaveStrength, 0.0f, 1.0f) : -1.0f);
    }

    // ANCHORED. There is no stagger or knockback system in the project yet
    // (Encounter-Design OPEN QUESTION 9), so the immunity half of this modifier
    // has nothing to be immune to and is recorded rather than faked. The half
    // that IS live is the slow on hit, applied in NotifyAttackLanded — and that
    // half is the whole design anyway: §1.2 says the slow is the punish.
}

void UBreakerEnemyModifierComponent::UpdateBlastRing()
{
    AActor* Body = GetOwner();
    UWorld* World = Body ? Body->GetWorld() : nullptr;
    if (!Body || !World) return;
    const bool bLit = IsFuseLit();

    if (!bLit)
    {
        if (BlastRingVisual) BlastRingVisual->SetVisibility(false, true);
        return;
    }

    // Built on first use rather than in the constructor: only a Volatile body
    // ever needs one, and every OTHER enemy in a wave would otherwise carry a
    // mesh component it never shows.
    //
    // AN OUTLINE, NOT A DISC, AND THE CAPTURE IS WHY. The first version was a
    // scaled cylinder — the same filled disc the Warden's slam telegraph uses
    // — and at this radius it swallowed a third of the screen in solid orange.
    // The Warden gets away with a fill because his is 650 cm for 0.9 s; this
    // is 900 cm for the whole fuse. So it is a ring of segments: one instanced
    // component, one draw, and the floor stays visible inside it, which is the
    // floor the player is deciding whether to leave.
    if (!BlastRingVisual)
    {
        BlastRingVisual = NewObject<UInstancedStaticMeshComponent>(Body, TEXT("BlastRingVisual"));
        if (!BlastRingVisual) return;
        BlastRingVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        BlastRingVisual->SetCastShadow(false);
        // REGISTER THEN ATTACH, in that order. SetupAttachment is a
        // CONSTRUCTOR-only call and silently does nothing at runtime — an
        // earlier version used it and the component registered unparented.
        BlastRingVisual->RegisterComponent();
        BlastRingVisual->AttachToComponent(Body->GetRootComponent(),
            FAttachmentTransformRules::KeepRelativeTransform);
        if (UStaticMesh* Segment = LoadObject<UStaticMesh>(nullptr,
            TEXT("/Engine/BasicShapes/Cube.Cube")))
        {
            BlastRingVisual->SetStaticMesh(Segment);
        }
        if (UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr,
            TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
        {
            BlastRingMaterial = UMaterialInstanceDynamic::Create(Base, BlastRingVisual);
            BlastRingVisual->SetMaterial(0, BlastRingMaterial);
        }
    }

    // ON THE FLOOR, FOUND BY TRACING FOR IT — not by subtracting the capsule's
    // half-height from the body. An earlier version did that and the capture
    // showed no ring at all: a corpse RAGDOLLS, so after death the capsule
    // stops describing where the body lies and the disc sat at world Z -61,
    // buried under the yard. The player is asking which FLOOR is dangerous, so
    // the floor is the thing to ask.
    const FVector Centre = Body->GetActorLocation();
    FHitResult Ground;
    FCollisionQueryParams GroundQuery(SCENE_QUERY_STAT(BreakerBlastRingFloor), false, Body);
    const bool bFloor = World->LineTraceSingleByChannel(Ground, Centre + FVector(0, 0, 200.0f),
        Centre - FVector(0, 0, 1000.0f), ECC_WorldStatic, GroundQuery);
    const float GroundZ = bFloor ? Ground.ImpactPoint.Z + 4.0f : Centre.Z;

    // The segments are laid once and then left alone: the radius is fixed for
    // the life of the fuse, so rebuilding them per frame would be work for an
    // identical answer.
    const float Radius = FMath::Max(0.0f, Params.VolatileOuterRadiusCm);
    if (BlastRingVisual->GetInstanceCount() == 0 && Radius > 0.0f)
    {
        constexpr int32 Segments = 32;            // O2 PLACEHOLDER
        constexpr float SegmentThicknessCm = 24.0f;   // O2 PLACEHOLDER
        constexpr float SegmentHeightCm = 8.0f;       // O2 PLACEHOLDER
        // Chord of one segment, with a gap: a dashed ring reads as a boundary
        // where a solid one reads as another piece of level geometry.
        const float Chord = 2.0f * PI * Radius / Segments * 0.6f;
        for (int32 Index = 0; Index < Segments; ++Index)
        {
            const float Angle = 2.0f * PI * Index / Segments;
            const FVector Offset(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius, 0.0f);
            const FRotator Facing(0.0f, FMath::RadiansToDegrees(Angle) + 90.0f, 0.0f);
            // The engine cube is 100 cm on a side, so each extent is cm/100.
            BlastRingVisual->AddInstance(FTransform(Facing, Offset,
                FVector(Chord / 100.0f, SegmentThicknessCm / 100.0f, SegmentHeightCm / 100.0f)));
        }
    }
    BlastRingVisual->SetWorldLocation(FVector(Centre.X, Centre.Y, GroundZ));
    BlastRingVisual->SetVisibility(true, true);

    if (BlastRingMaterial)
    {
        // THE PULSE CARRIES THE CLOCK. Remaining fraction drives how fast it
        // blinks, so a fuse about to go reads as frantic without the ring ever
        // misreporting its reach. Squared so the last third is where the
        // acceleration is actually felt.
        const float Remaining = FuseTotal > 0.0f
            ? FMath::Clamp(FuseRemaining / FuseTotal, 0.0f, 1.0f) : 0.0f;
        const float Urgency = (1.0f - Remaining) * (1.0f - Remaining);
        const float Hz = 2.0f + 10.0f * Urgency;   // O2 PLACEHOLDER
        const float Phase = FuseTotal - FuseRemaining;
        const float Blink = 0.55f + 0.45f * FMath::Abs(FMath::Sin(Phase * Hz * PI));
        BlastRingMaterial->SetVectorParameterValue(TEXT("Color"), BreakerUI::Orange * Blink);
    }
}

void UBreakerEnemyModifierComponent::AdvanceModifiers(float DeltaSeconds)
{
    if (Modifiers.IsEmpty() || DeltaSeconds <= 0.0f) return;

    // The Volatile fuse keeps running after the owner is dead — that is the
    // whole modifier — so it is advanced before the live-only clocks.
    if (FuseRemaining > 0.0f)
    {
        FuseRemaining -= DeltaSeconds;
        if (FuseRemaining <= 0.0f)
        {
            FuseRemaining = -1.0f;
            UpdateBlastRing();   // takes the ring away with the fuse
            DetonateVolatile();
        }
        else
        {
            UpdateBlastRing();
        }
        return;
    }

    if (HasModifier(EBreakerEnemyModifier::Warded))     TickWard(DeltaSeconds);
    if (HasModifier(EBreakerEnemyModifier::WardingAura)) TickAura(DeltaSeconds);
    if (HasModifier(EBreakerEnemyModifier::Phasing))    TickPhasing(DeltaSeconds);

    // Cascading hazards clean their own weak entries so the live count that
    // caps them cannot be inflated by zones the world already destroyed.
    LiveHazards.RemoveAll([](const TWeakObjectPtr<ABreakerZoneActor>& Zone) { return !Zone.IsValid(); });
}

void UBreakerEnemyModifierComponent::TickWard(float DeltaSeconds)
{
    UBreakerCombatComponent* Combat = OwnerCombat();
    ABreakerEnemy* Enemy = Cast<ABreakerEnemy>(GetOwner());
    if (!Combat || !Enemy || Combat->IsDead()) return;
    if (Combat->IsBeneficialEffectSuppressed()) return;
    if (Combat->GetSecondsSinceDamage() < FMath::Max(0.0f, Params.WardRechargeDelaySeconds)) return;

    const float Full = UBreakerEnemyModifierLibrary::GetWardShieldAmount(OwnerMaxHealth(), Params);
    if (Full <= 0.0f) return;
    const float Step = Full * FMath::Max(0.0f, Params.WardRechargeRatePerSecond) * DeltaSeconds;
    Enemy->AddModifierShield(Step);
}

void UBreakerEnemyModifierComponent::TickAura(float DeltaSeconds)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || !GetWorld()) return;

    // Cheap cadence rather than every frame: the aura is a radius check over
    // every live enemy, and a pack of 24 running that 60 times a second is
    // 34,000 distance tests for a buff that changes on a walking timescale.
    AuraPulseTimer -= DeltaSeconds;
    if (AuraPulseTimer > 0.0f) return;
    AuraPulseTimer = 0.25f;

    const float RadiusSq = FMath::Square(FMath::Max(0.0f, Params.AuraRadiusCm));
    const float Multiplier = 1.0f - FMath::Clamp(Params.AuraDamageReduction, 0.0f, 1.0f);
    const FVector Center = GetOwner()->GetActorLocation();

    TArray<TWeakObjectPtr<AActor>> StillInside;
    for (TActorIterator<ABreakerEnemy> It(GetWorld()); It; ++It)
    {
        ABreakerEnemy* Ally = *It;
        if (!Ally || Ally == GetOwner() || Ally->IsDeadEnemy()) continue;
        if (FVector::DistSquared(Ally->GetActorLocation(), Center) > RadiusSq) continue;
        if (UBreakerCombatComponent* AllyCombat = Ally->FindComponentByClass<UBreakerCombatComponent>())
        {
            // §1.2: "does not stack with another Warding Aura". Keyed pushes
            // REPLACE, and every aura in the game shares this one key, so two
            // overlapping auras are one aura by construction rather than by a
            // rule somebody has to remember.
            AllyCombat->PushBeneficialIncomingDamageModifier(BreakerModifierRuntime::BreakerAuraModifierKey, Multiplier);
            StillInside.Add(Ally);
        }
    }

    // Anyone who left the radius loses the entry immediately. A buff that
    // outlives its source is a buff the player cannot reason about.
    for (const TWeakObjectPtr<AActor>& Previous : AuraTargets)
    {
        AActor* Actor = Previous.Get();
        if (!Actor || StillInside.ContainsByPredicate(
            [Actor](const TWeakObjectPtr<AActor>& Kept) { return Kept.Get() == Actor; })) continue;
        if (UBreakerCombatComponent* Combat = Actor->FindComponentByClass<UBreakerCombatComponent>())
        {
            Combat->RemoveIncomingDamageModifier(BreakerModifierRuntime::BreakerAuraModifierKey);
        }
    }
    AuraTargets = MoveTemp(StillInside);
}

void UBreakerEnemyModifierComponent::TickPhasing(float DeltaSeconds)
{
    AActor* Target = TrackedTarget.Get();
    ABreakerEnemy* Enemy = Cast<ABreakerEnemy>(GetOwner());
    if (!Enemy || Enemy->IsDeadEnemy()) return;

    // The blink itself: briefly untargetable, then hittable again.
    if (BlinkRemaining > 0.0f)
    {
        BlinkRemaining -= DeltaSeconds;
        if (BlinkRemaining <= 0.0f)
        {
            BlinkRemaining = 0.0f;
            Enemy->SetModifierUntargetable(false);
        }
        return;
    }

    if (!Target) return;

    // The tell has to come FIRST. A blink with no wind-up is an unannounced
    // teleport, which §1.2's readability test forbids outright and which O1's
    // passive defence has no answer to.
    if (bPhaseTelegraphing)
    {
        PhaseTelegraphRemaining -= DeltaSeconds;
        if (PhaseTelegraphRemaining > 0.0f) return;

        bPhaseTelegraphing = false;
        const FVector Destination = UBreakerEnemyModifierLibrary::GetPhaseDestination(
            Enemy->GetActorLocation(), Target->GetActorLocation(), Params);
        if (!Destination.Equals(Enemy->GetActorLocation(), 1.0f))
        {
            Enemy->SetActorLocation(Destination, false);
        }
        BlinkRemaining = FMath::Max(0.0f, Params.PhaseBlinkSeconds);
        Enemy->SetModifierUntargetable(BlinkRemaining > 0.0f);
        PhaseTimer = 0.0f;
        return;
    }

    PhaseTimer += DeltaSeconds;
    if (PhaseTimer >= FMath::Max(0.1f, Params.PhaseIntervalSeconds))
    {
        bPhaseTelegraphing = true;
        PhaseTelegraphRemaining = FMath::Max(0.0f, Params.PhaseTelegraphSeconds);
    }
}

void UBreakerEnemyModifierComponent::NotifyAttackLanded(const FVector& ImpactLocation)
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;

    // ANCHORED's live half. The slow is applied to whatever was hit through the
    // movement layer's existing keyed push/pop; it is not a damage number and
    // deliberately does not scale with anything.
    if (HasModifier(EBreakerEnemyModifier::Anchored))
    {
        if (ABreakerEnemy* Enemy = Cast<ABreakerEnemy>(GetOwner()))
        {
            Enemy->ApplyModifierSlowToTarget(
                FMath::Clamp(1.0f - Params.AnchoredSlowFraction, 0.0f, 1.0f),
                FMath::Max(0.0f, Params.AnchoredSlowSeconds));
        }
    }

    if (HasModifier(EBreakerEnemyModifier::Cascading))
    {
        SpawnCascadeHazard(ImpactLocation);
    }
}

void UBreakerEnemyModifierComponent::SpawnCascadeHazard(const FVector& Location)
{
    if (!GetWorld()) return;
    LiveHazards.RemoveAll([](const TWeakObjectPtr<ABreakerZoneActor>& Zone) { return !Zone.IsValid(); });
    // §5.3 caps simultaneous ground hazards. Past the cap the oldest is
    // retired rather than the newest suppressed: a player who has just been
    // told "move" must find that the ground they moved to is the safe one.
    if (LiveHazards.Num() >= FMath::Max(1, Params.MaximumLiveHazardsPerEnemy))
    {
        if (ABreakerZoneActor* Oldest = LiveHazards[0].Get()) Oldest->Destroy();
        LiveHazards.RemoveAt(0);
    }

    FBreakerZoneSpec Spec;
    Spec.RadiusCm = FMath::Max(0.0f, Params.HazardRadiusCm);
    Spec.HalfHeightCm = 250.0f;
    Spec.Duration = FMath::Max(0.0f, Params.HazardDurationSeconds);
    Spec.TickInterval = FMath::Max(0.05f, Params.HazardTickInterval);
    Spec.ZoneColor = UBreakerEnemyModifierLibrary::GetModifierColor(EBreakerEnemyModifier::Cascading);
    Spec.TickDamage.BaseDamage = UBreakerEnemyModifierLibrary::GetHazardTickDamage(OwnerAttackDamage(), Params);
    Spec.TickDamage.DamageFamily = EBreakerDamageFamily::Physical;
    Spec.TickDamage.bCanCritical = false;

    // The zone is the shared primitive, not a bespoke hazard: it already routes
    // every tick through FBreakerDamageRequest/ReceiveDamage with a proper
    // Instigator, so the player's armour, shields and passive dodge/block layer
    // all apply exactly as they do to a bullet, and it already refuses to
    // damage other enemies.
    if (ABreakerZoneActor* Zone = GetWorld()->SpawnActor<ABreakerZoneActor>(
        ABreakerZoneActor::StaticClass(), Location, FRotator::ZeroRotator))
    {
        Zone->ConfigureZone(Spec, GetOwner());
        LiveHazards.Add(Zone);
    }
}

void UBreakerEnemyModifierComponent::HandleOwnerDamaged(const FBreakerHitContext& Hit)
{
    if (!HasModifier(EBreakerEnemyModifier::Reflective) || bReflecting) return;
    AActor* Attacker = Hit.Instigator;
    if (!Attacker || Attacker == GetOwner()) return;
    // A reflect is dealt as TrueDamage and this filter refuses TrueDamage, so
    // a reflect can never reflect a reflect. That is the loop guard; bReflecting
    // is only the belt to its braces.
    if (Hit.DamageFamily == EBreakerDamageFamily::TrueDamage) return;
    // §1.2 puts no reflect on damage over time, and it would be a strange rule:
    // a bleed is not the player holding a trigger.
    if (Hit.bFromDoT) return;

    UBreakerCombatComponent* AttackerCombat = Attacker->FindComponentByClass<UBreakerCombatComponent>();
    if (!AttackerCombat) return;

    const float Dealt = Hit.Result.HealthDamage + Hit.Result.ShieldDamage;
    const float Reflected = UBreakerEnemyModifierLibrary::GetReflectDamage(Dealt, OwnerMaxHealth(), Params);
    if (Reflected <= 0.0f) return;

    FBreakerDamageRequest Request;
    Request.BaseDamage = Reflected;
    // "Unmitigated-by-armour" per §1.2. TrueDamage is the family that already
    // means exactly that in this pipeline, so no new bypass flag is invented.
    Request.DamageFamily = EBreakerDamageFamily::TrueDamage;
    Request.bCanCritical = false;
    Request.SourceLocation = GetOwner()->GetActorLocation();
    Request.bHasSourceLocation = true;
    Request.SetInstigator(GetOwner());

    bReflecting = true;
    AttackerCombat->ReceiveDamage(Request);
    bReflecting = false;
}

bool UBreakerEnemyModifierComponent::TryConsumeWakefulRevive(bool bKilledByWeakPoint)
{
    if (!HasModifier(EBreakerEnemyModifier::Wakeful) || bWakefulSpent) return false;
    // §1.2: a weak-point killing blow denies the revive. That is the entire
    // mechanic — it converts "spray the body" into "finish the head", which is
    // a skill the game already teaches everywhere else.
    if (bKilledByWeakPoint)
    {
        bWakefulSpent = true;
        return false;
    }
    bWakefulSpent = true;
    return true;
}

float UBreakerEnemyModifierComponent::GetWakefulReviveDelay() const
{
    return FMath::Max(0.0f, Params.WakefulReviveDelaySeconds);
}

float UBreakerEnemyModifierComponent::GetWakefulReviveHealthFraction() const
{
    return FMath::Clamp(Params.WakefulReviveHealthFraction, 0.01f, 1.0f);
}

void UBreakerEnemyModifierComponent::NotifyOwnerDied()
{
    ReleaseExternalEffects();

    if (HasModifier(EBreakerEnemyModifier::Volatile))
    {
        // O245: snapshot the killer on the death frame. The blast is still
        // authored by the corpse — see DetonateVolatile — but the player who
        // chose where this body died earns what it kills.
        if (const AActor* Owner = GetOwner())
        {
            if (const UBreakerCombatComponent* Combat = Owner->FindComponentByClass<UBreakerCombatComponent>())
            {
                VolatileCreditTo = Combat->GetLastDamageInstigator();
            }
        }
        FuseTotal = FMath::Max(0.0f, Params.VolatileFuseSeconds);
        FuseRemaining = FuseTotal;
        // O203/O129: the colored modifier disc and its light remain retired.
        // The living health plate disappears at death. Until the
        // Niagara pass gives the corpse a fuse emitter, the existing HUD
        // projects an unfilled system-color marker and this replicated
        // countdown over the corpse. The retired disc/light stay absent.
        // A zero fuse is a legal authoring choice and must detonate now rather
        // than wait a frame for a clock that will never tick again.
        if (FuseTotal <= 0.0f)
        {
            FuseRemaining = -1.0f;
            DetonateVolatile();
        }
    }

    if (HasModifier(EBreakerEnemyModifier::Splitting)) SpawnSplits();
}

void UBreakerEnemyModifierComponent::DetonateVolatile()
{
    if (!GetWorld() || !GetOwner() || !GetOwner()->HasAuthority()) return;

    const FVector Center = GetOwner()->GetActorLocation();
    const float Full = UBreakerEnemyModifierLibrary::GetVolatileDetonationDamage(OwnerAttackDamage(), Params);
    if (Full <= 0.0f) return;
    const float OuterSq = FMath::Square(FMath::Max(Params.VolatileInnerRadiusCm, Params.VolatileOuterRadiusCm));

    // A Volatile blast reaches every LIVE pawn with a combat component inside
    // the outer radius, enemies included: a Volatile is a weapon the player
    // aims by choosing where it dies. Same Full * Falloff, same radii, same
    // instigator for every target; no separate enemy number. A dead body is
    // skipped the way the on-death chain skips one, so a pack that is
    // already down does not soak up a blast meant for the pack behind it.
    //
    // A default Volatile (VolatileDamageAsAttackMultiple 9.0 x chassis
    // BaseDamage 51.1) exceeds trash BaseHealth 220, so the blast kills every
    // trash body inside the inner radius outright. A separate enemy fraction
    // is authoring and waits on a playtest.
    //
    // THE AUTHOR IS THE CORPSE, THE EARNER IS ITS KILLER (O245). The
    // instigator stays the detonating enemy, because the instigator is what
    // the damage MATHS reads — Execute, Interrupt and every conditional More
    // look up the instigator's progression, so naming the player there would
    // quietly scale the blast with his build and break O217's one number.
    // CreditTo moves the hit and kill hooks alone, so the Feed / Scrap /
    // deployable listeners pay the player who popped the Volatile while the
    // number stays exactly the monster's. An unattributed death (a hazard, a
    // test) leaves CreditTo null and falls back to the corpse as before.
    for (TActorIterator<APawn> It(GetWorld()); It; ++It)
    {
        APawn* Candidate = *It;
        if (!Candidate) continue;
        const ABreakerEnemy* CandidateEnemy = Cast<ABreakerEnemy>(Candidate);
        const float DistanceSq = FVector::DistSquared(Candidate->GetActorLocation(), Center);
        if (!UBreakerEnemyModifierLibrary::VolatileDetonationReaches(
                Candidate == GetOwner(), CandidateEnemy && CandidateEnemy->IsDeadEnemy(), DistanceSq, OuterSq)) continue;
        UBreakerCombatComponent* Combat = Candidate->FindComponentByClass<UBreakerCombatComponent>();
        if (!Combat) continue;

        FBreakerDamageRequest Request;
        Request.BaseDamage = Full * UBreakerEnemyModifierLibrary::GetVolatileFalloff(FMath::Sqrt(DistanceSq), Params);
        Request.DamageFamily = EBreakerDamageFamily::Physical;
        Request.bCanCritical = false;
        Request.SourceLocation = Center;
        Request.bHasSourceLocation = true;
        Request.SetInstigator(GetOwner());
        Request.CreditTo = VolatileCreditTo;
        // THE BLAST IS NOT A BULLET, and once CreditTo made it reach the
        // player's HUD it had to stop claiming it was one. Delivery defaults to
        // Weapon and bHasImpactLocation to false, so a credited blast hit was
        // drawing its damage numbers on the WEAPON arrival clock — delayed by
        // tracer flight time from the player's muzzle to each victim's pivot,
        // for damage he did not fire. Naming the delivery and the real impact
        // point puts the number at the blast, when the blast happens.
        Request.Delivery = EBreakerDamageDelivery::Ability;
        Request.ImpactLocation = Candidate->GetActorLocation();
        Request.bHasImpactLocation = true;
        if (Request.BaseDamage > 0.0f) Combat->ReceiveDamage(Request);
    }
}

void UBreakerEnemyModifierComponent::SpawnSplits()
{
    ABreakerEnemy* Enemy = Cast<ABreakerEnemy>(GetOwner());
    if (!Enemy || !GetWorld() || !Enemy->HasAuthority()) return;

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    const FVector Center = Enemy->GetActorLocation();
    const int32 Count = FMath::Max(0, Params.SplitCount);

    for (int32 Index = 0; Index < Count; ++Index)
    {
        // Deterministic fan rather than a random scatter, so a Splitting death
        // always produces the same readable shape and the player learns where
        // the copies appear.
        const float Angle = 360.0f / FMath::Max(1, Count) * Index;
        const FVector Offset = FRotator(0.0f, Angle, 0.0f).RotateVector(
            FVector(FMath::Max(0.0f, Params.SplitScatterCm), 0.0f, 0.0f));
        ABreakerEnemy* Copy = GetWorld()->SpawnActor<ABreakerEnemy>(
            Enemy->GetClass(), Center + Offset, Enemy->GetActorRotation(), SpawnParams);
        // §1.2: copies carry NO modifiers and drop NO loot. No modifiers is why
        // this cannot recurse: a split of a Splitting enemy does not split.
        if (Copy) Copy->ConfigureAsSplitCopy(Enemy->GetAreaLevel(),
            FMath::Clamp(Params.SplitHealthFraction, 0.01f, 1.0f));
    }
}

void UBreakerEnemyModifierComponent::ReleaseExternalEffects()
{
    if (bExternalEffectsReleased) return;
    bExternalEffectsReleased = true;
    for (const TWeakObjectPtr<AActor>& Buffed : AuraTargets)
    {
        if (AActor* Actor = Buffed.Get())
        {
            if (UBreakerCombatComponent* Combat = Actor->FindComponentByClass<UBreakerCombatComponent>())
            {
                Combat->RemoveIncomingDamageModifier(BreakerModifierRuntime::BreakerAuraModifierKey);
            }
        }
    }
    AuraTargets.Reset();
}
