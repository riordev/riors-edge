#include "Combat/BreakerStatusRules.h"

#include "Data/BreakerDataFile.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

// ---------------------------------------------------------------------------
// THE LOADER — Data/statuses.json into the rule rows and the payload fraction.
// ---------------------------------------------------------------------------
// The same shape as the affix library's loader: every complaint is collected,
// and any complaint fails the WHOLE load. A file that dropped one row and
// served the rest would let a status the design meant to spread stop at the
// first body, silently.
namespace
{
    using BreakerDataFile::FBreakerDataErrors;

    struct FBreakerStatusRuleLoad
    {
        TArray<FBreakerStatusRule> Rules;
        // Zero until the file loads clean: with no rules there is nothing to
        // spread, and a zero payload is the honest reading of "no file".
        float PierceSpreadPayloadFraction = 0.0f;
        TArray<FString> Errors;
    };

    bool BreakerStatusRuleReadRow(const FJsonObject& Row, FBreakerStatusRule& Out, FBreakerDataErrors& Errors)
    {
        FString TagName;
        if (!Row.TryGetStringField(TEXT("tag"), TagName) || TagName.IsEmpty())
        {
            Errors.Add(TEXT("a row has no \"tag\""));
            return false;
        }
        // Registered tags only: a row for a tag nothing can apply is a comment.
        Out.Tag = FGameplayTag::RequestGameplayTag(FName(*TagName), false);
        bool bOk = true;
        if (!Out.Tag.IsValid())
        {
            Errors.Add(FString::Printf(TEXT("%s: not a registered gameplay tag"), *TagName));
            bOk = false;
        }
        if (!Row.TryGetBoolField(TEXT("spreadsOnPierce"), Out.bSpreadsOnPierce))
        {
            Errors.Add(FString::Printf(TEXT("%s: \"spreadsOnPierce\" is missing or not a boolean"), *TagName));
            bOk = false;
        }
        auto ReadTuning = [&](const TCHAR* Key, float& OutValue, double Maximum)
        {
            if (!Row.HasField(Key)) return;
            double Value = 0;
            if (!Row.TryGetNumberField(Key, Value) || !FMath::IsFinite(Value) || Value < 0 || Value > Maximum)
            {
                Errors.Add(FString::Printf(TEXT("%s: invalid %s"), *TagName, Key)); bOk = false;
            }
            else OutValue = static_cast<float>(Value);
        };
        if (Row.HasField(TEXT("dealsPeriodicDamage")) && !Row.TryGetBoolField(TEXT("dealsPeriodicDamage"), Out.bDealsPeriodicDamage))
        { Errors.Add(FString::Printf(TEXT("%s: dealsPeriodicDamage must be boolean"), *TagName)); bOk = false; }
        if (Row.HasField(TEXT("dealsDamageOnExpiry")) && !Row.TryGetBoolField(TEXT("dealsDamageOnExpiry"), Out.bDealsDamageOnExpiry))
        { Errors.Add(FString::Printf(TEXT("%s: dealsDamageOnExpiry must be boolean"), *TagName)); bOk = false; }
        if (Row.HasField(TEXT("dealsDamageOnApplication")) && !Row.TryGetBoolField(TEXT("dealsDamageOnApplication"), Out.bDealsDamageOnApplication))
        { Errors.Add(FString::Printf(TEXT("%s: dealsDamageOnApplication must be boolean"), *TagName)); bOk = false; }
        if (static_cast<int32>(Out.bDealsPeriodicDamage) + static_cast<int32>(Out.bDealsDamageOnExpiry) + static_cast<int32>(Out.bDealsDamageOnApplication) > 1)
        { Errors.Add(FString::Printf(TEXT("%s: status damage modes are mutually exclusive"), *TagName)); bOk = false; }
        ReadTuning(TEXT("durationSeconds"), Out.DurationSeconds, 3600);
        ReadTuning(TEXT("armorReductionPercent"), Out.ArmorReductionPercent, 100);
        ReadTuning(TEXT("healingReductionPercent"), Out.HealingReductionPercent, 100);
        if (Out.IsNonDamagingDebuff() && Out.DurationSeconds <= 0)
        { Errors.Add(FString::Printf(TEXT("%s: timed debuff requires positive durationSeconds"), *TagName)); bOk = false; }
        return bOk;
    }

