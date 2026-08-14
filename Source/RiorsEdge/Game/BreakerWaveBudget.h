#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "BreakerWaveBudget.generated.h"

// THE WAVE BUDGET SOLVER — Encounter-Design §4.2, §4.3 and §5.3.
//
// WHAT WAS THERE BEFORE. `4 + CurrentWave * 3` capped at 24, an elite every
// third wave, and a Lattice count of `Wave/2`. A hardcoded ramp: no budget, no
// archetype costs, no rest waves, no boss wave, no variety rule, and no
// enforcement of a single one of §5.3's hard caps. It also spawned up to 24
// live enemies against §5.3's ceiling of 12, which is the difference between
// "dense" and "a movement player cannot find a lane".
//
// This is the document's arithmetic, as pure world-free maths — the precedent
// is Combat/BreakerRangedBehavior.h and Combat/BreakerMonsterChassis.h. A wave
// is a composition decision, and a composition decision with a UWorld in it is
// a decision nobody can test.
//
// TWO THINGS THE DOCUMENT DOES NOT SAY, marked as derived rather than authored:
//
//   1. THE SKIRMISHER HAS NO BUDGET COST. §4.2's table predates the archetype.
//      It is given the Lattice's 3: same engagement band, same "one more thing
//      shooting at you" pressure, and §5.3's ranged cap is written about
//      converging PROJECTILE SOURCES rather than about the Lattice class, so
//      the two share that cap here.
//
//   2. §4.2's BUDGET CURVE AND §5.3's DENSITY CAP COLLIDE, and this solver
//      makes the collision visible instead of picking a winner silently. Under
//      the caps (12 live, 3 ranged, 1 Warden, 1 elite solo) the most a solo
//      wave can physically spend is about 35 points, which `Budget(n) = 6 + 4n`
//      passes at wave 8 — every wave after that leaves budget UNSPENT, and the
//      composition reports how much. The caps win, because they are the ones
//      written with a reason attached. The unspent figure is a finding for the
//      owner, not a licence to re-author either number under the O2 freeze.
//
// O2: every value in FBreakerWaveBudgetParams is EditAnywhere and a
// PLACEHOLDER. Nothing here is playtested; automation proves the arithmetic and
// cannot see whether a wave is fun.

UENUM(BlueprintType)
enum class EBreakerWaveKind : uint8
{
    // The ordinary escalating wave.
    Standard,
    // §4.3: "Rest waves every 6. Without them, wave mode measures endurance
    // instead of combat." Half budget, and one of the only two waves that drop
    // loot — §4.3 again: otherwise the gym becomes a farm and pollutes the
    // drop-rate data the instrument exists to gather.
    Rest,
    // §4.2's wave 12. THE FIELD MARSHAL, and nothing else: the boss deploys its
    // own adds and commands its own gallery Lattices, so a wave budget spent
    // alongside it would double-count the density it is already generating.
    Boss
};

USTRUCT(BlueprintType)
struct RIORSEDGE_API FBreakerWaveBudgetParams
{
    GENERATED_BODY()

    // §4.2: Budget(n) = 6 + 4n, capped at 90 (reached at wave 21).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Waves|Budget", meta=(ClampMin="0")) int32 BudgetBase = 6;   // O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Waves|Budget", meta=(ClampMin="0")) int32 BudgetPerWave = 4;   // O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Waves|Budget", meta=(ClampMin="1")) int32 BudgetCap = 90;   // O2 PLACEHOLDER

