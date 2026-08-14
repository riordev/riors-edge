#include "Items/BreakerEquipmentComponent.h"

#include "AbilitySystemInterface.h"
#include "AbilitySystemComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Items/BreakerAffixLibrary.h"
#include "Items/BreakerLootLibrary.h"
#include "Net/UnrealNetwork.h"

UBreakerEquipmentComponent::UBreakerEquipmentComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    SetIsReplicatedByDefault(true);
}

void UBreakerEquipmentComponent::BeginPlay()
{
    Super::BeginPlay();
    UBreakerAttributeSet* FoundAttributes = nullptr;
    if (const IAbilitySystemInterface* AbilityOwner = Cast<IAbilitySystemInterface>(GetOwner()))
    {
        if (UAbilitySystemComponent* ASC = AbilityOwner->GetAbilitySystemComponent())
        {
            FoundAttributes = const_cast<UBreakerAttributeSet*>(ASC->GetSet<UBreakerAttributeSet>());
        }
    }
    BindAttributes(FoundAttributes);
}

void UBreakerEquipmentComponent::BindAttributes(UBreakerAttributeSet* InAttributes)
{
    Attributes = InAttributes;
    // The attribute set owns the true bases; capturing is idempotent, so it
    // does not matter whether equipment or progression gets here first.
    if (Attributes) Attributes->CaptureAttributeBases();
    RecalculateStats();
}

bool UBreakerEquipmentComponent::HasAttributeAuthority() const
{
    // Unchanged rule, named once instead of repeated nine times: an ownerless
    // component has no server to be, and a client never mutates equipment.
    return GetOwner() && GetOwner()->HasAuthority();
}

void UBreakerEquipmentComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    // Conditional damage affixes are live state, so the offer they belong to
    // has to be rebuilt when the state changes. Cheap: the comparison is one
    // byte and the rebuild only runs on an actual transition.
    RefreshBuildConditions();
    // Gear-granted resource regeneration; the class loop adds its own on top.
    if (Attributes && GetOwner() && GetOwner()->HasAuthority() && CachedStats.ResourceRegenPerSecond > 0.0f)
    {
        Attributes->SetClassResource(FMath::Min(Attributes->GetMaxClassResource(), Attributes->GetClassResource() + CachedStats.ResourceRegenPerSecond * DeltaTime));
    }
}

void UBreakerEquipmentComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(UBreakerEquipmentComponent, Equipped);
    DOREPLIFETIME(UBreakerEquipmentComponent, Backpack);
}

namespace
{
    // One predicate for the bulk discard and for the count the confirmation
    // modal prints. Aberrant and Anomalous sit above every threshold the UI
    // offers, so "never bulk-discards your best gear" is a property of the
    // rarity order rather than a special case bolted on here.
    bool IsBelowRarity(const FBreakerItemInstance& Item, EBreakerItemRarity MinimumKept)
    {
        return static_cast<uint8>(Item.Rarity) < static_cast<uint8>(MinimumKept);
    }
}

bool UBreakerEquipmentComponent::EquipItem(const FBreakerItemInstance& Item)
{
    if (!Item.IsValid() || !HasAttributeAuthority()) return false;
    // Resolve the cap BEFORE anything moves, so the piece the UI named as
    // doomed is exactly the piece that leaves. The cap never refuses the
    // equip (UI-Inventory-Spec "Limit tells": disclosed, not blocked).
    const FBreakerEquipPreview Preview = PreviewEquipAgainst(Equipped, Item);
    UnequipSlot(Item.Slot);
    if (Preview.bExceedsRarityLimit && Preview.LimitDisplaced.IsValid())
    {
        UnequipSlot(Preview.LimitDisplaced.Slot);
    }
    Equipped.Add(Item);
    RecalculateStats();
    OnEquipmentChanged.Broadcast();
    return true;
}

