#include "Abilities/BreakerAbilityComponent.h"

#include "AbilitySystemComponent.h"
#include "Abilities/BreakerAbilityDefinition.h"
#include "Abilities/BreakerGameplayAbility.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Classes/BreakerMomentumComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Progression/BreakerProgressionComponent.h"
#include "TimerManager.h"

UBreakerAbilityComponent::UBreakerAbilityComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    SetIsReplicatedByDefault(true);
}

void UBreakerAbilityComponent::BeginPlay()
{
    Super::BeginPlay();
    RefreshGrants();

#if !UE_BUILD_SHIPPING
    // -BreakerAbilityProbe: the ability-cast half the capture harness cannot
    // reach (it cannot press a key). Self-scheduled against the harness's own
    // screenshot clock (first frame 6.0s, then every 2.0s): the class is
    // forced at 5.0s, slot two casts at 5.85s and the ultimate at 7.6s. The
    // probe clock (BeginPlay) and the harness clock (capture arm) skew by up
    // to ~0.3s run to run — one photograph landed BEFORE the cast it was
    // for — so each cast leads its frame by less than its flash's life
    // rather than by a margin the skew can eat. Casts
    // only what a REAL character holds — the default loadout through the one
    // grant site — so the probe can never photograph an ability a player
    // could not reach; the O176-gated abilities join the photograph when
    // their unlock rows land. Resource is granted through the loop's own
    // unmetered dev grant, not by touching the bank.
    if (FParse::Param(FCommandLine::Get(), TEXT("BreakerAbilityProbe")))
    {
        AActor* Owner = GetOwner();
        UWorld* World = GetWorld();
        if (Owner && World && Owner->HasAuthority() && Cast<APawn>(Owner))
        {
            TWeakObjectPtr<UBreakerAbilityComponent> WeakThis(this);
            FTimerHandle ClassHandle, SlotTwoHandle, UltimateHandle;
            World->GetTimerManager().SetTimer(ClassHandle, FTimerDelegate::CreateLambda([WeakThis]()
            {
                UBreakerAbilityComponent* Probe = WeakThis.Get();
                UBreakerProgressionComponent* Progression = Probe ? Probe->GetProgression() : nullptr;
                if (Progression && Progression->GetProgressionState().PermanentClass == EBreakerClassId::None)
                {
                    Progression->DevForceClass(EBreakerClassId::Swift);
                }
                if (Probe)
                {
                    Probe->RefreshGrants();
                }
            }), 5.0f, false);
            auto ProbeCast = [WeakThis](EBreakerAbilitySlot Slot)
            {
                UBreakerAbilityComponent* Probe = WeakThis.Get();
                if (!Probe)
                {
                    return;
                }
                if (UBreakerMomentumComponent* Momentum = Probe->GetOwner()->FindComponentByClass<UBreakerMomentumComponent>())
                {
                    Momentum->GrantMomentum(200.0f);
                }
                const bool bActivated = Probe->TryActivateSlot(Slot);
                UE_LOG(LogTemp, Log, TEXT("[BreakerAbilityProbe] slot %d activate=%d"), static_cast<int32>(Slot), bActivated ? 1 : 0);
            };
            World->GetTimerManager().SetTimer(SlotTwoHandle, FTimerDelegate::CreateLambda([ProbeCast]()
            {
                ProbeCast(EBreakerAbilitySlot::ClassAbilityTwo);
            }), 5.85f, false);
            World->GetTimerManager().SetTimer(UltimateHandle, FTimerDelegate::CreateLambda([ProbeCast]()
            {
                ProbeCast(EBreakerAbilitySlot::Ultimate);
            }), 7.6f, false);
        }
    }
#endif
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
        // The loadout id is not proof the ability is legitimately this
        // character's: TryEquipAbility/ValidateSelection guard the WRITE, but
        // nothing guarded the READ, and the loadout can carry a foreign-class
        // id without ever going through that write. DevForceClass (Progression/)
        // rewrites State.PermanentClass and ClassDefinition on a class switch
        // but does not migrate State.AbilityLoadout, so a Swift character who
        // dev-swaps to Caster keeps "Swift.Overdrive" sitting in the Ultimate
        // slot; a stale/hand-edited save can carry the same thing. This is the
        // ONE place a loadout id turns into an actually-granted GAS ability
        // (RefreshGrants below is its only caller), so it is where the class
        // check has to live for console/Blueprint/save-load paths to share it
        // — owner playtest: "im also able to have abilities from other classes
        // equipped".
        if (Equipped->ClassId == ClassId && Equipped->CanOccupySlot(Slot))
        {
            return Equipped;
        }
        // WRONG CLASS and WRONG SLOT are different failures and must not share
        // an answer. A foreign-class id is a STALE LOADOUT — the character
        // never chose it, DevForceClass simply failed to migrate it — so the
        // repair is the class default, exactly as for an unknown id, and the
        // slice stays playable. A wrong-SLOT id is a caller ERROR: someone
        // asked for the ultimate in a class-ability slot, and answering with a
        // different ability would quietly satisfy a request nobody made.
        // Refusing is the older rule and is asserted by
        // RiorsEdge.Abilities.SlotResolution; folding the two together broke it.
        if (Equipped->ClassId == ClassId)
        {
            return nullptr;
        }
        UE_LOG(LogTemp, Warning,
            TEXT("BreakerAbilityComponent: refusing to grant '%s' (class %d) to a class-%d character; falling back ")
            TEXT("to the class default. The loadout was not migrated across a class change."),
            *EquippedId.ToString(), static_cast<int32>(Equipped->ClassId), static_cast<int32>(ClassId));
    }
    // Nothing equipped, an unknown id, or (as above) a real id from the wrong
    // class: fall back to the class default so the slice is playable no
    // matter what a stale save's loadout carries. Granting the default beats
    // granting nothing — an unknown id silently killing every ability is
    // exactly the failure the owner hit.
    return UBreakerAbilityDefinition::FindFallback(UBreakerAbilityDefinition::DefaultAbilityIdForSlot(ClassId, Slot));
}

