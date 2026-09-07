#include "Combat/BreakerZoneActor.h"

#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerEnemy.h"
#include "Combat/BreakerStatusComponent.h"
#include "Combat/BreakerZoneMath.h"
#include "Classes/BreakerManaComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "UI/BreakerEffectMath.h"
#include "UI/BreakerEffectRenderer.h"

namespace BreakerZoneActorLocal
{
    // Prefixed namespace and prefixed names: identical anonymous-namespace
    // symbols in two .cpp files have collided under this project's unity build
    // twice, and the adaptive non-unity build excludes the file being edited,
    // so it does not catch it.
    static const TCHAR* BreakerZoneFootprintMesh = TEXT("/Engine/BasicShapes/Cylinder.Cylinder");
    static const TCHAR* BreakerZoneShapeMaterial = TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial");

    // Live server-side zones. A weak list rather than a world iterator so an
    // exit can ask "is this actor still standing in another zone of the same
    // tag" cheaply — that question is what stops the armour strip flickering
    // off for one tick when two Rots overlap.
    static TArray<TWeakObjectPtr<ABreakerZoneActor>> BreakerLiveZones;
}

ABreakerZoneActor::ABreakerZoneActor()
{
    PrimaryActorTick.bCanEverTick = true;
    bReplicates = true;
    SetReplicateMovement(true);

    Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(Root);

    // Presentation is a placeholder primitive in the gym's asset-free pattern:
    // a flat cylinder standing in for the decal a Blueprint pass will author.
    // It never collides — membership is an overlap QUERY, not a physics volume,
    // because a physics volume would push pawns around.
    Footprint = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Footprint"));
    Footprint->SetupAttachment(Root);
    Footprint->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Footprint->SetCastShadow(false);
    if (UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, BreakerZoneActorLocal::BreakerZoneFootprintMesh))
    {
        Footprint->SetStaticMesh(Mesh);
    }

    Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glow"));
    Glow->SetupAttachment(Root);
    Glow->SetMobility(EComponentMobility::Movable);
    Glow->SetCastShadows(false);
}

void ABreakerZoneActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ABreakerZoneActor, Spec);
}

void ABreakerZoneActor::BeginPlay()
{
    Super::BeginPlay();
    if (HasAuthority())
    {
        BreakerZoneActorLocal::BreakerLiveZones.RemoveAll([](const TWeakObjectPtr<ABreakerZoneActor>& Zone) { return !Zone.IsValid(); });
        BreakerZoneActorLocal::BreakerLiveZones.AddUnique(this);
    }
    RefreshPresentation();
}

void ABreakerZoneActor::EndPlay(const EEndPlayReason::Type Reason)
{
    // Unconditional teardown, including a level transition and a destroyed
    // caster. A zone that ends without releasing its armour strip leaves the
    // target permanently softened, and that bug is invisible until someone
    // notices a boss dying too fast an hour later.
    ReleaseAllOccupants();
    BreakerZoneActorLocal::BreakerLiveZones.RemoveAll(
        [this](const TWeakObjectPtr<ABreakerZoneActor>& Zone) { return !Zone.IsValid() || Zone.Get() == this; });
    Super::EndPlay(Reason);
}

const TArray<TWeakObjectPtr<ABreakerZoneActor>>& ABreakerZoneActor::GetLiveZones()
{
    return BreakerZoneActorLocal::BreakerLiveZones;
}

void ABreakerZoneActor::ConfigureZone(const FBreakerZoneSpec& InSpec, AActor* InInstigator)
{
    if (!HasAuthority()) return;
    Spec = InSpec;
    ZoneInstigator = InInstigator;
    BreakerZoneActorLocal::BreakerLiveZones.AddUnique(this);
    // Income samples the current membership/lifetime before zones age this frame.
    if (InInstigator)
        if (UBreakerManaComponent* Mana = InInstigator->FindComponentByClass<UBreakerManaComponent>())
            AddTickPrerequisiteComponent(Mana);
    RemainingDuration = FMath::Max(0.0f, Spec.Duration);
    // First tick lands one interval in, not immediately: a zone that damages on
    // the frame it is placed makes its cadence unreadable and gives a free tick
    // to anyone who recasts it on top of itself.
    TimeUntilNextTick = FMath::Max(0.05f, Spec.TickInterval);
    TicksDelivered = 0;
    bReleased = false;
    // Deliberately no SetLifeSpan: the zone owns its own clock (RemainingDuration
    // is pausable, a lifespan timer is not), and SetLifeSpan dereferences the
    // world, which is fatal for a zone constructed in automation.
    RefreshPresentation();
    // The rim rides ARMING, not BeginPlay: an unconfigured zone's default
    // spec is not a footprint anyone chose, and automation constructs zones
    // with no world for the renderer to live in.
    if (GetWorld()) SubmitRimEffect();
    if (Spec.bApplyStatusOnEntry && RemainingDuration > 0.0f) UpdateMembership();
}

