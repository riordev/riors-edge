#include "Items/BreakerAffixLibrary.h"

#include "Data/BreakerDataFile.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

float UBreakerAffixLibrary::ValueForTier(const FBreakerAffixDefinition& Affix, int32 Tier)
{
    const int32 ClampedTier = FMath::Clamp(Tier, TopTier, WorstTier);
    if (ClampedTier == 0) return Affix.ValueAtT1 * TierSpikeT0Multiplier;
    if (ClampedTier == TopTier) return Affix.ValueAtT1 * TierSpikeTopMultiplier;

    // Position along the ladder: 0 at the worst tier, 1 at the best normal one.
    const float Span = static_cast<float>(WorstTier - BestNormalTier);
    const float Position = (static_cast<float>(WorstTier - ClampedTier)) / Span;
    const float Shaped = FMath::Pow(FMath::Clamp(Position, 0.0f, 1.0f), TierCurveExponent);

    // Geometric between the two authored anchors — see the derivation on the
    // declaration. The geometric form needs a positive floor and a ceiling
    // above it; an affix authored with a zero or inverted band falls back to a
    // shaped LERP, which is still monotonic and still back-loaded rather than
    // producing a NaN in the damage pipeline. Nothing in the slice pool takes
    // that branch, and RiorsEdge.Items.TierLadder covers it so the first affix
    // that does is not a silent zero.
    if (Affix.ValueAtT12 <= UE_KINDA_SMALL_NUMBER || Affix.ValueAtT1 <= Affix.ValueAtT12)
    {
        return FMath::Lerp(Affix.ValueAtT12, Affix.ValueAtT1, Shaped);
    }
    return Affix.ValueAtT12 * FMath::Pow(Affix.ValueAtT1 / Affix.ValueAtT12, Shaped);
}

int32 UBreakerAffixLibrary::BestTierForItemLevel(int32 ItemLevel)
{
    // TWO SLOPES, not one. Owner ruling after playtesting O29: "the item level
    // tier capping at 8 might make for awkward feeling progression, let's bring
    // that to 6."
    //
    // A single slope of one tier per 10 levels put the CHARACTER CAP at T8 --
    // eight of twelve tiers, meaning a player who finished the levelling game
    // had seen only a third of the ladder and every tier they had met was in
    // its shallow, back-loaded lower half. The ladder was authored for the
    // endgame and the levelling game paid for it.
    //
    // So the levelling band is steeper than the endgame band:
    //
    //   ilvl   1 -> 50    T12 -> T6    ~8.2 levels per tier  (the campaign)
    //   ilvl  50 -> 120   T6  -> T1    ~14 levels per tier   (the chase)
    //
    // Reaching T6 by the character cap means a levelling player crosses half
    // the ladder and finishes standing on the shoulder of the curve, where the
    // steps start to be worth something. The endgame is longer per tier BECAUSE
    // each of those tiers is worth more, not as a tax.
    //
    // O2 PLACEHOLDER: both the anchor (T6 at the character cap) and the two
    // slopes are shape rather than balance.
    const int32 Clamped = FMath::Clamp(ItemLevel, 1, MaxItemLevel);
    if (Clamped <= CharacterCapItemLevel)
    {
        // T12 at ilvl 1 down to T6 at ilvl 50. Six steps across 49 levels.
        const int32 Steps = ((Clamped - 1) * (WorstTier - TierAtCharacterCap)) / (CharacterCapItemLevel - 1);
        return FMath::Clamp(WorstTier - Steps, TierAtCharacterCap, WorstTier);
    }
    // T6 at ilvl 50 down to T1 at ilvl 120. Five steps across 70 levels.
    const int32 Steps = ((Clamped - CharacterCapItemLevel) * (TierAtCharacterCap - BestNormalTier))
        / (MaxItemLevel - CharacterCapItemLevel);
    return FMath::Clamp(TierAtCharacterCap - Steps, BestNormalTier, TierAtCharacterCap);
}

int32 UBreakerAffixLibrary::TierCapForRarity(EBreakerItemRarity Rarity)
{
    // Re-derived for the 12-tier ladder; the derivation is on the declaration.
    switch (Rarity)
    {
    case EBreakerItemRarity::Standard: return 4;   // O2 PLACEHOLDER
    case EBreakerItemRarity::Uncommon: return 2;   // O2 PLACEHOLDER
    default: return TopTier;
    }
}

