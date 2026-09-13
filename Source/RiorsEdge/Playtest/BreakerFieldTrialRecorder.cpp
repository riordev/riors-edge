#include "Playtest/BreakerFieldTrialRecorder.h"
#include "Characters/BreakerCharacter.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Abilities/BreakerGameplayAbility.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Combat/BreakerCombatComponent.h"
#include "Game/BreakerGameMode.h"
#include "Items/BreakerEquipmentComponent.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Save/BreakerQuestJournal.h"
#include "Save/BreakerMissionContent.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/Parse.h"

bool UBreakerFieldTrialRecorder::ShouldCreateSubsystem(UObject* Outer) const
{
#if UE_BUILD_SHIPPING
    return false;
#else
    return Super::ShouldCreateSubsystem(Outer) && FParse::Param(FCommandLine::Get(), TEXT("BreakerFieldTrial"));
#endif
}

void UBreakerFieldTrialRecorder::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    StartedAt = FPlatformTime::Seconds();
    const FString Directory = FPaths::ProjectSavedDir() / TEXT("FieldTrials");
    IFileManager::Get().MakeDirectory(*Directory, true);
    OutputPath = Directory / (FGuid::NewGuid().ToString(EGuidFormats::Digits) + TEXT(".jsonl"));
    UE_LOG(LogTemp, Display, TEXT("[FieldTrial] observing ordinary play: %s"), *OutputPath);
    Ticker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &ThisClass::Sample), 1.0f);
}

void UBreakerFieldTrialRecorder::UnbindPlayer()
{
    if (Player.IsValid() && Player->GetCombat())
    {
        Player->GetCombat()->OnDeath.RemoveDynamic(this, &ThisClass::RecordDeath);
        Player->GetCombat()->OnKillDealt.RemoveDynamic(this, &ThisClass::RecordKill);
    }
    Player.Reset();
}

void UBreakerFieldTrialRecorder::Deinitialize()
{
    Sample(0);
    FTSTicker::GetCoreTicker().RemoveTicker(Ticker);
    UnbindPlayer();
    Super::Deinitialize();
}

void UBreakerFieldTrialRecorder::RecordDeath() { ++Deaths; Sample(0); }
void UBreakerFieldTrialRecorder::RecordKill(const FBreakerHitContext& Hit) { ++Kills; }
void UBreakerFieldTrialRecorder::RecordInterruptedCast() { ++InterruptedCasts; }

bool UBreakerFieldTrialRecorder::Sample(float DeltaSeconds)
{
    UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
    APlayerController* Controller = World ? World->GetFirstPlayerController() : nullptr;
    auto* Current = Controller ? Cast<ABreakerCharacter>(Controller->GetPawn()) : nullptr;
    if (Current != Player.Get())
    {
        UnbindPlayer();
        Player = Current;
        if (Current && Current->GetCombat())
        {
            Current->GetCombat()->OnDeath.AddUniqueDynamic(this, &ThisClass::RecordDeath);
            Current->GetCombat()->OnKillDealt.AddUniqueDynamic(this, &ThisClass::RecordKill);
        }
    }
    if (!Current || !Current->GetAttributes() || !Current->GetProgression()) return true;
    const auto* Stats = Current->GetAttributes();
    const auto* Progression = Current->GetProgression();
    const auto* Mode = Cast<ABreakerGameMode>(World->GetAuthGameMode());
    TSharedRef<FJsonObject> Row = MakeShared<FJsonObject>();
    Row->SetNumberField(TEXT("seconds"), FPlatformTime::Seconds() - StartedAt);
    Row->SetStringField(TEXT("map"), World->GetMapName());
    Row->SetNumberField(TEXT("level"), Progression->GetCharacterLevel());
    Row->SetNumberField(TEXT("xp"), Progression->GetTotalExperience());
    Row->SetNumberField(TEXT("health"), Stats->GetHealth());
    Row->SetNumberField(TEXT("max_health"), Stats->GetMaxHealth());
    Row->SetNumberField(TEXT("resource"), Stats->GetClassResource());
    if (const auto* Move = Current->GetBreakerMovement())
    {
        Row->SetNumberField(TEXT("ground_speed"), Move->Velocity.Size2D());
        Row->SetNumberField(TEXT("walk_cap"), Move->GetWalkSpeedCap());
        Row->SetNumberField(TEXT("sprint_cap"), Move->GetSprintSpeedCap());
        Row->SetNumberField(TEXT("move_multiplier"), Move->GetComposedMoveSpeedMultiplier());
        Row->SetNumberField(TEXT("movement_mode"), static_cast<int32>(Move->MovementMode.GetValue()));
    }
    Row->SetNumberField(TEXT("cast_rate"), UBreakerGameplayAbility::AbilityCastRateMultiplierFor(Current));
    Row->SetNumberField(TEXT("core_points"), Progression->GetUnspentPoints(EBreakerPointCurrency::CorePoints));
    Row->SetNumberField(TEXT("doctrine_points"), Progression->GetUnspentPoints(EBreakerPointCurrency::DoctrinePoints));
    Row->SetNumberField(TEXT("deaths"), Deaths);
    Row->SetNumberField(TEXT("kills"), Kills);
    Row->SetNumberField(TEXT("interrupted_casts"), InterruptedCasts);
    Row->SetNumberField(TEXT("wave"), Mode ? Mode->GetCurrentWave() : 0);
    Row->SetNumberField(TEXT("area_level"), Mode ? Mode->GetAreaLevelForWave(Mode->GetCurrentWave()) : 0);
    Row->SetBoolField(TEXT("rift_complete"), Mode && Mode->IsRiftRunCompleted());
    if (const auto* Journal = Current->GetQuestJournal())
    {
        Row->SetStringField(TEXT("objective"), FString::Join(UBreakerMissionLibrary::TrackerLines(Journal->GetState()), TEXT(" | ")));
        TArray<TSharedPtr<FJsonValue>> Flags;
        for (FName Flag : Journal->GetState().Flags) Flags.Add(MakeShared<FJsonValueString>(Flag.ToString()));
        Row->SetArrayField(TEXT("quest_flags"), Flags);
    }
    if (const auto* Equipment = Current->GetEquipment())
    {
        Row->SetNumberField(TEXT("backpack_items"), Equipment->GetBackpack().Num());
        TArray<TSharedPtr<FJsonValue>> Items;
        for (const FBreakerItemInstance& Item : Equipment->GetEquipped())
        {
            TSharedRef<FJsonObject> Gear = MakeShared<FJsonObject>();
            Gear->SetStringField(TEXT("definition"), Item.DefinitionId.ToString());
            Gear->SetNumberField(TEXT("item_level"), Item.ItemLevel);
            Gear->SetNumberField(TEXT("slot"), static_cast<int32>(Item.Slot));
            Items.Add(MakeShared<FJsonValueObject>(Gear));
        }
        Row->SetArrayField(TEXT("equipped"), Items);
    }
    FString Line;
    const auto Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Line);
    FJsonSerializer::Serialize(Row, Writer);
    if (!FFileHelper::SaveStringToFile(Line + TEXT("\n"), *OutputPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), FILEWRITE_Append))
    {
        UE_LOG(LogTemp, Warning, TEXT("[FieldTrial] cannot write %s; recording stopped"), *OutputPath);
        return false;
    }
    return true;
}