void ABreakerZoneActor::RefreshDuration(float NewDuration)
{
    if (!HasAuthority()) return;
    // Refresh, never extend: VW4's rule is that a recast resets the clock, so
    // spamming Rot cannot bank duration.
    RemainingDuration = FMath::Max(RemainingDuration, FMath::Max(0.0f, NewDuration));
}

void ABreakerZoneActor::SetFollowActor(AActor* Follow)
{
    if (!HasAuthority()) return;
    FollowActor = Follow;
    FollowOffset = Follow ? GetActorLocation() - Follow->GetActorLocation() : FVector::ZeroVector;
    Spec.bMobileFootprint = Follow != nullptr;
    if (ABreakerEffectRenderer* Renderer = RimRenderer.Get())
        for (int32 Handle : RimHandles) Renderer->EndEffect(Handle, 0.0f);
    RimHandles.Reset();
    RefreshPresentation();
    ForceNetUpdate();
}

float ABreakerZoneActor::OwnedOccupiedSeconds(AActor* Owner, FGameplayTag Tag, float DeltaSeconds)
{
    float Seconds = 0.0f;
    if (!Owner || !Owner->HasAuthority()) return Seconds;
    const TArray<TWeakObjectPtr<ABreakerZoneActor>> Snapshot = BreakerZoneActorLocal::BreakerLiveZones;
    for (const TWeakObjectPtr<ABreakerZoneActor>& Held : Snapshot)
    {
        ABreakerZoneActor* Zone = Held.Get();
        if (!Zone || Zone->bReleased || Zone->GetWorld() != Owner->GetWorld()
            || Zone->ZoneInstigator != Owner || Zone->Spec.ZoneTag != Tag) continue;
        if (AActor* Follow = Zone->FollowActor.Get()) Zone->SetActorLocation(Follow->GetActorLocation() + Zone->FollowOffset);
        Zone->UpdateMembership();
        if (!Zone->Occupants.IsEmpty())
            Seconds = FMath::Max(Seconds, Zone->bExpiryPaused ? FMath::Max(0.0f, DeltaSeconds)
                : FMath::Clamp(Zone->RemainingDuration, 0.0f, FMath::Max(0.0f, DeltaSeconds)));
    }
    return Seconds;
}

void ABreakerZoneActor::SetExpiryPaused(bool bPaused)
{
    if (!HasAuthority()) return;
    bExpiryPaused = bPaused;
}

void ABreakerZoneActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (HasAuthority()) AdvanceZone(DeltaSeconds);
}

void ABreakerZoneActor::AdvanceZone(float DeltaSeconds)
{
    if (!HasAuthority() || bReleased) return;

    if (AActor* Follow = FollowActor.Get())
    {
        SetActorLocation(Follow->GetActorLocation() + FollowOffset);
    }

    UpdateMembership();

    const float ActiveSeconds = bExpiryPaused ? FMath::Max(0.0f, DeltaSeconds)
        : FMath::Min(FMath::Max(0.0f, DeltaSeconds), FMath::Max(0.0f, RemainingDuration));
    const int32 Ticks = UBreakerZoneMath::ConsumeTicks(TimeUntilNextTick, ActiveSeconds, Spec.TickInterval, MaximumTicksPerAdvance);
    for (int32 Index = 0; Index < Ticks; ++Index)
    {
        DeliverTick();
    }

    RemainingDuration = UBreakerZoneMath::RemainingAfter(RemainingDuration, DeltaSeconds, bExpiryPaused);
    if (RemainingDuration <= 0.0f && !bExpiryPaused)
    {
        ReleaseAllOccupants();
        OnZoneExpired.Broadcast();
        Destroy();
    }
}

