#include "Combat/BreakerDamageLibrary.h"

float UBreakerDamageLibrary::CalculateArmorMitigation(float Armor, float ArmorPenetration)
{
    const float EffectiveArmor = FMath::Max(0.0f, Armor - FMath::Max(0.0f, ArmorPenetration));
    return FMath::Clamp(EffectiveArmor / (EffectiveArmor + 100.0f), 0.0f, 0.80f);
}

float UBreakerDamageLibrary::GetFacingArmorMultiplier(const FVector& Forward, const FVector& SelfLocation,
    const FVector& SourceLocation, float RearArmorMultiplier, float RearCosine)
{
    // A multiplier of exactly 1 is the off switch and must never cost a
    // normalise: every hit in the game runs through this.
    if (FMath::IsNearlyEqual(RearArmorMultiplier, 1.0f)) return 1.0f;

    // Facing is a 2D question. A player who gets above an enemy has not
    // flanked it, and letting Z into the dot would say they had.
    const FVector2D Facing(Forward.X, Forward.Y);
    const FVector2D ToSource(SourceLocation.X - SelfLocation.X, SourceLocation.Y - SelfLocation.Y);
    if (Facing.IsNearlyZero() || ToSource.IsNearlyZero()) return 1.0f;

    const float Dot = FVector2D::DotProduct(Facing.GetSafeNormal(), ToSource.GetSafeNormal());
    // Dot > threshold means the source is in FRONT: full armour. Below it, the
    // source is on the flank or behind and the arc applies.
    return Dot > FMath::Clamp(RearCosine, -1.0f, 1.0f) ? 1.0f : FMath::Max(0.0f, RearArmorMultiplier);
}

FBreakerDamageResult UBreakerDamageLibrary::ResolveDamage(const FBreakerDamageRequest& Request, const FBreakerDefenseState& Defense)
{
    FBreakerDamageResult Result;
    Result.RawDamage = FMath::Max(0.0f, Request.BaseDamage) * FMath::Max(0.0f, Request.SourceDamageMultiplier);
    Result.bWeakPoint = Request.bWeakPointHit;
    // O34 bound guard: the aim-skill lane is deliberately outside the O3 More
    // budget, so its multiplier is hard-bounded here instead. Clamping (rather
    // than warning) is correct at a per-hit site — the archetype-value test is
    // where an out-of-bounds author gets caught loudly.
    if (Result.bWeakPoint) Result.RawDamage *= FMath::Clamp(Request.WeakPointMultiplier, WeakPointMultiplierFloor, WeakPointMultiplierCeiling);

    if (Request.bCanCritical)
    {
        if (Request.bUseSnapshotCritical) Result.bCritical = Request.bSnapshotCriticalResult;
        else
        {
            FRandomStream Random(Request.RandomSeed);
            Result.bCritical = Random.FRand() < FMath::Clamp(Request.CriticalChance, 0.0f, 1.0f);
        }
    }
    if (Result.bCritical) Result.RawDamage *= FMath::Max(1.0f, Request.CriticalMultiplier);

    // Passive dodge and block resolve before mitigation and never affect
    // DoTs. Dodge fully evades; block reduces.
    if (!Request.bIsDamageOverTime)
    {
        if (Defense.DodgeChance > 0.0f)
        {
            FRandomStream DodgeRandom(HashCombine(static_cast<uint32>(Request.RandomSeed), 0xD0D6Eu));
            Result.bDodged = DodgeRandom.FRand() < FMath::Clamp(Defense.DodgeChance, 0.0f, 1.0f);
        }
        if (Result.bDodged)
        {
            Result.RemainingShield = FMath::Max(0.0f, Defense.Shield);
            Result.RemainingHealth = FMath::Max(0.0f, Defense.Health);
            return Result;
        }
        if (Defense.BlockChance > 0.0f)
        {
            FRandomStream BlockRandom(HashCombine(static_cast<uint32>(Request.RandomSeed), 0xB10Cu));
            Result.bBlocked = BlockRandom.FRand() < FMath::Clamp(Defense.BlockChance, 0.0f, 1.0f);
        }
    }

    float Mitigation = 0.0f;
    if (Request.DamageFamily != EBreakerDamageFamily::TrueDamage)
    {
        Mitigation = CalculateArmorMitigation(Defense.Armor, Request.ArmorPenetration);
        // Physical DoTs that bypass shields are intentionally only half as
        // affected by armour, matching the global status rule.
        if (Request.bIsDamageOverTime && Request.DamageFamily == EBreakerDamageFamily::Physical && Request.bBypassShield)
        {
            Mitigation *= 0.5f;
        }
    }

    Result.MitigatedDamage = Result.RawDamage * (1.0f - Mitigation) * FMath::Max(0.0f, Defense.IncomingDamageMultiplier);
    if (Result.bBlocked) Result.MitigatedDamage *= 1.0f - FMath::Clamp(Defense.BlockMitigation, 0.0f, 1.0f);
    float RemainingDamage = Result.MitigatedDamage;
    Result.RemainingShield = FMath::Max(0.0f, Defense.Shield);
    Result.RemainingHealth = FMath::Max(0.0f, Defense.Health);

    if (!Request.bBypassShield && Result.RemainingShield > 0.0f)
    {
        Result.ShieldDamage = FMath::Min(Result.RemainingShield, RemainingDamage);
        Result.RemainingShield -= Result.ShieldDamage;
        RemainingDamage -= Result.ShieldDamage;
        Result.bShieldBroken = Result.ShieldDamage > 0.0f && Result.RemainingShield <= 0.0f;
    }

    Result.HealthDamage = FMath::Min(Result.RemainingHealth, RemainingDamage);
    Result.RemainingHealth -= Result.HealthDamage;
    Result.bKilled = Result.HealthDamage > 0.0f && Result.RemainingHealth <= 0.0f;
    return Result;
}