int32 UBreakerEquipmentComponent::EquipLimitForRarity(EBreakerItemRarity Rarity)
{
    switch (Rarity)
    {
        case EBreakerItemRarity::Aberrant:  return 3;
        case EBreakerItemRarity::Anomalous: return 1;
        default:                            return INDEX_NONE;
    }
}

int32 UBreakerEquipmentComponent::CountEquippedOfRarity(EBreakerItemRarity Rarity) const
{
    int32 Count = 0;
    for (const FBreakerItemInstance& Item : Equipped)
    {
        if (Item.IsValid() && Item.Rarity == Rarity) ++Count;
    }
    return Count;
}

int32 UBreakerEquipmentComponent::CountBackpackBelowRarity(EBreakerItemRarity MinimumKept) const
{
    int32 Count = 0;
    for (const FBreakerItemInstance& Item : Backpack)
    {
        if (IsBelowRarity(Item, MinimumKept)) ++Count;
    }
    return Count;
}

FBreakerEquipPreview UBreakerEquipmentComponent::PreviewEquip(const FBreakerItemInstance& Candidate) const
{
    return PreviewEquipAgainst(Equipped, Candidate);
}

FBreakerEquipPreview UBreakerEquipmentComponent::PreviewEquipAgainst(const TArray<FBreakerItemInstance>& EquippedItems, const FBreakerItemInstance& Candidate)
{
    FBreakerEquipPreview Preview;
    if (!Candidate.IsValid()) return Preview;

    const FBreakerItemInstance* InSlot = EquippedItems.FindByPredicate(
        [&Candidate](const FBreakerItemInstance& Existing) { return Existing.IsValid() && Existing.Slot == Candidate.Slot; });
    if (InSlot)
    {
        Preview.bSlotOccupied = true;
        Preview.SlotDisplaced = *InSlot;
    }
    Preview.AffixDeltas = CompareAffixes(Candidate, InSlot ? *InSlot : FBreakerItemInstance());

    Preview.RarityLimit = EquipLimitForRarity(Candidate.Rarity);
    for (const FBreakerItemInstance& Existing : EquippedItems)
    {
        if (Existing.IsValid() && Existing.Rarity == Candidate.Rarity) ++Preview.RarityCount;
    }
    if (Preview.RarityLimit == INDEX_NONE) return Preview;

    // The piece already in the candidate's slot is leaving no matter what, so
    // it cannot be the cap's victim and its departure counts against the tally
    // first. Swapping an Aberrant helmet for another Aberrant helmet therefore
    // ejects nothing extra even at 3/3.
    int32 Surviving = Preview.RarityCount;
    if (InSlot && InSlot->Rarity == Candidate.Rarity) --Surviving;
    if (Surviving < Preview.RarityLimit) return Preview;

    // Over the cap: the WEAKEST equipped piece of that rarity leaves. Weakest
    // is lowest item level, ties broken by wear order (slot index), which
    // makes the choice deterministic — the player is told which piece dies and
    // that is the piece that dies.
    Preview.bExceedsRarityLimit = true;
    const FBreakerItemInstance* Weakest = nullptr;
    for (const FBreakerItemInstance& Existing : EquippedItems)
    {
        if (!Existing.IsValid() || Existing.Rarity != Candidate.Rarity) continue;
        if (Existing.Slot == Candidate.Slot) continue;
        const bool bBetterVictim = !Weakest
            || Existing.ItemLevel < Weakest->ItemLevel
            || (Existing.ItemLevel == Weakest->ItemLevel && static_cast<uint8>(Existing.Slot) < static_cast<uint8>(Weakest->Slot));
        if (bBetterVictim) Weakest = &Existing;
    }
    if (Weakest) Preview.LimitDisplaced = *Weakest;
    return Preview;
}