void UBreakerAffixLibrary::AffixCountRangeForRarity(EBreakerItemRarity Rarity, int32& OutMinimum, int32& OutMaximum)
{
    switch (Rarity)
    {
    case EBreakerItemRarity::Standard:    OutMinimum = 1; OutMaximum = 2; break;
    case EBreakerItemRarity::Uncommon:    OutMinimum = 2; OutMaximum = 3; break;
    case EBreakerItemRarity::Exceptional: OutMinimum = 3; OutMaximum = 5; break;
    case EBreakerItemRarity::Aberrant:    OutMinimum = 4; OutMaximum = 6; break;
    case EBreakerItemRarity::Anomalous:   OutMinimum = 5; OutMaximum = 6; break;
    default:                              OutMinimum = 1; OutMaximum = 1; break;
    }
}

// ---------------------------------------------------------------------------
// THE LOADER — Data/affixes.json into the four pools, the leans and the caps.
// ---------------------------------------------------------------------------
// The file is the library. Every row carries a "pool" word that decides which
// array it lands in; the exclusivity the special pools rely on (the generic
// loop and the Forge never iterate Aberrant, Anomalous, downside or elemental
// rows) is therefore a property of that one word, and the validator is what
// keeps the word honest. A row that fails any check below fails the WHOLE
// load: the pools come back empty behind an ensure, because a library that
// dropped one row and served the rest would roll items that quietly cannot
// carry a line the design authored.
namespace
{
    using BreakerDataFile::FBreakerDataErrors;

    const TCHAR* const BreakerAffixPoolSlice = TEXT("slice");
    const TCHAR* const BreakerAffixPoolAberrant = TEXT("aberrant");
    const TCHAR* const BreakerAffixPoolAnomalous = TEXT("anomalous");
    const TCHAR* const BreakerAffixPoolDownside = TEXT("downside");
    const TCHAR* const BreakerAffixPoolElemental = TEXT("elemental");

    struct FBreakerAffixLoad
    {
        FBreakerAffixLibraryData Data;
        TArray<FString> Errors;
    };

    template <typename TEnum>
    bool BreakerAffixReadEnum(const FJsonObject& Row, const TCHAR* Field, const FString& Id, TEnum& Out, FBreakerDataErrors& Errors)
    {
        FString Name;
        if (!Row.TryGetStringField(Field, Name) || !BreakerDataFile::ParseEnum(Name, Out))
        {
            Errors.Add(FString::Printf(TEXT("%s: \"%s\" is \"%s\", not a %s"),
                *Id, Field, *Name, *StaticEnum<TEnum>()->GetName()));
            return false;
        }
        return true;
    }

    bool BreakerAffixReadNumber(const FJsonObject& Row, const TCHAR* Field, const FString& Id, float& Out, FBreakerDataErrors& Errors)
    {
        double Value = 0.0;
        if (!Row.TryGetNumberField(Field, Value))
        {
            Errors.Add(FString::Printf(TEXT("%s: \"%s\" is missing or not a number"), *Id, Field));
            return false;
        }
        Out = static_cast<float>(Value);
        return true;
    }

