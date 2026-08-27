#include "Items/BreakerEquipmentComponent.h"

#include "Items/BreakerItemRequirements.h"
#include "Progression/BreakerProgressionComponent.h"

#include "AbilitySystemInterface.h"
#include "AbilitySystemComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Combat/BreakerCombatComponent.h"
#include "Items/BreakerAffixLibrary.h"
#include "Items/BreakerItemBaseStats.h"
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

    BindCombatEvents();

    // The no-save path (PIE drop-ins, fixtures that never call RestoreState):
    // a character that begins play owning nothing is armed here. The save
    // path re-runs the same check at the end of RestoreState, so both births
    // of a fresh equipment state produce the same armed spawn.
    EnsureStarterKit();
}

const FName UBreakerEquipmentComponent::StarterRifleDefinitionId(TEXT("IssueRifle"));

FBreakerItemInstance UBreakerEquipmentComponent::MakeStarterRifle()
{
    // Standard, ilvl 1, no affixes: "little to no stats" taken at its word.
    // A real item rather than a phantom archetype, so SyncArchetypesToEquipment
    // arms it, the save serialises it, and the first real drop displaces it
    // through the ordinary equip path.
    FBreakerItemInstance Item;
    Item.ItemId = FGuid::NewGuid();
    Item.DefinitionId = StarterRifleDefinitionId;
    Item.Slot = EBreakerEquipSlot::Primary;
    Item.Rarity = EBreakerItemRarity::Standard;
    Item.ItemLevel = 1;
    Item.WeaponArchetype = EBreakerWeaponArchetype::Rifle;
    return Item;
}

void UBreakerEquipmentComponent::EnsureStarterKit()
{
    if (!HasAttributeAuthority()) return;
    // Owning ANYTHING disqualifies — not merely an occupied Primary. A player
    // who deliberately unequips their gun into the backpack has made a choice;
    // silently handing them a second rifle would undo it.
    if (Equipped.Num() > 0 || Backpack.Num() > 0) return;
    EquipItem(MakeStarterRifle());
}

void UBreakerEquipmentComponent::BindCombatEvents()
{
    // Health on Kill and Resource on Kill are paid at an EVENT, so they need a
    // listener rather than an attribute. Bound once and never from
    // RecalculateStats — a rebind per recalculation would pay the affix once
    // per equipment change per kill, which is the kind of bug that only shows
    // up after an hour of play. IsAlreadyBound makes that structural rather
    // than a rule the next caller has to remember.
    UBreakerCombatComponent* Combat = GetOwner() ? GetOwner()->FindComponentByClass<UBreakerCombatComponent>() : nullptr;
    if (!Combat) return;
    if (Combat->OnKillDealt.IsAlreadyBound(this, &UBreakerEquipmentComponent::HandleKillDealt)) return;
    Combat->OnKillDealt.AddDynamic(this, &UBreakerEquipmentComponent::HandleKillDealt);
}

void UBreakerEquipmentComponent::HandleKillDealt(const FBreakerHitContext& Hit)
{
    if (!HasAttributeAuthority()) return;
    UBreakerCombatComponent* Combat = GetOwner()->FindComponentByClass<UBreakerCombatComponent>();
    if (!Combat) return;

    // DoT ticks credit their applier, which means a Bleed killing something
    // five seconds after the shot would otherwise pay on-kill sustain from a
    // fight the player may already have left. It still counts — the player
    // earned that kill — but it is worth being explicit that this fires on the
    // ATTACKER's component for every kill including a DoT's.
    if (CachedStats.LifeOnKill > 0.0f)
    {
        // Through ApplyHealing, the one healing path, rather than writing
        // Health: an ability that writes Health directly is invisible to the
        // overheal clamp and to every listener, and gear is not exempt.
        Combat->ApplyHealingAmount(CachedStats.LifeOnKill, GetOwner(), FGameplayTag());
    }
    if (CachedStats.ResourceOnKill > 0.0f && Attributes)
    {
        // Through UBreakerAttributeSet::ApplyClassResource rather than
        // UBreakerCombatComponent::AddClassResource, and the difference is only
        // testability: AddClassResource writes through the GENERATED setter,
        // which ensures when there is no owning ability system, so a rig
        // without one cannot observe the grant at all. ApplyClassResource is
        // the null-safe write the attribute set exposes for exactly this, and
        // it routes through the same PreAttributeChange clamp — [Floor, Max] —
        // so the observable behaviour is identical to AddClassResource's
        // Min-against-Max, in a live game and in automation alike.
        //
        // The matching fix in UBreakerCombatComponent::AddClassResource and
        // SpendClassResource has since landed, so the whole class-resource path
        // is now exercisable with no world.
        Attributes->ApplyClassResource(Attributes->GetClassResource() + CachedStats.ResourceOnKill);
    }
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
    // Resource regeneration, applied from the COMPOSED ClassResourceRegen
    // attribute rather than from this component's own cached stat. Two changes,
    // both of them corrections rather than features:
    //
    //  - It reads the shared bucket, so the day the class loops bid their
    //    PassiveRegenPerSecond here instead of ticking it themselves, gear and
    //    class regeneration are additive with no further work and an Increased
    //    Regeneration line can multiply both. Today nothing else bids, so the
    //    composed value equals gear's flat and behaviour is bit-identical.
    //  - It writes through UBreakerAttributeSet::ApplyClassResource, not the
    //    GAS-generated SetClassResource. The generated setter ensure()s when
    //    there is no owning ability system, so a rig without one could not
    //    observe the tick at all and the Resource Regeneration affix was the
    //    last resource path unexercisable in automation. ApplyClassResource
    //    routes through the same PreAttributeChange clamp -- [Floor, Max] --
    //    so the live result is identical, including Overcast's negative floor
    //    which the old Min-against-Max here did not even know about.
    if (Attributes && GetOwner() && GetOwner()->HasAuthority())
    {
        const float RegenPerSecond = Attributes->GetClassResourceRegen();
        if (RegenPerSecond > 0.0f)
        {
            Attributes->ApplyClassResource(Attributes->GetClassResource() + RegenPerSecond * DeltaTime);
        }
    }
}

void UBreakerEquipmentComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(UBreakerEquipmentComponent, Equipped);
    DOREPLIFETIME(UBreakerEquipmentComponent, Backpack);
    DOREPLIFETIME(UBreakerEquipmentComponent, ForgeWallet);
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

    // O37: legendary and (non-legendary) Anomalous are separate equip-cap
    // axes even though every legendary rolls Anomalous rarity (O32) — a worn
    // legendary must not spend the Anomalous cap, and a second Anomalous
    // piece must not evict a legendary, or be evicted for one, as if they
    // shared a single pool. Every OTHER rarity still shares its axis with
    // itself exactly as before. This is the one predicate PreviewEquipAgainst
    // consults for cap membership, so equip-time counting, eviction-victim
    // selection and the "surviving" tally cannot disagree about which axis a
    // piece is on.
    bool SharesEquipCapAxis(const FBreakerItemInstance& A, const FBreakerItemInstance& B)
    {
        if (A.IsLegendary() || B.IsLegendary()) return A.IsLegendary() && B.IsLegendary();
        return A.Rarity == B.Rarity;
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
    // A legendary whose rule claims another slot ejects whatever is standing in
    // it. Cadence occupies both hands, so it and a Secondary cannot be worn
    // together — in EITHER direction, which is why the conflict test is
    // symmetric and why this ejects rather than refusing. Nothing in this
    // component refuses an equip; the rarity cap does not, and a rule inventing
    // a second, harsher failure mode would be a worse experience than a
    // disclosed swap.
    if (Preview.bRuleDisplaces && Preview.RuleDisplaced.IsValid())
    {
        UnequipSlot(Preview.RuleDisplaced.Slot);
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
        if (!Item.IsValid() || Item.Rarity != Rarity) continue;
        // O37: a legendary occupies its own equip-cap axis and must not be
        // counted against the (non-legendary) Anomalous cap it happens to
        // share a rarity with — CountEquippedLegendaries answers for it.
        if (Rarity == EBreakerItemRarity::Anomalous && Item.IsLegendary()) continue;
        ++Count;
    }
    return Count;
}

int32 UBreakerEquipmentComponent::CountEquippedLegendaries() const
{
    int32 Count = 0;
    for (const FBreakerItemInstance& Item : Equipped)
    {
        if (Item.IsValid() && Item.IsLegendary()) ++Count;
    }
    return Count;
}