FBreakerDamageRequest UBreakerDamageLibrary::MakeSnapshotDotTick(const FBreakerStatusApplicationSpec& StatusSpec, EBreakerDamageFamily DamageFamily, int32 TickIndex, AActor* Instigator)
{
    FBreakerDamageRequest Request;
    Request.SetInstigator(Instigator);
    Request.BaseDamage = StatusSpec.BaseDamagePerTick * FMath::Max(1, StatusSpec.InitialStacks);
    Request.DamageFamily = DamageFamily;
    Request.DamageTypeTag = StatusSpec.StatusTag;
    Request.SourceTags = StatusSpec.Snapshot.SourceTags;
    Request.SourceDamageMultiplier = StatusSpec.Snapshot.SourcePower * StatusSpec.Snapshot.DamageOverTimeMultiplier;
    Request.CriticalChance = StatusSpec.Snapshot.CriticalChance;
    Request.CriticalMultiplier = StatusSpec.Snapshot.CriticalMultiplier;
    Request.ProcCoefficient = StatusSpec.ProcCoefficient;
    Request.bCanCritical = true;
    Request.bIsDamageOverTime = true;
    Request.bUseSnapshotCritical = true;
    Request.bSnapshotCriticalResult = StatusSpec.Snapshot.bRolledCritical;
    Request.RandomSeed = TickIndex;
    return Request;
}

FBreakerHealResult UBreakerDamageLibrary::ResolveHealing(const FBreakerHealRequest& Request, const FBreakerVitalsState& Vitals)
{
    FBreakerHealResult Result;
    Result.RemainingHealth = Vitals.Health;
    Result.RemainingShield = Vitals.Shield;
    Result.bWasAtFullHealth = Vitals.MaxHealth > 0.0f && Vitals.Health >= Vitals.MaxHealth;

    // A negative heal is NOT damage. Damage has exactly one entry point and
    // this is not it; turning a sign error into a hit here would route around
    // armour, shields and the passive dodge/block layer entirely.
    const float Requested = FMath::Max(0.0f, Request.Amount) * FMath::Max(0.0f, Request.HealingMultiplier);
    Result.RequestedAmount = Requested;
    if (Requested <= 0.0f) return Result;

    const float MissingHealth = FMath::Max(0.0f, Vitals.MaxHealth - Vitals.Health);
    Result.HealthHealed = FMath::Min(Requested, MissingHealth);
    Result.RemainingHealth = Vitals.Health + Result.HealthHealed;
    Result.Overheal = Requested - Result.HealthHealed;

    if (Request.bOverhealToShield && Result.Overheal > 0.0f && Vitals.MaxShield > 0.0f)
    {
        const float ShieldCap = Vitals.MaxShield * FMath::Clamp(Request.OverhealToShieldFraction, 0.0f, 1.0f);
        const float Headroom = FMath::Max(0.0f, ShieldCap - Vitals.Shield);
        Result.ShieldGranted = FMath::Min(Result.Overheal, Headroom);
        Result.RemainingShield = Vitals.Shield + Result.ShieldGranted;
        // Overheal is still REPORTED at its full value even when some of it
        // became shield. Support's Charge loop reads Overheal and must generate
        // nothing from it (Class-Kits §5, criterion 4); netting the shield out
        // here would quietly hand Support a generation source the design bans.
    }
    return Result;
}