    // One row into one definition. Returns false with every field-level
    // complaint recorded, not just the first.
    bool BreakerAffixReadRow(const FJsonObject& Row, FBreakerAffixDefinition& Out, FString& OutPool, FBreakerDataErrors& Errors)
    {
        FString Id;
        if (!Row.TryGetStringField(TEXT("id"), Id) || Id.IsEmpty())
        {
            Errors.Add(TEXT("a row has no \"id\""));
            return false;
        }
        Out.AffixId = FName(*Id);

        bool bOk = true;
        if (!Row.TryGetStringField(TEXT("pool"), OutPool) || OutPool.IsEmpty())
        {
            Errors.Add(FString::Printf(TEXT("%s: no \"pool\""), *Id));
            bOk = false;
        }

        FString DisplayName;
        if (!Row.TryGetStringField(TEXT("displayName"), DisplayName) || DisplayName.IsEmpty())
        {
            Errors.Add(FString::Printf(TEXT("%s: no \"displayName\""), *Id));
            bOk = false;
        }
        Out.DisplayName = FText::FromString(DisplayName);

        bOk = BreakerAffixReadEnum(Row, TEXT("category"), Id, Out.Category, Errors) && bOk;
        bOk = BreakerAffixReadEnum(Row, TEXT("target"), Id, Out.StatTarget, Errors) && bOk;
        bOk = BreakerAffixReadEnum(Row, TEXT("bucket"), Id, Out.StatBucket, Errors) && bOk;

        Out.AllowedSlots.Reset();
        const TArray<TSharedPtr<FJsonValue>>* Slots = nullptr;
        if (!Row.TryGetArrayField(TEXT("slots"), Slots) || Slots->IsEmpty())
        {
            Errors.Add(FString::Printf(TEXT("%s: \"slots\" is missing or empty"), *Id));
            bOk = false;
        }
        else
        {
            for (const TSharedPtr<FJsonValue>& Value : *Slots)
            {
                FString Name;
                EBreakerEquipSlot Slot = EBreakerEquipSlot::Helmet;
                if (!Value.IsValid() || !Value->TryGetString(Name) || !BreakerDataFile::ParseEnum(Name, Slot))
                {
                    Errors.Add(FString::Printf(TEXT("%s: slot \"%s\" is not an EBreakerEquipSlot"), *Id, *Name));
                    bOk = false;
                    continue;
                }
                if (Out.AllowedSlots.Contains(Slot))
                {
                    Errors.Add(FString::Printf(TEXT("%s: slot \"%s\" is listed twice"), *Id, *Name));
                    bOk = false;
                    continue;
                }
                Out.AllowedSlots.Add(Slot);
            }
        }

        bOk = BreakerAffixReadNumber(Row, TEXT("valueAtT12"), Id, Out.ValueAtT12, Errors) && bOk;
        bOk = BreakerAffixReadNumber(Row, TEXT("valueAtT1"), Id, Out.ValueAtT1, Errors) && bOk;
        bOk = BreakerAffixReadNumber(Row, TEXT("rollWeight"), Id, Out.RollWeight, Errors) && bOk;
        bOk = BreakerAffixReadEnum(Row, TEXT("condition"), Id, Out.Condition, Errors) && bOk;
        bOk = BreakerAffixReadEnum(Row, TEXT("minimumRarity"), Id, Out.MinimumRarity, Errors) && bOk;

        FString Paired;
        if (!Row.TryGetStringField(TEXT("pairedAffixId"), Paired))
        {
            Errors.Add(FString::Printf(TEXT("%s: no \"pairedAffixId\" (use \"\" for none)"), *Id));
            bOk = false;
        }
        Out.PairedAffixId = Paired.IsEmpty() ? NAME_None : FName(*Paired);
        return bOk;
    }

    // The rules a row must satisfy beyond parsing. The tier check reads the
    // SIGN of the floor: a positive line must not shrink toward T1, a
    // negative line (a bill) must not grow, and a bill is constant because
    // "this much, always" is the only version of a downside a player can
    // price.
    void BreakerAffixValidateRow(const FBreakerAffixDefinition& Affix, const FString& Pool, FBreakerDataErrors& Errors)
    {
        const FString Id = Affix.AffixId.ToString();
        const bool bDownside = Pool == BreakerAffixPoolDownside;

        if (Affix.RollWeight < 1.0f)
        {
            Errors.Add(FString::Printf(TEXT("%s: rollWeight %g is below 1"), *Id, Affix.RollWeight));
        }
        if (Affix.StatBucket == EBreakerStatBucket::MorePercent)
        {
            Errors.Add(FString::Printf(TEXT("%s: affixes author no More multipliers (O3)"), *Id));
        }
        if (bDownside)
        {
            if (!(Affix.ValueAtT12 < 0.0f) || Affix.ValueAtT1 != Affix.ValueAtT12)
            {
                Errors.Add(FString::Printf(TEXT("%s: a downside is a constant negative; got T12 %g, T1 %g"),
                    *Id, Affix.ValueAtT12, Affix.ValueAtT1));
            }
            if (!Affix.PairedAffixId.IsNone())
            {
                Errors.Add(FString::Printf(TEXT("%s: a downside carries no bill of its own"), *Id));
            }
        }
        else
        {
            const float Sign = Affix.ValueAtT12 < 0.0f ? -1.0f : 1.0f;
            if ((Affix.ValueAtT1 - Affix.ValueAtT12) * Sign < 0.0f)
            {
                Errors.Add(FString::Printf(TEXT("%s: tier anchors are not monotone in the sign of T12 (T12 %g, T1 %g)"),
                    *Id, Affix.ValueAtT12, Affix.ValueAtT1));
            }
        }
    }

