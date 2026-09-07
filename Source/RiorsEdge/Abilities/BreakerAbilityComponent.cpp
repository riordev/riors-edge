#include "Abilities/BreakerAbilityComponent.h"

#include "AbilitySystemComponent.h"
#include "Abilities/BreakerAbilityDefinition.h"
#include "Abilities/BreakerGameplayAbility.h"
#include "Abilities/BreakerGunsmithAbilities.h"
#include "Abilities/BreakerAbilityStateComponent.h"
#include "Abilities/BreakerAbilityTags.h"
#include "Combat/BreakerEnemy.h"
#include "Combat/BreakerCombatComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "EngineUtils.h"
#include "Progression/BreakerProgressionLibrary.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Classes/BreakerChargeComponent.h"
#include "Classes/BreakerGritComponent.h"
#include "Classes/BreakerManaComponent.h"
#include "Classes/BreakerMomentumComponent.h"
#include "Classes/BreakerScrapComponent.h"
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
    // forced at 5.0s, then slot one at 5.85s, slot two at 7.6s and the
    // ultimate at 9.6s (frames 6/8/10, -BreakerScreenshots=3). The
    // probe clock (BeginPlay) and the harness clock (capture arm) skew by up
    // to ~0.3s run to run — one photograph landed BEFORE the cast it was
    // for — so each cast leads its frame by less than its flash's life
    // rather than by a margin the skew can eat. Casts
    // only what a REAL character holds — the default loadout through the one
    // grant site — so the probe can never photograph an ability a player
    // could not reach; the O176-gated abilities join the photograph when
    // their unlock rows land. Resource is granted through the loop's own
    // unmetered dev grant, not by touching the bank.
    FString ProbeClassName;
    const bool bProbeFlag = FParse::Param(FCommandLine::Get(), TEXT("BreakerAbilityProbe"));
    const bool bProbeValue = FParse::Value(FCommandLine::Get(), TEXT("BreakerAbilityProbe="), ProbeClassName);
    if (bProbeFlag || bProbeValue)
    {
        // Opt-in, save-isolated companion to the existing ability probe. Both
        // diamonds are produced by actual purchased Lead casts on live enemies.
        if (FParse::Param(FCommandLine::Get(), TEXT("BreakerLeadPair")))
        {
            TWeakObjectPtr<UBreakerAbilityComponent> WeakProbe(this);
            FTimerHandle PairHandle;
            GetWorld()->GetTimerManager().SetTimer(PairHandle, FTimerDelegate::CreateLambda([WeakProbe]()
            {
                UBreakerAbilityComponent* Probe = WeakProbe.Get();
                APawn* Pawn = Probe ? Cast<APawn>(Probe->GetOwner()) : nullptr;
                APlayerController* Controller = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
                UBreakerProgressionComponent* Progression = Probe ? Probe->GetProgression() : nullptr;
                if (!Controller || !Progression || !Pawn->HasAuthority()) return;
                Progression->DevForceClass(EBreakerClassId::Swift);
                Progression->GrantPlaytestPoints(8, 0);
                const UBreakerProgressionTree* Tree = UBreakerProgressionLibrary::GetSwiftMarksmanTree();
                for (const TCHAR* Id : { TEXT("Swift.Marksman.Steady"), TEXT("Swift.Marksman.Steady"), TEXT("Swift.Marksman.Ledger"), TEXT("Swift.Marksman.MarkEconomy"), TEXT("Swift.Marksman.Lead") })
                {
                    FText Reason;
                    if (!Progression->PurchaseNode(Tree, Id, Reason))
                    {
                        UE_LOG(LogTemp, Error, TEXT("[BreakerLeadPair] purchase %s failed: %s"), Id, *Reason.ToString());
                        return;
                    }
                }
                Progression->DevForceEquipAbility(EBreakerAbilitySlot::ClassAbilityTwo, TEXT("Swift.Lead"));
                Probe->RefreshGrants();
                TArray<AActor*> Targets;
                FVector Eye; FRotator Rotation; Controller->GetPlayerViewPoint(Eye, Rotation);
                for (TActorIterator<ABreakerEnemy> It(Probe->GetWorld()); It; ++It)
                {
                    const UBreakerCombatComponent* Combat = It->FindComponentByClass<UBreakerCombatComponent>();
                    if (!Combat || Combat->IsDead()) continue;
                    const float Distance = FVector::Distance(Eye, It->GetActorLocation());
                    if (Distance > 12000.0f) { UE_LOG(LogTemp, Log, TEXT("[BreakerLeadPair] %s outside range %.0fcm"), *It->GetName(), Distance); continue; }
                    FHitResult Hit; FCollisionQueryParams Params(SCENE_QUERY_STAT(LeadPairCapture), false, Pawn);
                    if (Probe->GetWorld()->LineTraceSingleByChannel(Hit, Eye, It->GetActorLocation(), ECC_GameTraceChannel2, Params) && Hit.GetActor() == *It) Targets.Add(*It);
                    else UE_LOG(LogTemp, Log, TEXT("[BreakerLeadPair] %s %.0fcm blocked by %s"), *It->GetName(), Distance, *GetNameSafe(Hit.GetActor()));
                }
                Targets.Sort([Eye](const AActor& A, const AActor& B) { return FVector::DistSquared(Eye, A.GetActorLocation()) < FVector::DistSquared(Eye, B.GetActorLocation()); });
                if (Targets.Num() < 2) { UE_LOG(LogTemp, Error, TEXT("[BreakerLeadPair] fewer than two visible living enemies")); return; }
                for (int32 Index = 0; Index < 2; ++Index)
                {
                    Controller->SetControlRotation((Targets[Index]->GetActorLocation() - Eye).Rotation());
                    if (Controller->PlayerCameraManager) Controller->PlayerCameraManager->UpdateCamera(0.0f);
                    if (UBreakerMomentumComponent* Momentum = Pawn->FindComponentByClass<UBreakerMomentumComponent>()) Momentum->GrantMomentum(200);
                    FGameplayTagContainer Cooldown; Cooldown.AddTag(BreakerAbilityTags::Cooldown_Class_Swift_Lead.GetTag());
                    Probe->GetAbilitySystem()->RemoveActiveEffectsWithGrantedTags(Cooldown);
                    const bool bCast = Probe->TryActivateSlot(EBreakerAbilitySlot::ClassAbilityTwo);
                    UE_LOG(LogTemp, Log, TEXT("[BreakerLeadPair] cast%d=%d target=%s"), Index + 1, bCast, *Targets[Index]->GetName());
                }
                Controller->SetControlRotation(((Targets[0]->GetActorLocation() + Targets[1]->GetActorLocation()) * 0.5f - Eye).Rotation());
                const UBreakerAbilityStateComponent* State = Pawn->FindComponentByClass<UBreakerAbilityStateComponent>();
                UE_LOG(LogTemp, Log, TEXT("[BreakerLeadPair] live marks=%d"), State ? State->GetMarkedTargets().Num() : 0);
            }), 5.3f, false);
            return;
        }
        // =Class:AbilityId photographs an UNLOCKABLE (One-U item 16 + O181):
        // the id goes into its affinity slot through LEDGER's dev writer,
        // which bypasses the unlock and nothing else — impossible loadouts
        // still refuse loudly, and the never-save guard on the character
        // keeps the whole session out of every save.
        FString ProbeAbilityName;
        if (bProbeValue)
        {
            FString ClassPart;
            if (ProbeClassName.Split(TEXT(":"), &ClassPart, &ProbeAbilityName))
            {
                ProbeClassName = ClassPart;
            }
        }
        // Bare flag probes Swift; -BreakerAbilityProbe=<Tank|Support|...>
        // probes any class DevForceClass can inhabit, casting its DEFAULT
        // loadout: slot one at 5.85s, slot two at 7.6s, the ultimate at 9.6s
        // against frames at 6/8/10 (-BreakerScreenshots=3). An unrecognised
        // name falls back to Swift LOUDLY rather than probing nothing.
        EBreakerClassId ProbeClass = EBreakerClassId::Swift;
        if (bProbeValue)
        {
            if (ProbeClassName.Equals(TEXT("Caster"), ESearchCase::IgnoreCase)) ProbeClass = EBreakerClassId::Caster;
            else if (ProbeClassName.Equals(TEXT("Gunsmith"), ESearchCase::IgnoreCase)) ProbeClass = EBreakerClassId::Gunsmith;
            else if (ProbeClassName.Equals(TEXT("Tank"), ESearchCase::IgnoreCase)) ProbeClass = EBreakerClassId::Tank;
            else if (ProbeClassName.Equals(TEXT("Support"), ESearchCase::IgnoreCase)) ProbeClass = EBreakerClassId::Support;
            else if (!ProbeClassName.Equals(TEXT("Swift"), ESearchCase::IgnoreCase))
            {
                UE_LOG(LogTemp, Warning, TEXT("[BreakerAbilityProbe] unknown class '%s'; probing Swift"), *ProbeClassName);
            }
        }
        AActor* Owner = GetOwner();
        UWorld* World = GetWorld();
        if (Owner && World && Owner->HasAuthority() && Cast<APawn>(Owner))
        {
            TWeakObjectPtr<UBreakerAbilityComponent> WeakThis(this);
            FTimerHandle ClassHandle, SlotOneHandle, SlotTwoHandle, UltimateHandle;
            World->GetTimerManager().SetTimer(ClassHandle, FTimerDelegate::CreateLambda([WeakThis, ProbeClass]()
            {
                UBreakerAbilityComponent* Probe = WeakThis.Get();
                UBreakerProgressionComponent* Progression = Probe ? Probe->GetProgression() : nullptr;
                // Force whenever the held class is not the requested one — the
                // autoplay path has already chosen Swift by 5.0s, so a gate on
                // None never fires for any other class (the first Tank probe
                // photographed a Swift because of exactly that). The dev swap
                // leaves the old loadout ids in place on purpose: resolving
                // them through the guarded read IS the stale-loadout repair
                // path, probed live.
                if (Progression && Progression->GetProgressionState().PermanentClass != ProbeClass)
                {
                    Progression->DevForceClass(ProbeClass);
                }
                if (Probe)
                {
                    Probe->RefreshGrants();
                }
            }), 5.0f, false);
            if (!ProbeAbilityName.IsEmpty())
            {
                FTimerHandle EquipHandle;
                const FName ProbeAbilityId(*ProbeAbilityName);
                World->GetTimerManager().SetTimer(EquipHandle, FTimerDelegate::CreateLambda([WeakThis, ProbeAbilityId]()
                {
                    UBreakerAbilityComponent* Probe = WeakThis.Get();
                    UBreakerProgressionComponent* Progression = Probe ? Probe->GetProgression() : nullptr;
                    const UBreakerAbilityDefinition* Definition = UBreakerAbilityDefinition::FindFallback(ProbeAbilityId);
                    if (!Progression || !Definition)
                    {
                        UE_LOG(LogTemp, Warning, TEXT("[BreakerAbilityProbe] equip failed for '%s' (progression=%d definition=%d); the default loadout stands"),
                            *ProbeAbilityId.ToString(), Progression ? 1 : 0, Definition ? 1 : 0);
                        return;
                    }
                    // Into its own affinity slot, so the schedule's per-slot
                    // casts photograph it without a second argument.
                    Progression->DevForceEquipAbility(Definition->SlotAffinity, ProbeAbilityId);
                    Probe->RefreshGrants();
                }), 5.3f, false);
            }
            auto ProbeCast = [WeakThis](EBreakerAbilitySlot Slot)
            {
                UBreakerAbilityComponent* Probe = WeakThis.Get();
                if (!Probe)
                {
                    return;
                }
                // Fill whatever loop the probed class runs; each grant is a
                // no-op for every class it does not belong to.
                if (UBreakerMomentumComponent* Momentum = Probe->GetOwner()->FindComponentByClass<UBreakerMomentumComponent>())
                {
                    Momentum->GrantMomentum(200.0f);
                }
                if (UBreakerGritComponent* Grit = Probe->GetOwner()->FindComponentByClass<UBreakerGritComponent>())
                {
                    Grit->GrantGrit(200.0f);
                }
                if (UBreakerChargeComponent* ChargeLoop = Probe->GetOwner()->FindComponentByClass<UBreakerChargeComponent>())
                {
                    ChargeLoop->GrantCharge(200.0f);
                }
                if (UBreakerManaComponent* Mana = Probe->GetOwner()->FindComponentByClass<UBreakerManaComponent>())
                {
                    Mana->GrantMana(200.0f, true);
                }
                if (UBreakerScrapComponent* Scrap = Probe->GetOwner()->FindComponentByClass<UBreakerScrapComponent>())
                {
                    Scrap->GrantScrap(200.0f);
                }
                const bool bActivated = Probe->TryActivateSlot(Slot);
                UE_LOG(LogTemp, Log, TEXT("[BreakerAbilityProbe] slot %d activate=%d"), static_cast<int32>(Slot), bActivated ? 1 : 0);
            };
            World->GetTimerManager().SetTimer(SlotOneHandle, FTimerDelegate::CreateLambda([ProbeCast]()
            {
                ProbeCast(EBreakerAbilitySlot::ClassAbilityOne);
            }), 5.85f, false);
            World->GetTimerManager().SetTimer(SlotTwoHandle, FTimerDelegate::CreateLambda([ProbeCast]()
            {
                ProbeCast(EBreakerAbilitySlot::ClassAbilityTwo);
            }), 7.6f, false);
            World->GetTimerManager().SetTimer(UltimateHandle, FTimerDelegate::CreateLambda([ProbeCast]()
            {
                ProbeCast(EBreakerAbilitySlot::Ultimate);
            }), 9.6f, false);
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
    if (Registry == EBreakerAbilitySelectionResult::AlreadyEquipped)
    {
        FText SwapFailure;
        return Progression->CanEquipAbility(Slot, AbilityId, SwapFailure)
            ? EBreakerAbilitySelectionResult::Allowed : Registry;
    }
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

UBreakerAbilityDefinition* UBreakerAbilityComponent::ResolveLoadoutDefinition(EBreakerClassId ClassId, EBreakerAbilitySlot Slot, const FBreakerAbilityLoadout& Loadout)
{
    const EBreakerAbilitySlot Slots[] = { EBreakerAbilitySlot::ClassAbilityOne, EBreakerAbilitySlot::ClassAbilityTwo, EBreakerAbilitySlot::Ultimate };
    const FName Ids[] = { Loadout.ClassAbilityOne, Loadout.ClassAbilityTwo, Loadout.Ultimate };
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(Slots); ++Index)
    {
        if (Slots[Index] != Slot) continue;
        UBreakerAbilityDefinition* Definition = ResolveDefinition(ClassId, Slot, Ids[Index]);
        if (Definition && Definition->AbilityId != Ids[Index])
        {
            // A default must not duplicate an ability explicitly moved elsewhere.
            for (int32 Other = 0; Other < UE_ARRAY_COUNT(Slots); ++Other)
            {
                if (Other != Index && Ids[Other] == Definition->AbilityId) return nullptr;
            }
        }
        return Definition;
    }
    return nullptr;
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
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(Slots); ++Index)
    {
        const EBreakerAbilitySlot Slot = Slots[Index];
        UBreakerAbilityDefinition* Definition = ResolveLoadoutDefinition(ClassId, Slot, State.AbilityLoadout);
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

void UBreakerAbilityComponent::InterruptActiveActions()
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;
    UAbilitySystemComponent* ASC = GetAbilitySystem();
    if (!ASC) return;
    TArray<FGameplayAbilitySpecHandle> Pending;
    for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
    {
        const UBreakerGameplayAbility* Ability = Cast<UBreakerGameplayAbility>(Spec.Ability);
        if (Spec.IsActive() && Ability && Ability->IsStaggerInterruptible()) Pending.Add(Spec.Handle);
    }
    // End callbacks may mutate the granted list or interrupt again. Snapshot
    // handles and re-check each current spec before invoking its cleanup.
    for (const FGameplayAbilitySpecHandle Handle : Pending)
        if (const FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromHandle(Handle); Spec && Spec->IsActive())
            ASC->CancelAbilityHandle(Handle);
}

bool UBreakerAbilityComponent::TryActivateSlot(EBreakerAbilitySlot Slot)
{
    const UBreakerCombatComponent* Combat = GetOwner() ? GetOwner()->FindComponentByClass<UBreakerCombatComponent>() : nullptr;
    if (Combat && Combat->IsStaggered()) return false;
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
    FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromHandle(Granted->Handle);
    if (Spec && Spec->Ability && Spec->Ability->IsA<UBreakerGunsmithDeployAbility>())
    {
        if (GetOwner() && !GetOwner()->HasAuthority())
        {
            // Server-only casts have no reliable local active instance. Route
            // both first and second press through one authoritative decision.
            ServerActivateSlot(Slot);
            return true;
        }
        if (Spec->IsActive())
        {
            ASC->CancelAbilityHandle(Granted->Handle);
            return true;
        }
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
    TryActivateSlot(Slot);
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
