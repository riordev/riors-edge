#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Combat/BreakerCombatTypes.h"
#include "BreakerKillTelemetryComponent.generated.h"

// The bridge that lets the playtest report know WHAT it just killed.
//
// THE PROBLEM IT SOLVES, stated plainly because the shape looks indirect and
// the reason is a boundary rather than a preference. The TTK sample is fed from
// ABreakerEnemy::HandleDeath, which lives in Combat/. That call passes two
// booleans — `IsElite()` and `IsRangedForTelemetry()` — and neither of them can
// describe a Champion (rank ModifierBearing) or the Field Marshal (rank Boss),
// so both were being filed under MELEE TRASH: the one bucket O18's re-anchor
// reads, and the one a 25x-health kill wrecks fastest. Widening that call is a
// one-line change to Combat/, and Combat/ is owned by another lane.
//
// So the enemy tells the report about ITSELF, from a component the gym attaches
// at spawn. It reads only its own owner: rank, the ranged telemetry flag, and
// its modifier count. No player, no build, no item level (O27).
//
// THE ORDERING IS A CODE PATH, NOT A REGISTRATION ORDER. This codebase is
// explicit that delegate broadcast order is "an accident of component
// initialisation and not a contract", so nothing here depends on one:
// UBreakerCombatComponent::ApplyDamage broadcasts OnDamageReceived (where this
// component posts its hint) and only THEN broadcasts OnDeath, which is what
// eventually reaches HandleDeath and the sample. Two different delegates, fixed
// order, in one function.
UCLASS(ClassGroup=Playtest, BlueprintType, meta=(BlueprintSpawnableComponent))
class RIORSEDGE_API UBreakerKillTelemetryComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UBreakerKillTelemetryComponent();

    // Attaches and registers one of these onto an enemy. Runtime-created, in
    // the zero-setup convention: no Blueprint has to know it exists, and an
    // enemy spawned by something other than the gym simply keeps the old
    // two-boolean classification rather than breaking.
    static UBreakerKillTelemetryComponent* AttachTo(class ABreakerEnemy* Enemy);

protected:
    virtual void BeginPlay() override;

    UFUNCTION() void HandleOwnerDamaged(const FBreakerDamageResult& Result);
};