    FBreakerAffixLoad BreakerAffixLoadData()
    {
        FBreakerAffixLoad Load;
        FBreakerDataErrors Errors;
        FBreakerAffixLibraryData Data;
        const FString File = UBreakerAffixLibrary::DataRelativePath();

        const TSharedPtr<FJsonObject> Root = BreakerDataFile::Load(File, Errors);
        if (Root.IsValid())
        {
            // ---- rows ------------------------------------------------------
            TArray<FString> Pools;
            TArray<FBreakerAffixDefinition> Rows;
            const TArray<TSharedPtr<FJsonValue>>* RowValues = nullptr;
            if (!Root->TryGetArrayField(TEXT("affixes"), RowValues))
            {
                Errors.Add(FString::Printf(TEXT("%s: no \"affixes\" array"), *File));
            }
            else
            {
                for (const TSharedPtr<FJsonValue>& Value : *RowValues)
                {
                    const TSharedPtr<FJsonObject>* RowObject = nullptr;
                    if (!Value.IsValid() || !Value->TryGetObject(RowObject))
                    {
                        Errors.Add(FString::Printf(TEXT("%s: an \"affixes\" entry is not an object"), *File));
                        continue;
                    }
                    FBreakerAffixDefinition Affix;
                    FString Pool;
                    if (BreakerAffixReadRow(**RowObject, Affix, Pool, Errors))
                    {
                        Rows.Add(Affix);
                        Pools.Add(Pool);
                    }
                }
            }

            TSet<FName> Ids;
            int32 ElementalRows = 0;
            for (int32 Index = 0; Index < Rows.Num(); ++Index)
            {
                const FBreakerAffixDefinition& Affix = Rows[Index];
                const FString& Pool = Pools[Index];
                if (Ids.Contains(Affix.AffixId))
                {
                    Errors.Add(FString::Printf(TEXT("%s: id appears twice"), *Affix.AffixId.ToString()));
                }
                Ids.Add(Affix.AffixId);
                BreakerAffixValidateRow(Affix, Pool, Errors);

                if (Pool == BreakerAffixPoolSlice) { Data.Slice.Add(Affix); }
                else if (Pool == BreakerAffixPoolAberrant) { Data.Aberrant.Add(Affix); }
                else if (Pool == BreakerAffixPoolAnomalous) { Data.Anomalous.Add(Affix); }
                else if (Pool == BreakerAffixPoolDownside) { Data.Downsides.Add(Affix); }
                else if (Pool == BreakerAffixPoolElemental) { Data.Elemental = Affix; ++ElementalRows; }
                else
                {
                    Errors.Add(FString::Printf(TEXT("%s: pool \"%s\" is not slice, aberrant, anomalous, downside or elemental"),
                        *Affix.AffixId.ToString(), *Pool));
                }
            }
            if (ElementalRows != 1)
            {
                Errors.Add(FString::Printf(TEXT("%s: %d elemental rows; exactly one is expected"), *File, ElementalRows));
            }
            for (int32 Index = 0; Index < Rows.Num(); ++Index)
            {
                const FBreakerAffixDefinition& Affix = Rows[Index];
                if (Affix.PairedAffixId.IsNone()) { continue; }
                auto ById = [&Affix](const FBreakerAffixDefinition& Candidate) { return Candidate.AffixId == Affix.PairedAffixId; };
                if (!Data.Downsides.ContainsByPredicate(ById))
                {
                    Errors.Add(FString::Printf(TEXT("%s: pairedAffixId \"%s\" is not in the downside pool"),
                        *Affix.AffixId.ToString(), *Affix.PairedAffixId.ToString()));
                }
            }

            // ---- leans -----------------------------------------------------
            const TSharedPtr<FJsonObject>* Leans = nullptr;
            if (!Root->TryGetObjectField(TEXT("leans"), Leans))
            {
                Errors.Add(FString::Printf(TEXT("%s: no \"leans\" object"), *File));
            }
            else
            {
                for (const TPair<FString, TSharedPtr<FJsonValue>>& Table : (*Leans)->Values)
                {
                    FBreakerArchetypeLeans Entry;
                    if (!BreakerDataFile::ParseEnum(Table.Key, Entry.Archetype))
                    {
                        Errors.Add(FString::Printf(TEXT("leans: \"%s\" is not an EBreakerWeaponArchetype"), *Table.Key));
                        continue;
                    }
                    if (Data.Leans.ContainsByPredicate([&Entry](const FBreakerArchetypeLeans& Other) { return Other.Archetype == Entry.Archetype; }))
                    {
                        Errors.Add(FString::Printf(TEXT("leans: \"%s\" appears twice"), *Table.Key));
                        continue;
                    }
                    const TSharedPtr<FJsonObject>* RowsObject = nullptr;
                    if (!Table.Value.IsValid() || !Table.Value->TryGetObject(RowsObject))
                    {
                        Errors.Add(FString::Printf(TEXT("leans.%s: not an object"), *Table.Key));
                        continue;
                    }
                    for (const TPair<FString, TSharedPtr<FJsonValue>>& Row : (*RowsObject)->Values)
                    {
                        FBreakerAffixLean Lean;
                        Lean.AffixId = FName(*Row.Key);
                        double Multiplier = 0.0;
                        if (!Row.Value.IsValid() || !Row.Value->TryGetNumber(Multiplier) || !(Multiplier > 0.0))
                        {
                            Errors.Add(FString::Printf(TEXT("leans.%s.%s: multiplier is not a positive number"), *Table.Key, *Row.Key));
                            continue;
                        }
                        Lean.Multiplier = static_cast<float>(Multiplier);
                        // A lean bends the odds of the GENERIC loop, which walks the
                        // slice pool; a lean toward any other row is a comment.
                        if (!Data.Slice.ContainsByPredicate([&Lean](const FBreakerAffixDefinition& Candidate) { return Candidate.AffixId == Lean.AffixId; }))
                        {
                            Errors.Add(FString::Printf(TEXT("leans.%s: \"%s\" is not a slice-pool affix"), *Table.Key, *Row.Key));
                            continue;
                        }
                        if (Entry.Rows.ContainsByPredicate([&Lean](const FBreakerAffixLean& Other) { return Other.AffixId == Lean.AffixId; }))
                        {
                            Errors.Add(FString::Printf(TEXT("leans.%s: \"%s\" appears twice"), *Table.Key, *Row.Key));
                            continue;
                        }
                        Entry.Rows.Add(Lean);
                    }
                    Data.Leans.Add(Entry);
                }
            }

            // ---- caps ------------------------------------------------------
            const TSharedPtr<FJsonObject>* Caps = nullptr;
            if (!Root->TryGetObjectField(TEXT("caps"), Caps))
            {
                Errors.Add(FString::Printf(TEXT("%s: no \"caps\" object"), *File));
            }
            else
            {
                for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*Caps)->Values)
                {
                    FBreakerStatCap Cap;
                    if (!BreakerDataFile::ParseEnum(Pair.Key, Cap.Target))
                    {
                        Errors.Add(FString::Printf(TEXT("caps: \"%s\" is not an EBreakerStatTarget"), *Pair.Key));
                        continue;
                    }
                    double Value = 0.0;
                    if (!Pair.Value.IsValid() || !Pair.Value->TryGetNumber(Value) || !(Value > 0.0))
                    {
                        Errors.Add(FString::Printf(TEXT("caps.%s: not a positive number"), *Pair.Key));
                        continue;
                    }
                    if (Data.Caps.ContainsByPredicate([&Cap](const FBreakerStatCap& Other) { return Other.Target == Cap.Target; }))
                    {
                        Errors.Add(FString::Printf(TEXT("caps: \"%s\" appears twice"), *Pair.Key));
                        continue;
                    }
                    Cap.Cap = static_cast<float>(Value);
                    Data.Caps.Add(Cap);
                }
            }
        }

        if (!Errors.IsClean())
        {
            Load.Errors = Errors.Messages;
            ensureMsgf(false, TEXT("%s failed to load; the affix pools are EMPTY.\n%s"), *File, *Errors.Join());
            return Load;
        }
        Load.Data = MoveTemp(Data);
        return Load;
    }

    const FBreakerAffixLoad& BreakerAffixLoaded()
    {
        static const FBreakerAffixLoad Load = BreakerAffixLoadData();
        return Load;
    }
}

