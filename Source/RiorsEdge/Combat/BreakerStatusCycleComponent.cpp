#include "Combat/BreakerStatusCycleComponent.h"

#include "GameFramework/Actor.h"

UBreakerStatusCycleComponent::UBreakerStatusCycleComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(true);
}

UBreakerStatusCycleComponent* UBreakerStatusCycleComponent::FindOrAdd(AActor* Owner)
{
    if (!Owner) return nullptr;
    if (UBreakerStatusCycleComponent* Existing = Owner->FindComponentByClass<UBreakerStatusCycleComponent>())
    {
        return Existing;
    }
    UBreakerStatusCycleComponent* Created = NewObject<UBreakerStatusCycleComponent>(Owner);
    // RegisterComponent ensures on an owner with no world, which is exactly the
    // case in automation. The component is fully usable unregistered — it does
    // not tick and it owns no scene state — so a worldless owner gets a working
    // cycle rather than an ensure.
    if (Owner->GetWorld()) Created->RegisterComponent();
    Created->SeedDefaultCycle();
    return Created;
}

void UBreakerStatusCycleComponent::BeginPlay()
{
    Super::BeginPlay();
    SeedDefaultCycle();
}

void UBreakerStatusCycleComponent::SeedDefaultCycle()
{
    // Zero-setup convention: the cycle is playable before any Data Asset
    // exists, exactly like the weapon and ability fallback registries. An
    // authored list wins — seeding only ever fills an EMPTY cycle.
    if (bSeeded || !AvailableStatuses.IsEmpty()) return;
    bSeeded = true;

    // Bleed and Poison are what a Caster can actually apply today: Cleave
    // applies Bleed, Rot applies Poison. Void joins when Siphon is in the kit.
    // Every magnitude below is O2 PLACEHOLDER.
    const TCHAR* SeedTags[] = { TEXT("Status.Bleed"), TEXT("Status.Poison") };
    for (const TCHAR* TagName : SeedTags)
    {
        const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(TagName, false);
        if (!Tag.IsValid()) continue;
        FBreakerCycleEntry Entry;
        Entry.Spec.StatusTag = Tag;
        Entry.Spec.BaseDamagePerTick = 6.0f;
        Entry.Spec.Duration = 4.0f;
        Entry.Spec.TickInterval = 1.0f;
        Entry.DamageFamily = EBreakerDamageFamily::Physical;
        Entry.DisplayName = FText::FromString(Tag.GetTagName().ToString());
        AvailableStatuses.Add(Entry);
    }
}

FBreakerCycleEntry UBreakerStatusCycleComponent::PeekNextEntry(int32 Lookahead) const
{
    if (AvailableStatuses.IsEmpty()) return FBreakerCycleEntry();
    const int32 Index = (Cursor + FMath::Max(0, Lookahead)) % AvailableStatuses.Num();
    return AvailableStatuses[Index];
}

FGameplayTag UBreakerStatusCycleComponent::PeekNext(int32 Lookahead) const
{
    return PeekNextEntry(Lookahead).Spec.StatusTag;
}

FGameplayTag UBreakerStatusCycleComponent::AdvanceCycle()
{
    if (AvailableStatuses.IsEmpty()) return FGameplayTag();
    const FGameplayTag Current = AvailableStatuses[Cursor % AvailableStatuses.Num()].Spec.StatusTag;
    Cursor = (Cursor + 1) % AvailableStatuses.Num();
    OnCycleChanged.Broadcast(PeekNext(0));
    return Current;
}

void UBreakerStatusCycleComponent::SetAdvanceOnHit(bool bOnHit)
{
    bAdvanceOnHit = bOnHit;
}

TArray<FGameplayTag> UBreakerStatusCycleComponent::GetAvailableStatusTypes() const
{
    TArray<FGameplayTag> Tags;
    Tags.Reserve(AvailableStatuses.Num());
    for (const FBreakerCycleEntry& Entry : AvailableStatuses) Tags.Add(Entry.Spec.StatusTag);
    return Tags;
}

void UBreakerStatusCycleComponent::AddStatusType(const FBreakerCycleEntry& Entry)
{
    if (!Entry.Spec.StatusTag.IsValid()) return;
    // Idempotent by tag. A status added twice would come round twice as often,
    // which silently doubles its weight in a cycle the HUD claims is uniform.
    for (FBreakerCycleEntry& Existing : AvailableStatuses)
    {
        if (Existing.Spec.StatusTag == Entry.Spec.StatusTag)
        {
            Existing = Entry;
            return;
        }
    }
    AvailableStatuses.Add(Entry);
    OnCycleChanged.Broadcast(PeekNext(0));
}

void UBreakerStatusCycleComponent::RemoveStatusType(FGameplayTag StatusTag)
{
    const int32 Removed = AvailableStatuses.RemoveAll(
        [StatusTag](const FBreakerCycleEntry& Entry) { return Entry.Spec.StatusTag == StatusTag; });
    if (Removed <= 0) return;
    // Keep the cursor legal. A cursor left past the end would read the cycle
    // out of bounds on the next peek.
    Cursor = AvailableStatuses.IsEmpty() ? 0 : Cursor % AvailableStatuses.Num();
    OnCycleChanged.Broadcast(PeekNext(0));
}