void ABreakerZoneActor::UpdateMembership()
{
    UWorld* World = GetWorld();
    if (!World) return;

    const FVector Center = GetActorLocation();

    TSet<AActor*> Inside;
    TArray<FOverlapResult> Overlaps;
    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(BreakerZoneMembership), false, ZoneInstigator.Get());
    // The same channel weapons trace on, so anything shootable is also
    // zone-able and no actor needs a second collision setup. The sphere is the
    // broad phase; UBreakerZoneMath::IsInsideZone is the actual shape.
    // The broad phase must contain the cylinder's upper/lower outer rim.
    const float QueryRadius = Spec.HalfHeightCm > 0.0f
        ? FMath::Sqrt(FMath::Square(Spec.RadiusCm) + FMath::Square(Spec.HalfHeightCm))
        : FMath::Max(Spec.RadiusCm, 1.0f);
    World->OverlapMultiByChannel(Overlaps, Center, FQuat::Identity, ECC_GameTraceChannel2,
        FCollisionShape::MakeSphere(QueryRadius), QueryParams);

    for (const FOverlapResult& Overlap : Overlaps)
    {
        AActor* Candidate = Overlap.GetActor();
        if (!Candidate || Inside.Contains(Candidate)) continue;
        if (!ShouldAffectActor(Candidate)) continue;
        if (!UBreakerZoneMath::IsInsideZone(Center, Spec.RadiusCm, Spec.HalfHeightCm, Candidate->GetActorLocation())) continue;
        Inside.Add(Candidate);
    }

    // Exits first, so an actor that left and re-entered in the same frame is
    // not double-counted.
    for (int32 Index = Occupants.Num() - 1; Index >= 0; --Index)
    {
        AActor* Existing = Occupants[Index].Get();
        if (Existing && Inside.Contains(Existing)) continue;
        Occupants.RemoveAt(Index);
        if (!Existing) continue;
        ReleaseArmorStrip(Existing);
        OnOccupantExited.Broadcast(Existing);
    }

    for (AActor* Candidate : Inside)
    {
        // The physical zone owns this weak lease, not a shared ability
        // instance whose current-zone pointer changes on the next cast.
        if (Spec.ZoneTag == FGameplayTag::RequestGameplayTag(TEXT("Zone.Support.Suppress"), false))
            if (UBreakerCombatComponent* TargetCombat = Candidate->FindComponentByClass<UBreakerCombatComponent>())
                TargetCombat->AddBeneficialSuppressionLease(this);
        const bool bAlready = Occupants.ContainsByPredicate(
            [Candidate](const TWeakObjectPtr<AActor>& Held) { return Held.Get() == Candidate; });
        if (bAlready) { ApplyArmorStrip(Candidate); continue; }
        Occupants.Add(Candidate);
        ApplyArmorStrip(Candidate);
        if (Spec.bApplyStatusOnEntry && (RemainingDuration > 0.0f || bExpiryPaused)) ApplyStatusToOccupant(Candidate);
        OnOccupantEntered.Broadcast(Candidate);
    }
}

bool ABreakerZoneActor::ShouldAffectActor(AActor* Candidate) const
{
    if (!Candidate || Candidate == ZoneInstigator.Get()) return false;
    const UBreakerCombatComponent* Combat = Candidate->FindComponentByClass<UBreakerCombatComponent>();
    if (!Combat || Combat->IsDead()) return false;
    // Same friendly-fire rule ABreakerEnemyProjectile already uses: a zone cast
    // by an enemy never touches another enemy. Enemy friendly fire is not a
    // system this project has ruled on, and a pack melting itself in its own
    // hazard reads as a bug.
    const AActor* Caster = ZoneInstigator.Get();
    if (Caster && Caster->IsA<ABreakerEnemy>() && Candidate->IsA<ABreakerEnemy>()) return false;
    return true;
}

