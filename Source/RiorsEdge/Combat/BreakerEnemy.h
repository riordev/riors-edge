#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Pawn.h"
#include "Combat/BreakerCombatTypes.h"
#include "Combat/BreakerMonsterChassis.h"
#include "Combat/BreakerEnemyModifiers.h"
#include "Items/BreakerDropTable.h"
#include "BreakerEnemy.generated.h"

class UAbilitySystemComponent;
class UBreakerAttributeSet;
class UBreakerCombatComponent;
class UBreakerStatusComponent;
class UCapsuleComponent;
class UStaticMeshComponent;
class USphereComponent;
class UBoxComponent;

UCLASS(Blueprintable)
class RIORSEDGE_API ABreakerEnemy : public APawn, public IAbilitySystemInterface
{
    GENERATED_BODY()

public:
    ABreakerEnemy();
    virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
    virtual void Tick(float DeltaSeconds) override;
    UFUNCTION(BlueprintCallable, Category="Enemy") void ConfigureEncounter(const FVector& NewLeashOrigin, float NewPatrolPhase);
    // Elite modifier: bigger, hits harder, and its drops are never below
    // Exceptional. The health and damage numbers are NOT written here any
    // more — this just sets the rank, and the rank table in
    // FBreakerMonsterChassisParams is the single source of truth for what an
    // elite is (O27 / Power-Curve.md §2).
    UFUNCTION(BlueprintCallable, Category="Enemy") void ConfigureElite();
    // Wave spawns: no respawn, and the wave's AREA level (content difficulty),
    // which drives the chassis and, through it, drop item level.
    UFUNCTION(BlueprintCallable, Category="Enemy") void ConfigureWave(int32 NewAreaLevel);
    // Sets the area level of the content this monster belongs to and rebuilds
    // its chassis. This is the ONLY input to monster difficulty; nothing about
    // the player is ever consulted (O27).
    UFUNCTION(BlueprintCallable, Category="Enemy") void SetAreaLevel(int32 NewAreaLevel);
    UFUNCTION(BlueprintCallable, Category="Enemy") void SetMonsterRank(EBreakerMonsterRank NewRank);
    UFUNCTION(BlueprintPure, Category="Enemy") int32 GetAreaLevel() const { return AreaLevel; }
    // The item level this monster's drop rolls at. Public because the loot path
    // reads it and NOTHING COULD ASSERT IT: an O29 regression pinned every drop
    // in the game at the character cap while the whole suite stayed green,
    // because every test exercised the library function and none could see the
    // field the game actually uses.
    UFUNCTION(BlueprintPure, Category="Enemy") int32 GetEnemyLevel() const { return EnemyLevel; }
    UFUNCTION(BlueprintPure, Category="Enemy") EBreakerMonsterRank GetMonsterRank() const { return MonsterRank; }
    // Rank IS the elite flag now; there is no separate bool to drift from it.
    UFUNCTION(BlueprintPure, Category="Enemy") bool IsElite() const { return MonsterRank == EBreakerMonsterRank::Elite; }
    // Telemetry bucket, not behaviour: a ranged archetype is fought across a
    // band rather than in contact, so its kill time measures something else
    // and must not be averaged into the melee trash sample the O18 re-anchor
    // reads. Virtual so an archetype declares this rather than the playtest
    // layer needing to know the class list.
    virtual bool IsRangedForTelemetry() const { return false; }
    UFUNCTION(BlueprintPure, Category="Enemy") FString GetEnemyStateLabel() const;
    // The ANNOUNCEMENT half — family banner and modifier banner, newline
    // separated, empty for a plain Vestige. Separate from the state so a HUD
    // can print what the player must know (Encounter-Design §1.2's 1.5s
    // modifier read) without the per-frame status chatter that made a crowd's
    // labels overlap into mush.
    UFUNCTION(BlueprintPure, Category="Enemy") FString GetEnemyModifierBanner() const;

    // --- The modifier layer (Encounter-Design §1, O27) ---------------------
    // Every enemy carries the component; the overwhelmingly common case is an
    // EMPTY one, which costs a pointer and nothing else. Difficulty lives here
    // rather than in trash health, which is what O27 asks for.
    UFUNCTION(BlueprintPure, Category="Enemy|Modifiers")
    class UBreakerEnemyModifierComponent* GetModifierComponent() const { return ModifierComponent; }

