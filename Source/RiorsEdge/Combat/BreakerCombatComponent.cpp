#include "Combat/BreakerCombatComponent.h"

#include "AbilitySystemInterface.h"
#include "AbilitySystemComponent.h"
#include "Abilities/BreakerAbilityStateComponent.h"
#include "Classes/BreakerChargeComponent.h"
#include "Combat/BreakerZoneActor.h"
#include "Combat/BreakerZoneMath.h"
#include "Progression/BreakerProgressionLibrary.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Attributes/BreakerHealthBands.h"
#include "Combat/BreakerDamageLibrary.h"
#include "Combat/BreakerEnemy.h"
#include "Combat/BreakerShieldMath.h"
#include "Characters/BreakerCharacter.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/GameStateBase.h"
#include "Items/BreakerEquipmentComponent.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Net/UnrealNetwork.h"

UBreakerCombatComponent::UBreakerCombatComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    SetIsReplicatedByDefault(true);
}

void UBreakerCombatComponent::BeginPlay()
{
    Super::BeginPlay();
    if (GetOwner() && GetOwner()->HasAuthority())
    {
        if (UBreakerProgressionComponent* Progression = GetOwner()->FindComponentByClass<UBreakerProgressionComponent>())
            Progression->OnProgressionChanged.AddUniqueDynamic(this, &ThisClass::RefreshParryPermission);
        RefreshParryPermission();
    }
    if (const IAbilitySystemInterface* AbilityOwner = Cast<IAbilitySystemInterface>(GetOwner()))
    {
        if (UAbilitySystemComponent* ASC = AbilityOwner->GetAbilitySystemComponent())
        {
            Attributes = const_cast<UBreakerAttributeSet*>(ASC->GetSet<UBreakerAttributeSet>());
        }
    }
}

void UBreakerCombatComponent::BindAttributes(UBreakerAttributeSet* InAttributes)
{
    Attributes = InAttributes;
}

void UBreakerCombatComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME_CONDITION(UBreakerCombatComponent, bParryOwned, COND_OwnerOnly);
    DOREPLIFETIME_CONDITION(UBreakerCombatComponent, ParryWindowEnd, COND_OwnerOnly);
    DOREPLIFETIME_CONDITION(UBreakerCombatComponent, ParryCooldownEnd, COND_OwnerOnly);
    DOREPLIFETIME_CONDITION(UBreakerCombatComponent, ParryCounterEnd, COND_OwnerOnly);
}

float UBreakerCombatComponent::ParryClock() const
{
    if (!GetWorld()) return 0.0f;
    const AGameStateBase* GameState = GetWorld()->GetGameState();
    return GameState ? GameState->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
}

bool UBreakerCombatComponent::HasParryPermission() const
{
    if (!GetOwner()) return false;
    if (!GetOwner()->HasAuthority()) return bParryOwned;
    const UBreakerProgressionComponent* Progression = GetOwner()->FindComponentByClass<UBreakerProgressionComponent>();
    return Progression && Progression->HasNodeTag(BreakerNodeTags::Verb_Parry.GetTag());
}

bool UBreakerCombatComponent::IsParryAvailable() const
{
    return HasParryPermission() && !IsDead() && GetParryCooldownRemaining() <= 0.0f;
}
bool UBreakerCombatComponent::IsParryActive() const
{
    return HasParryPermission() && !IsDead() && ParryWindowEnd > ParryClock();
}
bool UBreakerCombatComponent::IsParryCounterActive() const
{
    return HasParryPermission() && !IsDead() && ParryCounterEnd > ParryClock();
}
float UBreakerCombatComponent::GetParryCooldownRemaining() const
{
    return HasParryPermission() && !IsDead() ? FMath::Max(0.0f, ParryCooldownEnd - ParryClock()) : 0.0f;
}
float UBreakerCombatComponent::GetParryWindowRemaining() const
{
    return IsParryActive() ? FMath::Max(0.0f, ParryWindowEnd - ParryClock()) : 0.0f;
}
void UBreakerCombatComponent::ClearParryWindows()
{
    const bool bHadCounter = ParryCounterEnd >= 0.0f;
    ParryWindowEnd = ParryCooldownEnd = ParryCounterEnd = -1.0f;
    if (bHadCounter && GetOwner())
        if (UBreakerProgressionComponent* Progression = GetOwner()->FindComponentByClass<UBreakerProgressionComponent>())
            Progression->RefreshBuildConditions();
}
void UBreakerCombatComponent::RefreshParryPermission()
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;
    bParryOwned = HasParryPermission();
    if (!bParryOwned || IsDead()) ClearParryWindows();
}
bool UBreakerCombatComponent::TryParry()
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || !GetWorld() || !Attributes || !IsParryAvailable()) return false;
    RefreshParryPermission();
    const UBreakerProgressionComponent* Progression = GetOwner()->FindComponentByClass<UBreakerProgressionComponent>();
    const float Bonus = Progression->HasNodeTag(BreakerNodeTags::Node_Read.GetTag()) ? ReadParryBonusSeconds : 0.0f;
    ParryWindowEnd = ParryClock() + FMath::Max(0.0f, ParryWindowSeconds + Bonus);
    ParryCooldownEnd = ParryClock() + FMath::Max(0.0f, ParryCooldownSeconds);
    return true;
}

void UBreakerCombatComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    if (GetOwner() && GetOwner()->HasAuthority()) RefreshParryPermission();

    // THE PLAYER-SIDE SHIELD RECHARGE — the missing source for a pool the
    // damage library has spent correctly since it shipped. Pure step in
    // Combat/BreakerShieldMath.h, clocked off this component's own
    // seconds-since-damage (the write at the top of ReceiveDamage is what
    // makes taking a hit stop the refill). Recharge fills SHIELD ONLY and
    // never routes through ApplyHealing — the sustain asymmetry is the
    // armour archetypes' whole mechanism. PLAYER pawns only: enemies own
    // their shields through the Warded modifier's recharge, and a shielded
    // target dummy that refilled itself would quietly move every TTK probe.
    if (Attributes && GetOwner() && GetOwner()->HasAuthority()
        && Cast<APawn>(GetOwner()) && Cast<APawn>(GetOwner())->IsPlayerControlled())
    {
        const float Current = Attributes->GetShield();
        const float Next = BreakerShield::RechargeStep(Current, Attributes->GetMaxShield(),
            GetSecondsSinceDamage(), DeltaTime);
        if (Next > Current) Attributes->ApplyShield(Next);

        const ABreakerCharacter* Player = Cast<ABreakerCharacter>(GetOwner());
        if (Player && !IsDead())
        {
            const float Health = Attributes->GetHealth();
            const float Recovered = BreakerHealthRegen::Step(Health, Attributes->GetMaxHealth(),
                Player->GetSecondsSinceCombat(), DeltaTime, BaseHealthRegenPerSecond, BaseHealthRegenDelaySeconds);
            // Passive recovery is not a healing action: it must not generate class resources or heal procs.
            if (Recovered > Health) Attributes->ApplyHealth(Recovered);
        }
    }
}

