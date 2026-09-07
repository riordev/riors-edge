#include "Abilities/BreakerAbilityDefinition.h"

#include "Abilities/BreakerAbilityData.h"
#include "Abilities/BreakerAbilityTags.h"
#include "Abilities/BreakerGameplayAbility.h"
#include "Abilities/BreakerAbility_CadenceBreak.h"
#include "Abilities/BreakerAbility_Cleave.h"
#include "Abilities/BreakerAbility_HardStop.h"
#include "Abilities/BreakerAbility_Sightline.h"
#include "Abilities/BreakerAbility_Slipcut.h"
#include "Abilities/BreakerAbility_Closequarter.h"
#include "Abilities/BreakerAbility_Fracture.h"
#include "Abilities/BreakerAbility_Lead.h"
#include "Abilities/BreakerAbility_Resonance.h"
#include "Abilities/BreakerAbility_Rot.h"
#include "Abilities/BreakerAbility_Siphon.h"
#include "Abilities/BreakerAbility_Unmake.h"
#include "Abilities/BreakerAbility_Overdrive.h"
#include "Abilities/BreakerAbility_Skim.h"
#include "Abilities/BreakerGunsmithAbilities.h"
#include "Abilities/BreakerSupportAbilities.h"
#include "Abilities/BreakerTankAbilities.h"
#include "Data/BreakerDataFile.h"
#include "UObject/UnrealType.h"

float UBreakerAbilityDefinition::Number(FName Key, float Default) const
{
    const float* Found = Numbers.Find(Key);
    return Found ? *Found : Default;
}

bool UBreakerAbilityDefinition::CanOccupySlot(EBreakerAbilitySlot Slot) const
{
    if (SlotAffinity == EBreakerAbilitySlot::Ultimate || Slot == EBreakerAbilitySlot::Ultimate)
    {
        return SlotAffinity == Slot;
    }
    return true;
}

FBreakerAbilityVariant UBreakerAbilityDefinition::ResolveVariant(const FGameplayTagContainer& OwnerTags) const
{
    const FBreakerAbilityVariant* Base = nullptr;
    for (const FBreakerAbilityVariant& Variant : Variants)
    {
        if (!Variant.KeystoneTag.IsValid())
        {
            if (!Base)
            {
                Base = &Variant;
            }
            continue;
        }
        if (OwnerTags.HasTag(Variant.KeystoneTag))
        {
            return Variant;
        }
    }
    if (Base)
    {
        return *Base;
    }
    FBreakerAbilityVariant Fallback;
    Fallback.WindowDuration = WindowDuration;
    return Fallback;
}

namespace
{
    UBreakerAbilityDefinition* MakeFallback(const TCHAR* ObjectName)
    {
        UBreakerAbilityDefinition* Definition = NewObject<UBreakerAbilityDefinition>(GetTransientPackage(), FName(ObjectName));
        Definition->AddToRoot();
        return Definition;
    }
}

// ---------------------------------------------------------------------------
// TAGS FOR THE THREE DESIGNED-BUT-UNBUILT CLASSES — FILE-LOCAL, AND THEY SHOULD
// NOT STAY THAT WAY.
// ---------------------------------------------------------------------------
// Every Swift and Caster tag is declared in `Abilities/BreakerAbilityTags.h`,
// whose header comment states the convention plainly: the ability infrastructure
// owns its own vocabulary so content authoring cannot silently drop a tag the
// fallback registry depends on. These belong there too. They are declared here
// instead for one reason and it is not a design one — this pass had write access
// to the registry and not to the tag header — and the split is recorded rather
// than hidden.
//
// **COORDINATOR: MOVE THESE.** Promote each line below to a
// `UE_DECLARE_GAMEPLAY_TAG_EXTERN` in `BreakerAbilityTags.h` plus a
// `UE_DEFINE_GAMEPLAY_TAG` in `BreakerAbilityTags.cpp`, keeping the tag STRINGS
// byte-identical, then delete this block and swap the references below. The
// strings are what a save, a Data Asset and a granted GameplayEffect key off;
// the C++ symbol names are not. Nothing else has to change.
//
// The keystone tags follow the `Keystone.<Class>.<Name>` shape the three
// existing sets use, so `RiorsEdge.Abilities.KeystoneReachability` sees them
// the same way it sees Caster's — and today it takes that test's HONEST
// EMPTINESS arm, because none of these three classes has a branch tree and so
// no node anywhere COULD grant one. The day a Gunsmith, Tank or Support branch
// tree is authored, that arm stops applying and the suite goes red until each
// keystone is sited on a real cornerstone node. That is the intended alarm.
namespace
{
    UE_DEFINE_GAMEPLAY_TAG_STATIC(Ability_Class_Gunsmith_SidearmRig, "Ability.Class.Gunsmith.SidearmRig");
    UE_DEFINE_GAMEPLAY_TAG_STATIC(Ability_Class_Gunsmith_Overhaul, "Ability.Class.Gunsmith.Overhaul");
    UE_DEFINE_GAMEPLAY_TAG_STATIC(Ability_Class_Gunsmith_Turret, "Ability.Class.Gunsmith.Turret");
    UE_DEFINE_GAMEPLAY_TAG_STATIC(Ability_Class_Gunsmith_AmmoCrate, "Ability.Class.Gunsmith.AmmoCrate");
    UE_DEFINE_GAMEPLAY_TAG_STATIC(Ability_Class_Gunsmith_MineCluster, "Ability.Class.Gunsmith.MineCluster");
    UE_DEFINE_GAMEPLAY_TAG_STATIC(Ability_Class_Gunsmith_Disruptor, "Ability.Class.Gunsmith.Disruptor");
    UE_DEFINE_GAMEPLAY_TAG_STATIC(Ability_Class_Gunsmith_FieldAssembly, "Ability.Class.Gunsmith.FieldAssembly");
    // Only the two ARMORY abilities carry cooldowns; the four deployables are
    // cost-gated and author no cooldown tag at all, so the HUD can tell
    // "cost-gated" from "cooldown of zero" (spec D3, Class-Kits-Gunsmith §1.5).
    UE_DEFINE_GAMEPLAY_TAG_STATIC(Cooldown_Class_Gunsmith_SidearmRig, "Cooldown.Class.Gunsmith.SidearmRig");
    UE_DEFINE_GAMEPLAY_TAG_STATIC(Cooldown_Class_Gunsmith_Overhaul, "Cooldown.Class.Gunsmith.Overhaul");
    UE_DEFINE_GAMEPLAY_TAG_STATIC(Keystone_Gunsmith_Machinist, "Keystone.Gunsmith.Machinist");
    UE_DEFINE_GAMEPLAY_TAG_STATIC(Keystone_Gunsmith_Foundry, "Keystone.Gunsmith.Foundry");
    UE_DEFINE_GAMEPLAY_TAG_STATIC(Keystone_Gunsmith_Minefield, "Keystone.Gunsmith.Minefield");

    UE_DEFINE_GAMEPLAY_TAG_STATIC(Ability_Class_Tank_Rend, "Ability.Class.Tank.Rend");
    UE_DEFINE_GAMEPLAY_TAG_STATIC(Ability_Class_Tank_Bloodline, "Ability.Class.Tank.Bloodline");
    UE_DEFINE_GAMEPLAY_TAG_STATIC(Ability_Class_Tank_AnchorPoint, "Ability.Class.Tank.AnchorPoint");
    UE_DEFINE_GAMEPLAY_TAG_STATIC(Ability_Class_Tank_Provoke, "Ability.Class.Tank.Provoke");
    UE_DEFINE_GAMEPLAY_TAG_STATIC(Ability_Class_Tank_BreachCharge, "Ability.Class.Tank.BreachCharge");
    UE_DEFINE_GAMEPLAY_TAG_STATIC(Ability_Class_Tank_GroundZero, "Ability.Class.Tank.GroundZero");
    UE_DEFINE_GAMEPLAY_TAG_STATIC(Ability_Class_Tank_Hold, "Ability.Class.Tank.Hold");
    UE_DEFINE_GAMEPLAY_TAG_STATIC(Cooldown_Class_Tank_Rend, "Cooldown.Class.Tank.Rend");
    UE_DEFINE_GAMEPLAY_TAG_STATIC(Cooldown_Class_Tank_Bloodline, "Cooldown.Class.Tank.Bloodline");
    UE_DEFINE_GAMEPLAY_TAG_STATIC(Cooldown_Class_Tank_AnchorPoint, "Cooldown.Class.Tank.AnchorPoint");
    UE_DEFINE_GAMEPLAY_TAG_STATIC(Cooldown_Class_Tank_Provoke, "Cooldown.Class.Tank.Provoke");
    UE_DEFINE_GAMEPLAY_TAG_STATIC(Cooldown_Class_Tank_BreachCharge, "Cooldown.Class.Tank.BreachCharge");
    UE_DEFINE_GAMEPLAY_TAG_STATIC(Cooldown_Class_Tank_GroundZero, "Cooldown.Class.Tank.GroundZero");
    UE_DEFINE_GAMEPLAY_TAG_STATIC(Keystone_Tank_Vein, "Keystone.Tank.Vein");
    UE_DEFINE_GAMEPLAY_TAG_STATIC(Keystone_Tank_Wall, "Keystone.Tank.Wall");
    UE_DEFINE_GAMEPLAY_TAG_STATIC(Keystone_Tank_Detonation, "Keystone.Tank.Detonation");

    UE_DEFINE_GAMEPLAY_TAG_STATIC(Ability_Class_Support_Patch, "Ability.Class.Support.Patch");
    UE_DEFINE_GAMEPLAY_TAG_STATIC(Ability_Class_Support_Purge, "Ability.Class.Support.Purge");
    UE_DEFINE_GAMEPLAY_TAG_STATIC(Ability_Class_Support_Cadence, "Ability.Class.Support.Cadence");
    UE_DEFINE_GAMEPLAY_TAG_STATIC(Ability_Class_Support_Metronome, "Ability.Class.Support.Metronome");
    UE_DEFINE_GAMEPLAY_TAG_STATIC(Ability_Class_Support_Mark, "Ability.Class.Support.Mark");
    UE_DEFINE_GAMEPLAY_TAG_STATIC(Ability_Class_Support_Suppress, "Ability.Class.Support.Suppress");
    UE_DEFINE_GAMEPLAY_TAG_STATIC(Ability_Class_Support_Conduit, "Ability.Class.Support.Conduit");
    UE_DEFINE_GAMEPLAY_TAG_STATIC(Cooldown_Class_Support_Patch, "Cooldown.Class.Support.Patch");
    UE_DEFINE_GAMEPLAY_TAG_STATIC(Cooldown_Class_Support_Purge, "Cooldown.Class.Support.Purge");
    UE_DEFINE_GAMEPLAY_TAG_STATIC(Cooldown_Class_Support_Cadence, "Cooldown.Class.Support.Cadence");
    UE_DEFINE_GAMEPLAY_TAG_STATIC(Cooldown_Class_Support_Metronome, "Cooldown.Class.Support.Metronome");
    UE_DEFINE_GAMEPLAY_TAG_STATIC(Cooldown_Class_Support_Mark, "Cooldown.Class.Support.Mark");
    UE_DEFINE_GAMEPLAY_TAG_STATIC(Cooldown_Class_Support_Suppress, "Cooldown.Class.Support.Suppress");
    UE_DEFINE_GAMEPLAY_TAG_STATIC(Keystone_Support_Triage, "Keystone.Support.Triage");
    UE_DEFINE_GAMEPLAY_TAG_STATIC(Keystone_Support_Downbeat, "Keystone.Support.Downbeat");
    UE_DEFINE_GAMEPLAY_TAG_STATIC(Keystone_Support_Blackout, "Keystone.Support.Blackout");
}