TArray<FBreakerAffixComparison> UBreakerEquipmentComponent::CompareAffixes(const FBreakerItemInstance& Candidate, const FBreakerItemInstance& Reference)
{
    const TArray<FBreakerAffixDefinition>& Pool = UBreakerAffixLibrary::GetSliceAffixPool();
    TArray<FBreakerAffixComparison> Comparisons;
    Comparisons.Reserve(Candidate.Affixes.Num());
    for (const FBreakerRolledAffix& Rolled : Candidate.Affixes)
    {
        FBreakerAffixComparison Row;
        Row.AffixId = Rolled.AffixId;
        Row.Tier = Rolled.Tier;
        Row.Value = Rolled.Value;

        const FBreakerAffixDefinition* Definition = UBreakerAffixLibrary::FindAffix(Pool, Rolled.AffixId);
        for (const FBreakerRolledAffix& Other : Reference.Affixes)
        {
            if (Definition)
            {
                const FBreakerAffixDefinition* OtherDefinition = UBreakerAffixLibrary::FindAffix(Pool, Other.AffixId);
                if (!OtherDefinition) continue;
                if (OtherDefinition->StatTarget != Definition->StatTarget) continue;
                if (OtherDefinition->StatBucket != Definition->StatBucket) continue;
            }
            // An affix with no definition (content removed under a live save)
            // can only be compared against its own id.
            else if (Other.AffixId != Rolled.AffixId) continue;
            Row.ComparedValue += Other.Value;
        }

        Row.Delta = FMath::IsNearlyEqual(Row.Value, Row.ComparedValue, UE_KINDA_SMALL_NUMBER)
            ? EBreakerAffixDelta::Parity
            : (Row.Value > Row.ComparedValue ? EBreakerAffixDelta::Better : EBreakerAffixDelta::Worse);
        Comparisons.Add(Row);
    }
    return Comparisons;
}

bool UBreakerEquipmentComponent::UnequipSlot(EBreakerEquipSlot Slot)
{
    if (!HasAttributeAuthority()) return false;
    const int32 Index = Equipped.IndexOfByPredicate([Slot](const FBreakerItemInstance& Existing) { return Existing.Slot == Slot; });
    if (Index == INDEX_NONE) return false;
    Backpack.Add(Equipped[Index]);
    Equipped.RemoveAt(Index);
    RecalculateStats();
    OnEquipmentChanged.Broadcast();
    return true;
}

void UBreakerEquipmentComponent::AddToBackpack(const FBreakerItemInstance& Item)
{
    if (!Item.IsValid() || !HasAttributeAuthority()) return;
    Backpack.Add(Item);
    OnItemAcquired.Broadcast(Item);
}

void UBreakerEquipmentComponent::RestoreState(const TArray<FBreakerItemInstance>& NewEquipped, const TArray<FBreakerItemInstance>& NewBackpack)
{
    if (!HasAttributeAuthority()) return;
    Equipped = NewEquipped;
    Backpack = NewBackpack;
    RecalculateStats();
    OnEquipmentChanged.Broadcast();
}

bool UBreakerEquipmentComponent::EquipFromBackpack(const FGuid& ItemId)
{
    if (!HasAttributeAuthority()) return false;
    const int32 Index = Backpack.IndexOfByPredicate([&ItemId](const FBreakerItemInstance& Existing) { return Existing.ItemId == ItemId; });
    if (Index == INDEX_NONE) return false;
    const FBreakerItemInstance Item = Backpack[Index];
    Backpack.RemoveAt(Index);
    return EquipItem(Item);
}

bool UBreakerEquipmentComponent::DiscardFromBackpack(const FGuid& ItemId)
{
    if (!HasAttributeAuthority()) return false;
    const int32 Index = Backpack.IndexOfByPredicate([&ItemId](const FBreakerItemInstance& Existing) { return Existing.ItemId == ItemId; });
    if (Index == INDEX_NONE) return false;
    Backpack.RemoveAt(Index);
    OnEquipmentChanged.Broadcast();
    return true;
}