void UBreakerCombatComponent::ArmMeleeDefenseSuppression(AActor* Attacker, float DurationSeconds)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || !Attacker || !GetWorld() || IsDead()
        || !FMath::IsFinite(DurationSeconds) || DurationSeconds <= 0) return;
    MeleeDefenseSuppressionExpiry.Add(Attacker, GetWorld()->GetTimeSeconds() + DurationSeconds);
}

FBreakerDamageResult UBreakerCombatComponent::ReceiveDamage(const FBreakerDamageRequest& Request)
{
    FBreakerDamageResult Result;
    if (!Attributes || !GetOwner() || !GetOwner()->HasAuthority() || IsDead()) return Result;
    const FVector TowardSource = (Request.SourceLocation - GetOwner()->GetActorLocation()).GetSafeNormal2D();
    if (IsParryActive() && Request.BaseDamage > 0.0f && !Request.bIsDamageOverTime
        && Request.Instigator.Get() != GetOwner()
        && Request.bHasSourceLocation && !TowardSource.IsNearlyZero()
        && FVector::DotProduct(GetOwner()->GetActorForwardVector().GetSafeNormal2D(), TowardSource) >= 0.0f)
    {
        ParryWindowEnd = -1.0f;
        ParryCounterEnd = ParryClock() + FMath::Max(0.0f, ParryCounterSeconds);
        // Incoming pressure still counts as combat for recovery; recording
        // the clock does not broadcast damage, block or dodge procs.
        LastDamageTime = GetWorld()->GetTimeSeconds();
        Result.bParried = true;
        Result.RemainingHealth = Attributes->GetHealth();
        Result.RemainingShield = Attributes->GetShield();
        if (UBreakerProgressionComponent* Progression = GetOwner()->FindComponentByClass<UBreakerProgressionComponent>())
            Progression->RefreshBuildConditions();
        // No hit/dodge/block broadcasts: a negated strike pays no such procs.
        return Result;
    }

    FBreakerDefenseState Defense;
    Defense.Health = Attributes->GetHealth();
    Defense.Shield = Attributes->GetShield();
    // THE FRONT POOL (O198). Facing decides whether the bearer's front pool
    // stands in the shield step for this hit: a frontal hit that does not
    // bypass shields sees the pool ahead of the ward, a rear-arc hit sees the
    // ward alone. The library resolves one combined shield figure; the split
    // below the resolve hands the pool its share first and the ward the spill.
    // Decided per hit off the request's own source location, the same
    // geometry IsRearArcHit answers with, so presentation and payment agree.
    const float WardBefore = Defense.Shield;
    const bool bFrontPoolStands = FrontShieldMax > 0.0f && !bFrontShieldBroken && FrontShield > 0.0f
        && !Request.bBypassShield && Request.bHasSourceLocation && !IsRearArcHit(Request.SourceLocation);
    if (bFrontPoolStands) Defense.Shield += FrontShield;
    // Flat strippers (Rot, Disruptor) come off here, clamped at zero: negative
    // armour would invert the mitigation formula into a damage bonus.
    Defense.Armor = GetEffectiveArmor();
    // Facing-dependent armour (Encounter-Design §7). Applied AFTER the flat
    // strippers and before the mitigation curve, so a Rot puddle and a flank
    // compose the way a player would expect rather than fighting over the same
    // number. Off by default (multiplier 1.0), so nothing that has not opted in
    // can change.
    if (Request.bHasSourceLocation && !FMath::IsNearlyEqual(RearArcArmorMultiplier, 1.0f))
    {
        Defense.Armor *= UBreakerDamageLibrary::GetFacingArmorMultiplier(
            GetOwner()->GetActorForwardVector(), GetOwner()->GetActorLocation(),
            Request.SourceLocation, RearArcArmorMultiplier, RearArcCosine);
    }
    // Core.Ruin.Execute: below the execute threshold, the ATTACKER's damage
    // ignores armour — resolved here, the one site that knows both actors
    // (the Stage-6 seam). The threshold reads the defender's live fraction;
    // armour alone is zeroed, so resistance, block and dodge stand.
    if (Attributes->GetMaxHealth() > 0.0f
        && Attributes->GetHealth() / Attributes->GetMaxHealth() <= ExecuteHealthFraction)
    {
        if (const AActor* ExecuteAttacker = Request.Instigator.Get())
        {
            const UBreakerProgressionComponent* AttackerProgression = ExecuteAttacker->FindComponentByClass<UBreakerProgressionComponent>();
            if (AttackerProgression && AttackerProgression->HasNodeTag(
                FGameplayTag::RequestGameplayTag(TEXT("Progression.Node.Core.Ruin.Execute"), false)))
            {
                Defense.Armor = 0.0f;
            }
        }
    }
    // Gear-rolled damage reduction folds into the incoming multiplier so the
    // resolution order stays single-path. Physical DR and Elemental
    // resistance are the same mechanism aimed at different families (the
    // defense triad, owner ruling 2026-08-16/17): each pays only against its
    // own family, and TrueDamage answers to neither — that is what True
    // means. The Elemental branch is authored ahead of any enemy that deals
    // Elemental damage, deliberately: the stat's consumer exists TODAY so
    // the day elemental incoming lands (O5/O38) the gear line pays with no
    // further wiring — and until then the branch is simply never taken,
    // because no request arrives carrying the family.
    // THE LOOKUP IS GEAR-SHAPED, AND THAT IS THE REAL BLOCKER ON ENEMY
    // RESISTANCE -- recorded, not built.
    //
    // Only ABreakerCharacter constructs a UBreakerEquipmentComponent, so an
    // enemy finds nothing here and takes no reduction of either family. That is
    // why Core.Elements.Penetrance still has nothing to penetrate. The obvious
    // fix -- give enemies the component -- is WRONG and was rejected: its
    // BeginPlay calls EnsureStarterKit, so every enemy would spawn holding an
    // Issue Rifle, and it also brings bCanEverTick, SetIsReplicatedByDefault
    // and BindCombatEvents per spawn. Six other sites look this component up
    // off an arbitrary owner and would all start finding one.
    //
    // THE SHAPE, when it is built: GetIncomingReduction(Family) on this
    // component -- equipment when there is one, the owner's chassis otherwise,
    // zero by default. The seam goes live without anything else moving, and
    // enemy resistance then sits on the CHASSIS beside ModifierHealthMultiplier,
    // which is where it belongs: effective health is HP x 1/(1-R), the chassis
    // already owns the other lever, and two independent tables producing one
    // quantity is the failure the reward ladders are already an instance of.
    //
    // NOT YET, and O116 is why. Time-to-die is solved at 4.50s and 4.53s
    // against one baseline character at both ends; a resistance term multiplies
    // it directly, so a non-zero value has to be folded into that derivation
    // rather than added on top of it.
    const UBreakerProgressionComponent* Progression = GetOwner()->FindComponentByClass<UBreakerProgressionComponent>();
    if (Request.DamageFamily != EBreakerDamageFamily::TrueDamage)
    {
        float ReductionPercent = 0.0f;
        if (const UBreakerEquipmentComponent* Equipment = GetOwner()->FindComponentByClass<UBreakerEquipmentComponent>())
        {
            ReductionPercent += Request.DamageFamily == EBreakerDamageFamily::Physical
                ? Equipment->GetStats().PhysicalDamageReductionPercent
                : Equipment->GetStats().ElementalResistancePercent;
        }
        // The tree's lane joins gear's family bucket here — points summed,
        // ONE 1-R application — never a second multiplier beside it. It pays
        // against both families because the node line names incoming damage,
        // not a family; TrueDamage answers to neither layer. Clamped at zero
        // now that two layers can sum past 100.
        if (Progression)
        {
            ReductionPercent += Progression->GetNodeStats().IncomingDamageReductionPercent;
        }
        Defense.IncomingDamageMultiplier *= FMath::Max(0.0f, 1.0f - ReductionPercent / 100.0f);
    }
    // Pushed incoming modifiers compose on top, in the same stage: Caster's
    // Overcast penalty, and defensive windows when they land.
    Defense.IncomingDamageMultiplier *= GetComposedIncomingDamageMultiplier();
    Defense.DodgeChance = DodgeChance;
    Defense.BlockChance = BlockChance;
    Defense.BlockMitigation = BlockMitigation;
    // Tree nodes raise the passive layers on top of the component baseline.
    if (Progression)
    {
        Defense.DodgeChance = FMath::Clamp(Defense.DodgeChance + Progression->GetDodgeChanceBonus(), 0.0f, 1.0f);
        Defense.BlockChance = FMath::Clamp(Defense.BlockChance + Progression->GetBlockChanceBonus(), 0.0f, 1.0f);
        // Core.Bulwark.Interposition: a block that succeeds cannot be
        // followed by a second hit in the same window — inside it, the block
        // roll becomes certainty. The chance path, not a bypass, so block
        // mitigation, O78's ordering and every downstream term are untouched.
        const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
        if (Now - LastSuccessfulBlockTime <= InterpositionWindowSeconds
            && Progression->HasNodeTag(FGameplayTag::RequestGameplayTag(TEXT("Progression.Node.Core.Bulwark.Interposition"), false)))
        {
            Defense.BlockChance = 1.0f;
        }
    }

    const double SuppressionNow = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
    for (auto It = MeleeDefenseSuppressionExpiry.CreateIterator(); It; ++It)
        if (!It.Key().IsValid() || It.Value() <= SuppressionNow) It.RemoveCurrent();
    const FGameplayTag MeleeTag = FGameplayTag::RequestGameplayTag(TEXT("Damage.Melee"), false);
    if (!Request.bIsDamageOverTime && Request.ProcCoefficient > 0 && Request.BaseDamage > 0
        && Request.SourceTags.HasTagExact(MeleeTag)
        && MeleeDefenseSuppressionExpiry.Remove(Request.Instigator) > 0)
    {
        Defense.DodgeChance = 0;
        Defense.BlockChance = 0;
    }

    // Stage 6 (H3): target-conditional riders resolve HERE, the one site that
    // knows both actors. A local copy so the caller's request is untouched;
    // ApplyTargetConditionRiders leaves it bit-identical unless a rider
    // actually fired against this target with the source split present.
    FBreakerDamageRequest ResolvedRequest = Request;
    ApplyTargetConditionRiders(ResolvedRequest);
    Result = UBreakerDamageLibrary::ResolveDamage(ResolvedRequest, Defense);
    if (Result.bDodged)
    {
        AddClassResource(DodgeResourceRefund);
        OnDamageReceived.Broadcast(Result);
        return Result;
    }
    // The front pool pays first; what spills goes on to the ward. The result's
    // RemainingShield is rewritten to the WARD's remainder so the attribute
    // write below stays the ward's alone — the pool lives on this component,
    // never in the attribute set, and a bar reads the sum through
    // GetDisplayShield. The break is latched here and broadcast after the
    // vitals write, so a listener sees the state the hit left.
    bool bFrontBrokeThisHit = false;
    if (bFrontPoolStands)
    {
        const BreakerShield::FFrontSpend Spend = BreakerShield::SpendFrontPool(FrontShield, Result.ShieldDamage);
        FrontShield = Spend.Remaining;
        Result.RemainingShield = FMath::Max(0.0f, WardBefore - Spend.Spill);
        if (Spend.bBroke)
        {
            bFrontShieldBroken = true;
            bFrontBrokeThisHit = true;
        }
    }
    // Same null-safe route the healing path uses: identical to the generated
    // setters when there is an ability system, and writable (rather than an
    // ensure) when there is not, which is what lets automation exercise a
    // whole damage submission instead of only the pure resolver.
    Attributes->ApplyShield(Result.RemainingShield);
    Attributes->ApplyHealth(Result.RemainingHealth);
    if (bFrontBrokeThisHit) OnFrontShieldBroken.Broadcast();
    // TargetBandBroken's write: did THIS hit move the health-band index?
    // Defense.Health is the pre-damage read from the top of this function, so
    // the pair brackets exactly the damage that just landed. Overwritten by
    // every landed hit (snapshotted DoT ticks included) and by nothing else:
    // a dodge returned before this line, and a heal raising the bar back
    // across the boundary leaves the bit true — it states a fact about the
    // previous HIT, not about current band arithmetic. A killing hit that
    // crossed a boundary sets it too, which matters only to a revived body:
    // Wakeful's revive keeps it honestly ("the previous hit broke a band"),
    // and the pool's reuse path clears it via ClearBandBreakTracking.
    {
        const ABreakerEnemy* OwnerEnemy = Cast<ABreakerEnemy>(GetOwner());
        const int32 Segments = BreakerHealthBands::SegmentCountFor(
            OwnerEnemy ? OwnerEnemy->GetMonsterRank() : EBreakerMonsterRank::Trash);
        const float MaxHealth = Attributes->GetMaxHealth();
        bBandBrokenByPreviousHit =
            BreakerHealthBands::IndexOf(Result.RemainingHealth, MaxHealth, Segments)
            < BreakerHealthBands::IndexOf(Defense.Health, MaxHealth, Segments);
    }
    LastDamageTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
    // Interposition's clock arms on the SUCCESSFUL block itself, so the
    // window measures from the block the player felt, not from the swing.
    if (Result.bBlocked)
    {
        LastSuccessfulBlockTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
    }
    OnDamageReceived.Broadcast(Result);
    if (Result.bKilled && !bDeathBroadcast)
    {
        bDeathBroadcast = true;
        ClearParryWindows();
        OnDeath.Broadcast();
    }
    DispatchHitDealt(Request, Result);
    return Result;
}