void ABreakerZoneActor::DeliverTick()
{
    ++TicksDelivered;
    AActor* Caster = ZoneInstigator.Get();
    UBreakerCombatComponent* CasterCombat = Caster ? Caster->FindComponentByClass<UBreakerCombatComponent>() : nullptr;
    const FVector Center = GetActorLocation();

    // Copy: an occupant dying inside its own tick mutates the membership set
    // through the death path, and iterating the live array would be a
    // use-after-free the first time a zone lands a killing blow.
    TArray<TWeakObjectPtr<AActor>> Snapshot = Occupants;
    int32 OccupantIndex = 0;
    for (const TWeakObjectPtr<AActor>& Held : Snapshot)
    {
        AActor* Occupant = Held.Get();
        if (!Occupant) continue;
        UBreakerCombatComponent* Combat = Occupant->FindComponentByClass<UBreakerCombatComponent>();
        if (!Combat) continue;

        if (Spec.TickDamage.BaseDamage > 0.0f)
        {
            FBreakerDamageRequest Request = Spec.TickDamage;
            // The caster is the Instigator on every tick, which is what makes
            // kill credit, OnHitDealt/OnKillDealt and outgoing-damage modifiers
            // work for a puddle exactly as they do for a bullet.
            if (!Request.Instigator.IsValid()) Request.SetInstigator(Caster);
            Request.SourceLocation = Center;
            Request.bHasSourceLocation = true;
            // Distinct per occupant and per tick, so two enemies in one puddle
            // do not share a critical roll and the sequence is still
            // reproducible on the server.
            Request.RandomSeed = HashCombine(HashCombine(Spec.TickDamage.RandomSeed, static_cast<uint32>(TicksDelivered)),
                static_cast<uint32>(OccupantIndex));
            // Outgoing modifiers are composed at submission, not at placement:
            // a window opened after the zone was cast must affect the ticks it
            // is open for, and one that closed must stop affecting them.
            if (CasterCombat) CasterCombat->ApplyOutgoingModifiers(Request);
            Combat->ReceiveDamage(Request);
        }

        ApplyStatusToOccupant(Occupant);
        ++OccupantIndex;
    }
}

void ABreakerZoneActor::ApplyStatusToOccupant(AActor* Occupant) const
{
    if (!IsValid(Occupant) || !Spec.bAppliesStatus || Spec.StatusSpec.Duration <= 0.0f) return;
    if (UBreakerStatusComponent* Status = Occupant->FindComponentByClass<UBreakerStatusComponent>())
    {
        Status->ApplyStatus(Spec.StatusSpec, Spec.StatusFamily, ZoneInstigator.Get());
    }
}
FName ABreakerZoneActor::ArmorKey() const
{
    // Keyed by TAG, deliberately not by instance: two overlapping Rots must
    // strip 40 between them, not 80 (spec §5.3 task 6). PushArmorReduction
    // replaces on an existing key, so the second zone refreshes the first.
    return Spec.ZoneTag.IsValid() ? Spec.ZoneTag.GetTagName() : FName(TEXT("Zone.Untagged"));
}

void ABreakerZoneActor::ApplyArmorStrip(AActor* Occupant) const
{
    ReconcileArmorStrip(Occupant, true);
}

void ABreakerZoneActor::ReleaseArmorStrip(AActor* Occupant) const
{
    ReconcileArmorStrip(Occupant, false);
}

float ABreakerZoneActor::ArmorStripFor(AActor* Occupant) const
{
    float Amount = FMath::Max(0.0f, Spec.FlatArmorReduction);
    if (const UBreakerStatusComponent* Status = Occupant ? Occupant->FindComponentByClass<UBreakerStatusComponent>() : nullptr)
        for (const FBreakerActiveStatus& Active : Status->GetActiveStatuses())
            if (Active.RemainingDuration > 0.0f && Active.Spec.BaseDamagePerTick > 0.0f)
            { Amount += FMath::Max(0.0f, Spec.AfflictedArmorReduction); break; }
    return Amount;
}

