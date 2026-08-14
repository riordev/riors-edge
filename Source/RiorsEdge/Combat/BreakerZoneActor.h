#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Combat/BreakerCombatTypes.h"
#include "BreakerZoneActor.generated.h"

class UBreakerCombatComponent;
class UPointLightComponent;
class UStaticMeshComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FBreakerZoneOccupancy, AActor*, Occupant);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FBreakerZoneExpired);

// Everything an authored zone is. One struct, replicated whole, so a client can
// draw the thing the server is resolving without a second source of truth for
// its radius. Every value is EditAnywhere (O2).
USTRUCT(BlueprintType)
struct RIORSEDGE_API FBreakerZoneSpec
{
    GENERATED_BODY()

    // Identity for the anti-stack rule (VW4) and for the armour-strip key. Two
    // zones sharing this tag and a caster are the SAME puddle refreshed, never
    // two puddles stacked.
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FGameplayTag ZoneTag;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0")) float RadiusCm = 400.0f;
    // Positive makes the volume a vertical cylinder. Ground effects want this:
    // a target on a crate inside the footprint is in the puddle, one on the
    // roof above it is not.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0")) float HalfHeightCm = 250.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0")) float Duration = 6.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0.05")) float TickInterval = 1.0f;

    // The damage every occupant takes per tick. BaseDamage of zero means a zone
    // that only applies statuses or only strips armour, which is a legal and
    // useful zone — Support's Suppress deals no damage at all.
    //
    // The whole request is carried, not just a number, because the caster's
    // snapshot (crit chance, crit multiplier, composed damage multiplier,
    // source tags) has to travel with it. The zone fills in Instigator, source
    // location and a per-occupant seed; everything else is the caster's.
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FBreakerDamageRequest TickDamage;

    // Status applied to every occupant on every tick. Reapplication stacks and
    // refreshes through the ordinary UBreakerStatusComponent rules, so a zone
    // does not need its own stacking policy.
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bAppliesStatus = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FBreakerStatusApplicationSpec StatusSpec;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EBreakerDamageFamily StatusFamily = EBreakerDamageFamily::Physical;

    // Flat armour stripped from every occupant while it stands in the zone,
    // pushed keyed so overlapping zones of the same tag cannot double-strip
    // and can never drive armour negative (see UBreakerCombatComponent::
    // PushArmorReduction). Flat, never percentage — Class-Kits VW7 says this
    // is what protects the boss armour cap.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0")) float FlatArmorReduction = 0.0f;

    // Presentation only. O2 PLACEHOLDER, and deliberately not teal: saturated
    // teal is reserved for rift objects and suppression hardware (O19).
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FLinearColor ZoneColor = FLinearColor(0.35f, 0.85f, 0.25f);
};

// A persistent damage/effect volume: the shared primitive behind C3 Rot,
// Support's Suppress, Gunsmith's Disruptor, every future boss telegraph and
// every environmental hazard (Ability-Implementation-Spec §5.3).
//
// It is a zone and not an ability because none of those things are Caster
// abilities, and building Rot's puddle inside Rot would mean building it again
// four times. What the zone owns: a lifetime, a membership set, a tick cadence,
// a damage/status payload, and an armour strip that is released when the
// occupant leaves. What it deliberately does NOT own: how it was aimed, who is
// allowed to be hurt by it (an overridable predicate), and what it looks like.
//
// EVERY damage submission goes through FBreakerDamageRequest / ReceiveDamage
// with the caster as Instigator, so armour, shields, the passive dodge/block
// layer, incoming modifiers, kill credit and Mana generation all apply exactly
// as they do for a bullet. A zone that wrote health directly would be invisible
// to all six.
//
// Authority: membership and damage are SERVER ONLY. The spec replicates so a
// client can draw the footprint; the overlap query never runs on a client.
UCLASS()
class RIORSEDGE_API ABreakerZoneActor : public AActor
{
    GENERATED_BODY()

public:
    ABreakerZoneActor();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // Server-side arming. Instigator is the actor credited for everything the
    // zone does; a zone with no instigator still works and simply credits
    // nobody, which is what an environmental hazard wants.
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Zone")
    void ConfigureZone(const FBreakerZoneSpec& InSpec, AActor* InInstigator);

    UFUNCTION(BlueprintPure, Category="Zone") int32 GetOccupantCount() const { return Occupants.Num(); }
    UFUNCTION(BlueprintPure, Category="Zone") const FBreakerZoneSpec& GetSpec() const { return Spec; }
    UFUNCTION(BlueprintPure, Category="Zone") float GetRemainingDuration() const { return RemainingDuration; }
    UFUNCTION(BlueprintPure, Category="Zone") AActor* GetZoneInstigator() const { return ZoneInstigator.Get(); }
    UFUNCTION(BlueprintPure, Category="Zone") int32 GetTicksDelivered() const { return TicksDelivered; }