EBreakerAbilitySelectionResult UBreakerAbilityComponent::ValidateSelection(
    EBreakerClassId ClassId, EBreakerAbilitySlot Slot, FName AbilityId,
    FName EquippedOne, FName EquippedTwo, FName EquippedUltimate)
{
    if (ClassId == EBreakerClassId::None) return EBreakerAbilitySelectionResult::NoClassChosen;

    const UBreakerAbilityDefinition* Definition = UBreakerAbilityDefinition::FindFallback(AbilityId);
    if (!Definition) return EBreakerAbilitySelectionResult::UnknownAbility;
    if (Definition->ClassId != ClassId) return EBreakerAbilitySelectionResult::WrongClass;
    if (!Definition->CanOccupySlot(Slot)) return EBreakerAbilitySelectionResult::WrongSlot;

    // Re-selecting what is already in THIS slot is allowed and is a no-op; only
    // a collision with a DIFFERENT slot is a duplicate. A picker that redraws
    // its own selection must not have to special-case its current state.
    const FName Occupants[] = {EquippedOne, EquippedTwo, EquippedUltimate};
    const EBreakerAbilitySlot OccupantSlots[] = {
        EBreakerAbilitySlot::ClassAbilityOne,
        EBreakerAbilitySlot::ClassAbilityTwo,
        EBreakerAbilitySlot::Ultimate
    };
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(Occupants); ++Index)
    {
        if (OccupantSlots[Index] != Slot && Occupants[Index] == AbilityId)
        {
            return EBreakerAbilitySelectionResult::AlreadyEquipped;
        }
    }
    return EBreakerAbilitySelectionResult::Allowed;
}

FText UBreakerAbilityComponent::DescribeSelectionResult(EBreakerAbilitySelectionResult Result)
{
    switch (Result)
    {
    case EBreakerAbilitySelectionResult::Allowed:        return FText::GetEmpty();
    case EBreakerAbilitySelectionResult::NoClassChosen:  return NSLOCTEXT("Breaker", "SelectNoClass", "Choose a class before equipping abilities.");
    case EBreakerAbilitySelectionResult::UnknownAbility: return NSLOCTEXT("Breaker", "SelectUnknown", "That ability does not exist.");
    case EBreakerAbilitySelectionResult::WrongClass:     return NSLOCTEXT("Breaker", "SelectWrongClass", "That ability belongs to another class.");
    case EBreakerAbilitySelectionResult::WrongSlot:      return NSLOCTEXT("Breaker", "SelectWrongSlot", "That ability cannot go in this slot.");
    case EBreakerAbilitySelectionResult::AlreadyEquipped:return NSLOCTEXT("Breaker", "SelectDuplicate", "That ability is already equipped.");
    // Names where it is bought, because a refusal the player cannot act on is
    // only half an answer.
    case EBreakerAbilitySelectionResult::NotUnlocked:    return NSLOCTEXT("Breaker", "SelectNotUnlocked", "Not unlocked. See the quartermaster.");
    default:                                             return NSLOCTEXT("Breaker", "SelectRefused", "That ability cannot be equipped.");
    }
}

TArray<FName> UBreakerAbilityComponent::GetSelectableAbilityIds(EBreakerAbilitySlot Slot) const
{
    const UBreakerProgressionComponent* Progression = GetProgression();
    const EBreakerClassId ClassId = Progression ? Progression->GetProgressionState().PermanentClass : EBreakerClassId::None;
    return UBreakerAbilityDefinition::GetClassAbilityIds(ClassId, Slot);
}

FName UBreakerAbilityComponent::GetEquippedAbilityId(EBreakerAbilitySlot Slot) const
{
    const UBreakerProgressionComponent* Progression = GetProgression();
    if (!Progression) return NAME_None;
    const FBreakerAbilityLoadout& Loadout = Progression->GetProgressionState().AbilityLoadout;
    switch (Slot)
    {
    case EBreakerAbilitySlot::ClassAbilityOne: return Loadout.ClassAbilityOne;
    case EBreakerAbilitySlot::ClassAbilityTwo: return Loadout.ClassAbilityTwo;
    case EBreakerAbilitySlot::Ultimate:        return Loadout.Ultimate;
    default:                                   return NAME_None;
    }
}