    FBreakerStatusRuleLoad BreakerStatusRuleLoadData()
    {
        FBreakerStatusRuleLoad Load;
        FBreakerDataErrors Errors;
        TArray<FBreakerStatusRule> Rules;
        float Fraction = 0.0f;
        const FString File = BreakerStatusRules::DataRelativePath();

        const TSharedPtr<FJsonObject> Root = BreakerDataFile::Load(File, Errors);
        if (Root.IsValid())
        {
            const TArray<TSharedPtr<FJsonValue>>* RowValues = nullptr;
            if (!Root->TryGetArrayField(TEXT("statuses"), RowValues))
            {
                Errors.Add(FString::Printf(TEXT("%s: no \"statuses\" array"), *File));
            }
            else
            {
                for (const TSharedPtr<FJsonValue>& Value : *RowValues)
                {
                    const TSharedPtr<FJsonObject>* RowObject = nullptr;
                    if (!Value.IsValid() || !Value->TryGetObject(RowObject))
                    {
                        Errors.Add(FString::Printf(TEXT("%s: a \"statuses\" entry is not an object"), *File));
                        continue;
                    }
                    FBreakerStatusRule Rule;
                    if (BreakerStatusRuleReadRow(**RowObject, Rule, Errors))
                    {
                        if (Rules.ContainsByPredicate([&Rule](const FBreakerStatusRule& Other) { return Other.Tag == Rule.Tag; }))
                        {
                            Errors.Add(FString::Printf(TEXT("%s: tag appears twice"), *Rule.Tag.ToString()));
                            continue;
                        }
                        Rules.Add(Rule);
                    }
                }
            }

            // In (0, 1]: a spread copy is never a fresh full application and
            // never more than one; zero would be a row that spreads nothing,
            // which is what the spreadsOnPierce column is for.
            double Value = 0.0;
            if (!Root->TryGetNumberField(TEXT("pierceSpreadPayloadFraction"), Value))
            {
                Errors.Add(FString::Printf(TEXT("%s: no \"pierceSpreadPayloadFraction\" number"), *File));
            }
            else if (!(Value > 0.0) || Value > 1.0)
            {
                Errors.Add(FString::Printf(TEXT("%s: pierceSpreadPayloadFraction %g is not in (0, 1]"), *File, Value));
            }
            else
            {
                Fraction = static_cast<float>(Value);
            }
        }

        if (!Errors.IsClean())
        {
            Load.Errors = Errors.Messages;
            ensureMsgf(false, TEXT("%s failed to load; the status rules are EMPTY and nothing spreads.\n%s"), *File, *Errors.Join());
            return Load;
        }
        Load.Rules = MoveTemp(Rules);
        Load.PierceSpreadPayloadFraction = Fraction;
        return Load;
    }

    const FBreakerStatusRuleLoad& BreakerStatusRuleLoaded()
    {
        static const FBreakerStatusRuleLoad Load = BreakerStatusRuleLoadData();
        return Load;
    }
}

FString BreakerStatusRules::DataRelativePath()
{
    return TEXT("Data/statuses.json");
}

const TArray<FBreakerStatusRule>& BreakerStatusRules::GetRules()
{
    return BreakerStatusRuleLoaded().Rules;
}

const TArray<FString>& BreakerStatusRules::GetDataErrors()
{
    return BreakerStatusRuleLoaded().Errors;
}

const FBreakerStatusRule* BreakerStatusRules::FindRule(FGameplayTag Tag)
{
    if (!Tag.IsValid()) return nullptr;
    return GetRules().FindByPredicate([Tag](const FBreakerStatusRule& Rule) { return Rule.Tag == Tag; });
}

float BreakerStatusRules::PierceSpreadPayloadFraction()
{
    return BreakerStatusRuleLoaded().PierceSpreadPayloadFraction;
}