// ---------------------------------------------------------------------------
// THE NUMERICS, FROM Data/abilities.json (O186).
// ---------------------------------------------------------------------------
// The registry below authors WHAT each ability is; the file authors HOW MUCH.
// Rows match by id, variants by keystone tag name and numbers by property
// name on the row's ability class, and the file has to describe the registry
// exactly — the same row count, every id known, every variant row present,
// every numeric property keyed and no key without a property — before a
// single number is applied. Anything short of that leaves every row at its
// default-constructed numerics and every ability class at its compiled
// initialisers behind an ensure, because a table that half-loaded would play
// as a table that loaded.
namespace
{
    struct FBreakerAbilityVariantData
    {
        FString Keystone;
        float WindowDuration = 0.0f;
        float SpeedMultiplier = 1.0f;
        float HitTimeoutSeconds = 0.0f;
        float AbilityCostMultiplier = 1.0f;
    };

    struct FBreakerAbilityRowData
    {
        FName Id = NAME_None;
        float ResourceCost = 0.0f;
        float CooldownSeconds = 0.0f;
        float WindowDuration = 0.0f;
        TMap<FName, float> Numbers;
        TArray<FBreakerAbilityVariantData> Variants;
    };

    TArray<FString> BreakerAbilityDataErrorsStore;

    // A number that is present, numeric and not below zero: every field on
    // these rows is a cost, a duration or a multiplier, and none has a
    // meaning below zero (the definition header clamps them the same way).
    bool BreakerAbilityDataReadNumber(const FJsonObject& Row, const TCHAR* Field, const FString& Context, float& Out, BreakerDataFile::FBreakerDataErrors& Errors)
    {
        double Value = 0.0;
        if (!Row.TryGetNumberField(Field, Value))
        {
            Errors.Add(FString::Printf(TEXT("%s: \"%s\" is missing or not a number"), *Context, Field));
            return false;
        }
        if (Value < 0.0)
        {
            Errors.Add(FString::Printf(TEXT("%s: \"%s\" is negative (%g)"), *Context, Field, Value));
            return false;
        }
        Out = static_cast<float>(Value);
        return true;
    }

    bool BreakerAbilityDataReadVariant(const FJsonObject& Row, const FString& Context, FBreakerAbilityVariantData& Out, BreakerDataFile::FBreakerDataErrors& Errors)
    {
        if (!Row.TryGetStringField(TEXT("keystone"), Out.Keystone))
        {
            Errors.Add(FString::Printf(TEXT("%s: a variant has no \"keystone\" (use \"\" for the base row)"), *Context));
            return false;
        }
        const FString VariantContext = FString::Printf(TEXT("%s variant \"%s\""), *Context, *Out.Keystone);
        bool bOk = BreakerAbilityDataReadNumber(Row, TEXT("windowDuration"), VariantContext, Out.WindowDuration, Errors);
        bOk = BreakerAbilityDataReadNumber(Row, TEXT("speedMultiplier"), VariantContext, Out.SpeedMultiplier, Errors) && bOk;
        bOk = BreakerAbilityDataReadNumber(Row, TEXT("hitTimeoutSeconds"), VariantContext, Out.HitTimeoutSeconds, Errors) && bOk;
        bOk = BreakerAbilityDataReadNumber(Row, TEXT("abilityCostMultiplier"), VariantContext, Out.AbilityCostMultiplier, Errors) && bOk;
        return bOk;
    }

    bool BreakerAbilityDataReadRow(const FJsonObject& Row, FBreakerAbilityRowData& Out, BreakerDataFile::FBreakerDataErrors& Errors)
    {
        FString Id;
        if (!Row.TryGetStringField(TEXT("id"), Id) || Id.IsEmpty())
        {
            Errors.Add(TEXT("a row has no \"id\""));
            return false;
        }
        Out.Id = FName(*Id);

        bool bOk = BreakerAbilityDataReadNumber(Row, TEXT("resourceCost"), Id, Out.ResourceCost, Errors);
        bOk = BreakerAbilityDataReadNumber(Row, TEXT("cooldownSeconds"), Id, Out.CooldownSeconds, Errors) && bOk;
        bOk = BreakerAbilityDataReadNumber(Row, TEXT("windowDuration"), Id, Out.WindowDuration, Errors) && bOk;

        // The ability class's own numbers, keyed by property name. Required
        // on every row, empty on a row whose class declares none, so a row
        // that forgot the object is a broken row and not a row with no
        // numbers. Which keys are legal is decided in the match phase,
        // against the class.
        const TSharedPtr<FJsonObject>* NumbersObject = nullptr;
        if (!Row.TryGetObjectField(TEXT("numbers"), NumbersObject))
        {
            Errors.Add(FString::Printf(TEXT("%s: no \"numbers\" object (use {} for none)"), *Id));
            bOk = false;
        }
        else
        {
            for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*NumbersObject)->Values)
            {
                double Value = 0.0;
                if (!Pair.Value.IsValid() || !Pair.Value->TryGetNumber(Value))
                {
                    Errors.Add(FString::Printf(TEXT("%s: \"numbers\".\"%s\" is not a number"), *Id, *Pair.Key));
                    bOk = false;
                    continue;
                }
                Out.Numbers.Add(FName(*Pair.Key), static_cast<float>(Value));
            }
        }

        const TArray<TSharedPtr<FJsonValue>>* VariantValues = nullptr;
        if (!Row.TryGetArrayField(TEXT("variants"), VariantValues))
        {
            Errors.Add(FString::Printf(TEXT("%s: no \"variants\" array (use [] for none)"), *Id));
            return false;
        }
        for (const TSharedPtr<FJsonValue>& Value : *VariantValues)
        {
            const TSharedPtr<FJsonObject>* VariantObject = nullptr;
            if (!Value.IsValid() || !Value->TryGetObject(VariantObject))
            {
                Errors.Add(FString::Printf(TEXT("%s: a \"variants\" entry is not an object"), *Id));
                bOk = false;
                continue;
            }
            FBreakerAbilityVariantData Variant;
            bOk = BreakerAbilityDataReadVariant(**VariantObject, Id, Variant, Errors) && bOk;
            Out.Variants.Add(Variant);
        }
        return bOk;
    }

    // The base row's keystone is the empty string; a keystone row's is the
    // tag's full name. That is the spelling the census writes back.
    FString BreakerAbilityDataKeystoneName(const FBreakerAbilityVariant& Variant)
    {
        return Variant.KeystoneTag.IsValid() ? Variant.KeystoneTag.GetTagName().ToString() : FString();
    }

    // Reads the file against the built registry and, only if every check
    // passes, writes the numbers onto the rows. Runs once, from
    // GetFallbackRegistry, before the registry is first handed out.
    void BreakerAbilityDataApply(TArray<UBreakerAbilityDefinition*>& Registry)
    {
        BreakerDataFile::FBreakerDataErrors Errors;
        const FString File = BreakerAbilityData::DataRelativePath();

        // ---- parse ---------------------------------------------------------
        TArray<FBreakerAbilityRowData> Rows;
        const TSharedPtr<FJsonObject> Root = BreakerDataFile::Load(File, Errors);
        if (Root.IsValid())
        {
            const TArray<TSharedPtr<FJsonValue>>* RowValues = nullptr;
            if (!Root->TryGetArrayField(TEXT("abilities"), RowValues))
            {
                Errors.Add(FString::Printf(TEXT("%s: no \"abilities\" array"), *File));
            }
            else
            {
                for (const TSharedPtr<FJsonValue>& Value : *RowValues)
                {
                    const TSharedPtr<FJsonObject>* RowObject = nullptr;
                    if (!Value.IsValid() || !Value->TryGetObject(RowObject))
                    {
                        Errors.Add(FString::Printf(TEXT("%s: an \"abilities\" entry is not an object"), *File));
                        continue;
                    }
                    FBreakerAbilityRowData Row;
                    if (BreakerAbilityDataReadRow(**RowObject, Row, Errors))
                    {
                        Rows.Add(Row);
                    }
                }
            }
        }

        // ---- match ---------------------------------------------------------
        // Every file row names a registry row, no registry row is named
        // twice, and every registry row is named. The count check is stated
        // separately so a file one row short says so in one line.
        if (Root.IsValid() && Rows.Num() != Registry.Num())
        {
            Errors.Add(FString::Printf(TEXT("%s: %d rows for a registry of %d"), *File, Rows.Num(), Registry.Num()));
        }
        TArray<UBreakerAbilityDefinition*> Matched;
        Matched.SetNum(Rows.Num());
        TSet<FName> Claimed;
        for (int32 Index = 0; Index < Rows.Num(); ++Index)
        {
            const FBreakerAbilityRowData& Row = Rows[Index];
            UBreakerAbilityDefinition* const* Found = Registry.FindByPredicate(
                [&Row](const UBreakerAbilityDefinition* Definition) { return Definition && Definition->AbilityId == Row.Id; });
            if (!Found)
            {
                Errors.Add(FString::Printf(TEXT("%s: \"%s\" is not a registered ability"), *File, *Row.Id.ToString()));
                continue;
            }
            if (Claimed.Contains(Row.Id))
            {
                Errors.Add(FString::Printf(TEXT("%s: \"%s\" appears twice"), *File, *Row.Id.ToString()));
                continue;
            }
            Claimed.Add(Row.Id);
            Matched[Index] = *Found;

            const UBreakerAbilityDefinition& Definition = **Found;

            // Numbers: every property on the class has a key and every key
            // names a property. A class that declares a number the file
            // does not carry would keep its compiled value while the rest of
            // the file applied, and a key the class does not declare is a
            // typo or a renamed member; both are the same all-or-nothing
            // failure as a missing row.
            const TArray<FNumericProperty*> Properties = BreakerAbilityData::NumberProperties(Definition.AbilityClass.Get());
            const FString ClassName = Definition.AbilityClass.Get() ? Definition.AbilityClass.Get()->GetName() : FString(TEXT("no ability class"));
            for (const FNumericProperty* Property : Properties)
            {
                if (!Row.Numbers.Contains(Property->GetFName()))
                {
                    Errors.Add(FString::Printf(TEXT("%s: \"numbers\" has no \"%s\" (%s declares it)"), *Row.Id.ToString(), *Property->GetName(), *ClassName));
                }
            }
            for (const TPair<FName, float>& Number : Row.Numbers)
            {
                const bool bDeclared = Properties.ContainsByPredicate(
                    [&Number](const FNumericProperty* Property) { return Property->GetFName() == Number.Key; });
                if (!bDeclared)
                {
                    Errors.Add(FString::Printf(TEXT("%s: \"numbers\".\"%s\" is not a numeric property of %s"), *Row.Id.ToString(), *Number.Key.ToString(), *ClassName));
                }
            }

            if (Row.Variants.Num() != Definition.Variants.Num())
            {
                Errors.Add(FString::Printf(TEXT("%s: %d variants for a row with %d"), *Row.Id.ToString(), Row.Variants.Num(), Definition.Variants.Num()));
                continue;
            }
            TSet<FString> ClaimedKeystones;
            for (const FBreakerAbilityVariantData& Variant : Row.Variants)
            {
                const bool bKnown = Definition.Variants.ContainsByPredicate(
                    [&Variant](const FBreakerAbilityVariant& Candidate) { return BreakerAbilityDataKeystoneName(Candidate) == Variant.Keystone; });
                if (!bKnown)
                {
                    Errors.Add(FString::Printf(TEXT("%s: no variant with keystone \"%s\""), *Row.Id.ToString(), *Variant.Keystone));
                }
                else if (ClaimedKeystones.Contains(Variant.Keystone))
                {
                    Errors.Add(FString::Printf(TEXT("%s: keystone \"%s\" appears twice"), *Row.Id.ToString(), *Variant.Keystone));
                }
                ClaimedKeystones.Add(Variant.Keystone);
            }
        }
        if (Root.IsValid())
        {
            for (const UBreakerAbilityDefinition* Definition : Registry)
            {
                if (Definition && !Claimed.Contains(Definition->AbilityId))
                {
                    Errors.Add(FString::Printf(TEXT("%s: no row for \"%s\""), *File, *Definition->AbilityId.ToString()));
                }
            }
        }

        if (!Errors.IsClean())
        {
            BreakerAbilityDataErrorsStore = Errors.Messages;
            ensureMsgf(false, TEXT("%s failed to load; every ability keeps zero cost, zero cooldown, zero window and its compiled class numbers.\n%s"), *File, *Errors.Join());
            return;
        }

        // ---- apply ---------------------------------------------------------
        for (int32 Index = 0; Index < Rows.Num(); ++Index)
        {
            const FBreakerAbilityRowData& Row = Rows[Index];
            UBreakerAbilityDefinition& Definition = *Matched[Index];
            Definition.ResourceCost = Row.ResourceCost;
            Definition.CooldownSeconds = Row.CooldownSeconds;
            Definition.WindowDuration = Row.WindowDuration;

            // The class numbers go onto the class default object, which every
            // instance copies at grant (InstancedPerActor), so the ability
            // body reads its own member and finds the file's value. The
            // value the compiler put there is recorded first. Ints round from
            // the file's float.
            Definition.CompiledNumbers.Reset();
            Definition.Numbers.Reset();
            if (UObject* Defaults = Definition.AbilityClass.Get() ? Definition.AbilityClass.Get()->GetDefaultObject() : nullptr)
            {
                for (FNumericProperty* Property : BreakerAbilityData::NumberProperties(Definition.AbilityClass.Get()))
                {
                    void* Value = Property->ContainerPtrToValuePtr<void>(Defaults);
                    const float Compiled = Property->IsFloatingPoint()
                        ? static_cast<float>(Property->GetFloatingPointPropertyValue(Value))
                        : static_cast<float>(Property->GetSignedIntPropertyValue(Value));
                    const float Authored = Row.Numbers[Property->GetFName()];
                    Definition.CompiledNumbers.Add(Property->GetFName(), Compiled);
                    Definition.Numbers.Add(Property->GetFName(), Authored);
                    if (Property->IsFloatingPoint())
                    {
                        Property->SetFloatingPointPropertyValue(Value, static_cast<double>(Authored));
                    }
                    else
                    {
                        Property->SetIntPropertyValue(Value, static_cast<int64>(FMath::RoundToInt(Authored)));
                    }
                }
            }

            for (const FBreakerAbilityVariantData& Variant : Row.Variants)
            {
                FBreakerAbilityVariant* Target = Definition.Variants.FindByPredicate(
                    [&Variant](const FBreakerAbilityVariant& Candidate) { return BreakerAbilityDataKeystoneName(Candidate) == Variant.Keystone; });
                Target->WindowDuration = Variant.WindowDuration;
                Target->SpeedMultiplier = Variant.SpeedMultiplier;
                Target->HitTimeoutSeconds = Variant.HitTimeoutSeconds;
                Target->AbilityCostMultiplier = Variant.AbilityCostMultiplier;
            }
        }
    }
}

