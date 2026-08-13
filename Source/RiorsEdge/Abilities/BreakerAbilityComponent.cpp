#include "Abilities/BreakerAbilityComponent.h"

#include "AbilitySystemComponent.h"
#include "Abilities/BreakerAbilityDefinition.h"
#include "Attributes/BreakerAttributeSet.h"
#include "GameFramework/Actor.h"
#include "Progression/BreakerProgressionComponent.h"

UBreakerAbilityComponent::UBreakerAbilityComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    SetIsReplicatedByDefault(true);
}

void UBreakerAbilityComponent::BeginPlay()
{
    Super::BeginPlay();
    RefreshGrants();
}

void UBreakerAbilityComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    // Cheap poll rather than a progression delegate: UBreakerProgressionComponent
    // has no change broadcast yet (spec SI-2 MISSING HOOK). Bind to
    // OnProgressionChanged and delete this poll once that lands.
    PollElapsed += DeltaTime;
    if (PollElapsed < LoadoutPollInterval)
    {
        return;
    }
    PollElapsed = 0.0f;

    FString Signature;
    if (BuildLoadoutSignature(Signature) && Signature != CachedLoadoutSignature)
    {
        RefreshGrants();
    }
}

UAbilitySystemComponent* UBreakerAbilityComponent::GetAbilitySystem() const
{
    if (!CachedAbilitySystem.IsValid())
    {
        if (const AActor* Owner = GetOwner())
        {
            CachedAbilitySystem = Owner->FindComponentByClass<UAbilitySystemComponent>();
        }
    }
    return CachedAbilitySystem.Get();
}

UBreakerProgressionComponent* UBreakerAbilityComponent::GetProgression() const
{
    if (!CachedProgression.IsValid())
    {
        if (const AActor* Owner = GetOwner())
        {
            CachedProgression = Owner->FindComponentByClass<UBreakerProgressionComponent>();
        }
    }
    return CachedProgression.Get();
}

bool UBreakerAbilityComponent::BuildLoadoutSignature(FString& OutSignature) const
{
    const UBreakerProgressionComponent* Progression = GetProgression();
    if (!Progression)
    {
        return false;
    }
    const FBreakerProgressionState& State = Progression->GetProgressionState();
    OutSignature = FString::Printf(TEXT("%d|%s|%s|%s"),
        static_cast<int32>(State.PermanentClass),
        *State.AbilityLoadout.ClassAbilityOne.ToString(),
        *State.AbilityLoadout.ClassAbilityTwo.ToString(),
        *State.AbilityLoadout.Ultimate.ToString());
    return true;
}

UBreakerAbilityDefinition* UBreakerAbilityComponent::ResolveDefinition(EBreakerClassId ClassId, EBreakerAbilitySlot Slot, FName EquippedId)
{
    if (UBreakerAbilityDefinition* Equipped = UBreakerAbilityDefinition::FindFallback(EquippedId))
    {
        return Equipped->CanOccupySlot(Slot) ? Equipped : nullptr;
    }
    // Nothing equipped (or an unknown id): fall back to the class default so
    // the slice is playable before the loadout UI writes anything.
    if (!EquippedId.IsNone())
    {
        return nullptr;
    }
    return UBreakerAbilityDefinition::FindFallback(UBreakerAbilityDefinition::DefaultAbilityIdForSlot(ClassId, Slot));
}

void UBreakerAbilityComponent::RefreshGrants()
{
    AActor* Owner = GetOwner();
    if (!Owner || !Owner->HasAuthority())
    {
        return;
    }
    UAbilitySystemComponent* ASC = GetAbilitySystem();
    const UBreakerProgressionComponent* Progression = GetProgression();
    if (!ASC || !Progression)
    {
        return;
    }

    const FBreakerProgressionState& State = Progression->GetProgressionState();
    const EBreakerClassId ClassId = State.PermanentClass;

    const EBreakerAbilitySlot Slots[] = {
        EBreakerAbilitySlot::ClassAbilityOne,
        EBreakerAbilitySlot::ClassAbilityTwo,
        EBreakerAbilitySlot::Ultimate
    };
    const FName EquippedIds[] = {
        State.AbilityLoadout.ClassAbilityOne,
        State.AbilityLoadout.ClassAbilityTwo,
        State.AbilityLoadout.Ultimate
    };

    for (int32 Index = 0; Index < UE_ARRAY_COUNT(Slots); ++Index)
    {
        const EBreakerAbilitySlot Slot = Slots[Index];
        UBreakerAbilityDefinition* Definition = ResolveDefinition(ClassId, Slot, EquippedIds[Index]);
        const FName DesiredId = Definition ? Definition->AbilityId : NAME_None;

        FBreakerGrantedAbility* Existing = GrantedBySlot.Find(Slot);
        if (Existing && Existing->AbilityId == DesiredId)
        {
            continue;
        }

        // Revoke first, always: re-granting before revoking double-grants the
        // same ability for a frame and clobbers its cooldown effect.
        if (Existing)
        {
            if (Existing->Handle.IsValid())
            {
                ASC->ClearAbility(Existing->Handle);
            }
            GrantedBySlot.Remove(Slot);
        }

        if (!Definition)
        {
            OnSlotChanged.Broadcast(Slot, NAME_None);
            continue;
        }

        FBreakerGrantedAbility Granted;
        Granted.AbilityId = Definition->AbilityId;
        Granted.Definition = Definition;
        Granted.bImplemented = Definition->IsImplemented();
        if (Granted.bImplemented)
        {
            FGameplayAbilitySpec Spec(Definition->AbilityClass, 1, static_cast<int32>(Slot), Owner);
            Granted.Handle = ASC->GiveAbility(Spec);
        }
        GrantedBySlot.Add(Slot, Granted);
        OnSlotChanged.Broadcast(Slot, Granted.AbilityId);
    }

    BuildLoadoutSignature(CachedLoadoutSignature);
}