void ABreakerZoneActor::ReconcileArmorStrip(AActor* Occupant, bool bIncludeThis) const
{
    UBreakerCombatComponent* Combat = Occupant ? Occupant->FindComponentByClass<UBreakerCombatComponent>() : nullptr;
    if (!Combat) return;
    float Strongest = bIncludeThis && !bReleased && !Combat->IsDead() ? ArmorStripFor(Occupant) : 0.0f;
    for (const TWeakObjectPtr<ABreakerZoneActor>& Held : BreakerZoneActorLocal::BreakerLiveZones)
    {
        const ABreakerZoneActor* Other = Held.Get();
        if (!Other || Other == this || Other->bReleased || Other->GetWorld() != GetWorld()
            || Other->Spec.ZoneTag != Spec.ZoneTag || (!Other->bExpiryPaused && Other->RemainingDuration <= 0.0f)
            || !Other->ShouldAffectActor(Occupant)) continue;
        if (UBreakerZoneMath::IsInsideZone(Other->GetActorLocation(), Other->Spec.RadiusCm, Other->Spec.HalfHeightCm, Occupant->GetActorLocation()))
            Strongest = FMath::Max(Strongest, Other->ArmorStripFor(Occupant));
    }
    if (Strongest > 0.0f) Combat->PushArmorReduction(ArmorKey(), Strongest);
    else Combat->PopArmorReduction(ArmorKey());
}

bool ABreakerZoneActor::AnyOtherZoneContains(AActor* Actor) const
{
    if (!Actor) return false;
    for (const TWeakObjectPtr<ABreakerZoneActor>& Held : BreakerZoneActorLocal::BreakerLiveZones)
    {
        const ABreakerZoneActor* Other = Held.Get();
        if (!Other || Other == this || Other->bReleased) continue;
        if (Other->Spec.ZoneTag != Spec.ZoneTag) continue;
        if (Other->Spec.FlatArmorReduction <= 0.0f) continue;
        if (UBreakerZoneMath::IsInsideZone(Other->GetActorLocation(), Other->Spec.RadiusCm, Other->Spec.HalfHeightCm, Actor->GetActorLocation()))
        {
            return true;
        }
    }
    return false;
}

void ABreakerZoneActor::ReleaseAllOccupants()
{
    if (bReleased) return;
    bReleased = true;
    for (const TWeakObjectPtr<AActor>& Held : Occupants)
    {
        AActor* Occupant = Held.Get();
        if (!Occupant) continue;
        ReleaseArmorStrip(Occupant);
        OnOccupantExited.Broadcast(Occupant);
    }
    Occupants.Reset();
}

ABreakerZoneActor* ABreakerZoneActor::FindRefreshableZone(const UWorld* World, FGameplayTag ZoneTag, const AActor* Instigator, const FVector& Center, float RadiusCm, float RefreshFractionOfRadius)
{
    for (const TWeakObjectPtr<ABreakerZoneActor>& Held : BreakerZoneActorLocal::BreakerLiveZones)
    {
        ABreakerZoneActor* Zone = Held.Get();
        if (!Zone || Zone->bReleased) continue;
        if (World && Zone->GetWorld() != World) continue;
        if (Zone->Spec.ZoneTag != ZoneTag) continue;
        // Same caster only. Two players dropping Rot on one pack is two
        // puddles, and folding them together would silently halve a party's
        // output.
        if (Zone->ZoneInstigator.Get() != Instigator) continue;
        if (UBreakerZoneMath::ShouldRefreshExisting(Zone->GetActorLocation(), Center, RadiusCm, RefreshFractionOfRadius))
        {
            return Zone;
        }
    }
    return nullptr;
}

void ABreakerZoneActor::OnRep_Spec()
{
    RefreshPresentation();
    SubmitRimEffect();
}

