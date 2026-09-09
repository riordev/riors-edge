#include "Interaction/BreakerBasinRecorder.h"
#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerCombatComponent.h"
#include "Game/BreakerPrototypeDestinations.h"
#include "Save/BreakerQuestJournal.h"
#include "Net/UnrealNetwork.h"
ABreakerBasinRecorder::ABreakerBasinRecorder()
{
    bReplicates=true;ConfigureConsoleBody();DialogueId=NAME_None;
    DisplayName=FText::FromString(TEXT("Homestead survey recorder"));
    Tags.Add(TEXT("RedBasin.Recorder"));
    InteractionRange=220.f; // O2 PLACEHOLDER, centimetres; matches close console use.
}
void ABreakerBasinRecorder::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{ Super::GetLifetimeReplicatedProps(OutLifetimeProps);DOREPLIFETIME(ABreakerBasinRecorder,bExtraction); }
void ABreakerBasinRecorder::ConfigureExtraction()
{ if(HasAuthority()){bExtraction=true;DisplayName=FText::FromString(TEXT("Survey extraction relay"));} }
FName ABreakerBasinRecorder::RecoveredFlag(){return TEXT("Quest.RedBasin.RecorderRecovered");}
FName ABreakerBasinRecorder::ExtractedFlag(){return TEXT("Quest.RedBasin.RecorderExtracted");}
bool ABreakerBasinRecorder::IsCurrentStep(const ABreakerCharacter* Player) const
{
    const auto* Journal=IsValid(Player)?Player->GetQuestJournal():nullptr;
    if(!Journal||Journal->HasFlag(ExtractedFlag()))return false;
    return bExtraction ? Journal->HasFlag(RecoveredFlag()) : !Journal->HasFlag(RecoveredFlag());
}
bool ABreakerBasinRecorder::IsInteractionReachable(const ABreakerCharacter* Player) const
{
    const auto* Destination=BreakerPrototypeDestinations::ForWorld(this);
    if(IsActorBeingDestroyed()||!Destination||Destination->Id!=TEXT("RedBasin")||!IsValid(Player)
        ||Player->GetWorld()!=GetWorld()||!Player->GetCombat()||Player->GetCombat()->IsDead()||!IsCurrentStep(Player)
        ||FVector::DistSquared(Player->GetActorLocation(),GetActorLocation())>FMath::Square(InteractionRange))return false;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(BasinRecorderLOS),false,Player);Query.AddIgnoredActor(this);
    FHitResult Hit;
    return !GetWorld()->LineTraceSingleByObjectType(Hit,Player->GetActorLocation(),GetActorLocation(),FCollisionObjectQueryParams(ECC_WorldStatic),Query);
}
bool ABreakerBasinRecorder::TryInteract(ABreakerCharacter* Player)
{
    if(!HasAuthority()||!IsInteractionReachable(Player)||!Player->HasAuthority())return false;
    // SetFlag commits before persistence callbacks and refuses duplicates;
    // carried state is durable, independent of backpack or actor lifetime.
    return Player->GetQuestJournal()->SetFlag(bExtraction?ExtractedFlag():RecoveredFlag());
}
FText ABreakerBasinRecorder::GetRecorderPrompt() const
{ return FText::FromString(bExtraction?TEXT("EXTRACT SURVEY RECORDER"):TEXT("RECOVER SURVEY RECORDER")); }