    // §4.2's cost table. Skirmisher is DERIVED, see the header note.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Waves|Costs", meta=(ClampMin="1")) int32 SkitterCost = 1;   // O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Waves|Costs", meta=(ClampMin="1")) int32 LatticeCost = 3;   // O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Waves|Costs", meta=(ClampMin="1")) int32 SkirmisherCost = 3;   // O2 PLACEHOLDER (derived)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Waves|Costs", meta=(ClampMin="1")) int32 WardenCost = 6;   // O2 PLACEHOLDER
    // "Elite promotion (any): +4 per modifier", ON TOP of the promoted body.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Waves|Costs", meta=(ClampMin="0")) int32 EliteModifierCost = 4;   // O2 PLACEHOLDER

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Waves|Cadence", meta=(ClampMin="1")) int32 RestWaveInterval = 6;   // O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Waves|Cadence", meta=(ClampMin="1")) int32 BossWaveInterval = 12;   // O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Waves|Cadence", meta=(ClampMin="0", ClampMax="1")) float RestBudgetFraction = 0.5f;   // O2 PLACEHOLDER
    // §4.3's breather. Nothing spawns for this long after a rest wave clears.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Waves|Cadence", meta=(ClampMin="0")) float RestBreatherSeconds = 20.0f;   // O2 PLACEHOLDER

    // First wave each archetype may appear on. Lattice from 2 and Warden from 3
    // are §4.2's example compositions; the Skirmisher is placed at 4 because it
    // is the archetype whose answer is a MOVEMENT decision, and the player
    // should have met all three simpler pressures first.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Waves|Introduction", meta=(ClampMin="1")) int32 LatticeFromWave = 2;   // O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Waves|Introduction", meta=(ClampMin="1")) int32 WardenFromWave = 3;   // O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Waves|Introduction", meta=(ClampMin="1")) int32 SkirmisherFromWave = 4;   // O2 PLACEHOLDER
    // §4.2: "elite count = floor(wave/4)".
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Waves|Introduction", meta=(ClampMin="1")) int32 WavesPerElite = 4;   // O2 PLACEHOLDER
    // Modifiers per elite, ramping. One is a readable teaching example; three
    // is §1.3's ceiling and the composition rules stop it being a sponge.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Waves|Introduction", meta=(ClampMin="1")) int32 WavesPerExtraEliteModifier = 8;   // O2 PLACEHOLDER

    // --- Modifier carriers (O27's kill-bucket producer) --------------------
    // NON-elite bodies that are promoted to rank ModifierBearing and KEEP it —
    // see Playtest/BreakerKillBuckets.h: today modifiers only ever land on the
    // elite promotion above, which is captured-and-restored back to rank Elite
    // after rolling (correctly, for an elite), so rank ModifierBearing never
    // existed at kill time and the ModifierBearing TTK bucket, "the one number
    // that says whether [O27] worked", was structurally empty. A carrier is the
    // fix: it is a promoted Skitter body exactly like the elite above (folded
    // into Out.Skitters, not counted separately), so introducing it spends more
    // of the SAME melee budget rather than adding bodies — the Lattice
    // precedent (taken OUT OF the melee budget so density is unchanged).
    // From wave 4 — the same wave the Skirmisher and the first elite both
    // arrive on, so budget is already tight there and a carrier may not
    // actually be affordable until a few waves later; that is the budget
    // curve and the density caps colliding exactly as documented above, not a
    // bug in this feature.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Waves|Introduction", meta=(ClampMin="1")) int32 ModifierCarrierFromWave = 4;   // O2 PLACEHOLDER
    // Wave/Divisor, so the count ramps the same shape the Lattice count does.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Waves|Introduction", meta=(ClampMin="1")) int32 ModifierCarrierWaveDivisor = 3;   // O2 PLACEHOLDER
    // Capped at 2: enough to make the bucket measurable without turning every
    // wave's whole Skitter fill into modifier reading.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Waves|Introduction", meta=(ClampMin="0")) int32 MaximumModifierCarriers = 2;   // O2 PLACEHOLDER

    // §5.3's hard caps. Every one of them has a reason written beside it in the
    // document, which is why they win against the budget curve when the two
    // disagree.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Waves|Caps", meta=(ClampMin="1")) int32 MaximumLiveEnemiesPerPlayer = 12;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Waves|Caps", meta=(ClampMin="0")) int32 MaximumRangedSources = 3;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Waves|Caps", meta=(ClampMin="0")) int32 MaximumWardensPerPlayer = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Waves|Caps", meta=(ClampMin="0")) int32 MaximumElitesSolo = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Waves|Caps", meta=(ClampMin="0")) int32 MaximumElitesFullParty = 3;

    // §4.3: "no wave may be more than 70% budget in a single archetype after
    // wave 3." The cheap archetype is the one that runs away with a budget, so
    // in practice this is a ceiling on Skitters.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Waves|Variety", meta=(ClampMin="0", ClampMax="1")) float MaximumSingleArchetypeShare = 0.70f;   // O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Waves|Variety", meta=(ClampMin="1")) int32 VarietyEnforcedFromWave = 4;   // O2 PLACEHOLDER ("after wave 3")
};

