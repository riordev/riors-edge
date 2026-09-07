#pragma once

#include "CoreMinimal.h"
#include "Combat/BreakerBossEnemy.h"
#include "BreakerHoldfastEnemy.generated.h"

// THE HOLDFAST — Act I's boss (O214): a Vestige mass. No Altered commands
// before Act II, so the Field Marshal is Act II's and this is the body Act I
// ends on. Data/missions.json names it on the Undercroft beat.
//
// It is the Field Marshal's machine on a Vestige body, and it ADDS one beat
// to the grammar: the add gate. While what it has raised still stands, it
// takes less and its next phase gate holds — the mass is the thing that
// holds, and the player breaks it by clearing what it raised. Every build
// participates (O31): the gate never approaches immunity, and the front pool
// (O198) is inherited unchanged, so the flanking answer is the same one the
// Warden taught.
//
// The class is a constructor and a label. Nothing about the order machine,
// the apparatus, the front or the arena is written here twice; it reads the
// same grammar the Marshal does, with one number turned on.
//
// EVERY NUMBER IS AN O2 PLACEHOLDER. Nothing here has been felt.
UCLASS(Blueprintable)
class RIORSEDGE_API ABreakerHoldfastEnemy : public ABreakerBossEnemy
{
    GENERATED_BODY()

public:
    ABreakerHoldfastEnemy();

protected:
    virtual void BeginPlay() override;
};