FString BreakerAbilityData::DataRelativePath()
{
    return TEXT("Data/abilities.json");
}

TArray<FNumericProperty*> BreakerAbilityData::NumberProperties(const UClass* AbilityClass)
{
    TArray<FNumericProperty*> Out;
    const UClass* Base = UBreakerGameplayAbility::StaticClass();
    if (!AbilityClass || !AbilityClass->IsChildOf(Base) || AbilityClass == Base)
    {
        return Out;
    }

    // The chain from the class up to, not including, the shared base, walked
    // super first so a base's number (the Gunsmith deploy range) precedes a
    // subclass's own. TFieldIterator yields the current class before its
    // super, so the chain is collected and read in reverse.
    TArray<const UClass*> Chain;
    for (const UClass* Class = AbilityClass; Class && Class != Base; Class = Class->GetSuperClass())
    {
        Chain.Insert(Class, 0);
    }
    for (const UClass* Class : Chain)
    {
        for (TFieldIterator<FProperty> It(Class, EFieldIteratorFlags::ExcludeSuper); It; ++It)
        {
            FProperty* Property = *It;
            if (!Property->HasAnyPropertyFlags(CPF_Edit))
            {
                continue;
            }
            if (CastField<FFloatProperty>(Property) || CastField<FIntProperty>(Property))
            {
                Out.Add(CastFieldChecked<FNumericProperty>(Property));
            }
        }
    }
    return Out;
}

const TArray<FString>& BreakerAbilityData::GetDataErrors()
{
    UBreakerAbilityDefinition::GetFallbackRegistry();
    return BreakerAbilityDataErrorsStore;
}