USTRUCT(BlueprintType)
struct RIORSEDGE_API FBreakerWaveComposition
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) int32 Wave = 0;
    UPROPERTY(BlueprintReadOnly) EBreakerWaveKind Kind = EBreakerWaveKind::Standard;
    UPROPERTY(BlueprintReadOnly) int32 Budget = 0;
    UPROPERTY(BlueprintReadOnly) int32 SpentBudget = 0;

    UPROPERTY(BlueprintReadOnly) int32 Skitters = 0;
    UPROPERTY(BlueprintReadOnly) int32 Lattices = 0;
    UPROPERTY(BlueprintReadOnly) int32 Skirmishers = 0;
    UPROPERTY(BlueprintReadOnly) int32 Wardens = 0;
    UPROPERTY(BlueprintReadOnly) bool bBoss = false;

    // Elites are PROMOTIONS, not bodies: an elite is one of the Skitters above,
    // upgraded. Counting it separately would double it into the density cap.
    UPROPERTY(BlueprintReadOnly) int32 Elites = 0;
    UPROPERTY(BlueprintReadOnly) int32 ModifiersPerElite = 0;

    // Non-elite modifier carriers — also PROMOTIONS, also folded into the
    // Skitters count above, exactly like Elites. The kill-bucket producer for
    // rank ModifierBearing (Playtest/BreakerKillBuckets.h).
    UPROPERTY(BlueprintReadOnly) int32 ModifierCarriers = 0;

    // §4.3: loot only on rest and boss waves, or the gym becomes a farm and the
    // drop-rate data it exists to collect is worthless.
    UPROPERTY(BlueprintReadOnly) bool bDropsLoot = false;

    UPROPERTY(BlueprintReadOnly) int32 UnspentBudget = 0;

    int32 TotalEnemies() const { return Skitters + Lattices + Skirmishers + Wardens + (bBoss ? 1 : 0); }
    int32 RangedSources() const { return Lattices + Skirmishers; }
};

UCLASS()
class RIORSEDGE_API UBreakerWaveBudgetLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintPure, Category="Waves") static int32 GetWaveBudget(int32 Wave, const FBreakerWaveBudgetParams& Params);
    UFUNCTION(BlueprintPure, Category="Waves") static EBreakerWaveKind GetWaveKind(int32 Wave, const FBreakerWaveBudgetParams& Params);
    UFUNCTION(BlueprintPure, Category="Waves") static int32 GetMaximumLiveEnemies(int32 PartySize, const FBreakerWaveBudgetParams& Params);
    UFUNCTION(BlueprintPure, Category="Waves") static int32 GetMaximumElites(int32 PartySize, const FBreakerWaveBudgetParams& Params);

    // The whole solve. Deterministic: same wave and party size, same
    // composition, every time — which is what makes a TTK sample from wave 7
    // comparable with the one taken last session.
    UFUNCTION(BlueprintPure, Category="Waves")
    static FBreakerWaveComposition SolveWave(int32 Wave, int32 PartySize, const FBreakerWaveBudgetParams& Params);

    // Checkable after the fact, so the caps are asserted rather than trusted.
    // OutReason names the rule that was broken, in the precedent of
    // UBreakerEnemyModifierLibrary::IsLegalModifierSet.
    UFUNCTION(BlueprintPure, Category="Waves")
    static bool IsCompositionLegal(const FBreakerWaveComposition& Composition, int32 PartySize,
        const FBreakerWaveBudgetParams& Params, FString& OutReason);

    UFUNCTION(BlueprintPure, Category="Waves")
    static FString DescribeComposition(const FBreakerWaveComposition& Composition);
};