bool UBreakerAbilityComponent::TryActivateSlot(EBreakerAbilitySlot Slot)
{
    const FBreakerGrantedAbility* Granted = GrantedBySlot.Find(Slot);
    UAbilitySystemComponent* ASC = GetAbilitySystem();
    if (!ASC)
    {
        return false;
    }
    if (!Granted || !Granted->bImplemented || !Granted->Handle.IsValid())
    {
        // Designed but unimplemented, or nothing equipped. Ask the server to
        // reconcile in case this client's grant bookkeeping is stale.
        const AActor* Owner = GetOwner();
        if (Owner && !Owner->HasAuthority())
        {
            ServerActivateSlot(Slot);
        }
        return false;
    }
    return ASC->TryActivateAbility(Granted->Handle);
}

void UBreakerAbilityComponent::ServerActivateSlot_Implementation(EBreakerAbilitySlot Slot)
{
    const FBreakerGrantedAbility* Granted = GrantedBySlot.Find(Slot);
    UAbilitySystemComponent* ASC = GetAbilitySystem();
    if (ASC && Granted && Granted->bImplemented && Granted->Handle.IsValid())
    {
        ASC->TryActivateAbility(Granted->Handle);
    }
}

FName UBreakerAbilityComponent::GetAbilityIdForSlot(EBreakerAbilitySlot Slot) const
{
    const FBreakerGrantedAbility* Granted = GrantedBySlot.Find(Slot);
    return Granted ? Granted->AbilityId : NAME_None;
}

UBreakerAbilityDefinition* UBreakerAbilityComponent::GetDefinitionForSlot(EBreakerAbilitySlot Slot) const
{
    const FBreakerGrantedAbility* Granted = GrantedBySlot.Find(Slot);
    return Granted ? Granted->Definition.Get() : nullptr;
}

bool UBreakerAbilityComponent::IsSlotImplemented(EBreakerAbilitySlot Slot) const
{
    const FBreakerGrantedAbility* Granted = GrantedBySlot.Find(Slot);
    return Granted && Granted->bImplemented;
}

bool UBreakerAbilityComponent::IsSlotGranted(EBreakerAbilitySlot Slot) const
{
    const FBreakerGrantedAbility* Granted = GrantedBySlot.Find(Slot);
    return Granted && Granted->Handle.IsValid();
}

bool UBreakerAbilityComponent::SlotHasCooldown(EBreakerAbilitySlot Slot) const
{
    const UBreakerAbilityDefinition* Definition = GetDefinitionForSlot(Slot);
    return Definition && Definition->HasCooldown();
}

float UBreakerAbilityComponent::GetCooldownDuration(EBreakerAbilitySlot Slot) const
{
    const UBreakerAbilityDefinition* Definition = GetDefinitionForSlot(Slot);
    return Definition ? Definition->CooldownSeconds : 0.0f;
}

float UBreakerAbilityComponent::GetCooldownRemaining(EBreakerAbilitySlot Slot) const
{
    const UBreakerAbilityDefinition* Definition = GetDefinitionForSlot(Slot);
    const UAbilitySystemComponent* ASC = GetAbilitySystem();
    if (!Definition || !ASC || !Definition->HasCooldown() || !Definition->CooldownTag.IsValid())
    {
        return 0.0f;
    }
    FGameplayTagContainer CooldownTags;
    CooldownTags.AddTag(Definition->CooldownTag);
    const FGameplayEffectQuery Query = FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(CooldownTags);
    float Longest = 0.0f;
    for (const TPair<float, float>& Pair : ASC->GetActiveEffectsTimeRemainingAndDuration(Query))
    {
        Longest = FMath::Max(Longest, Pair.Key);
    }
    return Longest;
}

float UBreakerAbilityComponent::GetCost(EBreakerAbilitySlot Slot) const
{
    const UBreakerAbilityDefinition* Definition = GetDefinitionForSlot(Slot);
    return Definition ? Definition->ResourceCost : 0.0f;
}

bool UBreakerAbilityComponent::CanAffordSlot(EBreakerAbilitySlot Slot) const
{
    const float Cost = GetCost(Slot);
    if (Cost <= 0.0f)
    {
        return true;
    }
    const UAbilitySystemComponent* ASC = GetAbilitySystem();
    if (!ASC)
    {
        return false;
    }
    bool bFound = false;
    const float Current = ASC->GetGameplayAttributeValue(UBreakerAttributeSet::GetClassResourceAttribute(), bFound);
    return bFound && Current >= Cost;
}

int32 UBreakerAbilityComponent::GetActiveCooldownCount() const
{
    int32 Count = 0;
    for (const TPair<EBreakerAbilitySlot, FBreakerGrantedAbility>& Pair : GrantedBySlot)
    {
        if (GetCooldownRemaining(Pair.Key) > 0.0f)
        {
            ++Count;
        }
    }
    return Count;
}

int32 UBreakerAbilityComponent::GetGrantedCount() const
{
    int32 Count = 0;
    for (const TPair<EBreakerAbilitySlot, FBreakerGrantedAbility>& Pair : GrantedBySlot)
    {
        if (Pair.Value.Handle.IsValid())
        {
            ++Count;
        }
    }
    return Count;
}