    // Rolls a legal modifier set, promotes the rank to ModifierBearing, and
    // rebuilds the chassis so the count-based health step lands. Deterministic
    // in the seed. Returns how many modifiers were granted.
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Enemy|Modifiers")
    int32 ConfigureWithModifiers(int32 Seed);

    // The authored path: grant exactly this set. Rejects illegal sets (returns
    // false and changes nothing) rather than trimming them silently.
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Enemy|Modifiers")
    bool ConfigureWithExactModifiers(const TArray<EBreakerEnemyModifier>& InModifiers);

    // Read-only views the modifier component needs. Both are ENEMY-side
    // numbers: the modifier layer is allowed to see the monster's own chassis
    // and nothing about the player (O27).
    UFUNCTION(BlueprintPure, Category="Enemy|Family") EBreakerEnemyFamily GetFamily() const { return Family; }
    UFUNCTION(BlueprintPure, Category="Enemy|Family") EBreakerSeveranceStage GetSeveranceStage() const { return SeveranceStage; }
    // Behavioural contracts, asked rather than hardcoded, so an archetype that
    // is authored as a different stage changes behaviour without a code change.
    UFUNCTION(BlueprintPure, Category="Enemy|Family") bool UsesCoverDiscipline() const;
    UFUNCTION(BlueprintPure, Category="Enemy|Family") bool FlinchesWhenHit() const;

    UFUNCTION(BlueprintPure, Category="Enemy") float GetMonsterMaxHealth() const;
    UFUNCTION(BlueprintPure, Category="Enemy") float GetAttackDamage() const { return AttackDamage; }
    UFUNCTION(BlueprintPure, Category="Enemy") bool IsDeadEnemy() const { return bDead; }
    // Read-only views of the authored tuning. Public so tools, the playtest
    // report and the automation suite can assert against what an archetype
    // SHIPS with, without opening the tuning itself for writing.
    UFUNCTION(BlueprintPure, Category="Enemy") float GetArchetypeHealthMultiplier() const { return ArchetypeHealthMultiplier; }
    UFUNCTION(BlueprintPure, Category="Enemy") float GetArchetypeDamageMultiplier() const { return ArchetypeDamageMultiplier; }
    // The same read-only-view discipline as the two above, extended to the two
    // movement tunables an archetype is most likely to retune. Added because a
    // subclass's tests could otherwise only assert them by being granted access
    // to the protected tuning itself, which would hand every test write access
    // to everything else on the chassis.
    UFUNCTION(BlueprintPure, Category="Enemy") float GetMoveSpeed() const { return MoveSpeed; }
    UFUNCTION(BlueprintPure, Category="Enemy") float GetWeaveStrength() const { return WeaveStrength; }
    UFUNCTION(BlueprintPure, Category="Enemy") float GetAttackRange() const { return AttackRange; }
    UFUNCTION(BlueprintPure, Category="Enemy") float GetLungeRange() const { return LungeRange; }
    UFUNCTION(BlueprintPure, Category="Enemy") bool DoesRespawn() const { return bRespawns; }
    UFUNCTION(BlueprintPure, Category="Enemy") bool DoesExplodeOnDeath() const { return bExplodesOnDeath; }

    // --- Mutators the modifier layer drives -------------------------------
    // Public rather than friend-classed: a modifier changing an enemy's speed
    // is a legitimate part of the enemy's contract, and a friend declaration
    // would let it change everything else too.

    // Warded. Sets MaxShield and fills it. Also used to clear the ward (0).
    UFUNCTION(BlueprintCallable, Category="Enemy|Modifiers") void SetModifierShield(float Amount);
    UFUNCTION(BlueprintCallable, Category="Enemy|Modifiers") void AddModifierShield(float Amount);

    // Fleetfoot. Multiplier is against the AUTHORED base speed, never against
    // the current one, so applying it twice does not compound. A negative
    // WeaveStrengthOverride means "leave the authored weave alone".
    UFUNCTION(BlueprintCallable, Category="Enemy|Modifiers")
    void ApplyModifierMovementProfile(float SpeedMultiplier, float WeaveStrengthOverride);

    // Anchored. Applies a temporary movement slow to whatever this enemy is
    // currently tracking, through the movement layer's own keyed push.
    UFUNCTION(BlueprintCallable, Category="Enemy|Modifiers")
    void ApplyModifierSlowToTarget(float SpeedMultiplier, float Duration);