FString UBreakerAffixLibrary::DataRelativePath()
{
    return TEXT("Data/affixes.json");
}

const FBreakerAffixLibraryData& UBreakerAffixLibrary::GetData()
{
    return BreakerAffixLoaded().Data;
}

const TArray<FString>& UBreakerAffixLibrary::GetDataErrors()
{
    return BreakerAffixLoaded().Errors;
}

const TArray<FBreakerAffixDefinition>& UBreakerAffixLibrary::GetSliceAffixPool()
{
    return GetData().Slice;
}

const TArray<FBreakerAffixDefinition>& UBreakerAffixLibrary::GetAberrantAffixPool()
{
    return GetData().Aberrant;
}

const TArray<FBreakerAffixDefinition>& UBreakerAffixLibrary::GetAnomalousAffixPool()
{
    return GetData().Anomalous;
}

const TArray<FBreakerAffixDefinition>& UBreakerAffixLibrary::GetSpecialDownsidePool()
{
    return GetData().Downsides;
}

const FBreakerAffixDefinition& UBreakerAffixLibrary::GetElementalResistanceAffix()
{
    return GetData().Elemental;
}

float UBreakerAffixLibrary::GetStatCap(EBreakerStatTarget Target)
{
    for (const FBreakerStatCap& Cap : GetData().Caps)
    {
        if (Cap.Target == Target) { return Cap.Cap; }
    }
    return TNumericLimits<float>::Max();
}

