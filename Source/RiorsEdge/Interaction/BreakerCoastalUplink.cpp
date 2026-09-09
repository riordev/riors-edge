#include "Interaction/BreakerCoastalUplink.h"
#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerCombatComponent.h"
#include "Game/BreakerPrototypeDestinations.h"
#include "Save/BreakerQuestJournal.h"
#include "Net/UnrealNetwork.h"

ABreakerCoastalUplink::ABreakerCoastalUplink()
{
    bReplicates=true;ConfigureConsoleBody();DialogueId=NAME_None;
    DisplayName=FText::FromString(TEXT("Coastal uplink"));Tags.Add(TEXT("BrokenCoast.Uplink"));
    InteractionRange=220.f; // O2 PLACEHOLDER, ordinary close console interaction.
    PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.bStartWithTickEnabled=false;
}
FName ABreakerCoastalUplink::CompletionFlag(){return TEXT("Quest.BrokenCoast.UplinkRestored");}
void ABreakerCoastalUplink::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps)const
{Super::GetLifetimeReplicatedProps(OutLifetimeProps);DOREPLIFETIME(ABreakerCoastalUplink,TransmittingPlayer);DOREPLIFETIME(ABreakerCoastalUplink,RemainingSeconds);}
bool ABreakerCoastalUplink::IsCompleteFor(const ABreakerCharacter* Player)const
{return IsValid(Player)&&Player->GetQuestJournal()&&Player->GetQuestJournal()->HasFlag(CompletionFlag());}
bool ABreakerCoastalUplink::IsAliveNearAndVisible(const ABreakerCharacter* Player,float Radius)const
{
    const auto* Destination=BreakerPrototypeDestinations::ForWorld(this);
    if(IsActorBeingDestroyed()||!Destination||Destination->Id!=TEXT("BrokenCoast")||!IsValid(Player)
        ||Player->IsActorBeingDestroyed()||Player->GetWorld()!=GetWorld()||!Player->GetCombat()||Player->GetCombat()->IsDead()
        ||!FMath::IsFinite(Radius)||Radius<=0||FVector::DistSquared(Player->GetActorLocation(),GetActorLocation())>FMath::Square(Radius))return false;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(CoastalUplinkLOS),false,Player);Query.AddIgnoredActor(this);
    FHitResult Hit;
    return !GetWorld()->LineTraceSingleByObjectType(Hit,Player->GetActorLocation(),GetActorLocation(),FCollisionObjectQueryParams(ECC_WorldStatic),Query);
}
bool ABreakerCoastalUplink::IsInteractionReachable(const ABreakerCharacter* Player)const
{return !IsCompleteFor(Player)&&IsAliveNearAndVisible(Player,InteractionRange);}
bool ABreakerCoastalUplink::TryInteract(ABreakerCharacter* Player)
{
    if(!HasAuthority()||!IsInteractionReachable(Player)||!Player->HasAuthority()||!Player->GetQuestJournal()
        ||TransmittingPlayer||!FMath::IsFinite(TransmissionSeconds)||TransmissionSeconds<=0
        ||!FMath::IsFinite(WorkingRadius)||WorkingRadius<InteractionRange)return false;
    // Claim ownership before registering callbacks or enabling time advancement.
    TransmittingPlayer=Player;RemainingSeconds=TransmissionSeconds;
    Player->GetCombat()->OnDeath.AddUniqueDynamic(this,&ABreakerCoastalUplink::CancelTransmission);
    SetActorTickEnabled(true);ForceNetUpdate();return true;
}
void ABreakerCoastalUplink::CancelTransmission()
{
    if(!HasAuthority())return;
    ABreakerCharacter* Previous=TransmittingPlayer;
    TransmittingPlayer=nullptr;RemainingSeconds=0;SetActorTickEnabled(false);
    if(IsValid(Previous)&&Previous->GetCombat())Previous->GetCombat()->OnDeath.RemoveDynamic(this,&ABreakerCoastalUplink::CancelTransmission);
    ForceNetUpdate();
}
void ABreakerCoastalUplink::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if(!HasAuthority())return;
    ABreakerCharacter* Player=TransmittingPlayer;
    if(!IsAliveNearAndVisible(Player,WorkingRadius)||IsCompleteFor(Player)){CancelTransmission();return;}
    if(!FMath::IsFinite(DeltaSeconds)||DeltaSeconds<=0)return;
    RemainingSeconds=FMath::Max(0.f,RemainingSeconds-DeltaSeconds);
    if(RemainingSeconds>0)return;
    // Clear the transaction before persistent flag callbacks can reenter.
    CancelTransmission();
    if(IsValid(Player)&&Player->GetQuestJournal())Player->GetQuestJournal()->SetFlag(CompletionFlag());
}
void ABreakerCoastalUplink::EndPlay(const EEndPlayReason::Type Reason)
{CancelTransmission();Super::EndPlay(Reason);}
FText ABreakerCoastalUplink::GetUplinkPrompt(const ABreakerCharacter* Player)const
{
    if(IsCompleteFor(Player))return FText::GetEmpty();
    return FText::FromString(TransmittingPlayer?FString::Printf(TEXT("UPLINK / %ds"),FMath::CeilToInt(RemainingSeconds)):TEXT("RESTORE COASTAL UPLINK"));
}
FText ABreakerCoastalUplink::GetObjectiveText(const ABreakerCharacter* Player)const
{
    if(IsCompleteFor(Player))return FText::FromString(TEXT("Coastal uplink restored. Return to Anchor 13; supply caches are optional."));
    if(TransmittingPlayer==Player)return FText::FromString(FString::Printf(TEXT("Uplink: %ds. Stay within %.0fm and in sight; leaving or dying resets."),FMath::CeilToInt(RemainingSeconds),WorkingRadius/100.f));
    return FText::FromString(TEXT("Restore the coastal uplink at Signal Point. Stay nearby and in sight until transmission completes."));
}