int32 UBreakerEquipmentComponent::DiscardBackpackBelowRarity(EBreakerItemRarity MinimumKept)
{
    if (!HasAttributeAuthority()) return 0;
    const int32 Removed = Backpack.RemoveAll([MinimumKept](const FBreakerItemInstance& Existing)
    {
        return IsBelowRarity(Existing, MinimumKept);
    });
    if (Removed > 0) OnEquipmentChanged.Broadcast();
    return Removed;
}

void UBreakerEquipmentComponent::DevGrantTestGear(int32 ItemLevel)
{
    if (!HasAttributeAuthority()) return;
    const int32 SafeLevel = FMath::Max(1, ItemLevel);
    // Distinct seed per slot per grant: the slot index spreads the affix roll,
    // the counter keeps repeat presses from producing the same eight items.
    static int32 GrantCounter = 0;
    ++GrantCounter;
    for (int32 SlotIndex = 0; SlotIndex < static_cast<int32>(EBreakerEquipSlot::Count); ++SlotIndex)
    {
        const EBreakerEquipSlot Slot = static_cast<EBreakerEquipSlot>(SlotIndex);
        const int32 Seed = GrantCounter * 7919 + SlotIndex * 104729 + SafeLevel * 31;
        const FBreakerItemInstance Item = UBreakerLootLibrary::RollItem(
            FName(*FString::Printf(TEXT("DevTestGear_%s"), *UEnum::GetValueAsString(Slot))),
            Slot, EBreakerItemRarity::Exceptional, SafeLevel, Seed);
        EquipItem(Item);
    }
}

bool UBreakerEquipmentComponent::GetEquippedItem(EBreakerEquipSlot Slot, FBreakerItemInstance& OutItem) const
{
    if (const FBreakerItemInstance* Found = Equipped.FindByPredicate([Slot](const FBreakerItemInstance& Existing) { return Existing.Slot == Slot; }))
    {
        OutItem = *Found;
        return true;
    }
    return false;
}