void UBreakerCombatComponent::ApplyTargetConditionRiders(FBreakerDamageRequest& Request) const
{
    // STAGE 6, the mechanism of Hook-And-Condition-Vocabulary §3.2 step by
    // step. Every early return below is a request resolving exactly as it did
    // before target riders existed — that bit-identity is test-pinned.
    //
    // The split gate first: without the source's Increased/More halves the
    // recomposition would have to guess how much of the composed multiplier is
    // additive bucket, and a guess here is a second More by accident. Every
    // live submission fills the split now that one function does it — the O54
    // pass routed the ability sites through FillSourcePools too, so abilities
    // get target riders for the first time. Snapshotted DoT ticks still take
    // this exit: their multiplier is the application-time snapshot, and its
    // halves were never carried.
    if (!Request.bHasSourceSplit) return;

    // A request outliving its dealer (a rocket in flight after the shooter
    // died) has nobody whose rider table could answer.
    const AActor* Attacker = Request.Instigator.Get();
    if (!Attacker) return;
    const UBreakerProgressionComponent* Progression = Attacker->FindComponentByClass<UBreakerProgressionComponent>();
    if (!Progression) return;
    const TArray<FBreakerTargetConditionRider>& Riders = Progression->GetTargetConditionRiders();
    if (Riders.IsEmpty()) return;

    // The event's condition state: the attacker's cached SELF half (the same
    // standing state its own aggregation uses, so a mixed "airborne AND
    // target bleeding" rider reads one truth) plus the target half supplied
    // from this component's owner — the call site the vocabulary document
    // named as SupplyTargetState's one honest home.
    FBreakerBuildConditionState Conditions = Progression->GetActiveConditions();
    Conditions.SupplyTargetState(GetOwner(), Attacker);

    float RiderPercent = 0.0f;
    float RiderMoreProduct = 1.0f;
    for (const FBreakerTargetConditionRider& Rider : Riders)
    {
        // O54: a rider pays only into the lane this hit actually drew. The
        // shared pool matches both deliveries by definition; a weapon-lane
        // rider on an ability hit, or the reverse, is skipped rather than
        // folded into the wrong bucket — which is the "silently pay into the
        // general bucket" failure the pre-split filter was placed here to
        // prevent, now that partition rows genuinely exist.
        //
        // O98 widens the same rule rather than adding a second one: a
        // rider-delivered slice (MeleeDamage) has no pool of its own, so
        // BreakerRiderLanePoolFor names the lane the slice cuts — weapon —
        // and the tag gate below is what makes it a SLICE of that lane
        // rather than the lane itself.
        const EBreakerDamagePool RiderPool = BreakerRiderLanePoolFor(Rider.StatTarget);
        const bool bDeliveredLane = RiderPool == EBreakerDamagePool::Weapon
            || RiderPool == EBreakerDamagePool::Ability
            || RiderPool == EBreakerDamagePool::Shared;
        const bool bLaneMatches = RiderPool == EBreakerDamagePool::Shared
            || (RiderPool == EBreakerDamagePool::Ability) == (Request.Delivery == EBreakerDamageDelivery::Ability);
        if (!bDeliveredLane || !bLaneMatches) continue;
        // The tag gate: a tag-keyed row pays only when the request says the
        // hit IS that slice. Cleave and the Tank sweep stamp Damage.Melee at
        // their fill sites; a bullet carries no tag and pays nothing here.
        if (Rider.RequiredSourceTag.IsValid() && !Request.SourceTags.HasTag(Rider.RequiredSourceTag)) continue;
        if (Conditions.SatisfiesAll(Rider.Condition, Rider.AlsoRequires))
        {
            RiderPercent += Rider.Percent;
            if (Rider.MorePercent > 0.0f) RiderMoreProduct *= 1.0f + Rider.MorePercent / 100.0f;
        }
    }
    const bool bRiderMoreFired = !FMath::IsNearlyEqual(RiderMoreProduct, 1.0f);
    if (FMath::IsNearlyZero(RiderPercent) && !bRiderMoreFired) return;

    // The recomposition: an Increased rider joins the source's ADDITIVE
    // bucket, under the flat factor the source carried (O196: the flat layer
    // multiplies the whole Increased bucket, riders included, never only the
    // source's half of it), and — per O141 — the ONE hit-time More rider
    // (Collapse; TreeContent.OneHitTimeMore pins the population at one, and a
    // second is a request to revisit the 1.30^3 ceiling, a different and
    // larger ruling) multiplies the standing More product under the one O34
    // ceiling: HEADROOM, never a slot, the same law the outgoing window chain
    // already spends by. The clamp cannot double-count because
    // SourceMoreProduct is the request's WHOLE prior More spend —
    // ApplyOutgoingModifiers folds the window chain into it as well as into
    // the composed value — so Ceiling/SourceMoreProduct is the true residual.
    // PaidRiderMore folds into SourceMoreProduct too, so the header's
    // identity (Flat x (1 + (Inc + Riders)/100) x SourceMoreProduct ==
    // SourceDamageMultiplier) stays literally true; nothing downstream reads
    // the split again, so this costs nothing and keeps the formula honest.
    // Every factor is floored at zero so a hostile authored negative can never
    // invert damage.
    const float FlatFactor = FMath::Max(0.0f, Request.SourceFlatFactor);
    const float IncreasedFactor = FMath::Max(0.0f, 1.0f + (Request.SourceIncreasedPercent + RiderPercent) / 100.0f);
    const float StandingMore = FMath::Max(0.0f, Request.SourceMoreProduct);
    if (bRiderMoreFired)
    {
        const float Ceiling = FBreakerAttributeAggregator::ComposedMoreCeiling();
        const float RiderBudget = FMath::Max(1.0f, Ceiling / FMath::Max(StandingMore, UE_SMALL_NUMBER));
        const float PaidRiderMore = FMath::Min(RiderMoreProduct, RiderBudget);
        if (PaidRiderMore < RiderMoreProduct - UE_KINDA_SMALL_NUMBER)
        {
            // Loud when the ceiling bites, once: the partial (or zero)
            // payment is the ruling's intent, not a defect, but it must be
            // audible rather than discovered on a damage sheet.
            static bool bWarnedClampOnce = false;
            if (!bWarnedClampOnce)
            {
                bWarnedClampOnce = true;
                UE_LOG(LogTemp, Log, TEXT("[BreakerCombat] a hit-time More rider (x%.3f) was clamped to x%.3f by the O34 ceiling (standing product %.3f) — headroom spent, working as ruled (O141)."),
                    RiderMoreProduct, PaidRiderMore, StandingMore);
            }
        }
        Request.SourceMoreProduct = StandingMore * PaidRiderMore;
    }
    Request.SourceDamageMultiplier = FlatFactor * IncreasedFactor * FMath::Max(0.0f, Request.SourceMoreProduct);
}