const TArray<UBreakerAbilityDefinition*>& UBreakerAbilityDefinition::GetFallbackRegistry()
{
    static TArray<UBreakerAbilityDefinition*> Registry;
    if (Registry.Num() > 0)
    {
        return Registry;
    }

    // ------------------------------------------------------------------
    // Swift, the vertical-slice class, at the FULL designed 6+1 (O175).
    // Costs and cooldowns are quoted from Class-Kits §1.2 (the doc is
    // deleted with the corpus; `git show 61c2d4f^:Docs/Design/Class-Kits.md`
    // is the provenance); nothing here is invented balance. Anything NOT
    // quoted is marked O2 PLACEHOLDER and must be replaced from wave-mode
    // instrumentation before content lock.
    //
    // THE NUMBERS LIVE IN Data/abilities.json (O186): every row's cost,
    // cooldown and window, every variant's four numerics, and every ability
    // class's own numeric defaults are overlaid by BreakerAbilityDataApply
    // before this table is returned. The citations beside each row say where
    // a number comes from; the file says what it is.
    // ------------------------------------------------------------------

    // S1 Slipcut — §1.2 row S1: 20 Momentum, 4s cooldown, 0.4s window.
    // The design's STARTER beside Skim (O176; the class-definition buckets
    // in Progression/ execute that half).
    UBreakerAbilityDefinition* Slipcut = MakeFallback(TEXT("FallbackAbility_Swift_Slipcut"));
    Slipcut->AbilityId = TEXT("Swift.Slipcut");
    Slipcut->ClassId = EBreakerClassId::Swift;
    Slipcut->DisplayName = FText::FromString(TEXT("Slipcut"));
    Slipcut->Description = FText::FromString(TEXT("A breath in which the trigger runs twice as fast. Ends early on reload; rewards a held magazine."));
    Slipcut->SlotAffinity = EBreakerAbilitySlot::ClassAbilityOne;
    Slipcut->Verb = EBreakerAbilityVerb::Weapon;
    Slipcut->AbilityTag = BreakerAbilityTags::Ability_Class_Swift_Slipcut;
    Slipcut->CooldownTag = BreakerAbilityTags::Cooldown_Class_Swift_Slipcut;
    Slipcut->AbilityClass = UBreakerAbility_Slipcut::StaticClass();
    Registry.Add(Slipcut);

    // S3 Skim — Class-Kits §1.2 row S3: 15 Momentum, 3s cooldown.
    UBreakerAbilityDefinition* Skim = MakeFallback(TEXT("FallbackAbility_Swift_Skim"));
    Skim->AbilityId = TEXT("Swift.Skim");
    Skim->ClassId = EBreakerClassId::Swift;
    Skim->DisplayName = FText::FromString(TEXT("Skim"));
    Skim->Description = FText::FromString(TEXT("Directional impulse that redirects existing horizontal speed."));
    Skim->SlotAffinity = EBreakerAbilitySlot::ClassAbilityOne;
    Skim->Verb = EBreakerAbilityVerb::Movement;
    Skim->AbilityTag = BreakerAbilityTags::Ability_Class_Swift_Skim;
    Skim->CooldownTag = BreakerAbilityTags::Cooldown_Class_Swift_Skim;
    Skim->AbilityClass = UBreakerAbility_Skim::StaticClass();
    // O2 PLACEHOLDER: the redirect/boost window length is not specified by any
    // design doc. Structure is complete; the number is a guess and must be
    // replaced (Ability-Implementation-Spec §11 GAP list).
    Registry.Add(Skim);

    // S6 Lead — Class-Kits §1.2 row S6: 40 Momentum, 10s cooldown, the mark
    // lasts 6s.
    // The task brief called this ability "Lash"; Class-Kits has no such ability
    // and S6 Lead is the Swift Marksman ability at that slot, so Lead is used.
    UBreakerAbilityDefinition* Lead = MakeFallback(TEXT("FallbackAbility_Swift_Lead"));
    Lead->AbilityId = TEXT("Swift.Lead");
    Lead->ClassId = EBreakerClassId::Swift;
    Lead->DisplayName = FText::FromString(TEXT("Lead"));
    Lead->Description = FText::FromString(TEXT("Marks a target; long-range hits on the mark count as weak points."));
    Lead->SlotAffinity = EBreakerAbilitySlot::ClassAbilityTwo;
    Lead->Verb = EBreakerAbilityVerb::Reward;
    Lead->AbilityTag = BreakerAbilityTags::Ability_Class_Swift_Lead;
    Lead->CooldownTag = BreakerAbilityTags::Cooldown_Class_Swift_Lead;
    Lead->AbilityClass = UBreakerAbility_Lead::StaticClass();
    Registry.Add(Lead);

    // S2 Cadence Break — Class-Kits §1.2 row S2: 35 Momentum, 8s cooldown,
    // 3s state. Reached through the quartermaster token (O176), not a node
    // grant — Slipcut Mastery's F7 grant clause stays a rule tag.
    // ClassAbilityTwo affinity like Lead's: affinity is a preference, not an
    // exclusive claim (CanOccupySlot only fences the Ultimate slot).
    UBreakerAbilityDefinition* CadenceBreak = MakeFallback(TEXT("FallbackAbility_Swift_CadenceBreak"));
    CadenceBreak->AbilityId = TEXT("Swift.CadenceBreak");
    CadenceBreak->ClassId = EBreakerClassId::Swift;
    CadenceBreak->DisplayName = FText::FromString(TEXT("Cadence Break"));
    CadenceBreak->Description = FText::FromString(TEXT("Completes the reload and opens a state where consecutive hits on one target stack flat damage."));
    CadenceBreak->SlotAffinity = EBreakerAbilitySlot::ClassAbilityTwo;
    CadenceBreak->Verb = EBreakerAbilityVerb::Weapon;
    CadenceBreak->AbilityTag = BreakerAbilityTags::Ability_Class_Swift_CadenceBreak;
    CadenceBreak->CooldownTag = BreakerAbilityTags::Cooldown_Class_Swift_CadenceBreak;
    CadenceBreak->AbilityClass = UBreakerAbility_CadenceBreak::StaticClass();
    Registry.Add(CadenceBreak);

    // S4 Hard Stop — §1.2 row S4: 30 Momentum, 6s cooldown, 0.6s window
    // ("0.6s of Damage Reduction ... treatment").
    // Its own row at last (O177): until this pass the verb rode Skim as a
    // pitch-gated modal branch behind the K7 node, at Skim's price.
    UBreakerAbilityDefinition* HardStop = MakeFallback(TEXT("FallbackAbility_Swift_HardStop"));
    HardStop->AbilityId = TEXT("Swift.HardStop");
    HardStop->ClassId = EBreakerClassId::Swift;
    HardStop->DisplayName = FText::FromString(TEXT("Hard Stop"));
    HardStop->Description = FText::FromString(TEXT("Cancels all velocity instantly and guards the landing for a moment. Spending Momentum to stop is the point."));
    HardStop->SlotAffinity = EBreakerAbilitySlot::ClassAbilityTwo;
    HardStop->Verb = EBreakerAbilityVerb::Movement;
    HardStop->AbilityTag = BreakerAbilityTags::Ability_Class_Swift_HardStop;
    HardStop->CooldownTag = BreakerAbilityTags::Cooldown_Class_Swift_HardStop;
    HardStop->AbilityClass = UBreakerAbility_HardStop::StaticClass();
    Registry.Add(HardStop);

    // S5 Sightline — §1.2 row S5: 25 Momentum, 6s cooldown, 2s window.
    // The design's cover-state clause is RETIRED (Part One-U item 15) — see
    // the ability header. Sightline is the pierce window, whole.
    UBreakerAbilityDefinition* Sightline = MakeFallback(TEXT("FallbackAbility_Swift_Sightline"));
    Sightline->AbilityId = TEXT("Swift.Sightline");
    Sightline->ClassId = EBreakerClassId::Swift;
    Sightline->DisplayName = FText::FromString(TEXT("Sightline"));
    Sightline->Description = FText::FromString(TEXT("The next shot pierces every target on its line. Two seconds to take it."));
    Sightline->SlotAffinity = EBreakerAbilitySlot::ClassAbilityTwo;
    Sightline->Verb = EBreakerAbilityVerb::Weapon;
    Sightline->AbilityTag = BreakerAbilityTags::Ability_Class_Swift_Sightline;
    Sightline->CooldownTag = BreakerAbilityTags::Cooldown_Class_Swift_Sightline;
    Sightline->AbilityClass = UBreakerAbility_Sightline::StaticClass();
    Registry.Add(Sightline);

    // Overdrive — Class-Kits §1.2 ultimate: 100 Momentum (full bar), no
    // cooldown; the cost is the cooldown. Base window 8s.
    UBreakerAbilityDefinition* Overdrive = MakeFallback(TEXT("FallbackAbility_Swift_Overdrive"));
    Overdrive->AbilityId = TEXT("Swift.Overdrive");
    Overdrive->ClassId = EBreakerClassId::Swift;
    Overdrive->DisplayName = FText::FromString(TEXT("Overdrive"));
    Overdrive->Description = FText::FromString(TEXT("Momentum stops decaying and generation doubles for the duration."));
    Overdrive->SlotAffinity = EBreakerAbilitySlot::Ultimate;
    Overdrive->Verb = EBreakerAbilityVerb::Movement;
    Overdrive->AbilityTag = BreakerAbilityTags::Ability_Class_Swift_Overdrive;
    // Deliberately no cooldown tag: cost-gated, and the HUD must be able to
    // tell "no cooldown" from "cooldown of zero" (spec D3).
    Overdrive->AbilityClass = UBreakerAbility_Overdrive::StaticClass();

    // Keystone variant rows (spec D1). Index 0 is the base row. Class-Kits
    // specifies the *behavior* of each rewrite but no numbers for the
    // parametric part, so every SpeedMultiplier in the file and the
    // Bloodrhythm timeout's companion values are O2 PLACEHOLDER — structure
    // is the deliverable, not balance. Durations are quoted (8s base, 1.5s
    // Bloodrhythm hit timeout).
    {
        FBreakerAbilityVariant BaseRow;
        BaseRow.VariantName = FText::FromString(TEXT("Overdrive"));
        Overdrive->Variants.Add(BaseRow);

        // Class-Kits F12: the ultimate ends if the player goes 1.5s without a hit.
        FBreakerAbilityVariant Bloodrhythm;
        Bloodrhythm.KeystoneTag = BreakerAbilityTags::Keystone_Swift_Bloodrhythm;
        Bloodrhythm.VariantName = FText::FromString(TEXT("Overdrive — Bloodrhythm"));
        Overdrive->Variants.Add(Bloodrhythm);

        // Availability rewrite, not a speed rewrite (Class-Kits K12 quotes
        // Master 5.4 explicitly), so the multiplier stays at 1.0.
        FBreakerAbilityVariant TerminalVelocity;
        TerminalVelocity.KeystoneTag = BreakerAbilityTags::Keystone_Swift_TerminalVelocity;
        TerminalVelocity.VariantName = FText::FromString(TEXT("Overdrive — Terminal Velocity"));
        Overdrive->Variants.Add(TerminalVelocity);

        // The stationary Swift ultimate: no movement contribution at all.
        FBreakerAbilityVariant StandingWave;
        StandingWave.KeystoneTag = BreakerAbilityTags::Keystone_Swift_StandingWave;
        StandingWave.VariantName = FText::FromString(TEXT("Overdrive — Standing Wave"));
        Overdrive->Variants.Add(StandingWave);
    }
    Registry.Add(Overdrive);

    // ------------------------------------------------------------------
    // Caster. Costs quoted from Class-Kits §2.2. NO COOLDOWNS ANYWHERE in
    // this class: Mana *is* the cooldown (Class-Kits §2.1), so no entry below
    // authors a CooldownTag, every Caster cooldownSeconds in the file is 0
    // (RiorsEdge.Data.Abilities.Fresh pins it), and the HUD can therefore
    // tell "cost-gated" from "cooldown of zero" (spec D3).
    // ------------------------------------------------------------------

    // C1 Cleave — Class-Kits §2.2 row C1: 20 Mana, no cooldown.
    UBreakerAbilityDefinition* Cleave = MakeFallback(TEXT("FallbackAbility_Caster_Cleave"));
    Cleave->AbilityId = TEXT("Caster.Cleave");
    Cleave->ClassId = EBreakerClassId::Caster;
    Cleave->DisplayName = FText::FromString(TEXT("Cleave"));
    Cleave->Description = FText::FromString(TEXT("Short forward melee arc that always applies Bleed."));
    Cleave->SlotAffinity = EBreakerAbilitySlot::ClassAbilityOne;
    Cleave->Verb = EBreakerAbilityVerb::Weapon;
    Cleave->AbilityTag = BreakerAbilityTags::Ability_Class_Caster_Cleave;
    Cleave->AbilityClass = UBreakerAbility_Cleave::StaticClass();
    // O2 PLACEHOLDER: the window is the animation lock, named by Class-Kits
    // (Edgework removes it) but never timed. Mirrors the ability's own default.
    Registry.Add(Cleave);

    // C2 Closequarter — Class-Kits §2.2 row C2: 35 Mana, no cooldown.
    UBreakerAbilityDefinition* Closequarter = MakeFallback(TEXT("FallbackAbility_Caster_Closequarter"));
    Closequarter->AbilityId = TEXT("Caster.Closequarter");
    Closequarter->ClassId = EBreakerClassId::Caster;
    Closequarter->DisplayName = FText::FromString(TEXT("Closequarter"));
    Closequarter->Description = FText::FromString(TEXT("Blink to the target under the crosshair, arriving just short of it."));
    Closequarter->SlotAffinity = EBreakerAbilitySlot::ClassAbilityTwo;
    Closequarter->Verb = EBreakerAbilityVerb::Movement;
    Closequarter->AbilityTag = BreakerAbilityTags::Ability_Class_Caster_Closequarter;
    Closequarter->AbilityClass = UBreakerAbility_Closequarter::StaticClass();
    Registry.Add(Closequarter);

    // C3 Rot — Class-Kits §2.2 row C3: 25 Mana, no cooldown, 4 m / 6 s.
    UBreakerAbilityDefinition* Rot = MakeFallback(TEXT("FallbackAbility_Caster_Rot"));
    Rot->AbilityId = TEXT("Caster.Rot");
    Rot->ClassId = EBreakerClassId::Caster;
    Rot->DisplayName = FText::FromString(TEXT("Rot"));
    Rot->Description = FText::FromString(TEXT("A 4 m zone that deals Entropy damage, builds Rot and strips armour."));
    Rot->SlotAffinity = EBreakerAbilitySlot::ClassAbilityTwo;
    Rot->Verb = EBreakerAbilityVerb::Weapon;
    Rot->AbilityTag = BreakerAbilityTags::Ability_Class_Caster_Rot;
    Rot->AbilityClass = UBreakerAbility_Rot::StaticClass();
    // The window is the puddle's lifetime, for the HUD.
    Registry.Add(Rot);

    // C4 Siphon — Class-Kits §2.2 row C4: 30 Mana, no cooldown, 5 s channel.
    UBreakerAbilityDefinition* Siphon = MakeFallback(TEXT("FallbackAbility_Caster_Siphon"));
    Siphon->AbilityId = TEXT("Caster.Siphon");
    Siphon->ClassId = EBreakerClassId::Caster;
    Siphon->DisplayName = FText::FromString(TEXT("Siphon"));
    Siphon->Description = FText::FromString(TEXT("Channel on one target: Void damage over time that heals you for a portion."));
    Siphon->SlotAffinity = EBreakerAbilitySlot::ClassAbilityTwo;
    Siphon->Verb = EBreakerAbilityVerb::Reward;
    Siphon->AbilityTag = BreakerAbilityTags::Ability_Class_Caster_Siphon;
    Siphon->AbilityClass = UBreakerAbility_Siphon::StaticClass();
    Registry.Add(Siphon);

    // C5 Fracture — Class-Kits §2.2 row C5: 30 Mana, no cooldown.
    UBreakerAbilityDefinition* Fracture = MakeFallback(TEXT("FallbackAbility_Caster_Fracture"));
    Fracture->AbilityId = TEXT("Caster.Fracture");
    Fracture->ClassId = EBreakerClassId::Caster;
    Fracture->DisplayName = FText::FromString(TEXT("Fracture"));
    Fracture->Description = FText::FromString(TEXT("Projectile applying the next status in your cycle."));
    Fracture->SlotAffinity = EBreakerAbilitySlot::ClassAbilityTwo;
    Fracture->Verb = EBreakerAbilityVerb::Weapon;
    Fracture->AbilityTag = BreakerAbilityTags::Ability_Class_Caster_Fracture;
    Fracture->AbilityClass = UBreakerAbility_Fracture::StaticClass();
    Registry.Add(Fracture);

    // C6 Resonance — Class-Kits §2.2 row C6: 40 Mana, no cooldown.
    UBreakerAbilityDefinition* Resonance = MakeFallback(TEXT("FallbackAbility_Caster_Resonance"));
    Resonance->AbilityId = TEXT("Caster.Resonance");
    Resonance->ClassId = EBreakerClassId::Caster;
    Resonance->DisplayName = FText::FromString(TEXT("Resonance"));
    Resonance->Description = FText::FromString(TEXT("Detonates every status on the target, consuming them."));
    Resonance->SlotAffinity = EBreakerAbilitySlot::ClassAbilityTwo;
    Resonance->Verb = EBreakerAbilityVerb::Weapon;
    Resonance->AbilityTag = BreakerAbilityTags::Ability_Class_Caster_Resonance;
    Resonance->AbilityClass = UBreakerAbility_Resonance::StaticClass();
    Registry.Add(Resonance);

    // UNMAKE — Class-Kits §2.2 ultimate: 80 Mana, no cooldown, 6s base window.
    UBreakerAbilityDefinition* Unmake = MakeFallback(TEXT("FallbackAbility_Caster_Unmake"));
    Unmake->AbilityId = TEXT("Caster.Unmake");
    Unmake->ClassId = EBreakerClassId::Caster;
    Unmake->DisplayName = FText::FromString(TEXT("Unmake"));
    Unmake->Description = FText::FromString(TEXT("Caster abilities cost nothing and Mana generation stops."));
    Unmake->SlotAffinity = EBreakerAbilitySlot::Ultimate;
    Unmake->Verb = EBreakerAbilityVerb::Weapon;
    Unmake->AbilityTag = BreakerAbilityTags::Ability_Class_Caster_Unmake;
    Unmake->AbilityClass = UBreakerAbility_Unmake::StaticClass();

    // Keystone variant rows (spec D1). Every duration and cost scalar in the
    // file is quoted from Class-Kits §2.2 — 6s/0% base, 12s/50% for Long
    // Dark. Edgework and Cascade change behavior, not parameters, so their
    // rows carry the base numbers and exist so the selector resolves them
    // rather than silently falling through to base.
    {
        FBreakerAbilityVariant BaseRow;
        BaseRow.VariantName = FText::FromString(TEXT("Unmake"));
        Unmake->Variants.Add(BaseRow);

        FBreakerAbilityVariant Edgework;
        Edgework.KeystoneTag = BreakerAbilityTags::Keystone_Caster_Edgework;
        Edgework.VariantName = FText::FromString(TEXT("Unmake - Edgework"));
        Unmake->Variants.Add(Edgework);

        FBreakerAbilityVariant LongDark;
        LongDark.KeystoneTag = BreakerAbilityTags::Keystone_Caster_LongDark;
        LongDark.VariantName = FText::FromString(TEXT("Unmake - Long Dark"));
        Unmake->Variants.Add(LongDark);

        FBreakerAbilityVariant Cascade;
        Cascade.KeystoneTag = BreakerAbilityTags::Keystone_Caster_Cascade;
        Cascade.VariantName = FText::FromString(TEXT("Unmake - Cascade"));
        Unmake->Variants.Add(Cascade);
    }
    Registry.Add(Unmake);

    // ==================================================================
    // GUNSMITH, TANK AND SUPPORT — IMPLEMENTED (owner authorization
    // 2026-08-16: "feel free to do all 5 classes").
    // ==================================================================
    // These twenty-one rows were registered as DATA ONLY with null
    // `AbilityClass`, and this block used to forbid nearest-fit pointing and
    // wait for "the day the abilities run". That day is this pass: every row
    // now names its real `UGameplayAbility` subclass
    // (Abilities/BreakerGunsmithAbilities.h / BreakerTankAbilities.h /
    // BreakerSupportAbilities.h), the minimal deployable system behind five of
    // them exists (Combat/BreakerDeployable.h — O30's hole, opened), the class
    // definitions are registered (Progression/BreakerProgressionLibrary.cpp)
    // AFTER the abilities execute — never before, which is the T7 ordering
    // that recreated the every-ability-reads-locked bug — and
    // `DefaultAbilityIdForSlot` below carries the three classes' starters.
    //
    // Costs and cooldowns remain QUOTED from the three class-kit documents
    // (per-row citations kept), all under the blanket O2 PLACEHOLDER banner:
    // the classes are PLAYABLE, not balanced. Behavioural gaps that could not
    // be built honestly (threat, stagger, status immunity, reload tempo, the
    // party layer) are recorded at each ability's own site, never faked.

    // ------------------------------------------------------------------
    // GUNSMITH — Scrap. Costs and cooldowns quoted from
    // Docs/Design/Class-Kits-Gunsmith.md §3.
    //
    // THE CLASS'S DEFINING ERGONOMIC IS VISIBLE IN THIS TABLE AND NOWHERE ELSE
    // IN CODE: the two Armory abilities cost NOTHING and carry a cooldown; the
    // four deployables cost Scrap and carry NO cooldown. Two clocks in one kit,
    // which is what makes "one of each" a real loadout shape rather than a
    // default. A future editor pass that "tidies" a cooldown onto a deployable
    // deletes the class's ergonomic, so the split is stated here as well as in
    // the design doc.
    // ------------------------------------------------------------------

    // G1 Sidearm Rig — §3 row G1: no cost, 10s cooldown. STARTER (Armory).
    UBreakerAbilityDefinition* SidearmRig = MakeFallback(TEXT("FallbackAbility_Gunsmith_SidearmRig"));
    SidearmRig->AbilityId = TEXT("Gunsmith.SidearmRig");
    SidearmRig->ClassId = EBreakerClassId::Gunsmith;
    SidearmRig->DisplayName = FText::FromString(TEXT("Sidearm Rig"));
    SidearmRig->Description = FText::FromString(TEXT("The next magazine deals bonus flat damage and pierces one more target."));
    SidearmRig->SlotAffinity = EBreakerAbilitySlot::ClassAbilityOne;
    SidearmRig->Verb = EBreakerAbilityVerb::Weapon;
    SidearmRig->AbilityTag = Ability_Class_Gunsmith_SidearmRig;
    SidearmRig->CooldownTag = Cooldown_Class_Gunsmith_SidearmRig;
    SidearmRig->AbilityClass = UBreakerAbility_SidearmRig::StaticClass();
    // windowDuration is 0 in the file DELIBERATELY. Sidearm Rig's window is
    // counted in SHOTS, not seconds — it ends when the magazine empties or on
    // reload, whichever comes first — and that is what makes it a
    // magazine-economy ability rather than a burst window. Authoring a
    // seconds value would be inventing a second, contradictory expiry.
    Registry.Add(SidearmRig);

    // G2 Overhaul — §3 row G2: no cost, 18s cooldown, 10s window.
    UBreakerAbilityDefinition* Overhaul = MakeFallback(TEXT("FallbackAbility_Gunsmith_Overhaul"));
    Overhaul->AbilityId = TEXT("Gunsmith.Overhaul");
    Overhaul->ClassId = EBreakerClassId::Gunsmith;
    Overhaul->DisplayName = FText::FromString(TEXT("Overhaul"));
    Overhaul->Description = FText::FromString(TEXT("Converts reserve ammunition into magazine capacity, and settles the unspent remainder back."));
    Overhaul->SlotAffinity = EBreakerAbilitySlot::ClassAbilityTwo;
    Overhaul->Verb = EBreakerAbilityVerb::Weapon;
    Overhaul->AbilityTag = Ability_Class_Gunsmith_Overhaul;
    Overhaul->CooldownTag = Cooldown_Class_Gunsmith_Overhaul;
    Overhaul->AbilityClass = UBreakerAbility_Overhaul::StaticClass();
    Registry.Add(Overhaul);

    // G3 Turret — §3 row G3: 40 Scrap, no cooldown, 30s emplacement lifetime
    // (the window, for the HUD). STARTER (Field Tech).
    UBreakerAbilityDefinition* Turret = MakeFallback(TEXT("FallbackAbility_Gunsmith_Turret"));
    Turret->AbilityId = TEXT("Gunsmith.Turret");
    Turret->ClassId = EBreakerClassId::Gunsmith;
    Turret->DisplayName = FText::FromString(TEXT("Turret"));
    Turret->Description = FText::FromString(TEXT("An emplacement that fires on the nearest target it can see. Consistent, never optimal."));
    Turret->SlotAffinity = EBreakerAbilitySlot::ClassAbilityTwo;
    Turret->Verb = EBreakerAbilityVerb::Weapon;
    Turret->AbilityTag = Ability_Class_Gunsmith_Turret;
    Turret->AbilityClass = UBreakerAbility_Turret::StaticClass();
    Registry.Add(Turret);

    // G4 Ammo Crate — §3 row G4: 30 Scrap, no cooldown.
    UBreakerAbilityDefinition* AmmoCrate = MakeFallback(TEXT("FallbackAbility_Gunsmith_AmmoCrate"));
    AmmoCrate->AbilityId = TEXT("Gunsmith.AmmoCrate");
    AmmoCrate->ClassId = EBreakerClassId::Gunsmith;
    AmmoCrate->DisplayName = FText::FromString(TEXT("Ammo Crate"));
    AmmoCrate->Description = FText::FromString(TEXT("A crate of reserve ammunition. You are a valid interactor with your own, at full value."));
    AmmoCrate->SlotAffinity = EBreakerAbilitySlot::ClassAbilityTwo;
    AmmoCrate->Verb = EBreakerAbilityVerb::Weapon;
    AmmoCrate->AbilityTag = Ability_Class_Gunsmith_AmmoCrate;
    AmmoCrate->AbilityClass = UBreakerAbility_AmmoCrate::StaticClass();
    Registry.Add(AmmoCrate);

    // G5 Mine Cluster — §3 row G5: 35 Scrap, no cooldown.
    UBreakerAbilityDefinition* MineCluster = MakeFallback(TEXT("FallbackAbility_Gunsmith_MineCluster"));
    MineCluster->AbilityId = TEXT("Gunsmith.MineCluster");
    MineCluster->ClassId = EBreakerClassId::Gunsmith;
    MineCluster->DisplayName = FText::FromString(TEXT("Mine Cluster"));
    MineCluster->Description = FText::FromString(TEXT("Three proximity charges that arm on a delay. The cluster is one placement, not three."));
    MineCluster->SlotAffinity = EBreakerAbilitySlot::ClassAbilityTwo;
    MineCluster->Verb = EBreakerAbilityVerb::Weapon;
    MineCluster->AbilityTag = Ability_Class_Gunsmith_MineCluster;
    MineCluster->AbilityClass = UBreakerAbility_MineCluster::StaticClass();
    Registry.Add(MineCluster);

    // G6 Disruptor — §3 row G6: 45 Scrap, no cooldown, 20s lifetime.
    UBreakerAbilityDefinition* Disruptor = MakeFallback(TEXT("FallbackAbility_Gunsmith_Disruptor"));
    Disruptor->AbilityId = TEXT("Gunsmith.Disruptor");
    Disruptor->ClassId = EBreakerClassId::Gunsmith;
    Disruptor->DisplayName = FText::FromString(TEXT("Disruptor"));
    Disruptor->Description = FText::FromString(TEXT("A field that slows enemies inside it and strips a flat amount of their armour."));
    Disruptor->SlotAffinity = EBreakerAbilitySlot::ClassAbilityTwo;
    Disruptor->Verb = EBreakerAbilityVerb::Weapon;
    Disruptor->AbilityTag = Ability_Class_Gunsmith_Disruptor;
    Disruptor->AbilityClass = UBreakerAbility_Disruptor::StaticClass();
    Registry.Add(Disruptor);

    // FIELD ASSEMBLY — §3 ultimate: 100 Scrap (full bar), no cooldown, 20s.
    UBreakerAbilityDefinition* FieldAssembly = MakeFallback(TEXT("FallbackAbility_Gunsmith_FieldAssembly"));
    FieldAssembly->AbilityId = TEXT("Gunsmith.FieldAssembly");
    FieldAssembly->ClassId = EBreakerClassId::Gunsmith;
    FieldAssembly->DisplayName = FText::FromString(TEXT("Field Assembly"));
    FieldAssembly->Description = FText::FromString(TEXT("Deploys every unlocked deployable type at once and raises the density cap for the window."));
    FieldAssembly->SlotAffinity = EBreakerAbilitySlot::Ultimate;
    FieldAssembly->Verb = EBreakerAbilityVerb::Weapon;
    FieldAssembly->AbilityTag = Ability_Class_Gunsmith_FieldAssembly;
    FieldAssembly->AbilityClass = UBreakerAbility_FieldAssembly::StaticClass();
    {
        // NOTE ON abilityCostMultiplier ACROSS ALL FOUR ROWS: it stays 1.0.
        // The field states what the window does to the price of the owner's
        // OTHER abilities, which is Unmake's mechanic. Field Assembly does not
        // discount casts — it performs one free mass placement at activation
        // and then raises a DENSITY CAP, and the cap is a deployable-system
        // concept with no field on this struct and no system behind it (O30).
        // Leaving the multiplier at 1.0 is the accurate statement; setting it
        // to 0 would silently make every subsequent Gunsmith cast free.
        FBreakerAbilityVariant BaseRow;
        BaseRow.VariantName = FText::FromString(TEXT("Field Assembly"));
        FieldAssembly->Variants.Add(BaseRow);

        // Machinist — the solo / no-deployable ultimate: places nothing and
        // applies every unlocked type's effect to the player instead. The most
        // bespoke of the fifteen keystone rewrites in the corpus; the class-kit
        // document authors the SHAPE of its per-type mapping table and
        // explicitly not its magnitudes, so no parametric delta is authored
        // here either.
        FBreakerAbilityVariant Machinist;
        Machinist.KeystoneTag = Keystone_Gunsmith_Machinist;
        Machinist.VariantName = FText::FromString(TEXT("Field Assembly - Machinist"));
        FieldAssembly->Variants.Add(Machinist);

        // Foundry — deployables placed during the window never expire (their
        // lifetime clock pauses). A lifetime pause is not a window duration, so
        // this row carries the base 20s and the behaviour is a named C++ branch
        // when the deployable system exists (spec D1's explicit allowance).
        FBreakerAbilityVariant Foundry;
        Foundry.KeystoneTag = Keystone_Gunsmith_Foundry;
        Foundry.VariantName = FText::FromString(TEXT("Field Assembly - Foundry"));
        FieldAssembly->Variants.Add(Foundry);

        // Minefield — placements are invisible and excluded from enemy
        // perception until they act. Behavioural, not parametric.
        FBreakerAbilityVariant Minefield;
        Minefield.KeystoneTag = Keystone_Gunsmith_Minefield;
        Minefield.VariantName = FText::FromString(TEXT("Field Assembly - Minefield"));
        FieldAssembly->Variants.Add(Minefield);
    }
    Registry.Add(FieldAssembly);

    // ------------------------------------------------------------------
    // TANK — Grit. Costs and cooldowns quoted from
    // Docs/Design/Class-Kits-Tank.md §2. Every Tank ability costs Grit AND
    // carries a cooldown, and the cooldown band (5-12s) is the longest of the
    // five classes because Tank abilities are the most survivability-dense —
    // the invulnerability audit leans on cooldown LENGTH as its primary guard,
    // so shortening one of these numbers is a safety change, not a feel change.
    // ------------------------------------------------------------------

    // T1 Rend — §2 row T1: 25 Grit, 6s. STARTER (Leech).
    UBreakerAbilityDefinition* Rend = MakeFallback(TEXT("FallbackAbility_Tank_Rend"));
    Rend->AbilityId = TEXT("Tank.Rend");
    Rend->ClassId = EBreakerClassId::Tank;
    Rend->DisplayName = FText::FromString(TEXT("Rend"));
    Rend->Description = FText::FromString(TEXT("Melee sweep that heals for a portion of the damage dealt; overheal becomes shield."));
    Rend->SlotAffinity = EBreakerAbilitySlot::ClassAbilityOne;
    Rend->Verb = EBreakerAbilityVerb::Weapon;
    Rend->AbilityTag = Ability_Class_Tank_Rend;
    Rend->CooldownTag = Cooldown_Class_Tank_Rend;
    Rend->AbilityClass = UBreakerAbility_Rend::StaticClass();
    Registry.Add(Rend);

    // T2 Bloodline — §2 row T2: 40 Grit, 12s, 8s window.
    UBreakerAbilityDefinition* Bloodline = MakeFallback(TEXT("FallbackAbility_Tank_Bloodline"));
    Bloodline->AbilityId = TEXT("Tank.Bloodline");
    Bloodline->ClassId = EBreakerClassId::Tank;
    Bloodline->DisplayName = FText::FromString(TEXT("Bloodline"));
    Bloodline->Description = FText::FromString(TEXT("Doubles your Life on Hit and extends it to damage-over-time ticks. Multiplies what you have; grants nothing if you have none."));
    Bloodline->SlotAffinity = EBreakerAbilitySlot::ClassAbilityTwo;
    Bloodline->Verb = EBreakerAbilityVerb::Reward;
    Bloodline->AbilityTag = Ability_Class_Tank_Bloodline;
    Bloodline->CooldownTag = Cooldown_Class_Tank_Bloodline;
    Bloodline->AbilityClass = UBreakerAbility_Bloodline::StaticClass();
    Registry.Add(Bloodline);

    // T3 Anchor Point — §2 row T3: 30 Grit, 10s, 12s lifetime. STARTER (Bastion).
    UBreakerAbilityDefinition* AnchorPoint = MakeFallback(TEXT("FallbackAbility_Tank_AnchorPoint"));
    AnchorPoint->AbilityId = TEXT("Tank.AnchorPoint");
    AnchorPoint->ClassId = EBreakerClassId::Tank;
    AnchorPoint->DisplayName = FText::FromString(TEXT("Anchor Point"));
    AnchorPoint->Description = FText::FromString(TEXT("A frontal cover panel with its own health. Blocks enemy fire; you and allies shoot through it from behind."));
    AnchorPoint->SlotAffinity = EBreakerAbilitySlot::ClassAbilityTwo;
    AnchorPoint->Verb = EBreakerAbilityVerb::Weapon;
    AnchorPoint->AbilityTag = Ability_Class_Tank_AnchorPoint;
    AnchorPoint->CooldownTag = Cooldown_Class_Tank_AnchorPoint;
    AnchorPoint->AbilityClass = UBreakerAbility_AnchorPoint::StaticClass();
    Registry.Add(AnchorPoint);

    // T4 Provoke — §2 row T4: 35 Grit, 12s, 4s forced-target window.
    UBreakerAbilityDefinition* Provoke = MakeFallback(TEXT("FallbackAbility_Tank_Provoke"));
    Provoke->AbilityId = TEXT("Tank.Provoke");
    Provoke->ClassId = EBreakerClassId::Tank;
    Provoke->DisplayName = FText::FromString(TEXT("Provoke"));
    Provoke->Description = FText::FromString(TEXT("Forces nearby enemies to target you, and each one provoked adds flat damage to your next seconds."));
    Provoke->SlotAffinity = EBreakerAbilitySlot::ClassAbilityTwo;
    Provoke->Verb = EBreakerAbilityVerb::Taunt;
    Provoke->AbilityTag = Ability_Class_Tank_Provoke;
    Provoke->CooldownTag = Cooldown_Class_Tank_Provoke;
    Provoke->AbilityClass = UBreakerAbility_Provoke::StaticClass();
    Registry.Add(Provoke);

    // T5 Breach Charge — §2 row T5: 30 Grit, 8s.
    UBreakerAbilityDefinition* BreachCharge = MakeFallback(TEXT("FallbackAbility_Tank_BreachCharge"));
    BreachCharge->AbilityId = TEXT("Tank.BreachCharge");
    BreachCharge->ClassId = EBreakerClassId::Tank;
    BreachCharge->DisplayName = FText::FromString(TEXT("Breach Charge"));
    BreachCharge->Description = FText::FromString(TEXT("Thrown explosive with a short fuse. Full control of the self-knockback; the self-damage is reduced and never zero."));
    BreachCharge->SlotAffinity = EBreakerAbilitySlot::ClassAbilityTwo;
    BreachCharge->Verb = EBreakerAbilityVerb::Weapon;
    BreachCharge->AbilityTag = Ability_Class_Tank_BreachCharge;
    BreachCharge->CooldownTag = Cooldown_Class_Tank_BreachCharge;
    BreachCharge->AbilityClass = UBreakerAbility_BreachCharge::StaticClass();
    Registry.Add(BreachCharge);

    // T6 Ground Zero — §2 row T6: 45 Grit, 10s.
    UBreakerAbilityDefinition* GroundZero = MakeFallback(TEXT("FallbackAbility_Tank_GroundZero"));
    GroundZero->AbilityId = TEXT("Tank.GroundZero");
    GroundZero->ClassId = EBreakerClassId::Tank;
    GroundZero->DisplayName = FText::FromString(TEXT("Ground Zero"));
    GroundZero->Description = FText::FromString(TEXT("Airborne slam that staggers, scaling with how far you fell. A normal jump is enough."));
    GroundZero->SlotAffinity = EBreakerAbilitySlot::ClassAbilityTwo;
    GroundZero->Verb = EBreakerAbilityVerb::Weapon;   // O179 by effect; O2
    GroundZero->AbilityTag = Ability_Class_Tank_GroundZero;
    GroundZero->CooldownTag = Cooldown_Class_Tank_GroundZero;
    GroundZero->AbilityClass = UBreakerAbility_GroundZero::StaticClass();
    Registry.Add(GroundZero);

    // HOLD — §2.1 ultimate: 100 Grit (full bar), no cooldown, 10s base window.
    UBreakerAbilityDefinition* Hold = MakeFallback(TEXT("FallbackAbility_Tank_Hold"));
    Hold->AbilityId = TEXT("Tank.Hold");
    Hold->ClassId = EBreakerClassId::Tank;
    Hold->DisplayName = FText::FromString(TEXT("Hold"));
    Hold->Description = FText::FromString(TEXT("Caps the damage any single hit can do to you, and triples Grit generation for the duration."));
    Hold->SlotAffinity = EBreakerAbilitySlot::Ultimate;
    Hold->Verb = EBreakerAbilityVerb::Taunt;   // O179 by effect; O2
    Hold->AbilityTag = Ability_Class_Tank_Hold;
    Hold->AbilityClass = UBreakerAbility_Hold::StaticClass();
    {
        // A PER-HIT CAP IS NOT DAMAGE REDUCTION, and the struct has no field for
        // one. That is the right outcome rather than a gap to paper over: the
        // cap is a `min` composed against other caps, never a product, and
        // expressing it as a multiplier here is precisely the bug the audit's
        // third guard exists to prevent. All four rows therefore carry the
        // window and nothing else, and the cap lands as a named C++ branch on a
        // hook that does not exist yet.
        FBreakerAbilityVariant BaseRow;
        BaseRow.VariantName = FText::FromString(TEXT("Hold"));
        Hold->Variants.Add(BaseRow);

        // Vein — the cap is REMOVED and incoming damage instead converts to
        // healing at a reduced rate: a damped attrition window, explicitly not
        // immunity. A Tank inside Vein still dies to enough incoming damage,
        // and that is the guard rather than a caveat.
        FBreakerAbilityVariant Vein;
        Vein.KeystoneTag = Keystone_Tank_Vein;
        Vein.VariantName = FText::FromString(TEXT("Hold - Vein"));
        Hold->Variants.Add(Vein);

        // Wall — the cap extends to nearby allies, and SOLO it is twice as
        // effective on the Tank. Party and solo are two different good outcomes
        // rather than a party bonus with a solo penalty, which is what the
        // party branch owing the strongest solo conversion actually means.
        FBreakerAbilityVariant Wall;
        Wall.KeystoneTag = Keystone_Tank_Wall;
        Wall.VariantName = FText::FromString(TEXT("Hold - Wall"));
        Hold->Variants.Add(Wall);

        // Detonation — ends early on command, releasing the absorbed damage as
        // a radial blast. The only ability in the game needing a second input
        // binding, and the one place the class exempts itself from its own
        // self-damage: the exemption is on the ultimate, not on a repeatable
        // ability, so the rocket case O13 governs is untouched.
        FBreakerAbilityVariant Detonation;
        Detonation.KeystoneTag = Keystone_Tank_Detonation;
        Detonation.VariantName = FText::FromString(TEXT("Hold - Detonation"));
        Hold->Variants.Add(Detonation);
    }
    Registry.Add(Hold);

    // ------------------------------------------------------------------
    // SUPPORT — Charge. Costs and cooldowns quoted from
    // Docs/Design/Class-Kits-Support.md §3. Costs run 20-40 and cooldowns 5-10s.
    // Mark is deliberately the cheapest and shortest: it is the loop's ignition,
    // and a loop whose ignition is expensive stalls at low Charge. Suppress is
    // the most expensive because it is the only ability with no generation
    // attached to it.
    // ------------------------------------------------------------------

    // U1 Patch — §3 row U1: 25 Charge, 6s. STARTER (Medic).
    UBreakerAbilityDefinition* Patch = MakeFallback(TEXT("FallbackAbility_Support_Patch"));
    Patch->AbilityId = TEXT("Support.Patch");
    Patch->ClassId = EBreakerClassId::Support;
    Patch->DisplayName = FText::FromString(TEXT("Patch"));
    Patch->Description = FText::FromString(TEXT("Heals the ally under your crosshair, or yourself with no target. The self-cast is worth exactly the same."));
    Patch->SlotAffinity = EBreakerAbilitySlot::ClassAbilityOne;
    Patch->Verb = EBreakerAbilityVerb::Reward;
    Patch->AbilityTag = Ability_Class_Support_Patch;
    Patch->CooldownTag = Cooldown_Class_Support_Patch;
    Patch->AbilityClass = UBreakerAbility_Patch::StaticClass();
    Registry.Add(Patch);

    // U2 Purge — §3 row U2: 30 Charge, 10s, 3s status immunity (the window,
    // which is the whole value).
    UBreakerAbilityDefinition* Purge = MakeFallback(TEXT("FallbackAbility_Support_Purge"));
    Purge->AbilityId = TEXT("Support.Purge");
    Purge->ClassId = EBreakerClassId::Support;
    Purge->DisplayName = FText::FromString(TEXT("Purge"));
    Purge->Description = FText::FromString(TEXT("Strips every status from the target and makes them immune to more for a moment. Self-castable."));
    Purge->SlotAffinity = EBreakerAbilitySlot::ClassAbilityTwo;
    Purge->Verb = EBreakerAbilityVerb::Movement;   // O179: cleansing rides the movement verb
    Purge->AbilityTag = Ability_Class_Support_Purge;
    Purge->CooldownTag = Cooldown_Class_Support_Purge;
    Purge->AbilityClass = UBreakerAbility_Purge::StaticClass();
    Registry.Add(Purge);

    // U3 Cadence — §3 row U3: 30 Charge, 8s, 8s aura.
    UBreakerAbilityDefinition* Cadence = MakeFallback(TEXT("FallbackAbility_Support_Cadence"));
    Cadence->AbilityId = TEXT("Support.Cadence");
    Cadence->ClassId = EBreakerClassId::Support;
    Cadence->DisplayName = FText::FromString(TEXT("Cadence"));
    Cadence->Description = FText::FromString(TEXT("An aura that follows you, improving reload and swap tempo for everyone inside it — starting with you."));
    Cadence->SlotAffinity = EBreakerAbilitySlot::ClassAbilityTwo;
    Cadence->Verb = EBreakerAbilityVerb::Weapon;   // O179 by effect; O2
    Cadence->AbilityTag = Ability_Class_Support_Cadence;
    Cadence->CooldownTag = Cooldown_Class_Support_Cadence;
    Cadence->AbilityClass = UBreakerAbility_Cadence::StaticClass();
    Registry.Add(Cadence);

    // U4 Metronome — §3 row U4: 35 Charge, 9s, 8s state.
    UBreakerAbilityDefinition* Metronome = MakeFallback(TEXT("FallbackAbility_Support_Metronome"));
    Metronome->AbilityId = TEXT("Support.Metronome");
    Metronome->ClassId = EBreakerClassId::Support;
    Metronome->DisplayName = FText::FromString(TEXT("Metronome"));
    Metronome->Description = FText::FromString(TEXT("Consecutive hits by anyone you have buffed build a cadence ramp. Each holder builds their own."));
    Metronome->SlotAffinity = EBreakerAbilitySlot::ClassAbilityTwo;
    Metronome->Verb = EBreakerAbilityVerb::Weapon;   // O179 by effect; O2
    Metronome->AbilityTag = Ability_Class_Support_Metronome;
    Metronome->CooldownTag = Cooldown_Class_Support_Metronome;
    Metronome->AbilityClass = UBreakerAbility_Metronome::StaticClass();
    Registry.Add(Metronome);

    // U5 Mark — §3 row U5: 20 Charge, 5s, 10s mark. STARTER (Warden).
    UBreakerAbilityDefinition* Mark = MakeFallback(TEXT("FallbackAbility_Support_Mark"));
    Mark->AbilityId = TEXT("Support.Mark");
    Mark->ClassId = EBreakerClassId::Support;
    Mark->DisplayName = FText::FromString(TEXT("Mark"));
    Mark->Description = FText::FromString(TEXT("Paints a target: it takes more damage from everyone, and the damage you do to it pays you Charge."));
    Mark->SlotAffinity = EBreakerAbilitySlot::ClassAbilityTwo;
    Mark->Verb = EBreakerAbilityVerb::Reward;
    Mark->AbilityTag = Ability_Class_Support_Mark;
    Mark->CooldownTag = Cooldown_Class_Support_Mark;
    Mark->AbilityClass = UBreakerAbility_Mark::StaticClass();
    Registry.Add(Mark);

    // U6 Suppress — §3 row U6: 40 Charge, 10s, 6s zone.
    UBreakerAbilityDefinition* Suppress = MakeFallback(TEXT("FallbackAbility_Support_Suppress"));
    Suppress->AbilityId = TEXT("Support.Suppress");
    Suppress->ClassId = EBreakerClassId::Support;
    Suppress->DisplayName = FText::FromString(TEXT("Suppress"));
    Suppress->Description = FText::FromString(TEXT("A zone that slows enemies and spoils their aim. It deals no damage at all."));
    Suppress->SlotAffinity = EBreakerAbilitySlot::ClassAbilityTwo;
    Suppress->Verb = EBreakerAbilityVerb::Weapon;   // O179 by effect; O2
    Suppress->AbilityTag = Ability_Class_Support_Suppress;
    Suppress->CooldownTag = Cooldown_Class_Support_Suppress;
    Suppress->AbilityClass = UBreakerAbility_Suppress::StaticClass();
    Registry.Add(Suppress);

    // CONDUIT — §3.1 ultimate: 100 Charge (full bar), no cooldown, 12s base.
    UBreakerAbilityDefinition* Conduit = MakeFallback(TEXT("FallbackAbility_Support_Conduit"));
    Conduit->AbilityId = TEXT("Support.Conduit");
    Conduit->ClassId = EBreakerClassId::Support;
    Conduit->DisplayName = FText::FromString(TEXT("Conduit"));
    Conduit->Description = FText::FromString(TEXT("For the duration your abilities cost nothing and reach every valid target near you — always including yourself."));
    Conduit->SlotAffinity = EBreakerAbilitySlot::Ultimate;
    Conduit->Verb = EBreakerAbilityVerb::Reward;
    Conduit->AbilityTag = Ability_Class_Support_Conduit;
    Conduit->AbilityClass = UBreakerAbility_Conduit::StaticClass();
    {
        // THE ONE ULTIMATE OF THE THREE WHOSE KEYSTONES DIFFER PARAMETRICALLY,
        // and the difference is real rather than cosmetic: two of the three
        // rewrites STOP the free-cast window, because they replace casting
        // rather than enabling it. That is exactly what AbilityCostMultiplier
        // exists to say (Caster's Unmake authors 0.0 and 0.5 through the same
        // field), so these rows carry behaviour and not just identity.
        //
        // Base — free casts, breadth not spam: cooldowns still apply, so the
        // window removes the cost gate and not the cadence gate. Generation
        // deliberately CONTINUES, which partially refunds the ultimate and,
        // bounded by those cooldowns, cannot fully refund it.
        FBreakerAbilityVariant BaseRow;
        BaseRow.VariantName = FText::FromString(TEXT("Conduit"));
        Conduit->Variants.Add(BaseRow);

        // Triage — becomes a continuous healing field with one lethal-hit save
        // per target, and STOPS enabling free casts. Hence 1.0, not 0.0: the
        // defensive ultimate buys a field, not a spending spree, and authoring
        // 0.0 here would quietly hand it both.
        FBreakerAbilityVariant Triage;
        Triage.KeystoneTag = Keystone_Support_Triage;
        Triage.VariantName = FText::FromString(TEXT("Conduit - Triage"));
        Conduit->Variants.Add(Triage);

        // Downbeat — keeps the free casts and doubles the cadence effects,
        // adding flat weapon damage for every buffed target. Flat is
        // load-bearing: it enters the flat-sum stage before the additive
        // Increased bucket and therefore cannot double-dip with gear.
        FBreakerAbilityVariant Downbeat;
        Downbeat.KeystoneTag = Keystone_Support_Downbeat;
        Downbeat.VariantName = FText::FromString(TEXT("Conduit - Downbeat"));
        Conduit->Variants.Add(Downbeat);

        // Blackout — marks and suppresses every enemy in radius INSTEAD of
        // casting abilities, so the free-cast discount is meaningless and is
        // not authored. The control ultimate, and the one that turns a full bar
        // straight into damage.
        FBreakerAbilityVariant Blackout;
        Blackout.KeystoneTag = Keystone_Support_Blackout;
        Blackout.VariantName = FText::FromString(TEXT("Conduit - Blackout"));
        Conduit->Variants.Add(Blackout);
    }
    Registry.Add(Conduit);

    // The numbers, last, once every row and variant the file has to name
    // exists. Nothing above this line authors a cost, a cooldown or a window.
    BreakerAbilityDataApply(Registry);
    return Registry;
}

