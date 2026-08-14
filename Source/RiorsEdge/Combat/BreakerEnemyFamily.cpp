#include "Combat/BreakerEnemyFamily.h"

FString UBreakerEnemyFamilyLibrary::GetFamilyName(EBreakerEnemyFamily Family)
{
    return Family == EBreakerEnemyFamily::Altered ? TEXT("ALTERED") : TEXT("VESTIGE");
}

FString UBreakerEnemyFamilyLibrary::GetSeveranceStageName(EBreakerSeveranceStage Stage)
{
    switch (Stage)
    {
    case EBreakerSeveranceStage::Early: return TEXT("EARLY SEVERANCE");
    case EBreakerSeveranceStage::Mid:   return TEXT("SEVERED");
    case EBreakerSeveranceStage::Late:  return TEXT("LATE SEVERANCE");
    case EBreakerSeveranceStage::NotApplicable:
    default:                            return FString();
    }
}

FString UBreakerEnemyFamilyLibrary::GetFamilyBanner(EBreakerEnemyFamily Family, EBreakerSeveranceStage Stage)
{
    const FString FamilyText = GetFamilyName(Family);
    // A Vestige has no stage and must never print one: the whole point of the
    // family split is that a Vestige was never a person and therefore never
    // degraded from anything.
    if (Family != EBreakerEnemyFamily::Altered) return FamilyText;
    const FString StageText = GetSeveranceStageName(Stage);
    return StageText.IsEmpty() ? FamilyText : FamilyText + TEXT(" - ") + StageText;
}

bool UBreakerEnemyFamilyLibrary::StageUsesCover(EBreakerEnemyFamily Family, EBreakerSeveranceStage Stage)
{
    // A Vestige NEVER uses cover, whatever stage is set on it. §1.5 is
    // explicit: "no tactics that resemble a military". Making this a hard
    // family gate rather than a stage lookup means a content author cannot
    // accidentally produce a rift-native creature with cover discipline.
    if (Family != EBreakerEnemyFamily::Altered) return false;
    return Stage == EBreakerSeveranceStage::Early;
}

bool UBreakerEnemyFamilyLibrary::StageFlinches(EBreakerEnemyFamily Family, EBreakerSeveranceStage Stage)
{
    // Same gate, same reason. A flinch is a person's reaction to being shot.
    if (Family != EBreakerEnemyFamily::Altered) return false;
    return Stage == EBreakerSeveranceStage::Early;
}