void UBreakerCombatComponent::DispatchHitDealt(const FBreakerDamageRequest& Request, const FBreakerDamageResult& Result)
{
    AActor* Dealer = Request.Instigator.Get();

    FBreakerHitContext Context;
    Context.Instigator = Dealer;
    Context.Target = GetOwner();
    Context.Result = Result;
    Context.bFromDoT = Request.bIsDamageOverTime;
    Context.SourceTags = Request.SourceTags;
    Context.ProcCoefficient = Request.ProcCoefficient;
    Context.bWeakPoint = Result.bWeakPoint;
    Context.DamageFamily = Request.DamageFamily;
    Context.Delivery = Request.Delivery;
    // The IMPACT point when the request carries one (weapons trace real hits,
    // projectiles resolve at a real location), so the HUD's floating number
    // draws where the shot landed. The victim's pivot is only the FALLBACK for
    // paths with no impact of their own (DoT ticks, hazards) — before this,
    // every number drew at the pivot and the owner read it as bad hit
    // recognition on spells and effects.
    Context.WorldLocation = Request.bHasImpactLocation
        ? Request.ImpactLocation
        : (GetOwner() ? GetOwner()->GetActorLocation() : Request.SourceLocation);

    // Victim side first, and unconditionally: a hit with no instigator at all
    // (an environmental hazard, a test) is still a hit the victim took, and the
    // Reflective modifier's "there is nobody to answer" case has to be a live
    // broadcast with a null Instigator rather than silence.
    OnDamageTaken.Broadcast(Context);

    // Self-damage would otherwise let a listener that deals damage on hit
    // re-enter its own dealer component without bound.
    if (!Dealer || Dealer == GetOwner()) return;
    UBreakerCombatComponent* DealerCombat = Dealer->FindComponentByClass<UBreakerCombatComponent>();
    if (!DealerCombat) return;

    DealerCombat->OnHitDealt.Broadcast(Context);
    if (Result.bKilled) DealerCombat->OnKillDealt.Broadcast(Context);
}

