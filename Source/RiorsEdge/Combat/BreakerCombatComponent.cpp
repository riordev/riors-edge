#include "Combat/BreakerCombatComponent.h"

#include "AbilitySystemInterface.h"
#include "AbilitySystemComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Combat/BreakerDamageLibrary.h"
#include "GameFramework/Actor.h"
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

void UBreakerCombatComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

FBreakerDamageResult UBreakerCombatComponent::ReceiveDamage(const FBreakerDamageRequest& Request)
{
    FBreakerDamageResult Result;
    if (!Attributes || !GetOwner() || !GetOwner()->HasAuthority() || IsDead()) return Result;

    FBreakerDefenseState Defense;
    Defense.Health = Attributes->GetHealth();
    Defense.Shield = Attributes->GetShield();
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
    if (Request.DamageFamily != EBreakerDamageFamily::TrueDamage)
    {
        if (const UBreakerEquipmentComponent* Equipment = GetOwner()->FindComponentByClass<UBreakerEquipmentComponent>())
        {
            const float ReductionPercent = Request.DamageFamily == EBreakerDamageFamily::Physical
                ? Equipment->GetStats().PhysicalDamageReductionPercent
                : Equipment->GetStats().ElementalResistancePercent;
            Defense.IncomingDamageMultiplier *= 1.0f - ReductionPercent / 100.0f;
        }
    }
    // Pushed incoming modifiers compose on top, in the same stage: Caster's
    // Overcast penalty, and defensive windows when they land.
    Defense.IncomingDamageMultiplier *= GetComposedIncomingDamageMultiplier();
    Defense.DodgeChance = DodgeChance;
    Defense.BlockChance = BlockChance;
    Defense.BlockMitigation = BlockMitigation;
    // Tree nodes raise the passive layers on top of the component baseline.
    if (const UBreakerProgressionComponent* Progression = GetOwner()->FindComponentByClass<UBreakerProgressionComponent>())
    {
        Defense.DodgeChance = FMath::Clamp(Defense.DodgeChance + Progression->GetDodgeChanceBonus(), 0.0f, 1.0f);
        Defense.BlockChance = FMath::Clamp(Defense.BlockChance + Progression->GetBlockChanceBonus(), 0.0f, 1.0f);
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
    // Same null-safe route the healing path uses: identical to the generated
    // setters when there is an ability system, and writable (rather than an
    // ensure) when there is not, which is what lets automation exercise a
    // whole damage submission instead of only the pure resolver.
    Attributes->ApplyShield(Result.RemainingShield);
    Attributes->ApplyHealth(Result.RemainingHealth);
    LastDamageTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
    OnDamageReceived.Broadcast(Result);
    if (Result.bKilled && !bDeathBroadcast)
    {
        bDeathBroadcast = true;
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
        }
    }
    if (FMath::IsNearlyZero(RiderPercent)) return;

    // The recomposition (§3.3): the rider joins the source's ADDITIVE
    // Increased bucket and the More product is reapplied on top, unchanged.
    // Floored at zero on both factors so a hostile authored negative can
    // never invert damage.
    const float IncreasedFactor = FMath::Max(0.0f, 1.0f + (Request.SourceIncreasedPercent + RiderPercent) / 100.0f);
    Request.SourceDamageMultiplier = IncreasedFactor * FMath::Max(0.0f, Request.SourceMoreProduct);
}

void UBreakerCombatComponent::DispatchHitDealt(const FBreakerDamageRequest& Request, const FBreakerDamageResult& Result)
{
    AActor* Dealer = Request.Instigator.Get();

    FBreakerHitContext Context;
    Context.Instigator = Dealer;
    Context.Target = GetOwner();
    Context.Result = Result;
    Context.bFromDoT = Request.bIsDamageOverTime;
    Context.bWeakPoint = Result.bWeakPoint;
    Context.DamageFamily = Request.DamageFamily;
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
    // recovered by dividing each composed attribute by its own post-clamp More
    // product (the aggregator's fold is (1 + Inc) x prod(More), so the division
    // is exact), folded into one bucket, and the More side — Damage Mores, the
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
        const float DamageIncreased = (bAbility ? SourceAttributes->GetAbilityDamageMultiplier() : SourceAttributes->GetDamageMultiplier()) / DamageMore;
        const float DotIncreased = SourceAttributes->GetDamageOverTimeMultiplier() / DotMore;
        IncreasedBucket = DamageIncreased + (DotIncreased - 1.0f);
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
    return IncreasedBucket * TotalMore;
}

void UBreakerCombatComponent::PushIncomingDamageModifier(FName Key, float Multiplier)
{
    if (Key.IsNone()) return;
    // Re-pushing the same key replaces rather than stacks, matching the
    // outgoing chain's rule.
    IncomingDamageModifiers.Add(Key, FMath::Max(0.0f, Multiplier));
}

void UBreakerCombatComponent::RemoveIncomingDamageModifier(FName Key)
{
    IncomingDamageModifiers.Remove(Key);
}

float UBreakerCombatComponent::GetComposedIncomingDamageMultiplier() const
{
    float Product = 1.0f;
    for (const TPair<FName, float>& Entry : IncomingDamageModifiers) Product *= Entry.Value;
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
    if (IsDead()) return Result;

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
    // must keep (1 + Increased/100) x MoreProduct == SourceDamageMultiplier
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