UBreakerAbilityDefinition* UBreakerAbilityDefinition::FindFallback(FName InAbilityId)
{
    if (InAbilityId.IsNone())
    {
        return nullptr;
    }
    for (UBreakerAbilityDefinition* Definition : GetFallbackRegistry())
    {
        if (Definition && Definition->AbilityId == InAbilityId)
        {
            return Definition;
        }
    }
    return nullptr;
}

TArray<UBreakerAbilityDefinition*> UBreakerAbilityDefinition::GetClassAbilities(EBreakerClassId ClassId, EBreakerAbilitySlot Slot)
{
    TArray<UBreakerAbilityDefinition*> Result;
    if (ClassId == EBreakerClassId::None) return Result;
    for (UBreakerAbilityDefinition* Definition : GetFallbackRegistry())
    {
        // CanOccupySlot is the slot rule in one place: an ultimate may only sit
        // in the ultimate slot, and a class ability may sit in either of the
        // two. Reading it here means the picker and the equip validation cannot
        // disagree about what fits where.
        if (Definition && Definition->ClassId == ClassId && Definition->CanOccupySlot(Slot))
        {
            Result.Add(Definition);
        }
    }
    return Result;
}

TArray<FName> UBreakerAbilityDefinition::GetClassAbilityIds(EBreakerClassId ClassId, EBreakerAbilitySlot Slot)
{
    TArray<FName> Ids;
    for (const UBreakerAbilityDefinition* Definition : GetClassAbilities(ClassId, Slot))
    {
        Ids.Add(Definition->AbilityId);
    }
    return Ids;
}