bool UBreakerCombatComponent::IsRearArcHit(const FVector& SourceLocation) const
{
    if (!GetOwner()) return false;
    // Asks the same pure function the damage path asks, with a rear multiplier
    // of zero, so "did that land on the seams" can never disagree with what the
    // armour step actually did.
    return UBreakerDamageLibrary::GetFacingArmorMultiplier(
        GetOwner()->GetActorForwardVector(), GetOwner()->GetActorLocation(),
        SourceLocation, 0.0f, RearArcCosine) < 1.0f;
}

void UBreakerCombatComponent::ArmFrontShield(float Amount)
{
    // Sets both figures and clears the latch: the one refill the pool has.
    // RestoreVitals does NOT call this — the archetype that means a front
    // pool binds OnVitalsRestored to its own arming, so a body that never
    // armed one never grows one on a reset.
    FrontShieldMax = FMath::Max(0.0f, Amount);
    FrontShield = FrontShieldMax;
    bFrontShieldBroken = false;
}

float UBreakerCombatComponent::GetDisplayShield() const
{
    const float Ward = Attributes ? Attributes->GetShield() : 0.0f;
    return Ward + (bFrontShieldBroken ? 0.0f : FrontShield);
}

float UBreakerCombatComponent::GetDisplayMaxShield() const
{
    const float Ward = Attributes ? Attributes->GetMaxShield() : 0.0f;
    return Ward + (bFrontShieldBroken ? 0.0f : FrontShieldMax);
}

