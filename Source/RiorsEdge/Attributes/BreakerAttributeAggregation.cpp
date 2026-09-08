#include "Attributes/BreakerAttributeAggregation.h"

namespace
{
    FORCEINLINE int32 AttributeIndex(EBreakerAggregatedAttribute Attribute)
    {
        const int32 Index = static_cast<int32>(Attribute);
        return (Index >= 0 && Index < FBreakerAttributeContribution::AttributeCount) ? Index : INDEX_NONE;
    }

    FORCEINLINE int32 ContributorIndex(EBreakerAttributeContributor Contributor)
    {
        const int32 Index = static_cast<int32>(Contributor);
        return (Index >= 0 && Index < FBreakerAttributeAggregator::ContributorCount) ? Index : INDEX_NONE;
    }
}

void FBreakerAttributeContribution::Reset()
{
    DamageMoreSources.Reset();
    for (int32 Index = 0; Index < AttributeCount; ++Index)
    {
        Flat[Index] = 0.0f;
        IncreasedPercent[Index] = 0.0f;
        MoreMultiplier[Index] = 1.0f;
    }
}

void FBreakerAttributeContribution::AddFlat(EBreakerAggregatedAttribute Attribute, float Value)
{
    const int32 Index = AttributeIndex(Attribute);
    if (Index != INDEX_NONE) Flat[Index] += Value;
}

void FBreakerAttributeContribution::AddIncreasedPercent(EBreakerAggregatedAttribute Attribute, float Percent)
{
    const int32 Index = AttributeIndex(Attribute);
    if (Index != INDEX_NONE) IncreasedPercent[Index] += Percent;
}

void FBreakerAttributeContribution::AddSharedIncreasedDamage(float Percent)
{
    AddIncreasedPercent(EBreakerAggregatedAttribute::DamageMultiplier, Percent);
    AddIncreasedPercent(EBreakerAggregatedAttribute::AbilityDamageMultiplier, Percent);
}

void FBreakerAttributeContribution::ComposeSharedMoreDamage(float Multiplier)
{
    AddDamageMoreSource(FName(*FString::Printf(TEXT("Anonymous.%d"), DamageMoreSources.Num())), EBreakerDamageMoreLane::Shared, Multiplier);
}

void FBreakerAttributeContribution::ComposeMore(EBreakerAggregatedAttribute Attribute, float Multiplier)
{
    if (FBreakerAttributeAggregator::IsMoreCappedAttribute(Attribute))
    {
        const EBreakerDamageMoreLane Lane = Attribute == EBreakerAggregatedAttribute::DamageMultiplier ? EBreakerDamageMoreLane::Weapon
            : Attribute == EBreakerAggregatedAttribute::AbilityDamageMultiplier ? EBreakerDamageMoreLane::Ability : EBreakerDamageMoreLane::Dot;
        AddDamageMoreSource(FName(*FString::Printf(TEXT("Anonymous.%d"), DamageMoreSources.Num())), Lane, Multiplier);
        return;
    }
    const int32 Index = AttributeIndex(Attribute);
    if (Index != INDEX_NONE) MoreMultiplier[Index] *= Multiplier;
}

void FBreakerAttributeContribution::AddDamageMoreSource(FName Key, EBreakerDamageMoreLane Lane, float Multiplier)
{
    if (Key.IsNone()) return;
    if (!FMath::IsFinite(Multiplier) || Multiplier <= 1.0f)
    {
        DamageMoreSources.RemoveAll([Key](const FBreakerDamageMoreSource& Source) { return Source.Key == Key; });
        return;
    }
    for (FBreakerDamageMoreSource& Existing : DamageMoreSources)
        if (Existing.Key == Key) { Existing.Multiplier = Multiplier; Existing.Lane = Lane; return; }
    DamageMoreSources.Add({ Key, Multiplier, Lane });
}

TArray<FBreakerDamageMoreSource> FBreakerAttributeContribution::SelectDamageMoreSources(const TArray<FBreakerDamageMoreSource>& Sources)
{
    TArray<FBreakerDamageMoreSource> Selected = Sources;
    // Stable raw-magnitude ordering retains authored tree traversal ties.
    Selected.StableSort([](const FBreakerDamageMoreSource& A, const FBreakerDamageMoreSource& B) { return A.Multiplier > B.Multiplier; });
    if (Selected.Num() > FBreakerAttributeAggregator::MaxComposedMoreSources) Selected.SetNum(FBreakerAttributeAggregator::MaxComposedMoreSources);
    return Selected;
}