bool UBreakerAbilityDefinition::ClassGrantsAbility(EBreakerClassId ClassId, FName AbilityId)
{
    if (ClassId == EBreakerClassId::None || AbilityId.IsNone()) return false;
    const UBreakerAbilityDefinition* Definition = FindFallback(AbilityId);
    return Definition && Definition->ClassId == ClassId;
}

bool UBreakerAbilityDefinition::ClassHasImplementedKit(EBreakerClassId ClassId)
{
    if (ClassId == EBreakerClassId::None) return false;
    // ONE implemented ability is enough to say a kit exists — the same
    // any-of shape the class screen's own gate uses — because the question is
    // whether locking into this class grants a player anything that runs, not
    // whether the kit is finished. Deliberately does NOT consult
    // GetFallbackClassDefinition: that is the progression layer's separate
    // gate on the same decision, and two independent answers to "is this class
    // real" is how the Caster null-definition bug stayed invisible.
    for (const UBreakerAbilityDefinition* Definition : GetFallbackRegistry())
    {
        if (Definition && Definition->ClassId == ClassId && Definition->IsImplemented()) return true;
    }
    return false;
}

FName UBreakerAbilityDefinition::DefaultAbilityIdForSlot(EBreakerClassId ClassId, EBreakerAbilitySlot Slot)
{
    // This is the whole reachability chain for a class with no authored class
    // definition: UBreakerProgressionComponent::ChoosePermanentClassById leaves
    // the loadout FNames as None for anything but Swift, and
    // UBreakerAbilityComponent::ResolveDefinition then asks here. A class
    // missing from this switch has three dead keys.
    switch (ClassId)
    {
    // Ruling 1 (ORDERS, overturning O176-as-written): Swift's second slot
    // ships EMPTY until the first quartermaster unlock — the empty slot is
    // the feature, the first thing the token fills. NAME_None here means a
    // stale foreign-class id in slot two now repairs to EMPTY rather than
    // to a playable default; for Swift that is the ruled shape, not a
    // regression of the stale-save repair.
    case EBreakerClassId::Swift:
        switch (Slot)
        {
        case EBreakerAbilitySlot::ClassAbilityOne: return TEXT("Swift.Skim");
        case EBreakerAbilitySlot::ClassAbilityTwo: return NAME_None;
        case EBreakerAbilitySlot::Ultimate:        return TEXT("Swift.Overdrive");
        default: return NAME_None;
        }
    case EBreakerClassId::Caster:
        switch (Slot)
        {
        // Class-Kits §2.2 names Cleave and Rot as the Caster STARTERS, so they
        // are what an unchosen loadout is handed. This is now a DEFAULT and no
        // longer the only answer: GetClassAbilities is the catalogue,
        // UBreakerAbilityComponent::TryEquipAbility is how a choice is made,
        // and ResolveDefinition prefers the chosen id over this row. Editing
        // this line changes what a fresh character starts with — it no longer
        // decides which four abilities are unreachable.
        case EBreakerAbilitySlot::ClassAbilityOne: return TEXT("Caster.Cleave");
        case EBreakerAbilitySlot::ClassAbilityTwo: return TEXT("Caster.Rot");
        case EBreakerAbilitySlot::Ultimate:        return TEXT("Caster.Unmake");
        default: return NAME_None;
        }
    // The three formerly-unbuilt classes (implemented 2026-08-16, owner
    // authorization). Each default pair is the kit's two STARTERS, sitting on
    // opposite sides of the class tension exactly as the treatments name them:
    // Gunsmith holds a gun buff and a turret (Class-Kits-Gunsmith §3), Tank
    // holds sustain and position (Class-Kits-Tank §2), Support holds a heal
    // and a mark (Class-Kits-Support §3).
    case EBreakerClassId::Gunsmith:
        switch (Slot)
        {
        case EBreakerAbilitySlot::ClassAbilityOne: return TEXT("Gunsmith.SidearmRig");
        case EBreakerAbilitySlot::ClassAbilityTwo: return TEXT("Gunsmith.Turret");
        case EBreakerAbilitySlot::Ultimate:        return TEXT("Gunsmith.FieldAssembly");
        default: return NAME_None;
        }
    case EBreakerClassId::Tank:
        switch (Slot)
        {
        case EBreakerAbilitySlot::ClassAbilityOne: return TEXT("Tank.Rend");
        case EBreakerAbilitySlot::ClassAbilityTwo: return TEXT("Tank.AnchorPoint");
        case EBreakerAbilitySlot::Ultimate:        return TEXT("Tank.Hold");
        default: return NAME_None;
        }
    case EBreakerClassId::Support:
        switch (Slot)
        {
        case EBreakerAbilitySlot::ClassAbilityOne: return TEXT("Support.Patch");
        case EBreakerAbilitySlot::ClassAbilityTwo: return TEXT("Support.Mark");
        case EBreakerAbilitySlot::Ultimate:        return TEXT("Support.Conduit");
        default: return NAME_None;
        }
    default:
        return NAME_None;
    }
}