void UBreakerCombatComponent::PushOutgoingModifier(FName Key, float FlatBonus, float MoreMultiplier, float ExpirySeconds)
{
    if (Key.IsNone()) return;
    FBreakerOutgoingModifier Modifier;
    Modifier.Key = Key;
    Modifier.FlatBonus = FlatBonus;
    Modifier.MoreMultiplier = FMath::Max(0.0f, MoreMultiplier);
    // O34: a single More source is capped at the SAME per-source ceiling the
    // aggregator's budget is derived from (O3's 1.30, cited — never restated).
    // The tree's selection already clamps its own sources this way; a window
    // that pushed 1.6x would otherwise be a stronger More than any node may be.
    if (Modifier.MoreMultiplier > FBreakerAttributeAggregator::SingleMoreCeiling)
    {
        UE_LOG(LogTemp, Warning, TEXT("Outgoing modifier '%s' More %.3f exceeds the single-More ceiling %.2f (O3/O34); clamping."),
            *Key.ToString(), Modifier.MoreMultiplier, FBreakerAttributeAggregator::SingleMoreCeiling);
        Modifier.MoreMultiplier = FBreakerAttributeAggregator::SingleMoreCeiling;
    }
    Modifier.ExpiryTime = ExpirySeconds > 0.0f && GetWorld()
        ? static_cast<float>(GetWorld()->GetTimeSeconds()) + ExpirySeconds
        : -1.0f;

    // Re-pushing the same key replaces rather than stacks: a window refreshed
    // mid-flight must not compose with itself.
    for (FBreakerOutgoingModifier& Existing : OutgoingModifiers)
    {
        if (Existing.Key == Key)
        {
            Existing = Modifier;
            return;
        }
    }
    OutgoingModifiers.Add(Modifier);
}

void UBreakerCombatComponent::RemoveOutgoingModifier(FName Key)
{
    OutgoingModifiers.RemoveAll([Key](const FBreakerOutgoingModifier& Modifier) { return Modifier.Key == Key; });
}

void UBreakerCombatComponent::PruneExpiredOutgoingModifiers()
{
    if (OutgoingModifiers.IsEmpty() || !GetWorld()) return;
    const float Now = static_cast<float>(GetWorld()->GetTimeSeconds());
    OutgoingModifiers.RemoveAll([Now](const FBreakerOutgoingModifier& Modifier)
    {
        return Modifier.ExpiryTime >= 0.0f && Modifier.ExpiryTime <= Now;
    });
}

float UBreakerCombatComponent::GetAttributeSideMoreProduct() const
{
    // The aggregator recomputes from the live equipment and progression
    // contributions, so this is always the post-clamp product the composed
    // DamageMultiplier attribute actually contains. No attribute set bound
    // (an enemy, a bare test rig) means no attribute-side Mores: 1.0.
    return Attributes
        ? Attributes->GetAttributeAggregator().ComposedMoreProduct(EBreakerAggregatedAttribute::DamageMultiplier)
        : 1.0f;
}

float UBreakerCombatComponent::GetComposedMoreMultiplier() const
{
    float Product = 1.0f;
    const float Now = GetWorld() ? static_cast<float>(GetWorld()->GetTimeSeconds()) : -1.0f;
    for (const FBreakerOutgoingModifier& Modifier : OutgoingModifiers)
    {
        if (Now >= 0.0f && Modifier.ExpiryTime >= 0.0f && Modifier.ExpiryTime <= Now) continue;
        Product *= Modifier.MoreMultiplier;
    }

    // O34: ONE More ceiling. The chain spends whatever headroom the attribute
    // side (tree keystones, Anomalous rewrites) left under the aggregator's
    // budget — total effective More is (attribute-side product x chain product)
    // and may never exceed FBreakerAttributeAggregator::ComposedMoreCeiling().
    // On a build already holding three Mores near the ceiling a window buys
    // little; that competition is the ruling's intent, not a defect.
    const float Ceiling = FBreakerAttributeAggregator::ComposedMoreCeiling();
    const float AttributeSide = FMath::Max(GetAttributeSideMoreProduct(), UE_SMALL_NUMBER);
    const float ChainBudget = Ceiling / AttributeSide;
    if (Product > ChainBudget + UE_KINDA_SMALL_NUMBER)
    {
        // Loud but suite-safe: automation intentionally crosses the ceiling,
        // and an ensure would fail the run it exists to protect.
        UE_LOG(LogTemp, Warning, TEXT("Composed More total %.3f (attribute side %.3f x chain %.3f) exceeds the %.3f ceiling (O34); clamping the chain to %.3f."),
            AttributeSide * Product, AttributeSide, Product, Ceiling, ChainBudget);
        Product = ChainBudget;
    }
    return Product;
}

float UBreakerCombatComponent::ComposeDotSourcePower(const UBreakerAttributeSet* SourceAttributes, const UBreakerCombatComponent* OwnerCombat,
    EBreakerDamageDelivery Delivery)
{
    // A4 RULED (owner ruling 2026-08-16): DoT ticks share ONE additive
    // Increased bucket. Increased Damage and Increased DoT no longer multiply
    // for ticks — a build holding +50% Damage and +40% DoT ticks at
    // (1 + 0.50 + 0.40), never 1.50 x 1.40. The two lanes' Increased sums are
    // read directly from the aggregator, folded under the delivery lane's flat
    // factor into one bucket, and the More side — Damage Mores, the
    // DoT More lane VW12/Long Dark authors, and the outgoing window chain —
    // multiplies back on top under the ONE O34 ceiling.
    //
    // Application-time snapshot only, as before: the returned value is the
    // spec's whole truth and ticks never re-read anything.
    //
    // O54/O55: WHICH damage lane joins that bucket is decided by what delivered
    // the status. A Bleed put on by a weapon swing folds the weapon pool; a Rot
    // zone placed by an ability folds the ability pool. Getting this wrong is
    // not visible in a number — the tick simply scales off the wrong half of a
    // build — which is why the delivery is a parameter every caller states
    // rather than a default this function guesses.
    float IncreasedBucket = 1.0f;   // 1 + sum(Increased Damage) + sum(Increased DoT)
    float FlatFactor = 1.0f;
    float AttributeMoreProduct = 1.0f;
    if (SourceAttributes)
    {
        const FBreakerAttributeAggregator& Aggregator = SourceAttributes->GetAttributeAggregator();
        const bool bAbility = Delivery == EBreakerDamageDelivery::Ability;
        const EBreakerAggregatedAttribute Lane = bAbility
            ? EBreakerAggregatedAttribute::AbilityDamageMultiplier
            : EBreakerAggregatedAttribute::DamageMultiplier;
        const float DamageMore = FMath::Max(Aggregator.ComposedMoreProduct(Lane), UE_SMALL_NUMBER);
        const float DotMore = FMath::Max(Aggregator.ComposedMoreProduct(EBreakerAggregatedAttribute::DamageOverTimeMultiplier), UE_SMALL_NUMBER);
        // Dividing the composed damage value by More still leaves its flat
        // factor inside it. Adding DoT Increased there would leave that part
        // unpaid by flat damage. Use the same explicit layers as hit riders.
        if (Aggregator.HasCapturedBases())
        {
            FlatFactor = FMath::Max(0.0f, Aggregator.ComposedFlatFactor(Lane));
            IncreasedBucket = FMath::Max(0.0f, 1.0f + (Aggregator.ComposedIncreasedPercent(Lane)
                + Aggregator.ComposedIncreasedPercent(EBreakerAggregatedAttribute::DamageOverTimeMultiplier)) / 100.0f);
        }
        else
        {
            // Before the first contribution, attribute fields are initialized
            // but aggregator bases are not captured. Preserve that original
            // field-based path; no flat contribution exists to split yet.
            const float DamageIncreased = (bAbility ? SourceAttributes->GetAbilityDamageMultiplier()
                : SourceAttributes->GetDamageMultiplier()) / DamageMore;
            const float DotIncreased = SourceAttributes->GetDamageOverTimeMultiplier() / DotMore;
            IncreasedBucket = FMath::Max(0.0f, DamageIncreased + DotIncreased - 1.0f);
        }
        AttributeMoreProduct = DamageMore * DotMore;
    }
    const float WindowProduct = OwnerCombat ? OwnerCombat->GetComposedMoreMultiplier() : 1.0f;
    // O34: ONE More ceiling for the tick path too. The window chain is already
    // budgeted against the Damage More side; the DoT More lane joins the same
    // single budget here rather than opening a second one. Loud when it bites,
    // like every other clamp site — a silent clamp is a build that lies.
    const float RawMore = AttributeMoreProduct * WindowProduct;
    const float Ceiling = FBreakerAttributeAggregator::ComposedMoreCeiling();
    if (RawMore > Ceiling + UE_KINDA_SMALL_NUMBER)
    {
        UE_LOG(LogTemp, Warning, TEXT("Tick More total %.3f (attributes %.3f x window %.3f) exceeds the %.3f ceiling (O34); clamping."),
            RawMore, AttributeMoreProduct, WindowProduct, Ceiling);
    }
    const float TotalMore = FMath::Min(RawMore, Ceiling);
    return FlatFactor * IncreasedBucket * TotalMore;
}