    // Phasing. Turns the hit boxes off for the blink, so "briefly untargetable"
    // is literally true rather than a damage filter the player cannot see.
    UFUNCTION(BlueprintCallable, Category="Enemy|Modifiers") void SetModifierUntargetable(bool bUntargetable);

    // Splitting. A copy is a plain enemy at a fraction of health with no
    // modifiers, no loot and no respawn.
    UFUNCTION(BlueprintCallable, Category="Enemy|Modifiers")
    void ConfigureAsSplitCopy(int32 InAreaLevel, float HealthFraction);

    // --- Keyed enemy-side seams (the FlatArmorReduction pattern) -----------
    // Three multiplicative, keyed lanes the player's node layer can drive.
    // The rules are the armour lane's rules, one layer over: re-pushing a key
    // REPLACES it (two overlapping copies of one effect share a key and so
    // never stack), popping removes it, and the composed value is the PRODUCT
    // of the live keys. An empty lane composes to exactly 1.0, and every
    // consumer multiplies an authored value by it, so unkeyed behaviour is
    // bit-identical to the authored numbers — the suite pins that.

    // (1) Telegraph duration. >1.0 stretches every wind-up this enemy reads
    // through the seam (the melee lunge wind-up, LATTICE's WindupSeconds).
    // Sampled PER FRAME, so a delay pushed mid-wind-up extends the remainder
    // of the tell rather than waiting for the next one. A stretched tell is
    // strictly kinder to the player; a multiplier below 1.0 would SHORTEN a
    // telegraph, which Encounter-Design §0 forbids — consumers clamp at 1.0.
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Enemy|Seams")
    void PushWindupDurationMultiplier(FName Key, float Multiplier);
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Enemy|Seams")
    void PopWindupDurationMultiplier(FName Key);
    UFUNCTION(BlueprintPure, Category="Enemy|Seams")
    float GetComposedWindupDurationMultiplier() const;

    // (2) Aim error. A DEGRADATION multiplier on the ranged archetypes' shot
    // direction: 1.0 is the authored aim, and the excess over 1.0 both scales
    // any authored spread and opens AimErrorUnitDegrees of fresh error cone
    // per unit of excess — see GetEffectiveSpreadDegrees. That last clause is
    // what lets an accuracy cut land on LATTICE, whose authored aim is exact
    // (a plain multiplier would multiply zero and change nothing).
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Enemy|Seams")
    void PushAimErrorMultiplier(FName Key, float Multiplier);
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Enemy|Seams")
    void PopAimErrorMultiplier(FName Key);
    UFUNCTION(BlueprintPure, Category="Enemy|Seams")
    float GetComposedAimErrorMultiplier() const;

    // (3) Outgoing damage. <1.0 means this enemy hits SOFTER. Composed into
    // GetEffectiveAttackDamage, which every attack-damage build site reads
    // (melee contact hit, LATTICE volley, VEIL burst), so a mark that pushes
    // here softens the enemy's whole output while the key lives.
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Enemy|Seams")
    void PushOutgoingDamageMultiplier(FName Key, float Multiplier);
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Enemy|Seams")
    void PopOutgoingDamageMultiplier(FName Key);
    UFUNCTION(BlueprintPure, Category="Enemy|Seams")
    float GetComposedOutgoingDamageMultiplier() const;

    // AttackDamage after the keyed outgoing lane. THE damage-build input:
    // attack sites read this, never the raw field, so the softening seam has
    // exactly one place to land. Unkeyed it IS AttackDamage, bit-identical.
    UFUNCTION(BlueprintPure, Category="Enemy|Seams")
    float GetEffectiveAttackDamage() const { return AttackDamage * GetComposedOutgoingDamageMultiplier(); }