bool UBreakerEquipmentComponent::ValidateEquipCaps(const TArray<FBreakerItemInstance>& Items, FText& OutFailureReason)
{
    int32 LegendaryCount = 0;
    int32 AnomalousCount = 0;
    int32 AberrantCount = 0;
    for (const FBreakerItemInstance& Item : Items)
    {
        if (!Item.IsValid()) continue;
        // Same axis split as SharesEquipCapAxis / CountEquippedOfRarity: a
        // legendary is never ALSO counted as a non-legendary Anomalous.
        if (Item.IsLegendary()) ++LegendaryCount;
        else if (Item.Rarity == EBreakerItemRarity::Anomalous) ++AnomalousCount;
        else if (Item.Rarity == EBreakerItemRarity::Aberrant) ++AberrantCount;
    }

    // O37: "exactly 1 equipped legendary, 1 non-legendary Anomalous, 3
    // Aberrant" (O11 reaffirmed). Equip-time displacement enforces this live,
    // so finding MORE than the cap here means a save predating O37, a
    // hand-edited fixture, or a content bug — never a normal equip.
    if (LegendaryCount > 1)
    {
        OutFailureReason = FText::Format(
            NSLOCTEXT("BreakerEquipment", "TooManyLegendaries", "{0} legendaries are equipped; the cap is 1 (O37)."),
            FText::AsNumber(LegendaryCount));
        return false;
    }
    if (AnomalousCount > 1)
    {
        OutFailureReason = FText::Format(
            NSLOCTEXT("BreakerEquipment", "TooManyAnomalous", "{0} non-legendary Anomalous items are equipped; the cap is 1 (O37)."),
            FText::AsNumber(AnomalousCount));
        return false;
    }
    if (AberrantCount > 3)
    {
        OutFailureReason = FText::Format(
            NSLOCTEXT("BreakerEquipment", "TooManyAberrant", "{0} Aberrant items are equipped; the cap is 3 (O11/O37)."),
            FText::AsNumber(AberrantCount));
        return false;
    }
    OutFailureReason = FText::GetEmpty();
    return true;
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

    // The rule displacement, resolved BEFORE the rarity cap so the two are
    // independent: the piece the slot rule ejects is not a candidate for the
    // cap's victim, because it is already leaving. The piece in the candidate's
    // own slot is excluded for the same reason it is excluded below.
    for (const FBreakerItemInstance& Existing : EquippedItems)
    {
        if (!Existing.IsValid() || Existing.Slot == Candidate.Slot) continue;
        if (!UBreakerItemRuleLibrary::RulesConflict(Candidate, Existing)) continue;
        Preview.bRuleDisplaces = true;
        Preview.RuleDisplaced = Existing;
        break;
    }

    // O37: the cap axis is SharesEquipCapAxis, not bare Rarity equality — a
    // legendary and a non-legendary Anomalous share a rarity (O32) but sit on
    // separate axes, so neither counts against, evicts, or is evicted for the
    // other. Every other rarity is unaffected: SharesEquipCapAxis falls back
    // to plain Rarity equality whenever neither side is a legendary.
    Preview.RarityLimit = EquipLimitForRarity(Candidate.Rarity);
    for (const FBreakerItemInstance& Existing : EquippedItems)
    {
        if (Existing.IsValid() && SharesEquipCapAxis(Existing, Candidate)) ++Preview.RarityCount;
    }
    if (Preview.RarityLimit == INDEX_NONE) return Preview;

    // The piece already in the candidate's slot is leaving no matter what, so
    // it cannot be the cap's victim and its departure counts against the tally
    // first. Swapping an Aberrant helmet for another Aberrant helmet therefore
    // ejects nothing extra even at 3/3.
    int32 Surviving = Preview.RarityCount;
    if (InSlot && SharesEquipCapAxis(*InSlot, Candidate)) --Surviving;
    // A piece the SLOT RULE already ejects is leaving too, so it cannot also be
    // the cap's victim and its departure counts against the tally the same way.
    // Without this, equipping Cadence over an Anomalous Secondary at a cap of
    // one would eject the Secondary AND name a second piece to die for a cap
    // the first ejection had already satisfied.
    if (Preview.bRuleDisplaces && SharesEquipCapAxis(Preview.RuleDisplaced, Candidate)) --Surviving;
    if (Surviving < Preview.RarityLimit) return Preview;

    // Over the cap: the WEAKEST equipped piece ON THE SAME AXIS leaves.
    // Weakest is lowest item level, ties broken by wear order (slot index),
    // which makes the choice deterministic — the player is told which piece
    // dies and that is the piece that dies.
    Preview.bExceedsRarityLimit = true;
    const FBreakerItemInstance* Weakest = nullptr;
    for (const FBreakerItemInstance& Existing : EquippedItems)
    {
        if (!Existing.IsValid() || !SharesEquipCapAxis(Existing, Candidate)) continue;
        if (Existing.Slot == Candidate.Slot) continue;
        if (Preview.bRuleDisplaces && Existing.Slot == Preview.RuleDisplaced.Slot) continue;
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
    // A fresh character's save carries two empty containers (the roster
    // writes the save before the first session ever runs), so the fresh-save
    // path lands here with nothing — the second birth of a fresh equipment
    // state. EnsureStarterKit refuses to touch anyone who owns anything, so
    // a veteran's load is byte-identical to before.
    EnsureStarterKit();
}

bool UBreakerEquipmentComponent::EquipFromBackpack(const FGuid& ItemId)
{
    if (!HasAttributeAuthority()) return false;
    const int32 Index = Backpack.IndexOfByPredicate([&ItemId](const FBreakerItemInstance& Existing) { return Existing.ItemId == ItemId; });
    if (Index == INDEX_NONE) return false;
    // The One-AA gate, at the player-facing entry and NOT inside EquipItem
    // (the mechanism stays ungated for the systems and rigs that compose
    // loadouts directly). A character the component cannot find a level for
    // REFUSES rather than passes — a gate that passes on missing data is a
    // silent bypass waiting for the day a component goes missing in the real
    // game, which is One-AA's own warning.
    const UBreakerProgressionComponent* Progression = GetOwner()
        ? GetOwner()->FindComponentByClass<UBreakerProgressionComponent>() : nullptr;
    if (!Progression)
    {
        UE_LOG(LogTemp, Warning, TEXT("EquipFromBackpack refused: no progression component to read a character level from (the gate refuses rather than passes on missing data)."));
        return false;
    }
    const int32 CharacterLevel = Progression->GetProgressionState().CharacterLevel;
    if (!BreakerItemRequirements::CanEquipAtLevel(Backpack[Index].ItemLevel, CharacterLevel))
    {
        UE_LOG(LogTemp, Display, TEXT("EquipFromBackpack refused: item level %d requires character level %d, character is %d (One-AA)."),
            Backpack[Index].ItemLevel, BreakerItemRequirements::RequiredLevelFor(Backpack[Index].ItemLevel), CharacterLevel);
        return false;
    }
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

// ---------------------------------------------------------------------------
// THE FORGE
// ---------------------------------------------------------------------------
// Authority-checked, wallet-owning wrappers over the pure functions in
// UBreakerForgeLibrary. Everything that can be arithmetic is arithmetic and
// lives there; this layer only owns the wallet, finds the item, and decides
// whether the change has to re-fold attributes.
//
// KNOWN GAP, stated rather than hidden: there is no Forge UI. UI/ is another
// lane's directory this pass, so these are reachable from Blueprint, from a
// console exec, and from automation, and not yet from the inventory screen.
// The mechanic is real; the button is not.

void UBreakerEquipmentComponent::RestoreForgeWallet(const FBreakerForgeWallet& NewWallet)
{
    if (!HasAttributeAuthority()) return;
    ForgeWallet = NewWallet;
    OnEquipmentChanged.Broadcast();
}

void UBreakerEquipmentComponent::GrantForgeCurrency(int32 Amount)
{
    if (!HasAttributeAuthority()) return;
    ForgeWallet.Add(Amount);
    OnEquipmentChanged.Broadcast();
}

void UBreakerEquipmentComponent::CreditForgeCurrency(const FBreakerForgeWallet& Yield)
{
    if (!HasAttributeAuthority()) return;
    const int32 Amount = Yield.Get();
    // A zero-Riftglass kill (a Trash roll can land on 0) is common and skipped
    // rather than added, so no change broadcast fires on a no-op.
    if (Amount == 0) return;
    ForgeWallet.Add(Amount);
    OnEquipmentChanged.Broadcast();
}

bool UBreakerEquipmentComponent::SalvageFromBackpack(const FGuid& ItemId)
{
    if (!HasAttributeAuthority()) return false;
    const int32 Index = Backpack.IndexOfByPredicate([&ItemId](const FBreakerItemInstance& Existing) { return Existing.ItemId == ItemId; });
    if (Index == INDEX_NONE) return false;

    const FBreakerForgeWallet Yield = UBreakerForgeLibrary::SalvageValue(Backpack[Index]);
    Backpack.RemoveAt(Index);
    ForgeWallet.Add(Yield.Get());
    OnEquipmentChanged.Broadcast();
    return true;
}

int32 UBreakerEquipmentComponent::SalvageBackpackBelowRarity(EBreakerItemRarity MinimumKept)
{
    if (!HasAttributeAuthority()) return 0;
    // Shares IsBelowRarity with the plain discard, so the count the
    // confirmation modal prints is the count that melts. Salvage and discard
    // are deliberately different verbs: one pays, one does not, and both
    // destroy exactly the same set.
    int32 Salvaged = 0;
    for (int32 Index = Backpack.Num() - 1; Index >= 0; --Index)
    {
        if (!IsBelowRarity(Backpack[Index], MinimumKept)) continue;
        ForgeWallet.Add(UBreakerForgeLibrary::SalvageValue(Backpack[Index]).Get());
        Backpack.RemoveAt(Index);
        ++Salvaged;
    }
    if (Salvaged > 0) OnEquipmentChanged.Broadcast();
    return Salvaged;
}

FBreakerItemInstance* UBreakerEquipmentComponent::FindHeldItem(const FGuid& ItemId, bool& bOutEquipped)
{
    bOutEquipped = true;
    if (FBreakerItemInstance* Worn = Equipped.FindByPredicate([&ItemId](const FBreakerItemInstance& Existing) { return Existing.ItemId == ItemId; }))
    {
        return Worn;
    }
    bOutEquipped = false;
    return Backpack.FindByPredicate([&ItemId](const FBreakerItemInstance& Existing) { return Existing.ItemId == ItemId; });
}

void UBreakerEquipmentComponent::OnHeldItemMutated(bool bWasEquipped)
{
    // A worn item that changed has to re-fold, because its affixes are inside
    // the submitted contribution. A backpack item only has to repaint — but
    // recalculating anyway is cheap and is one branch fewer to get wrong.
    if (bWasEquipped) RecalculateStats();
    OnEquipmentChanged.Broadcast();
}

EBreakerForgeResult UBreakerEquipmentComponent::TemperItem(const FGuid& ItemId, int32 AffixIndex, bool bIsAtForge)
{
    if (!HasAttributeAuthority()) return EBreakerForgeResult::InvalidItem;
    bool bEquipped = false;
    FBreakerItemInstance* Item = FindHeldItem(ItemId, bEquipped);
    if (!Item) return EBreakerForgeResult::InvalidItem;
    const EBreakerForgeResult Result = UBreakerForgeLibrary::Temper(*Item, AffixIndex, ForgeWallet, bIsAtForge);
    if (Result == EBreakerForgeResult::Success) OnHeldItemMutated(bEquipped);
    return Result;
}

EBreakerForgeResult UBreakerEquipmentComponent::ReforgeItem(const FGuid& ItemId, bool bIsAtForge)
{
    if (!HasAttributeAuthority()) return EBreakerForgeResult::InvalidItem;
    bool bEquipped = false;
    FBreakerItemInstance* Item = FindHeldItem(ItemId, bEquipped);
    if (!Item) return EBreakerForgeResult::InvalidItem;
    // Seeded from the item's own GUID plus the wallet state, so a reforge is
    // deterministic for a given (item, wallet) and cannot be save-scummed by
    // repeating the call — the wallet moved, so the next roll differs.
    const int32 Seed = GetTypeHash(Item->ItemId) ^ (ForgeWallet.Get() * 7919);
    const EBreakerForgeResult Result = UBreakerForgeLibrary::Reforge(*Item, ForgeWallet, bIsAtForge, Seed);
    if (Result == EBreakerForgeResult::Success) OnHeldItemMutated(bEquipped);
    return Result;
}

EBreakerForgeResult UBreakerEquipmentComponent::AttuneItem(const FGuid& ItemId, bool bIsAtForge)
{
    if (!HasAttributeAuthority()) return EBreakerForgeResult::InvalidItem;
    bool bEquipped = false;
    FBreakerItemInstance* Item = FindHeldItem(ItemId, bEquipped);
    if (!Item) return EBreakerForgeResult::InvalidItem;
    const int32 Seed = GetTypeHash(Item->ItemId) ^ (ForgeWallet.Get() * 104729);
    const EBreakerForgeResult Result = UBreakerForgeLibrary::Attune(*Item, ForgeWallet, bIsAtForge, Seed);
    if (Result == EBreakerForgeResult::Success) OnHeldItemMutated(bEquipped);
    return Result;
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

void UBreakerEquipmentComponent::DevGrantLegendaries(int32 ItemLevel)
{
    if (!HasAttributeAuthority()) return;
    const int32 SafeLevel = FMath::Max(1, ItemLevel);
    static int32 LegendaryGrantCounter = 0;
    ++LegendaryGrantCounter;
    // Into the BACKPACK, not equipped: only one Anomalous piece may be worn, so
    // equipping all three would silently eject two and the grant would look
    // broken. Handing over three and making the player choose is the mechanic.
    for (const FBreakerLegendaryDefinition& Definition : UBreakerItemRuleLibrary::GetLegendaries())
    {
        const int32 Seed = LegendaryGrantCounter * 7919 + GetTypeHash(Definition.LegendaryId) + SafeLevel * 31;
        AddToBackpack(UBreakerLootLibrary::RollLegendary(Definition.LegendaryId, SafeLevel, Seed));
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

    // RULES FIRST. Every rewrite below changes how the affixes are READ, so the
    // resolved set has to exist before a single line is folded. Resolving is
    // order-independent (flags OR, caps take the loosest value), so which slot
    // carried the rewrite cannot change the answer.
    const FBreakerItemRuleSet Rules = UBreakerItemRuleLibrary::ResolveRules(Items);

    // Sized off the enum so a new stat target never silently overruns these.
    constexpr int32 TargetCount = static_cast<int32>(EBreakerStatTarget::Count);
    float FlatByTarget[TargetCount] = {};
    float IncreasedByTarget[TargetCount] = {};
    // What the conditional lines are worth right now and what they would be
    // worth with everything satisfied. Display figures only; the live half is
    // already inside IncreasedByTarget.
    float ActiveConditionalPercent = 0.0f;
    float PotentialConditionalPercent = 0.0f;
    // The condition test, with the rewrites folded in. One lambda, consulted
    // once per conditional line, so UNBOUND and DEADFALL cannot end up applied
    // in one place and forgotten in another.
    auto BreakerConditionSatisfied = [&Conditions, &Rules](EBreakerBuildCondition Condition)
    {
        // UNBOUND (and nothing else) makes the predicate itself vacuous.
        if (Rules.bAllConditionsSatisfied) return true;
        if (Conditions.IsActive(Condition)) return true;
        // DEADFALL is narrower on purpose: it REDIRECTS the airborne family
        // onto the two grounded traversal states rather than freeing every
        // conditional line. A legendary that was strictly better than the
        // generic Anomalous rewrite would make the generic one dead content.
        if (Rules.bAirborneAlsoGroundTraversal && Condition == EBreakerBuildCondition::Airborne)
        {
            return Conditions.IsActive(EBreakerBuildCondition::Sliding)
                || Conditions.IsActive(EBreakerBuildCondition::WallRiding);
        }
        return false;
    };

    for (const FBreakerItemInstance& Item : Items)
    {
        // PROLIFIC resolves THIS item's affixes one tier better. Per item, not
        // per wearer: folding it into the rule set would leak the uplift onto
        // every other piece the character is wearing.
        const int32 TierUplift = UBreakerItemRuleLibrary::TierUpliftForItem(Item);
        for (const FBreakerRolledAffix& Rolled : Item.Affixes)
        {
            const FBreakerAffixDefinition* Definition = UBreakerAffixLibrary::FindAffix(Pool, Rolled.AffixId);
            if (!Definition) continue;

            float Value = Rolled.Value;
            if (TierUplift > 0)
            {
                // Scaled by the RATIO between the two tiers rather than
                // re-derived at the better tier, so a lucky in-band roll is
                // carried upward instead of being flattened to the floor of the
                // new tier. T1 -> T0 is x2.2 and T0 -> T-1 is x1.636 (O29's
                // re-sited spikes — UBreakerAffixLibrary::TierSpikeT0Multiplier
                // / TierSpikeTopMultiplier, both against ValueAtT1; the pre-O29
                // ratios were x1.4 and x1.286), straight off the authored
                // spike; an affix already at T-1 has nowhere to go and the
                // ratio is exactly 1.
                const int32 UpliftedTier = FMath::Max(Rolled.Tier - TierUplift, -1);
                const float FromTier = UBreakerAffixLibrary::ValueForTier(*Definition, Rolled.Tier);
                const float ToTier = UBreakerAffixLibrary::ValueForTier(*Definition, UpliftedTier);
                if (FromTier > UE_KINDA_SMALL_NUMBER) Value *= ToTier / FromTier;
            }

            if (Definition->IsConditional())
            {
                PotentialConditionalPercent += Value;
                // A conditional line whose condition is false contributes
                // NOTHING — not a reduced amount, not a separate multiplier.
                // That is what keeps the locked one-bucket rule intact while
                // the bucket's contents change with the movement state.
                if (!BreakerConditionSatisfied(Definition->Condition)) continue;
                ActiveConditionalPercent += Value;
            }
            const int32 Target = static_cast<int32>(Definition->StatTarget);
            if (Definition->StatBucket == EBreakerStatBucket::Flat) FlatByTarget[Target] += Value;
            else if (Definition->StatBucket == EBreakerStatBucket::IncreasedPercent) IncreasedByTarget[Target] += Value;
            else
            {
                // Audit item 2, the Items-side twin of the Progression fix:
                // gear must never author EBreakerStatBucket::MorePercent (More
                // multipliers are reserved for trees/Anomalous rewrites, and
                // RiorsEdge.Items.Affixes.Breadth pins that no pool entry does)
                // — but before this, a definition that DID would silently drop
                // here with no signal, same as the Progression aggregator did.
                // Loud once per offending affix id rather than silent forever.
                static TSet<FName> WarnedOnceAffixIds;
                if (!WarnedOnceAffixIds.Contains(Definition->AffixId))
                {
                    WarnedOnceAffixIds.Add(Definition->AffixId);
                    UE_LOG(LogTemp, Warning,
                        TEXT("[BreakerEquipment] affix '%s' is authored with StatBucket MorePercent, which gear never composes — this effect is silently dropped."),
                        *Definition->AffixId.ToString());
                }
            }
        }
    }

    // Every conditional damage line and the unconditional one share the single
    // additive Increased bucket for outgoing damage. Summed here once so both
    // the display figure below and the contribution submit the same number.
    //
    // O54: this sum is the WEAPON-delivered pool. The conditional family rides
    // it because every one of those lines is a weapon-state condition — being
    // airborne, sliding, wall-riding, in Redline, recently dashed — and the two
    // bucket-crossing rewrites below feed it because Added Damage and Fire Rate
    // are both weapon-side. The ability pool and the shared pool are summed
    // separately just below; nothing here is silently doing double duty.
    float TotalIncreasedDamagePercent =
        IncreasedByTarget[static_cast<int32>(EBreakerStatTarget::WeaponDamage)]
        + IncreasedByTarget[static_cast<int32>(EBreakerStatTarget::AirborneDamage)]
        + IncreasedByTarget[static_cast<int32>(EBreakerStatTarget::SlidingDamage)]
        + IncreasedByTarget[static_cast<int32>(EBreakerStatTarget::WallRideDamage)]
        + IncreasedByTarget[static_cast<int32>(EBreakerStatTarget::RedlineDamage)]
        + IncreasedByTarget[static_cast<int32>(EBreakerStatTarget::RecentlyDashedDamage)];

    // ---- The two bucket-CROSSING rewrites ---------------------------------
    // Both add to the SAME single additive bucket every other Increased
    // percentage lands in. Neither is a More, and that is the point: they take
    // a number the player already owns in one lane and make it count in a
    // second, which changes what is worth stacking without adding a multiplier
    // against the O3 budget.
    //
    // OVERFLOW: each point of Added Damage also grants 1% Increased Damage.
    // Added Damage is authored in percentage points of base weapon damage, so
    // "a point is a percent" is a real exchange rate rather than a coincidence.
    if (Rules.bAddedDamageAlsoIncreased)
    {
        TotalIncreasedDamagePercent += FlatByTarget[static_cast<int32>(EBreakerStatTarget::AddedDamage)];
    }
    // CADENCE: half of Fire Rate also counts as Increased Damage. Fire Rate is
    // a peer of Weapon Damage that lands on a different attribute, so the two
    // normally cannot compound at all; this is the one item that makes them.
    if (Rules.FireRateToIncreasedDamage > 0.0f)
    {
        TotalIncreasedDamagePercent += IncreasedByTarget[static_cast<int32>(EBreakerStatTarget::FireRate)]
            * Rules.FireRateToIncreasedDamage;
    }

    // O54's other two pools. The ability line is its own additive bucket; the
    // shared line is added to BOTH lanes at the contribution, which is what
    // "feeds both" means and why it is not summed into either total here.
    const float TotalIncreasedAbilityDamagePercent =
        IncreasedByTarget[static_cast<int32>(EBreakerStatTarget::AbilityDamage)];
    const float TotalSharedDamagePercent =
        IncreasedByTarget[static_cast<int32>(EBreakerStatTarget::SharedDamage)];

    auto Increased = [&IncreasedByTarget](EBreakerStatTarget Target)
    {
        return 1.0f + IncreasedByTarget[static_cast<int32>(Target)] / 100.0f;
    };

    FBreakerEquipmentStats Stats;
    Stats.BonusHealth = FlatByTarget[static_cast<int32>(EBreakerStatTarget::Health)];
    // The armour archetypes' base pools, summed per piece. Derived at read
    // time from slot, level and archetype — never stored on the instance —
    // so a base retune is an edit to BreakerItemBaseStats.h, not a save
    // migration.
    for (const FBreakerItemInstance& Item : Items)
    {
        Stats.BaseHealthFromGear += BreakerItemBase::BaseLifeOf(Item);
        Stats.BaseShieldFromGear += BreakerItemBase::BaseShieldOf(Item);
    }
    Stats.ResourceRegenPerSecond = FlatByTarget[static_cast<int32>(EBreakerStatTarget::ResourceRegen)];
    Stats.BonusMaxResource = FlatByTarget[static_cast<int32>(EBreakerStatTarget::MaxResource)];
    Stats.MoveSpeedMultiplier = Increased(EBreakerStatTarget::MoveSpeed);
    Stats.DropChancePercent = IncreasedByTarget[static_cast<int32>(EBreakerStatTarget::DropChance)];
    // RELENTLESS rewrites this cap. The clamp was a bare 60.0f literal; it is
    // now the one number the rule set publishes, and the resolved value is
    // published on the stats so the inventory can show a raised cap instead of
    // a figure that mysteriously stops moving.
    Stats.PhysicalDamageReductionCap = Rules.PhysicalDamageReductionCap;
    Stats.PhysicalDamageReductionPercent = FMath::Min(
        IncreasedByTarget[static_cast<int32>(EBreakerStatTarget::PhysicalDamageReduction)], Rules.PhysicalDamageReductionCap);
    // The other two legs of the defense triad, capped at their own named
    // ceilings the same way. Both are read from GetStats() at exactly one
    // consumer each (the status component's application roll; the combat
    // component's Elemental-family branch) rather than submitted as
    // attributes — the Physical DR pattern, kept deliberately, so the caps
    // here are the only clamp anyone has to audit. Clamped at zero from
    // below too: no authored downside exists for either yet, but a negative
    // avoidance CHANCE would be a probability nobody can roll.
    Stats.AilmentAvoidanceChancePercent = FMath::Clamp(
        IncreasedByTarget[static_cast<int32>(EBreakerStatTarget::AilmentAvoidance)],
        0.0f, FBreakerEquipmentStats::AilmentAvoidanceCapPercent);
    Stats.ElementalResistancePercent = FMath::Clamp(
        IncreasedByTarget[static_cast<int32>(EBreakerStatTarget::ElementalDamageReduction)],
        0.0f, FBreakerEquipmentStats::ElementalResistanceCapPercent);
    Stats.CriticalChanceBonus = FlatByTarget[static_cast<int32>(EBreakerStatTarget::CriticalChance)] / 100.0f;
    Stats.CriticalMultiplierBonus = FlatByTarget[static_cast<int32>(EBreakerStatTarget::CriticalDamage)] / 100.0f;
    Stats.SlideSpeedMultiplier = Increased(EBreakerStatTarget::SlideSpeed);
    // DEADFALL's bill. An ordinary NEGATIVE Increased percentage into the same
    // additive bucket, never a sub-1.0 More: affixes and items must not author
    // More multipliers (RiorsEdge.Items.Affixes.Breadth pins it), and a
    // downside is not an exemption from the locked aggregation rule.
    const float AirControlPercent =
        IncreasedByTarget[static_cast<int32>(EBreakerStatTarget::AirControl)] + Rules.AirControlPercentDelta;
    Stats.AirControlMultiplier = 1.0f + AirControlPercent / 100.0f;
    Stats.DashCooldownMultiplier = 1.0f / Increased(EBreakerStatTarget::DashCooldownReduction);
    // The gear-only display multiplier now includes whatever conditional lines
    // are live, because that is what the player's damage actually is at this
    // instant. The two figures below break it down for the tooltip.
    Stats.WeaponDamageMultiplier = 1.0f + TotalIncreasedDamagePercent / 100.0f;
    Stats.AddedDamagePercent = FlatByTarget[static_cast<int32>(EBreakerStatTarget::AddedDamage)];
    Stats.ActiveConditionalDamagePercent = ActiveConditionalPercent;
    Stats.PotentialConditionalDamagePercent = PotentialConditionalPercent;

    // ---- The non-damage breadth pass --------------------------------------
    Stats.BonusArmour = FlatByTarget[static_cast<int32>(EBreakerStatTarget::Armour)];
    Stats.LifeOnKill = FlatByTarget[static_cast<int32>(EBreakerStatTarget::LifeOnKill)];
    Stats.ResourceOnKill = FlatByTarget[static_cast<int32>(EBreakerStatTarget::ResourceOnKill)];
    Stats.DamageOverTimeMultiplier = Increased(EBreakerStatTarget::DamageOverTime);

    // OVERRUN rewrites what resource regeneration IS. Gear regen is a flat
    // per-second trickle ticked on the server, which pays a player standing
    // still exactly as well as one in a fight; this deletes the trickle and
    // pays triple for fast traversal, which is the only line in the game that
    // makes the class-resource loop a movement question.
    //
    // Applied to the composed number rather than per affix, because the rule is
    // about the wearer's regeneration, not about any one roll.
    if (Rules.bRegenGatedOnTraversal)
    {
        const bool bTraversing = Conditions.IsActive(EBreakerBuildCondition::Airborne)
            || Conditions.IsActive(EBreakerBuildCondition::Sliding)
            || Conditions.IsActive(EBreakerBuildCondition::WallRiding);
        Stats.ResourceRegenPerSecond *= bTraversing ? Rules.TraversalRegenMultiplier : 0.0f;
    }

    // Which rewrites are in force, for the card and for the tests.
    for (const FBreakerItemInstance& Item : Items)
    {
        if (Item.HasRule()) Stats.ActiveRules.AddUnique(Item.Rule);
    }

    if (OutContribution)
    {
        // Built from the raw buckets, not from the composed multipliers above:
        // the Increased percentages have to reach the attribute set unmerged so
        // they can join the tree's percentages in ONE additive bucket per stat.
        // Gear authors no More multipliers — those are reserved for trees and
        // Anomalous items (O3).
        OutContribution->Reset();
        // Base health rides the same lane as the Health affix: one flat
        // bucket, folded with the tree's, so a Life piece and a Health line
        // compose instead of stacking through different doors.
        OutContribution->AddFlat(EBreakerAggregatedAttribute::MaxHealth, Stats.BonusHealth + Stats.BaseHealthFromGear);
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
        // Deadfall's negative is inside AirControlPercent, so the tree's air
        // control and the legendary's bill meet in the SAME additive bucket
        // rather than the item multiplying the tree's result down.
        OutContribution->AddIncreasedPercent(EBreakerAggregatedAttribute::AirControlMultiplier, AirControlPercent);
        OutContribution->AddIncreasedPercent(EBreakerAggregatedAttribute::DashCooldownReduction, IncreasedByTarget[static_cast<int32>(EBreakerStatTarget::DashCooldownReduction)]);
        OutContribution->AddIncreasedPercent(EBreakerAggregatedAttribute::FireRateMultiplier, IncreasedByTarget[static_cast<int32>(EBreakerStatTarget::FireRate)]);
        // Efficiency is authored as a percentage of cost REMOVED, and the
        // attribute is the cost SCALE, so the sign flips exactly once, here.
        // Negating at the contribution keeps both gear and any future tree node
        // in the same additive bucket -- which is the whole reason the attribute
        // stores a scale rather than a reduction.
        OutContribution->AddIncreasedPercent(EBreakerAggregatedAttribute::ResourceCostMultiplier,
            -IncreasedByTarget[static_cast<int32>(EBreakerStatTarget::ResourceEfficiency)]);
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
        // O54: the ability pool's own bucket, and the shared pool bidding into
        // both lanes from one rolled line. A weapon hit composes
        // (1 + (Weapon + Shared)/100); an ability composes
        // (1 + (Ability + Shared)/100). Neither reads the other's specific
        // pool, so no line is counted twice inside one hit.
        OutContribution->AddIncreasedPercent(EBreakerAggregatedAttribute::AbilityDamageMultiplier, TotalIncreasedAbilityDamagePercent);
        OutContribution->AddSharedIncreasedDamage(TotalSharedDamagePercent);
        // Added Damage lands in the FLAT lane of the same attribute, whose base
        // is 1.0. Flat sums first, so it is multiplied by the Increased bucket
        // rather than added after it — which is precisely why a flat line and an
        // increased line are two different decisions instead of one.
        OutContribution->AddFlat(EBreakerAggregatedAttribute::DamageMultiplier,
            FlatByTarget[static_cast<int32>(EBreakerStatTarget::AddedDamage)] / 100.0f);

        // Flat armour, joining the fold for the first time. Its consumer is
        // UBreakerCombatComponent::GetEffectiveArmor(), which is the same route
        // the flat armour STRIPPERS already take — so a point of gear armour
        // and a point of authored armour are stripped identically, which is
        // the property that would have been lost had gear armour been given a
        // private path of its own.
        OutContribution->AddFlat(EBreakerAggregatedAttribute::Armor,
            FlatByTarget[static_cast<int32>(EBreakerStatTarget::Armour)]);
        // Damage over time, into the attribute every DoT snapshots at
        // application. Six skill nodes already bid here; gear now does too, in
        // the same additive bucket rather than beside it.
        OutContribution->AddIncreasedPercent(EBreakerAggregatedAttribute::DamageOverTimeMultiplier,
            IncreasedByTarget[static_cast<int32>(EBreakerStatTarget::DamageOverTime)]);

        // Resource Regeneration, bid FLAT into the one regeneration bucket.
        // Bid from Stats rather than from FlatByTarget because Overrun's rule
        // rewrite (triple while traversing, zero otherwise) has already been
        // applied to Stats by this point, and a per-ITEM rule must resolve
        // before the shared bucket sees the number -- exactly as the tier
        // uplift does.
        //
        // Before this the equipment component ticked the resource itself while
        // the class loop ticked its own PassiveRegenPerSecond, so gear regen and
        // class regen composed by bare addition in two different files. See
        // EBreakerAggregatedAttribute::ClassResourceRegen for why that had to
        // stop now that Mana starts full and drains.
        OutContribution->AddFlat(EBreakerAggregatedAttribute::ClassResourceRegen,
            Stats.ResourceRegenPerSecond);

        // Life on Kill and Resource on Kill submit NOTHING here on purpose.
        // They are amounts paid at an event, not values an attribute can hold,
        // so an attribute for them would be a number nobody reads. They reach
        // gameplay through HandleKillDealt below.
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

    // O106 REWRITE: gear owns the BASE of MaxShield — Shield-archetype
    // pieces are the pool's source, and MaxShield stays deliberately outside
    // the aggregation pass (a single system owns it outright; that system is
    // now this one). The class conversion ceilings (Charge MD5/MD9, Grit
    // §T1, both 25%-of-max-health) are RAISE-ONLY per their own tick, so
    // this hard set can dip below a ceiling for at most one class tick
    // before it is re-raised; a non-Tank/Support has no ceiling and reads
    // the gear base exactly. Current shield clamps into the new cap so
    // unequipping a Shield piece can never leave a bar above its maximum.
    Attributes->ApplyMaxShield(CachedStats.BaseShieldFromGear);
    if (Attributes->GetShield() > Attributes->GetMaxShield())
    {
        Attributes->ApplyShield(Attributes->GetMaxShield());
    }
}

void UBreakerEquipmentComponent::OnRep_Equipped()
{
    CachedStats = AggregateStats(Equipped, &CachedContribution, ActiveConditions);
    OnEquipmentChanged.Broadcast();
}