void UBreakerCombatComponent::PushIncomingDamageModifier(FName Key, float Multiplier)
{
    if (Key.IsNone()) return;
    BeneficialIncomingModifierKeys.Remove(Key);
    // Re-pushing the same key replaces rather than stacks, matching the
    // outgoing chain's rule.
    IncomingDamageModifiers.Add(Key, FMath::Max(0.0f, Multiplier));
}

void UBreakerCombatComponent::PushBeneficialIncomingDamageModifier(FName Key, float Multiplier)
{
    PushIncomingDamageModifier(Key, Multiplier);
    if (!Key.IsNone()) BeneficialIncomingModifierKeys.Add(Key);
}

void UBreakerCombatComponent::AddBeneficialSuppressionLease(ABreakerZoneActor* Zone)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || !Zone) return;
    for (auto It = BeneficialSuppressionLeases.CreateIterator(); It; ++It)
        if (!It->IsValid() || It->Get()->IsActorBeingDestroyed() || It->Get()->IsReleased() || It->Get()->GetRemainingDuration() <= 0) It.RemoveCurrent();
    BeneficialSuppressionLeases.Add(Zone);
}

bool UBreakerCombatComponent::IsBeneficialEffectSuppressed() const
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || IsDead() || !GetOwner()->IsA<ABreakerEnemy>()) return false;
    for (const TWeakObjectPtr<ABreakerZoneActor>& Lease : BeneficialSuppressionLeases)
    {
        const ABreakerZoneActor* Zone = Lease.Get();
        if (!Zone || Zone->IsActorBeingDestroyed() || Zone->IsReleased() || Zone->GetWorld() != GetWorld() || Zone->GetRemainingDuration() <= 0) continue;
        const FBreakerZoneSpec& Spec = Zone->GetSpec();
        if (Spec.ZoneTag != FGameplayTag::RequestGameplayTag(TEXT("Zone.Support.Suppress"), false)
            || !UBreakerZoneMath::IsInsideZone(Zone->GetActorLocation(), Spec.RadiusCm, Spec.HalfHeightCm, GetOwner()->GetActorLocation())) continue;
        const ABreakerCharacter* Source = Cast<ABreakerCharacter>(Zone->GetZoneInstigator());
        if (!Source || Source->GetWorld() != GetWorld() || Source->IsActorBeingDestroyed()
            || !Source->GetProgression() || !Source->GetCombat() || Source->GetCombat()->IsDead()) continue;
        const UBreakerChargeComponent* Charge = Source->FindComponentByClass<UBreakerChargeComponent>();
        const UBreakerAbilityStateComponent* State = Source->FindComponentByClass<UBreakerAbilityStateComponent>();
        if (Charge && Charge->IsActiveForOwner() && Charge->GetChargeBand() == EBreakerChargeBand::Resonant
            && Source->GetProgression()->HasNodeTag(BreakerNodeTags::Node_WA_BlackoutProtocol.GetTag())
            && State && State->IsMarked(GetOwner())) return true;
    }
    return false;
}

void UBreakerCombatComponent::RemoveIncomingDamageModifier(FName Key)
{
    IncomingDamageModifiers.Remove(Key);
    BeneficialIncomingModifierKeys.Remove(Key);
}

float UBreakerCombatComponent::GetComposedIncomingDamageMultiplier() const
{
    float Product = 1.0f;
    const bool bSuppressBuffs = !BeneficialIncomingModifierKeys.IsEmpty() && IsBeneficialEffectSuppressed();
    for (const TPair<FName, float>& Entry : IncomingDamageModifiers)
        if (!bSuppressBuffs || !BeneficialIncomingModifierKeys.Contains(Entry.Key)) Product *= Entry.Value;
    return Product;
}

void UBreakerCombatComponent::PushArmorReduction(FName Key, float FlatAmount)
{
    if (Key.IsNone()) return;
    // Re-pushing the same key REPLACES. This is the whole anti-stack rule: two
    // overlapping Rots share a key, so the second one refreshes the first
    // instead of doubling the strip (Ability-Implementation-Spec §5.3 task 6).
    ArmorReductions.Add(Key, FMath::Max(0.0f, FlatAmount));
}

void UBreakerCombatComponent::PopArmorReduction(FName Key)
{
    ArmorReductions.Remove(Key);
}