    // Pure: the shot cone (half-angle, degrees) for an authored spread under a
    // composed aim-error multiplier M:
    //   Authored * M + UnitDegrees * (M - 1), floored at zero.
    // M == 1.0 returns the authored spread EXACTLY (x*1 and +0 are exact in
    // float), M > 1 degrades even a zero-spread marksman, and M < 1 (an
    // accuracy BUFF, no consumer today) tightens toward zero. Static so the
    // suite pins the arithmetic with no world.
    static float GetEffectiveSpreadDegrees(float AuthoredSpreadDegrees, float ComposedAimErrorMultiplier, float AimErrorUnitDegrees);

protected:
    virtual void BeginPlay() override;
    // Virtual so the boss can end an encounter rather than recycle a corpse.
    UFUNCTION() virtual void HandleDeath();
    // Wakeful's downed state: the body goes away, nothing drops, and the enemy
    // stands back up at a fraction of health. Deliberately NOT RespawnEnemy —
    // that returns to the leash origin at full health, which is a respawn and
    // not a revive.
    void EnterWakefulDowned();
    UFUNCTION() void FinishWakefulRevive();
    UFUNCTION() void HandleDamageReceived(const FBreakerDamageResult& Result);
    void GrantLoot();
    // The XP loop O40(b) was waiting for. Unconditional on every kill.
    void GrantExperience();
    // Ammo economy: kills return reserve ammo to the killer (O2 placeholder).
    void GrantAmmo();
    void RespawnEnemy();
    virtual void PerformAttack(APawn* TargetPawn);
    // Shows/hides the whole humanoid assembly across death and respawn.
    // Virtual so archetypes with extra presentation (a charging emitter) can
    // clear it on the same edges.
    virtual void SetBodyVisible(bool bVisible);

    // Recomputes MaxHealth and AttackDamage from (AreaLevel, MonsterRank,
    // Chassis, archetype multipliers) and restores vitals. Idempotent: calling
    // it twice with the same inputs produces the same numbers, because it
    // never reads the CURRENT health or damage — it always rebuilds from the
    // authored parameters. That property is what the "same area level always
    // produces the same chassis" test checks.
    void ApplyChassis();

    // The per-frame decision an enemy makes while it has a live target. The
    // base implementation is the melee three-gear chase below; ranged
    // archetypes override it to hold an engagement band instead of closing.
    // Everything shared — target selection, the safe-zone rules, applying the
    // move, and the ground snap — stays in Tick and is not overridable.
    virtual void TickEngagedBehaviour(class ABreakerCharacter* Player, float Distance, float DeltaSeconds,
        FVector& OutDirection, float& OutSpeedScale);

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UCapsuleComponent> BodyCollision;
    // Primitive humanoid assembly. BodyVisual is the torso; the rest are
    // cosmetic siblings under BodyCollision, all NoCollision.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> BodyVisual;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> HeadVisual;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> LeftArmVisual;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> RightArmVisual;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> LeftLegVisual;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> RightLegVisual;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UBoxComponent> BodyHitBox;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<USphereComponent> WeakPoint;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> WeakPointVisual;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UAbilitySystemComponent> AbilitySystem;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UBreakerAttributeSet> Attributes;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UBreakerCombatComponent> Combat;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UBreakerStatusComponent> Status;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<class UBreakerEnemyModifierComponent> ModifierComponent;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy", meta=(ClampMin="0")) float DetectionRange = 2200.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy", meta=(ClampMin="0")) float AttackRange = 260.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy", meta=(ClampMin="0")) float MoveSpeed = 330.0f;
    // DERIVED, not authored: ApplyChassis overwrites this every time the area
    // level or rank changes. Author the archetype's damage as
    // Chassis.BaseDamage (the value at area level 1) instead.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Enemy") float AttackDamage = 14.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy", meta=(ClampMin="0")) float AttackCooldown = 1.15f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy", meta=(ClampMin="0")) float RespawnDelay = 3.0f;

    // --- The content axis (O27 / Power-Curve.md) ---------------------------
    // Area level describes how hard this piece of CONTENT is. It drives
    // monster health, monster damage, and the item level of the drop. It is
    // never derived from the player's level, gear or build: content-scaling
    // preserves the loop, player-scaling destroys it, because a build that
    // changes nothing observable is not a build.
    //
    // It is allowed past the character cap of 50 — endgame tiers keep climbing
    // and that is what keeps drop item level (and therefore gear) improving
    // after the cap.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy|Area", meta=(ClampMin="1", ClampMax="100")) int32 AreaLevel = 10;   // O2 PLACEHOLDER
    // Rank multiplies the chassis rather than replacing it.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy|Area") EBreakerMonsterRank MonsterRank = EBreakerMonsterRank::Trash;