float UBreakerAffixLibrary::CapStat(EBreakerStatTarget Target, float SummedPercent)
{
    return FMath::Min(SummedPercent, GetStatCap(Target));
}

bool UBreakerAffixLibrary::IsOffensiveTarget(EBreakerStatTarget Target)
{
    switch (Target)
    {
    case EBreakerStatTarget::WeaponDamage:
    // O54's other two pools. Both are damage by any reading, and the breadth
    // test's per-slot "can this slot raise damage at all" question has to count
    // them or a slot carrying only ability lines would read as defensive.
    case EBreakerStatTarget::AbilityDamage:
    case EBreakerStatTarget::SharedDamage:
    case EBreakerStatTarget::AddedDamage:
    case EBreakerStatTarget::CriticalChance:
    case EBreakerStatTarget::CriticalDamage:
    case EBreakerStatTarget::AirborneDamage:
    case EBreakerStatTarget::SlidingDamage:
    case EBreakerStatTarget::WallRideDamage:
    case EBreakerStatTarget::RedlineDamage:
    case EBreakerStatTarget::RecentlyDashedDamage:
    // Fire rate raises sustained damage output, so the breadth test counts it
    // as offence even though it lands on a different attribute.
    case EBreakerStatTarget::FireRate:
    // Damage over time is damage. It lands on its own attribute and its own
    // snapshot, but a build whose output is a DoT is not a defensive build.
    case EBreakerStatTarget::DamageOverTime:
        return true;
    default:
        return false;
    }
}

const FBreakerAffixDefinition* UBreakerAffixLibrary::FindAffix(const TArray<FBreakerAffixDefinition>& Pool, FName AffixId)
{
    auto ById = [AffixId](const FBreakerAffixDefinition& Affix) { return Affix.AffixId == AffixId; };
    if (const FBreakerAffixDefinition* Found = Pool.FindByPredicate(ById)) return Found;
    // The special-pool fallback. Aggregation, comparison rows, tooltips and the
    // Forge all resolve rolled ids through this function with the SLICE pool as
    // the argument; a special line on an Aberrant/Anomalous item must resolve
    // there or it would aggregate to nothing — a line that lies. Falling back
    // here (instead of merging the pools) is what keeps the special entries out
    // of every generic candidate walk, so they can never be OFFERED below their
    // rarity while still always being READ.
    if (const FBreakerAffixDefinition* Found = GetAberrantAffixPool().FindByPredicate(ById)) return Found;
    if (const FBreakerAffixDefinition* Found = GetAnomalousAffixPool().FindByPredicate(ById)) return Found;
    // The authored-but-ungated elemental leg resolves here for the same
    // reason the special pools do: an item that CARRIES the line (granted,
    // test fixture, future content) must aggregate and print it truthfully,
    // while no generic candidate walk can ever OFFER it.
    if (GetElementalResistanceAffix().AffixId == AffixId) return &GetElementalResistanceAffix();
    return GetSpecialDownsidePool().FindByPredicate(ById);
}

float UBreakerAffixLibrary::ArchetypeAffixWeightMultiplier(EBreakerWeaponArchetype Archetype, FName AffixId)
{
    for (const FBreakerArchetypeLeans& Table : GetData().Leans)
    {
        if (Table.Archetype != Archetype) { continue; }
        for (const FBreakerAffixLean& Lean : Table.Rows)
        {
            if (Lean.AffixId == AffixId)
            {
                // Clamped at 1.0 from below: a "lean" may only ever make a line
                // MORE likely. Making one less likely is a different feature with
                // a different failure mode (a stat that quietly cannot be found),
                // and nobody has asked for it.
                return FMath::Max(1.0f, Lean.Multiplier);
            }
        }
        return 1.0f;
    }
    return 1.0f;
}