float UBreakerCombatComponent::GetComposedArmorReduction() const
{
    float Total = 0.0f;
    for (const TPair<FName, float>& Entry : ArmorReductions) Total += Entry.Value;
    return Total;
}

float UBreakerCombatComponent::GetEffectiveArmor() const
{
    const float Base = Attributes ? Attributes->GetArmor() : 0.0f;
    return FMath::Max(0.0f, Base - GetComposedArmorReduction());
}

FBreakerHealResult UBreakerCombatComponent::ApplyHealing(const FBreakerHealRequest& Request)
{
    FBreakerHealResult Result;
    if (!Attributes || !GetOwner() || !GetOwner()->HasAuthority()) return Result;
    // Healing is not revival. A heal landing on a corpse would resurrect it
    // without any of the state a real revive has to restore.
    if (IsDead() || IsBeneficialEffectSuppressed()) return Result;

    FBreakerVitalsState Vitals;
    Vitals.Health = Attributes->GetHealth();
    Vitals.MaxHealth = Attributes->GetMaxHealth();
    Vitals.Shield = Attributes->GetShield();
    Vitals.MaxShield = Attributes->GetMaxShield();

    Result = UBreakerDamageLibrary::ResolveHealing(Request, Vitals);
    if (Result.RequestedAmount <= 0.0f) return Result;

    // Null-safe writes: the generated setters ensure() without an owning
    // ability system, which would make every heal untestable in automation.
    Attributes->ApplyHealth(Result.RemainingHealth);
    if (Result.ShieldGranted > 0.0f) Attributes->ApplyShield(Result.RemainingShield);
    OnHealed.Broadcast(Result);

    // Healer-side dispatch, mirroring DispatchHitDealt. Self-heals report on
    // the same component once, not twice: a listener that heals on heal would
    // otherwise re-enter itself without bound.
    AActor* Healer = Request.Healer.Get();
    if (Healer && Healer != GetOwner())
    {
        if (UBreakerCombatComponent* HealerCombat = Healer->FindComponentByClass<UBreakerCombatComponent>())
        {
            FBreakerHealContext Context;
            Context.Healer = Healer;
            Context.Target = GetOwner();
            Context.Result = Result;
            Context.SourceTag = Request.SourceTag;
            HealerCombat->OnHealingDealt.Broadcast(Context);
        }
    }
    return Result;
}

FBreakerHealResult UBreakerCombatComponent::ApplyHealingAmount(float Amount, AActor* Healer, FGameplayTag SourceTag)
{
    FBreakerHealRequest Request;
    Request.Amount = Amount;
    Request.SourceTag = SourceTag;
    Request.SetHealer(Healer);
    return ApplyHealing(Request);
}

void UBreakerCombatComponent::ApplyOutgoingModifiers(FBreakerDamageRequest& Request)
{
    PruneExpiredOutgoingModifiers();
    if (OutgoingModifiers.IsEmpty()) return;

    float Flat = 0.0f;
    for (const FBreakerOutgoingModifier& Modifier : OutgoingModifiers) Flat += Modifier.FlatBonus;

    // Flat first, then the More product — resolution order step 1. The chain's
    // product is a More, so it lands in BOTH the composed convenience value
    // and the split's More half: a request carrying the Stage 6 source split
    // must keep FlatFactor x (1 + Increased/100) x MoreProduct == SourceDamageMultiplier
    // through this pass, or the target-side recomposition would silently
    // shed (or double) the window. Harmless when the split is absent — the
    // default SourceMoreProduct is 1.0 and bHasSourceSplit stays false.
    const float ChainMoreProduct = GetComposedMoreMultiplier();
    Request.BaseDamage = FMath::Max(0.0f, Request.BaseDamage + Flat);
    Request.SourceDamageMultiplier *= ChainMoreProduct;
    Request.SourceMoreProduct *= ChainMoreProduct;
}

// Both writes go through UBreakerAttributeSet::ApplyClassResource rather than
// the GAS generated SetClassResource. The generated setter ensure()s when there
// is no owning AbilitySystemComponent, so every rig without one — which is every
// automation test — could not exercise this path at all: Momentum generation,
// Mana generation, the dodge refund and every gear ResourceOnKill grant were
// proven only by the pure-maths layers either side of the write, never end to
// end. ApplyClassResource is the null-safe write added for exactly this reason
// (it is already how ApplyHealth/ApplyShield and the equipment kill-grant work),
// and it routes through the SAME PreAttributeChange clamp — [Floor, Max] — so
// the live behaviour is unchanged and the Overcast floor is honoured on the way
// in rather than by a Min written here.
bool UBreakerCombatComponent::SpendClassResource(float Cost)
{
    // Affordability is deliberately still measured against ZERO, not against
    // ClassResourceFloor. This is the generic non-GAS helper; Overcast's debt
    // allowance is spent by ability costs, which are GameplayEffects, and their
    // affordability rule lives in UBreakerCasterAbility::CheckCost where it can
    // refuse a cast that would breach the floor instead of truncating it.
    if (!Attributes || Cost < 0.0f || Attributes->GetClassResource() < Cost) return false;
    Attributes->ApplyClassResource(Attributes->GetClassResource() - Cost);
    return true;
}

void UBreakerCombatComponent::AddClassResource(float Amount)
{
    // No Min against MaxClassResource here: PreAttributeChange applies it. One
    // clamp policy, one place, so a future change to the cap rule cannot be
    // half-applied.
    if (Attributes && Amount > 0.0f) Attributes->ApplyClassResource(Attributes->GetClassResource() + Amount);
}

void UBreakerCombatComponent::RestoreVitals()
{
    if (!Attributes || !GetOwner() || !GetOwner()->HasAuthority()) return;
    // Same null-safe route as the damage and healing paths, and for the same
    // reason: the generated setters ensure() with no ability system, which made
    // the reset path unexercisable in automation exactly like the resource one.
    Attributes->ApplyHealth(Attributes->GetMaxHealth());
    Attributes->ApplyShield(Attributes->GetMaxShield());
    bDeathBroadcast = false;
    OnVitalsRestored.Broadcast();
}

bool UBreakerCombatComponent::IsDead() const
{
    return Attributes && Attributes->GetHealth() <= 0.0f;
}

float UBreakerCombatComponent::GetSecondsSinceDamage() const
{
    return GetWorld() ? static_cast<float>(GetWorld()->GetTimeSeconds() - LastDamageTime) : BIG_NUMBER;
}