    // --- The two families (Story-Source §1.5) ------------------------------
    // Vestige is the default because everything that existed before the
    // families did is rift-native by fiction and by behaviour. It is a
    // GAMEPLAY field, not a lore tag: it scopes which modifiers may appear
    // (a Vestige has no tactics, so no tactical modifier), and on an Altered
    // the severance stage decides whether the archetype takes cover and
    // whether it flinches.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy|Family") EBreakerEnemyFamily Family = EBreakerEnemyFamily::Vestige;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy|Family") EBreakerSeveranceStage SeveranceStage = EBreakerSeveranceStage::NotApplicable;
    // The curve itself, all O2 placeholders, tunable per enemy in-editor.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy|Area") FBreakerMonsterChassisParams Chassis;

    // The DROP pipeline's authored block, same precedent as Chassis above: one
    // struct, EditAnywhere, every value O2 PLACEHOLDER, so drop rates retune
    // from the details panel with no recompile. It is what GrantLoot runs
    // instead of calling RollRarity unconditionally on every death — see
    // Items/BreakerDropTable.h for the full derivation and the owner report it
    // answers.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy|Loot") FBreakerDropTableParams DropTable;
    // Crafting currency, on the same per-actor tuning pattern as DropTable.
    // Owner: "resources for crafting should drop from mobs at a reasonable
    // rate" — before this the Forge wallet's ONLY income was salvaging loot the
    // player already had, so the economy could not grow, only recycle.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy|Loot") FBreakerCurrencyDropParams CurrencyDropTable;
    // Per-archetype ratios ON TOP of rank, so two trash monsters in the same
    // area can differ. Encounter-Design §2.2's 1.6x Lattice is the first user.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy|Area", meta=(ClampMin="0")) float ArchetypeHealthMultiplier = 1.0f;   // O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy|Area", meta=(ClampMin="0")) float ArchetypeDamageMultiplier = 1.0f;   // O2 PLACEHOLDER

    // Item level of the drop. Kept in sync with AreaLevel by ApplyChassis —
    // area level driving drop item level is exactly the mechanism that makes
    // rising item level correspond to gameplay (O27), and under O29 it is the
    // mechanism that makes gear the ENDGAME power source rather than a
    // statement about one. It follows area level to 120 now; the old clamp to
    // the character cap of 50 was written when affix tiers stopped there, and
    // it outlived that reason by exactly one ruling.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Enemy") int32 EnemyLevel = 10;
    // Elites drop richer loot than their area level alone would give. This is
    // a LOOT bonus only and deliberately does not touch the chassis, which
    // takes its elite ratio from the rank table instead.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy", meta=(ClampMin="0")) int32 EliteDropItemLevelBonus = 5;   // O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy") bool bDropsLoot = true;
    // Wave-mode enemies die for good instead of recycling.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy") bool bRespawns = true;
    // Death chain: dying enemies detonate for a fraction of their max
    // health, damaging nearby enemies (never the player). Dense packs pop.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy") bool bExplodesOnDeath = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy", meta=(ClampMin="0", ClampMax="1")) float DeathExplosionHealthFraction = 0.35f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy", meta=(ClampMin="0")) float DeathExplosionRadius = 420.0f;

    // --- Approach feel (all O2 placeholders) -------------------------------
    // Owner feedback: "the models just slowly walk at me". The chase now has
    // three gears — closing sprint, weave, committed lunge — so the approach
    // reads as intent rather than a conveyor belt. Inspired by Skitter's
    // committed leap (Encounter-Design §2) without the full archetype roster.

