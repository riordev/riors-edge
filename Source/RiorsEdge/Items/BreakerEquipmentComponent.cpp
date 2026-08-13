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

bool UBreakerEquipmentComponent::EquipItem(const FBreakerItemInstance& Item)
{
    if (!Item.IsValid() || !HasAttributeAuthority()) return false;
    UnequipSlot(Item.Slot);
    Equipped.Add(Item);
    RecalculateStats();
    OnEquipmentChanged.Broadcast();
    return true;
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
    const uint8 Threshold = static_cast<uint8>(MinimumKept);
    const int32 Removed = Backpack.RemoveAll([Threshold](const FBreakerItemInstance& Existing)
    {
        return static_cast<uint8>(Existing.Rarity) < Threshold;
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

FBreakerEquipmentStats UBreakerEquipmentComponent::AggregateStats(const TArray<FBreakerItemInstance>& Items, FBreakerAttributeContribution* OutContribution)
{
    const TArray<FBreakerAffixDefinition>& Pool = UBreakerAffixLibrary::GetSliceAffixPool();

    // Sized off the enum so a new stat target never silently overruns these.
    constexpr int32 TargetCount = static_cast<int32>(EBreakerStatTarget::Count);
    float FlatByTarget[TargetCount] = {};
    float IncreasedByTarget[TargetCount] = {};
    for (const FBreakerItemInstance& Item : Items)
    {
        for (const FBreakerRolledAffix& Rolled : Item.Affixes)
        {
            const FBreakerAffixDefinition* Definition = UBreakerAffixLibrary::FindAffix(Pool, Rolled.AffixId);
            if (!Definition) continue;
            const int32 Target = static_cast<int32>(Definition->StatTarget);
            if (Definition->StatBucket == EBreakerStatBucket::Flat) FlatByTarget[Target] += Rolled.Value;
            else if (Definition->StatBucket == EBreakerStatBucket::IncreasedPercent) IncreasedByTarget[Target] += Rolled.Value;
        }
    }

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
    Stats.WeaponDamageMultiplier = Increased(EBreakerStatTarget::WeaponDamage);

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
    }
    return Stats;
}

void UBreakerEquipmentComponent::RecalculateStats()
{
    CachedStats = AggregateStats(Equipped, &CachedContribution);
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
    CachedStats = AggregateStats(Equipped, &CachedContribution);
    OnEquipmentChanged.Broadcast();
}
