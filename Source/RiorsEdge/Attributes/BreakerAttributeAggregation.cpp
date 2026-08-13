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

void FBreakerAttributeContribution::ComposeMore(EBreakerAggregatedAttribute Attribute, float Multiplier)
{
    const int32 Index = AttributeIndex(Attribute);
    if (Index != INDEX_NONE) MoreMultiplier[Index] *= Multiplier;
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
    const int32 Index = AttributeIndex(Attribute);
    return Index != INDEX_NONE ? MoreMultiplier[Index] : 1.0f;
}

bool FBreakerAttributeContribution::IsIdentity() const
{
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

    float Flat = 0.0f;
    float IncreasedPercent = 0.0f;
    float More = 1.0f;
    // Fixed order over the contributor enum: the result cannot depend on the
    // sequence in which the layers happened to recalculate.
    for (int32 Contributor = 0; Contributor < ContributorCount; ++Contributor)
    {
        const FBreakerAttributeContribution& Contribution = Contributions[Contributor];
        Flat += Contribution.GetFlat(Attribute);
        IncreasedPercent += Contribution.GetIncreasedPercent(Attribute);
        More *= Contribution.GetMore(Attribute);
    }

    return (Bases[Index] + Flat) * (1.0f + IncreasedPercent / 100.0f) * More;
}