void ABreakerZoneActor::SubmitRimEffect()
{
    if (bRimSubmitted || Spec.Duration <= 0.0f || Spec.bMobileFootprint) return;
    ABreakerEffectRenderer* Renderer = ABreakerEffectRenderer::FindOrSpawn(GetWorld());
    if (!Renderer) return;
    bRimSubmitted = true;
    RimRenderer = Renderer;

    // The clip is the zone's whole life with the final second as the visible
    // expiry — the rim dims to nothing exactly as the volume stops biting.
    // THE STATIC CLIP IS FIXED AT SUBMISSION, which leaves two recorded gaps,
    // of them lifetime mutations this placeholder does not track:
    // RefreshDuration (a VW4 recast resets the clock; the old rim still dies
    // on the old clock), and SetExpiryPaused (Long Dark freezes the zone but
    // not the rim). Mobile zones instead use attached geometry and cancel
    // this clip, so Wellspring follows and expires with its gameplay volume.
    // An EARLY Destroy has the mirror gap: the rim finishes its clip alone.
    BreakerFX::FEffectTiming Timing;
    Timing.DurationSeconds = Spec.Duration;
    Timing.FadeInSeconds = 0.2f;    // O2 PLACEHOLDER
    Timing.FadeOutSeconds = 1.0f;   // O2 PLACEHOLDER

    // Lifted a hand off the floor so the strokes never z-fight the disc, and
    // thick enough to read at placement range. Both O2 PLACEHOLDER.
    const FVector Center = GetActorLocation() + FVector(0.0f, 0.0f, 6.0f);
    constexpr float RimThicknessCm = 7.0f;
    constexpr float RimIntensity = 2.8f;
    for (int32 Index = 0; Index < BreakerFX::GroundRingStrokes; ++Index)
    {
        FVector A, B;
        BreakerFX::RingStroke(Center, Spec.RadiusCm, Index, BreakerFX::GroundRingStrokes, A, B);
        RimHandles.Add(Renderer->AddStroke(A, B, RimThicknessCm, Spec.ZoneColor, RimIntensity, Timing));
    }
}

void ABreakerZoneActor::RefreshPresentation()
{
    if (Spec.bMobileFootprint && !RimHandles.IsEmpty())
    {
        if (ABreakerEffectRenderer* Renderer = RimRenderer.Get())
            for (int32 Handle : RimHandles) Renderer->EndEffect(Handle, 0.0f);
        RimHandles.Reset();
    }
    if (Footprint)
    {
        // BasicShapes/Cylinder is 100 cm across and 100 cm tall, so a unit of
        // scale is a metre. Deliberately flat: this stands in for a decal.
        const float Diameter = FMath::Max(Spec.RadiusCm, 1.0f) * 2.0f / 100.0f;
        Footprint->SetRelativeScale3D(FVector(Diameter, Diameter, 0.06f));
        if (UMaterialInterface* BaseMaterial = LoadObject<UMaterialInterface>(nullptr, BreakerZoneActorLocal::BreakerZoneShapeMaterial))
        {
            if (UMaterialInstanceDynamic* Dynamic = UMaterialInstanceDynamic::Create(BaseMaterial, Footprint))
            {
                Dynamic->SetVectorParameterValue(TEXT("Color"), Spec.ZoneColor);
                Footprint->SetMaterial(0, Dynamic);
            }
        }
    }
    // Mobile rings belong to the zone transform/lifetime, not a fixed pooled
    // world-space clip. Replicated Spec builds the same footprint on clients.
    if (Spec.bMobileFootprint && MobileRim.IsEmpty())
        if (UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")))
            for (int32 Index = 0; Index < BreakerFX::GroundRingStrokes; ++Index)
            {
                UStaticMeshComponent* Stroke = NewObject<UStaticMeshComponent>(this);
                AddInstanceComponent(Stroke); Stroke->SetupAttachment(Root);
                Stroke->SetStaticMesh(Mesh); Stroke->SetCollisionEnabled(ECollisionEnabled::NoCollision);
                Stroke->SetCastShadow(false); Stroke->RegisterComponent(); MobileRim.Add(Stroke);
            }
    for (int32 Index = 0; Index < MobileRim.Num(); ++Index)
    {
        UStaticMeshComponent* Stroke = MobileRim[Index];
        Stroke->SetVisibility(Spec.bMobileFootprint);
        FVector A, B;
        BreakerFX::RingStroke(FVector(0, 0, 6), Spec.RadiusCm, Index, BreakerFX::GroundRingStrokes, A, B);
        Stroke->SetRelativeLocation((A + B) * 0.5f);
        Stroke->SetRelativeRotation((B - A).Rotation());
        Stroke->SetRelativeScale3D(FVector(FVector::Distance(A, B) / 100.0f, 0.07f, 0.07f)); // O2 PLACEHOLDER, existing rim7cm.
        if (Footprint) Stroke->SetMaterial(0, Footprint->GetMaterial(0));
    }
    if (Glow)
    {
        Glow->SetLightColor(Spec.ZoneColor);
        Glow->SetIntensity(1800.0f);
        Glow->SetAttenuationRadius(FMath::Max(Spec.RadiusCm, 100.0f) * 1.5f);
    }
}
