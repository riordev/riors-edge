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
    // Gear-rolled physical damage reduction folds into the incoming
    // multiplier so the resolution order stays single-path.
    if (Request.DamageFamily == EBreakerDamageFamily::Physical)
    {
        if (const UBreakerEquipmentComponent* Equipment = GetOwner()->FindComponentByClass<UBreakerEquipmentComponent>())
        {
            Defense.IncomingDamageMultiplier *= 1.0f - Equipment->GetStats().PhysicalDamageReductionPercent / 100.0f;
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

    Result = UBreakerDamageLibrary::ResolveDamage(Request, Defense);
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

float UBreakerCombatComponent::ComposeDotSourcePower(const UBreakerAttributeSet* SourceAttributes, const UBreakerCombatComponent* OwnerCombat)
{
    // Application-time snapshot only. The chain's product is budgeted by
    // GetComposedMoreMultiplier, so a window folded in here and the attribute
    // side it rides on still compose to at most the one O34 ceiling.
    const float AttributePower = SourceAttributes ? SourceAttributes->GetDamageMultiplier() : 1.0f;
    const float WindowProduct = OwnerCombat ? OwnerCombat->GetComposedMoreMultiplier() : 1.0f;
    return AttributePower * WindowProduct;
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

    // Flat first, then the More product — resolution order step 1.
    Request.BaseDamage = FMath::Max(0.0f, Request.BaseDamage + Flat);
    Request.SourceDamageMultiplier *= GetComposedMoreMultiplier();
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