float FBreakerAttributeContribution::DamageMoreProduct(const TArray<FBreakerDamageMoreSource>& Selected, EBreakerAggregatedAttribute Attribute)
{
    float Product = 1.0f;
    for (const FBreakerDamageMoreSource& Source : Selected)
    {
        const bool bApplies = (Attribute == EBreakerAggregatedAttribute::DamageMultiplier && (Source.Lane == EBreakerDamageMoreLane::Weapon || Source.Lane == EBreakerDamageMoreLane::Shared))
            || (Attribute == EBreakerAggregatedAttribute::AbilityDamageMultiplier && (Source.Lane == EBreakerDamageMoreLane::Ability || Source.Lane == EBreakerDamageMoreLane::Shared))
            || (Attribute == EBreakerAggregatedAttribute::DamageOverTimeMultiplier && Source.Lane == EBreakerDamageMoreLane::Dot);
        if (bApplies) Product *= FMath::Min(Source.Multiplier, FBreakerAttributeAggregator::SingleMoreCeiling);
    }
    return Product;
}

float FBreakerAttributeContribution::GetFlat(EBreakerAggregatedAttribute Attribute) const
{
    const int32 Index = AttributeIndex(Attribute);
    return Index != INDEX_NONE ? Flat[Index] : 0.0f;
}

float FBreakerAttributeContribution::GetIncreasedPercent(EBreakerAggregatedAttribute Attribute) const
{
    const int32 Index = AttributeIndex(Attribute);
    return Index != INDEX_NONE ? IncreasedPercent[Index] : 0.0f;
}

float FBreakerAttributeContribution::GetMore(EBreakerAggregatedAttribute Attribute) const
{
    if (FBreakerAttributeAggregator::IsMoreCappedAttribute(Attribute)) return DamageMoreProduct(SelectDamageMoreSources(DamageMoreSources), Attribute);
    const int32 Index = AttributeIndex(Attribute);
    return Index != INDEX_NONE ? MoreMultiplier[Index] : 1.0f;
}

bool FBreakerAttributeContribution::IsIdentity() const
{
    if (!DamageMoreSources.IsEmpty()) return false;
    for (int32 Index = 0; Index < AttributeCount; ++Index)
    {
        if (Flat[Index] != 0.0f || IncreasedPercent[Index] != 0.0f || MoreMultiplier[Index] != 1.0f) return false;
    }
    return true;
}

bool FBreakerAttributeAggregator::CaptureBases(const float (&Values)[AttributeCount])
{
    if (bBasesCaptured) return false;
    for (int32 Index = 0; Index < AttributeCount; ++Index) Bases[Index] = Values[Index];
    bBasesCaptured = true;
    return true;
}

void FBreakerAttributeAggregator::SetBase(EBreakerAggregatedAttribute Attribute, float Value)
{
    const int32 Index = AttributeIndex(Attribute);
    if (Index == INDEX_NONE) return;
    Bases[Index] = Value;
    bBasesCaptured = true;
}

float FBreakerAttributeAggregator::GetBase(EBreakerAggregatedAttribute Attribute) const
{
    const int32 Index = AttributeIndex(Attribute);
    return Index != INDEX_NONE ? Bases[Index] : 0.0f;
}

void FBreakerAttributeAggregator::SetContribution(EBreakerAttributeContributor Contributor, const FBreakerAttributeContribution& Contribution)
{
    const int32 Index = ContributorIndex(Contributor);
    if (Index != INDEX_NONE) Contributions[Index] = Contribution;
}

void FBreakerAttributeAggregator::ClearContribution(EBreakerAttributeContributor Contributor)
{
    const int32 Index = ContributorIndex(Contributor);
    if (Index != INDEX_NONE) Contributions[Index].Reset();
}

const FBreakerAttributeContribution& FBreakerAttributeAggregator::GetContribution(EBreakerAttributeContributor Contributor) const
{
    static const FBreakerAttributeContribution Identity;
    const int32 Index = ContributorIndex(Contributor);
    return Index != INDEX_NONE ? Contributions[Index] : Identity;
}