FBreakerEquipmentStats UBreakerEquipmentComponent::AggregateStats(const TArray<FBreakerItemInstance>& Items, FBreakerAttributeContribution* OutContribution,
    const FBreakerBuildConditionState& Conditions)
{
    const TArray<FBreakerAffixDefinition>& Pool = UBreakerAffixLibrary::GetSliceAffixPool();

    // Sized off the enum so a new stat target never silently overruns these.
    constexpr int32 TargetCount = static_cast<int32>(EBreakerStatTarget::Count);
    float FlatByTarget[TargetCount] = {};
    float IncreasedByTarget[TargetCount] = {};
    // What the conditional lines are worth right now and what they would be
    // worth with everything satisfied. Display figures only; the live half is
    // already inside IncreasedByTarget.
    float ActiveConditionalPercent = 0.0f;
    float PotentialConditionalPercent = 0.0f;
    for (const FBreakerItemInstance& Item : Items)
    {
        for (const FBreakerRolledAffix& Rolled : Item.Affixes)
        {
            const FBreakerAffixDefinition* Definition = UBreakerAffixLibrary::FindAffix(Pool, Rolled.AffixId);
            if (!Definition) continue;
            if (Definition->IsConditional())
            {
                PotentialConditionalPercent += Rolled.Value;
                // A conditional line whose condition is false contributes
                // NOTHING — not a reduced amount, not a separate multiplier.
                // That is what keeps the locked one-bucket rule intact while
                // the bucket's contents change with the movement state.
                if (!Conditions.IsActive(Definition->Condition)) continue;
                ActiveConditionalPercent += Rolled.Value;
            }
            const int32 Target = static_cast<int32>(Definition->StatTarget);
            if (Definition->StatBucket == EBreakerStatBucket::Flat) FlatByTarget[Target] += Rolled.Value;
            else if (Definition->StatBucket == EBreakerStatBucket::IncreasedPercent) IncreasedByTarget[Target] += Rolled.Value;
        }
    }

    // Every conditional damage line and the unconditional one share the single
    // additive Increased bucket for outgoing damage. Summed here once so both
    // the display figure below and the contribution submit the same number.
    const float TotalIncreasedDamagePercent =
        IncreasedByTarget[static_cast<int32>(EBreakerStatTarget::WeaponDamage)]
        + IncreasedByTarget[static_cast<int32>(EBreakerStatTarget::AirborneDamage)]
        + IncreasedByTarget[static_cast<int32>(EBreakerStatTarget::SlidingDamage)]
        + IncreasedByTarget[static_cast<int32>(EBreakerStatTarget::WallRideDamage)]
        + IncreasedByTarget[static_cast<int32>(EBreakerStatTarget::RedlineDamage)]
        + IncreasedByTarget[static_cast<int32>(EBreakerStatTarget::RecentlyDashedDamage)];

    auto Increased = [&IncreasedByTarget](EBreakerStatTarget Target)
    {
        return 1.0f + IncreasedByTarget[static_cast<int32>(Target)] / 100.0f;
    };

    FBreakerEquipmentStats Stats;
    Stats.BonusHealth = FlatByTarget[static_cast<int32>(EBreakerStatTarget::Health)];
    Stats.ResourceRegenPerSecond = FlatByTarget[static_cast<int32>(EBreakerStatTarget::ResourceRegen)];
    Stats.BonusMaxResource = FlatByTarget[static_cast<int32>(EBreakerStatTarget::MaxResource)];
    Stats.MoveSpeedMultiplier = Increased(EBreakerStatTarget::MoveSpeed);
    Stats.DropChancePercent = IncreasedByTarget[static_cast<int32>(EBreakerStatTarget::DropChance)];
    Stats.PhysicalDamageReductionPercent = FMath::Min(IncreasedByTarget[static_cast<int32>(EBreakerStatTarget::PhysicalDamageReduction)], 60.0f);
    Stats.CriticalChanceBonus = FlatByTarget[static_cast<int32>(EBreakerStatTarget::CriticalChance)] / 100.0f;
    Stats.CriticalMultiplierBonus = FlatByTarget[static_cast<int32>(EBreakerStatTarget::CriticalDamage)] / 100.0f;
    Stats.SlideSpeedMultiplier = Increased(EBreakerStatTarget::SlideSpeed);
    Stats.AirControlMultiplier = Increased(EBreakerStatTarget::AirControl);
    Stats.DashCooldownMultiplier = 1.0f / Increased(EBreakerStatTarget::DashCooldownReduction);
    // The gear-only display multiplier now includes whatever conditional lines
    // are live, because that is what the player's damage actually is at this
    // instant. The two figures below break it down for the tooltip.
    Stats.WeaponDamageMultiplier = 1.0f + TotalIncreasedDamagePercent / 100.0f;
    Stats.AddedDamagePercent = FlatByTarget[static_cast<int32>(EBreakerStatTarget::AddedDamage)];
    Stats.ActiveConditionalDamagePercent = ActiveConditionalPercent;
    Stats.PotentialConditionalDamagePercent = PotentialConditionalPercent;

    if (OutContribution)
    {
        // Built from the raw buckets, not from the composed multipliers above:
        // the Increased percentages have to reach the attribute set unmerged so
        // they can join the tree's percentages in ONE additive bucket per stat.
        // Gear authors no More multipliers — those are reserved for trees and
        // Anomalous items (O3).
        OutContribution->Reset();
        OutContribution->AddFlat(EBreakerAggregatedAttribute::MaxHealth, Stats.BonusHealth);
        OutContribution->AddFlat(EBreakerAggregatedAttribute::MaxClassResource, Stats.BonusMaxResource);
        OutContribution->AddFlat(EBreakerAggregatedAttribute::CriticalChance, Stats.CriticalChanceBonus);
        OutContribution->AddFlat(EBreakerAggregatedAttribute::CriticalMultiplier, Stats.CriticalMultiplierBonus);
        OutContribution->AddIncreasedPercent(EBreakerAggregatedAttribute::MoveSpeed, IncreasedByTarget[static_cast<int32>(EBreakerStatTarget::MoveSpeed)]);
        // Slide speed, air control and dash cooldown reduction were the last
        // stats that reached gameplay WITHOUT passing through the aggregator:
        // the movement component read the composed multipliers below and the
        // tree's own multipliers and multiplied the two. Same bug class as
        // WeaponDamage. The Stats.* fields above survive as display figures.
        OutContribution->AddIncreasedPercent(EBreakerAggregatedAttribute::SlideSpeedMultiplier, IncreasedByTarget[static_cast<int32>(EBreakerStatTarget::SlideSpeed)]);
        OutContribution->AddIncreasedPercent(EBreakerAggregatedAttribute::AirControlMultiplier, IncreasedByTarget[static_cast<int32>(EBreakerStatTarget::AirControl)]);
        OutContribution->AddIncreasedPercent(EBreakerAggregatedAttribute::DashCooldownReduction, IncreasedByTarget[static_cast<int32>(EBreakerStatTarget::DashCooldownReduction)]);
        OutContribution->AddIncreasedPercent(EBreakerAggregatedAttribute::FireRateMultiplier, IncreasedByTarget[static_cast<int32>(EBreakerStatTarget::FireRate)]);
        // Weapon Damage used to reach the weapon on its own private path
        // (GearWeaponDamageMultiplier, multiplied against the DamageMultiplier
        // attribute), which meant gear damage and tree damage would have
        // composed MULTIPLICATIVELY. It now bids into the shared additive
        // bucket like every other Increased percentage. Stats
        // .WeaponDamageMultiplier survives as the gear-only display figure the
        // inventory totals panel prints; nothing in combat reads it any more.
        //
        // Every conditional line rides the SAME bid — they are not a second
        // multiplier and never were. Only the live ones are in the sum.
        OutContribution->AddIncreasedPercent(EBreakerAggregatedAttribute::DamageMultiplier, TotalIncreasedDamagePercent);
        // Added Damage lands in the FLAT lane of the same attribute, whose base
        // is 1.0. Flat sums first, so it is multiplied by the Increased bucket
        // rather than added after it — which is precisely why a flat line and an
        // increased line are two different decisions instead of one.
        OutContribution->AddFlat(EBreakerAggregatedAttribute::DamageMultiplier,
            FlatByTarget[static_cast<int32>(EBreakerStatTarget::AddedDamage)] / 100.0f);
    }
    return Stats;
}

void UBreakerEquipmentComponent::RefreshBuildConditions()
{
    if (!HasAttributeAuthority()) return;
    const FBreakerBuildConditionState Evaluated = FBreakerBuildConditionState::EvaluateForActor(GetOwner());
    if (Evaluated == ActiveConditions) return;
    ActiveConditions = Evaluated;
    RecalculateStats();
}

void UBreakerEquipmentComponent::RecalculateStats()
{
    CachedStats = AggregateStats(Equipped, &CachedContribution, ActiveConditions);
    ApplyStatsToAttributes();
}

void UBreakerEquipmentComponent::ApplyStatsToAttributes()
{
    // One submission, no absolute writes: the attribute set folds this against
    // the true bases and every other contributor. Recalculating in any order,
    // any number of times, converges to the same numbers.
    if (!Attributes || !HasAttributeAuthority()) return;
    Attributes->ApplyAttributeContribution(EBreakerAttributeContributor::Equipment, CachedContribution);
}

void UBreakerEquipmentComponent::OnRep_Equipped()
{
    CachedStats = AggregateStats(Equipped, &CachedContribution, ActiveConditions);
    OnEquipmentChanged.Broadcast();
}