    // VW4: a recast refreshes, it never stacks.
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Zone")
    void RefreshDuration(float NewDuration);

    // VW8 Wellspring: the zone rides an actor instead of the ground.
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Zone")
    void SetFollowActor(AActor* Follow);

    // Long Dark (VW12): zones placed inside the window stop ageing until it
    // closes. Membership and damage continue — only the lifetime is frozen.
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Zone")
    void SetExpiryPaused(bool bPaused);
    UFUNCTION(BlueprintPure, Category="Zone") bool IsExpiryPaused() const { return bExpiryPaused; }

    // Drives membership, cadence and expiry. Called from Tick, and directly by
    // tests so the entire zone is exercisable without a running world — the
    // precedent is UBreakerAbilityStateComponent::AdvanceTime.
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Zone")
    void AdvanceZone(float DeltaSeconds);

    // Releases every armour strip and clears the occupant set, broadcasting
    // exits. Called on expiry and on destruction; safe to call twice.
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Zone")
    void ReleaseAllOccupants();

    UPROPERTY(BlueprintAssignable, Category="Zone") FBreakerZoneOccupancy OnOccupantEntered;
    UPROPERTY(BlueprintAssignable, Category="Zone") FBreakerZoneOccupancy OnOccupantExited;
    UPROPERTY(BlueprintAssignable, Category="Zone") FBreakerZoneExpired OnZoneExpired;

    // The VW4 spawn-side query: an existing live zone of the same tag, from the
    // same caster, close enough to be the same puddle. Static because the
    // decision belongs to the SPAWNER — put the check in one place, never once
    // per ability (spec §5.3).
    static ABreakerZoneActor* FindRefreshableZone(const UWorld* World, FGameplayTag ZoneTag, const AActor* Instigator, const FVector& Center, float RadiusCm, float RefreshFractionOfRadius);

    // Live zones, server-side. A weak list rather than an actor iterator so the
    // membership/exit bookkeeping can ask "is this actor still inside some
    // other zone of the same tag" without a full world scan per exit.
    static const TArray<TWeakObjectPtr<ABreakerZoneActor>>& GetLiveZones();

    // How close two zones of the same tag must be to count as one, as a
    // fraction of the radius. O2 PLACEHOLDER: half a radius reads as "the same
    // puddle" and two 4 m Rots 3 m apart still read as two.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Zone", meta=(ClampMin="0"))
    float RefreshFractionOfRadius = 0.5f;

    // Hitch guard passed to UBreakerZoneMath::ConsumeTicks.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Zone", meta=(ClampMin="1"))
    int32 MaximumTicksPerAdvance = 8;

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;

    // Who this zone is allowed to affect. Default rule mirrors the one
    // ABreakerEnemyProjectile already uses, so zones and projectiles do not
    // disagree about friendly fire: never the caster, and a zone cast by an
    // enemy never touches another enemy. Override for a healing or buffing
    // zone (Support's Medic branch) rather than adding a flag here.
    virtual bool ShouldAffectActor(AActor* Candidate) const;

    // Cosmetic footprint, rebuilt from the replicated spec on every machine.
    void RefreshPresentation();

    UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Root;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Footprint;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UPointLightComponent> Glow;

    UPROPERTY(ReplicatedUsing=OnRep_Spec) FBreakerZoneSpec Spec;
    UFUNCTION() void OnRep_Spec();

private:
    void DeliverTick();
    void UpdateMembership();
    void ApplyArmorStrip(AActor* Occupant) const;
    void ReleaseArmorStrip(AActor* Occupant) const;
    // The keyed name used for the armour strip. Keyed by ZONE TAG only, never
    // by instance, so two overlapping Rots share one entry and cannot
    // double-strip (spec §5.3 task 6).
    FName ArmorKey() const;
    bool AnyOtherZoneContains(AActor* Actor) const;

    // Weak: an occupant that dies or is destroyed mid-zone must not be kept
    // alive by the membership set, and must not be dereferenced.
    TArray<TWeakObjectPtr<AActor>> Occupants;
    TWeakObjectPtr<AActor> ZoneInstigator;
    TWeakObjectPtr<AActor> FollowActor;
    float RemainingDuration = 0.0f;
    float TimeUntilNextTick = 0.0f;
    int32 TicksDelivered = 0;
    bool bExpiryPaused = false;
    bool bReleased = false;
};