float FBreakerAttributeAggregator::Compose(EBreakerAggregatedAttribute Attribute) const
{
    const int32 Index = AttributeIndex(Attribute);
    if (Index == INDEX_NONE) return 0.0f;

    // Each factor comes from the ONE function that owns it — the More factor
    // from the O3/O34 clamp, the flat and Increased sums from their own fixed-
    // order folds — so what Compose folds in and what a damage request carries
    // as its split can never be two different numbers. The multiplication
    // order is the law's: flat, then Increased, then More.
    return ComposedFlatFactor(Attribute) * (1.0f + ComposedIncreasedPercent(Attribute) / 100.0f) * ComposedMoreProduct(Attribute);
}

float FBreakerAttributeAggregator::ComposedFlatFactor(EBreakerAggregatedAttribute Attribute) const
{
    const int32 Index = AttributeIndex(Attribute);
    if (Index == INDEX_NONE) return 0.0f;

    float Flat = 0.0f;
    // Fixed order over the contributor enum: the result cannot depend on the
    // sequence in which the layers happened to recalculate.
    for (int32 Contributor = 0; Contributor < ContributorCount; ++Contributor)
    {
        Flat += Contributions[Contributor].GetFlat(Attribute);
    }
    return Bases[Index] + Flat;
}

float FBreakerAttributeAggregator::ComposedIncreasedPercent(EBreakerAggregatedAttribute Attribute) const
{
    const int32 Index = AttributeIndex(Attribute);
    if (Index == INDEX_NONE) return 0.0f;

    float IncreasedPercent = 0.0f;
    // Same fixed order as the flat fold, for the same reason.
    for (int32 Contributor = 0; Contributor < ContributorCount; ++Contributor)
    {
        IncreasedPercent += Contributions[Contributor].GetIncreasedPercent(Attribute);
    }
    return IncreasedPercent;
}

float FBreakerAttributeAggregator::ComposedMoreProduct(EBreakerAggregatedAttribute Attribute) const
{
    if (IsMoreCappedAttribute(Attribute)) return FBreakerAttributeContribution::DamageMoreProduct(GetSelectedDamageMoreSources(), Attribute);
    const int32 Index = AttributeIndex(Attribute);
    if (Index == INDEX_NONE) return 1.0f;

    float More = 1.0f;
    for (int32 Contributor = 0; Contributor < ContributorCount; ++Contributor)
    {
        More *= Contributions[Contributor].GetMore(Attribute);
    }

    return More;
}

int32 FBreakerAttributeAggregator::GetDamageMoreSourceCount() const
{
    int32 Count = 0;
    for (const FBreakerAttributeContribution& Contribution : Contributions) Count += Contribution.GetDamageMoreSources().Num();
    return Count;
}

int32 FBreakerAttributeAggregator::GetSelectedDamageMoreSourceCount() const
{
    return FMath::Min(GetDamageMoreSourceCount(), MaxComposedMoreSources);
}

TArray<FBreakerDamageMoreSource> FBreakerAttributeAggregator::GetSelectedDamageMoreSources() const
{
    // Progression wins equal ties in its existing authored order. Equipment
    // order is canonical slot/affix key, independent of equip arrival order.
    TArray<FBreakerDamageMoreSource> Sources = GetContribution(EBreakerAttributeContributor::Progression).GetDamageMoreSources();
    TArray<FBreakerDamageMoreSource> Gear = GetContribution(EBreakerAttributeContributor::Equipment).GetDamageMoreSources();
    Gear.Sort([](const FBreakerDamageMoreSource& A, const FBreakerDamageMoreSource& B) { return A.Key.LexicalLess(B.Key); });
    Sources.Append(Gear);
    return FBreakerAttributeContribution::SelectDamageMoreSources(Sources);
}

float FBreakerAttributeAggregator::ComposedMoreCeiling()
{
    return FMath::Pow(SingleMoreCeiling, static_cast<float>(MaxComposedMoreSources));
}

float FBreakerAttributeAggregator::GetScopedMoreProduct(bool bElemental, bool bVoid, bool bReaction, bool bEffectiveHealth) const
{
    float Product = 1.0f;
    for (const FBreakerDamageMoreSource& Source : GetSelectedDamageMoreSources())
    {
        const bool bApplies = (bElemental && Source.Lane == EBreakerDamageMoreLane::Elemental)
            || (bVoid && Source.Lane == EBreakerDamageMoreLane::Void)
            || (bReaction && Source.Lane == EBreakerDamageMoreLane::Reaction)
            || (bEffectiveHealth && Source.Lane == EBreakerDamageMoreLane::EffectiveHealth);
        if (bApplies) Product *= FMath::Min(Source.Multiplier, SingleMoreCeiling);
    }
    return Product;
}
