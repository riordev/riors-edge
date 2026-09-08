#include "Combat/BreakerStatusCycleComponent.h"

#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Data/BreakerStrings.h"
#include "Combat/BreakerStatusRules.h"

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
        Existing->SeedDefaultCycle();
        Existing->BindProgression();
        Existing->SyncProgression();
        return Existing;
    }
    if (!Owner->HasAuthority()) return nullptr;
    UBreakerStatusCycleComponent* Created = NewObject<UBreakerStatusCycleComponent>(Owner);
    Owner->AddInstanceComponent(Created);
    // RegisterComponent ensures on an owner with no world, which is exactly the
    // case in automation. The component is fully usable unregistered — it does
    // not tick and it owns no scene state — so a worldless owner gets a working
    // cycle rather than an ensure.
    if (Owner->GetWorld()) Created->RegisterComponent();
    Created->SeedDefaultCycle();
    Created->BindProgression();
    Created->SyncProgression();
    return Created;
}

void UBreakerStatusCycleComponent::BeginPlay()
{
    Super::BeginPlay();
    SeedDefaultCycle();
    BindProgression();
    SyncProgression();
}

void UBreakerStatusCycleComponent::BindProgression()
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;
    UBreakerProgressionComponent* Progression = GetOwner()->FindComponentByClass<UBreakerProgressionComponent>();
    if (BoundProgression.Get() == Progression) return;
    if (BoundProgression.IsValid())
        BoundProgression->OnProgressionChanged.RemoveDynamic(this, &UBreakerStatusCycleComponent::SyncProgression);
    BoundProgression = Progression;
    if (Progression)
        Progression->OnProgressionChanged.AddUniqueDynamic(this, &UBreakerStatusCycleComponent::SyncProgression);
}

void UBreakerStatusCycleComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    if (BoundProgression.IsValid())
        BoundProgression->OnProgressionChanged.RemoveDynamic(this, &UBreakerStatusCycleComponent::SyncProgression);
    BoundProgression.Reset();
    Super::EndPlay(Reason);
}

void UBreakerStatusCycleComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME_CONDITION(UBreakerStatusCycleComponent, AvailableStatuses, COND_OwnerOnly);
    DOREPLIFETIME_CONDITION(UBreakerStatusCycleComponent, Cursor, COND_OwnerOnly);
    DOREPLIFETIME_CONDITION(UBreakerStatusCycleComponent, bAdvanceOnHit, COND_OwnerOnly);
    DOREPLIFETIME_CONDITION(UBreakerStatusCycleComponent, bPreviewAhead, COND_OwnerOnly);
}

void UBreakerStatusCycleComponent::OnRep_Cycle() { OnCycleChanged.Broadcast(PeekNext()); }

void UBreakerStatusCycleComponent::SyncProgression()
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;
    const UBreakerProgressionComponent* Progression = GetOwner()->FindComponentByClass<UBreakerProgressionComponent>();
    const bool bCaster = Progression && Progression->GetProgressionState().PermanentClass == EBreakerClassId::Caster;
    const bool bNewPreview = bCaster && Progression->GetNodeRank(TEXT("Caster.Multispell.Cycle"), EBreakerPointCurrency::DoctrinePoints) >= 2;
    bool bChanged = bPreviewAhead != bNewPreview;
    bPreviewAhead = bNewPreview;
    const FGameplayTag Erased = FGameplayTag::RequestGameplayTag(TEXT("Status.Erased"));
    const bool bHasSiphon = bCaster && Progression->IsAbilityUnlocked(TEXT("Caster.Siphon"));
    const int32 Existing = AvailableStatuses.IndexOfByPredicate([Erased](const FBreakerCycleEntry& Entry)
        { return Entry.Spec.StatusTag == Erased; });
    if (bHasSiphon && Existing == INDEX_NONE)
    {
        FBreakerCycleEntry Void;
        Void.Element = EBreakerElement::Void;
        Void.DamageFamily = EBreakerDamageFamily::Elemental;
        Void.Spec.StatusTag = Erased;
        Void.DisplayName = FText::FromString(BreakerStrings::Get(EBreakerStringKey::CycleVoid));
        AvailableStatuses.Add(Void);
        bOwnsSiphonEntry = true;
        bChanged = true;
    }
    else if (!bHasSiphon && bOwnsSiphonEntry)
    {
        bOwnsSiphonEntry = false;
        if (Existing != INDEX_NONE)
        {
            AvailableStatuses.RemoveAt(Existing);
            // Preserve the next surviving position if authored entries follow Void.
            if (Cursor > Existing) --Cursor;
            Cursor = AvailableStatuses.IsEmpty() ? 0 : Cursor % AvailableStatuses.Num();
            bChanged = true;
        }
    }
    if (bChanged) OnRep_Cycle();
}