    // Beyond this range the enemy sprints to close the gap.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy|Approach", meta=(ClampMin="0")) float SprintRange = 1200.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy|Approach", meta=(ClampMin="1")) float SprintSpeedMultiplier = 1.4f;
    // Lateral sinusoidal weave layered on the chase vector. Per-enemy phase
    // (seeded from PatrolPhase) keeps a pack from strafing in lockstep.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy|Approach", meta=(ClampMin="0")) float WeaveFrequency = 1.6f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy|Approach", meta=(ClampMin="0", ClampMax="1")) float WeaveStrength = 0.45f;
    // Committed lunge: a short burst inside this range, telegraphed one
    // frame ahead through StateLabel so the HUD shows "LUNGE".
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy|Approach", meta=(ClampMin="0")) float LungeRange = 450.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy|Approach", meta=(ClampMin="1")) float LungeSpeedMultiplier = 2.2f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy|Approach", meta=(ClampMin="0")) float LungeDuration = 0.35f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy|Approach", meta=(ClampMin="0")) float LungeCooldown = 1.2f;

    // SKITTER's committed leap, Encounter-Design §2.1. The lunge shipped
    // without either half of what the document specifies: no wind-up, and a
    // direction re-solved every frame, so it tracked the player through the
    // whole burst and there was nothing to step out of. The document is
    // explicit that "the committed leap is the whole design" — the direction is
    // LOCKED at wind-up and it cannot track, so any lateral movement defeats
    // it, and the wind-up EXPOSES the weak point so the correct answer is
    // "step sideways and shoot the thing it just showed you".
    //
    // O1 makes this the right shape for the whole game: a passive-defence
    // player answers with position, so a committed, telegraphed, non-tracking
    // commitment is fair where a tracking one is not.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy|Approach", meta=(ClampMin="0")) float LungeWindupSeconds = 0.55f;   // O2 PLACEHOLDER
    // The tell: the weak point swells during the wind-up. A scale change on one
    // primitive, so it survives in untextured graybox (§2.1).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy|Approach", meta=(ClampMin="1")) float LungeWeakPointSwell = 2.1f;   // O2 PLACEHOLDER
    // Movement during the wind-up, as a fraction of normal. The crouch.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy|Approach", meta=(ClampMin="0", ClampMax="1")) float LungeWindupMoveScale = 0.25f;   // O2 PLACEHOLDER

    // The aim-error seam's unit: how many degrees of error cone ONE full unit
    // of keyed degradation (multiplier 2.0) opens on this enemy's shots, over
    // and above any authored spread. Only read when a key is live — at the
    // neutral 1.0 the term is exactly zero and authored aim is untouched.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy|Seams", meta=(ClampMin="0")) float AimErrorUnitDegrees = 10.0f;   // O2 PLACEHOLDER

    // Shared state a derived archetype legitimately needs to read or drive.
    // PatrolPhase doubles as the per-enemy desync seed so a pack never acts in
    // lockstep; StateLabel is what the playtest HUD prints over the enemy.
    float PatrolPhase = 0.0f;
    double LastAttackTime = -1000.0;
    bool bDead = false;
    FString StateLabel = TEXT("PATROL");
    // Optional facing override for one frame. Zero means "face the direction
    // you are moving", which is the melee behaviour. A ranged enemy strafes
    // sideways while facing the player, so it sets this every frame.
    FVector DesiredFacing = FVector::ZeroVector;

    // Composed into the chassis alongside ArchetypeHealthMultiplier. DERIVED
    // from the modifier count (Encounter-Design §1.1's "+0.35x per modifier
    // beyond the first"); never authored, because that would be a second source
    // of truth for what a modifier is worth.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Enemy|Area") float ModifierCountHealthMultiplier = 1.0f;

    // The authored speed and weave, captured once so Fleetfoot multiplies
    // against a base rather than compounding against whatever it did last time.
    float BaseMoveSpeed = -1.0f;
    float BaseWeaveStrength = -1.0f;
    // Whoever this enemy is currently engaging, handed to the modifier layer as
    // a bare AActor*. The modifier layer reads its POSITION and nothing else.
    TWeakObjectPtr<AActor> ModifierTrackedTarget;
    // Wakeful needs to know how the killing blow landed.
    bool bLastHitWasWeakPoint = false;

private:
    FVector LeashOrigin = FVector::ZeroVector;
    int32 KillCount = 0;
    double FirstDamageTime = -1.0;
    // Engagement-gapped TTK: sums time between damage events, capping each
    // gap, so tagging an enemy and returning later doesn't book the idle
    // time as time-to-kill.
    double LastDamageEventTime = -1.0;
    float EngagedSeconds = 0.0f;
    float WeaveTime = 0.0f;
    double LungeStartTime = -1000.0;
    // Set when the wind-up begins, cleared when the burst ends. The whole point
    // of the archetype: this is decided ONCE and never re-solved.
    double LungeWindupStartTime = -1000.0;
    FVector LungeLockedDirection = FVector::ZeroVector;
    bool bLungeWindingUp = false;
    float WeakPointBaseScale = 0.4f;

    // The three seam lanes. Plain maps, not UPROPERTYs: entries are pushed and
    // popped by live effects that also own the teardown (the armour lane's
    // discipline), so nothing here needs to survive serialization.
    TMap<FName, float> WindupDurationMultipliers;
    TMap<FName, float> AimErrorMultipliers;
    TMap<FName, float> OutgoingDamageMultipliers;
};