EBreakerAbilitySelectionResult UBreakerAbilityComponent::PreviewSelection(EBreakerAbilitySlot Slot, FName AbilityId) const
{
    const UBreakerProgressionComponent* Progression = GetProgression();
    if (!Progression) return EBreakerAbilitySelectionResult::NoClassChosen;
    const FBreakerProgressionState& State = Progression->GetProgressionState();
    const EBreakerAbilitySelectionResult Registry = ValidateSelection(State.PermanentClass, Slot, AbilityId,
        State.AbilityLoadout.ClassAbilityOne, State.AbilityLoadout.ClassAbilityTwo, State.AbilityLoadout.Ultimate);
    if (Registry != EBreakerAbilitySelectionResult::Allowed) return Registry;
    // The registry rules pass; the last question is whether this character has
    // bought it. ASKED, NOT RESTATED — progression owns the unlock rule and
    // this calls it, so the preview and the equip cannot disagree. Registry
    // reasons win when both apply: "belongs to another class" is more useful
    // than "not unlocked" for an id that could never be unlocked here.
    if (!Progression->IsAbilityUnlocked(AbilityId)) return EBreakerAbilitySelectionResult::NotUnlocked;
    return EBreakerAbilitySelectionResult::Allowed;
}

bool UBreakerAbilityComponent::TryEquipAbility(EBreakerAbilitySlot Slot, FName AbilityId, FText& OutFailureReason)
{
    const AActor* Owner = GetOwner();
    if (Owner && !Owner->HasAuthority())
    {
        OutFailureReason = NSLOCTEXT("Breaker", "SelectNoAuthority", "Ability loadout changes are server-authoritative.");
        return false;
    }
    UBreakerProgressionComponent* Progression = GetProgression();
    if (!Progression)
    {
        OutFailureReason = DescribeSelectionResult(EBreakerAbilitySelectionResult::NoClassChosen);
        return false;
    }

    const EBreakerAbilitySelectionResult Result = PreviewSelection(Slot, AbilityId);
    if (Result != EBreakerAbilitySelectionResult::Allowed)
    {
        OutFailureReason = DescribeSelectionResult(Result);
        return false;
    }
    // Already in this slot: allowed, and deliberately not routed through the
    // write. Re-equipping would broadcast OnProgressionChanged, which would
    // re-grant the spec and clobber a live cooldown for no reason.
    if (GetEquippedAbilityId(Slot) == AbilityId)
    {
        OutFailureReason = FText::GetEmpty();
        return true;
    }

    // ONE writer. Progression owns the state and the unlock rules; this
    // component owns the registry rules the checks above enforce. If
    // progression refuses (the ability is not unlocked yet), its own reason is
    // what the player sees — restating that rule here is how two copies drift.
    //
    // The blocker this comment used to describe is gone: every class has a
    // fallback definition now, so no class is refused wholesale for want of one.
    // What progression refuses today is an ability the character has not
    // UNLOCKED (O100) — the starters and the ultimate are free, everything else
    // is bought with a token at the quartermaster — and that refusal is a real
    // answer rather than a missing row.
    if (!Progression->EquipAbility(Slot, AbilityId, OutFailureReason))
    {
        return false;
    }
    // The poll would pick this up within LoadoutPollInterval anyway; doing it
    // now means the key is live on the frame the player pressed Equip.
    RefreshGrants();
    OutFailureReason = FText::GetEmpty();
    return true;
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
    const bool bActivated = ASC->TryActivateAbility(Granted->Handle);
    if (bActivated)
    {
        // Broadcast here rather than inside each ability: this is the one
        // funnel every input path already goes through, and it fires on the
        // machine that pressed the key, which is where the HUD lives.
        OnAbilityActivated.Broadcast(Slot);
    }
    return bActivated;
}

void UBreakerAbilityComponent::ServerActivateSlot_Implementation(EBreakerAbilitySlot Slot)
{
    const FBreakerGrantedAbility* Granted = GrantedBySlot.Find(Slot);
    UAbilitySystemComponent* ASC = GetAbilitySystem();
    if (ASC && Granted && Granted->bImplemented && Granted->Handle.IsValid())
    {
        if (ASC->TryActivateAbility(Granted->Handle))
        {
            OnAbilityActivated.Broadcast(Slot);
        }
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
    bool bFoundResource = false;
    const float Current = ASC->GetGameplayAttributeValue(UBreakerAttributeSet::GetClassResourceAttribute(), bFoundResource);
    bool bFoundFloor = false;
    const float Floor = ASC->GetGameplayAttributeValue(UBreakerAttributeSet::GetClassResourceFloorAttribute(), bFoundFloor);
    // Spec D8. The HUD reads this to grey a slot out, so it must agree with the
    // rule GAS actually enforces in CheckCost: with a closed floor (every class
    // but an Overcasting Caster) this is the identical comparison, and with an
    // open one a slot the player can genuinely overdraft into stays lit while
    // everything is unaffordable during the debt.
    return bFoundResource && UBreakerGameplayAbility::IsAffordableWithFloor(Current, Cost, bFoundFloor ? Floor : 0.0f);
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