void UBreakerStatusCycleComponent::SeedDefaultCycle()
{
    // Zero-setup convention: the cycle is playable before any Data Asset
    // exists, exactly like the weapon and ability fallback registries. An
    // authored list wins — seeding only ever fills an EMPTY cycle.
    if ((GetOwner() && !GetOwner()->HasAuthority()) || bSeeded || !AvailableStatuses.IsEmpty()) return;
    bSeeded = true;

    // The starter cycle retains its three authored positions. Siphon unlock
    // appends the real Void delivery separately in SyncProgression.
    // Every magnitude below is O2 PLACEHOLDER.
    const TCHAR* SeedTags[] = { TEXT("Status.Bleed"), TEXT("Status.Poison") };
    const EBreakerStringKey Names[] = { EBreakerStringKey::CycleBleed, EBreakerStringKey::CyclePoison };
    for (int32 SeedIndex = 0; SeedIndex < UE_ARRAY_COUNT(SeedTags); ++SeedIndex)
    {
        const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(SeedTags[SeedIndex], false);
        if (!Tag.IsValid()) continue;
        FBreakerCycleEntry Entry;
        Entry.Spec.StatusTag = Tag;
        Entry.Spec.BaseDamagePerTick = 6.0f;
        Entry.Spec.Duration = 4.0f;
        Entry.Spec.TickInterval = 1.0f;
        Entry.DamageFamily = EBreakerDamageFamily::Physical;
        Entry.DisplayName = FText::FromString(BreakerStrings::Get(Names[SeedIndex]));
        AvailableStatuses.Add(Entry);
    }
    FBreakerCycleEntry Entropy;
    Entropy.Element = EBreakerElement::Entropy;
    Entropy.Spec.StatusTag = FGameplayTag::RequestGameplayTag(TEXT("Status.Rot"));
    Entropy.DisplayName = FText::FromString(BreakerStrings::Get(EBreakerStringKey::CycleEntropy));
    AvailableStatuses.Add(Entropy);
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
    if (GetOwner() && !GetOwner()->HasAuthority()) return FGameplayTag();
    if (AvailableStatuses.IsEmpty()) return FGameplayTag();
    const FGameplayTag Current = AvailableStatuses[Cursor % AvailableStatuses.Num()].Spec.StatusTag;
    Cursor = (Cursor + 1) % AvailableStatuses.Num();
    OnCycleChanged.Broadcast(PeekNext(0));
    return Current;
}

void UBreakerStatusCycleComponent::SetAdvanceOnHit(bool bOnHit)
{
    if (GetOwner() && !GetOwner()->HasAuthority()) return;
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
    if (GetOwner() && !GetOwner()->HasAuthority()) return;
    if (!Entry.Spec.StatusTag.IsValid()) return;
    // Explicit authoring takes ownership of this entry; progression must not
    // later delete a custom payload when an unlock or class changes.
    if (Entry.Spec.StatusTag == FGameplayTag::RequestGameplayTag(TEXT("Status.Erased"))) bOwnsSiphonEntry = false;
    // Idempotent by tag. A status added twice would come round twice as often,
    // which silently doubles its weight in a cycle the HUD claims is uniform.
    for (FBreakerCycleEntry& Existing : AvailableStatuses)
    {
        if (Existing.Spec.StatusTag == Entry.Spec.StatusTag)
        {
            Existing = Entry;
            OnCycleChanged.Broadcast(PeekNext());
            return;
        }
    }
    AvailableStatuses.Add(Entry);
    OnCycleChanged.Broadcast(PeekNext(0));
}

void UBreakerStatusCycleComponent::RemoveStatusType(FGameplayTag StatusTag)
{
    if (GetOwner() && !GetOwner()->HasAuthority()) return;
    if (StatusTag == FGameplayTag::RequestGameplayTag(TEXT("Status.Erased"))) bOwnsSiphonEntry = false;
    const int32 Removed = AvailableStatuses.RemoveAll(
        [StatusTag](const FBreakerCycleEntry& Entry) { return Entry.Spec.StatusTag == StatusTag; });
    if (Removed <= 0) return;
    // Keep the cursor legal. A cursor left past the end would read the cycle
    // out of bounds on the next peek.
    Cursor = AvailableStatuses.IsEmpty() ? 0 : Cursor % AvailableStatuses.Num();
    OnCycleChanged.Broadcast(PeekNext(0));
}
