#include "Interaction/BreakerMeridianGroundCrew.h"
#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerEnemy.h"
#include "Game/BreakerPrototypeDestinations.h"
#include "Save/BreakerQuestJournal.h"

ABreakerMeridianGroundCrew::ABreakerMeridianGroundCrew()
{
    DisplayName=FText::FromString(TEXT("Meridian ground crew"));Tags.Add(TEXT("PortMeridian.GroundCrew"));
}
FName ABreakerMeridianGroundCrew::CompletionFlag(){return TEXT("Quest.PortMeridian.GroundCrewEvacuated");}
bool ABreakerMeridianGroundCrew::IsCompleteFor(const ABreakerCharacter* Player)const
{return IsValid(Player)&&Player->GetQuestJournal()&&Player->GetQuestJournal()->HasFlag(CompletionFlag());}
bool ABreakerMeridianGroundCrew::ConfigureMeridian(const TArray<FVector>& Districts,const TArray<TArray<ABreakerEnemy*>>& Pockets)
{
    if(!HasAuthority()||bConfigured)return false;
    bConfigured=true;bRosterValid=Districts.Num()==3&&Pockets.Num()==3;
    if(!bRosterValid)return false;
    for(const auto& Pocket:Pockets)
    {
        // Exact authored roster: a partial spawn or duplicate can never certify
        // a checkpoint. Missing actors are not equivalent to accepted deaths.
        if(Pocket.Num()!=6)bRosterValid=false;
        for(auto* Guard:Pocket)
        {
            auto* Combat=IsValid(Guard)?Guard->FindComponentByClass<UBreakerCombatComponent>():nullptr;
            if(!Combat||Guard->GetWorld()!=GetWorld()||Guards.Contains(Guard)){bRosterValid=false;continue;}
            Guards.Add(Guard);ObservedDeaths.Add(false);
            Combat->OnDeath.AddUniqueDynamic(this,&ABreakerMeridianGroundCrew::ObserveGuardDeaths);
        }
    }
    if(!bRosterValid)return false;
    TArray<FBreakerSurvivorRoutePoint> MeridianRoute;
    for(int32 Pocket=0;Pocket<Districts.Num();++Pocket)
    {
        MeridianRoute.Add({Districts[Pocket]+FVector(0,0,RouteCenterHeight),Pocket});
        if(Pocket+1<Districts.Num())MeridianRoute.Add({(Districts[Pocket]+Districts[Pocket+1])*.5f+FVector(0,0,RouteCenterHeight),INDEX_NONE});
    }
    const FVector MeridianExtraction=Districts.Last()+ExtractionOffset;
    MeridianRoute.Add({MeridianExtraction,INDEX_NONE});
    ConfigureEscortRoute(Districts[0]+ShelterOffset,MeridianExtraction,MeridianRoute,Pockets.Num());
    OnEscortReadyForExtraction.AddUObject(this,&ABreakerMeridianGroundCrew::CompleteExtraction);
    ObserveGuardDeaths();return bRosterValid;
}
void ABreakerMeridianGroundCrew::ObserveGuardDeaths()
{
    if(!HasAuthority()||!bConfigured||!bRosterValid)return;
    for(int32 Index=0;Index<Guards.Num();++Index)
    {
        if(ObservedDeaths[Index])continue;
        auto* Guard=Guards[Index].Get();const auto* Combat=IsValid(Guard)?Guard->FindComponentByClass<UBreakerCombatComponent>():nullptr;
        if(Combat&&Combat->IsDead())ObservedDeaths[Index]=true;
        else if(!Combat||Guard->IsActorBeingDestroyed()){bRosterValid=false;break;}
    }
    for(int32 Pocket=0;Pocket<3;++Pocket)
    {
        bool bClear=bRosterValid;
        for(int32 Member=0;Member<6;++Member)bClear=bClear&&ObservedDeaths.IsValidIndex(Pocket*6+Member)&&ObservedDeaths[Pocket*6+Member];
        SetPocketCleared(Pocket,bClear);
    }
}
bool ABreakerMeridianGroundCrew::IsEscortAdmitted(const ABreakerCharacter* Player)const
{
    const auto* Destination=BreakerPrototypeDestinations::ForWorld(this);
    if(!bRosterValid||!Destination||Destination->Id!=TEXT("PortMeridian")||!IsValid(Player)||!Player->HasAuthority()
        ||Player->IsActorBeingDestroyed()||IsCompleteFor(Player)||bCompletionClaimed)return false;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(MeridianEscortAdmissionLOS),false,Player);Query.AddIgnoredActor(this);FHitResult Wall;
    return !GetWorld()->LineTraceSingleByObjectType(Wall,Player->GetActorLocation(),GetActorLocation(),FCollisionObjectQueryParams(ECC_WorldStatic),Query);
}
void ABreakerMeridianGroundCrew::CompleteExtraction(ABreakerSurvivor* Survivor,ABreakerCharacter* Player)
{
    if(!HasAuthority()||bCompletionClaimed||Survivor!=this||!IsValid(Player)||Player!=GetEscortPlayer()
        ||!Player->HasAuthority()||!Player->GetCombat()||Player->GetCombat()->IsDead()||!Player->GetQuestJournal()
        ||!bRosterValid||!AreAllPocketsCleared()||!IsAtExtraction()||GetEscortState()!=EBreakerSurvivorEscortState::AtExtraction)return;
    bCompletionClaimed=true; // Claim before journal persistence/delegate callbacks.
    Player->GetQuestJournal()->SetFlag(CompletionFlag());
}
FText ABreakerMeridianGroundCrew::GetObjectiveText(const ABreakerCharacter* Player)const
{
    if(IsCompleteFor(Player))return FText::FromString(TEXT("Ground crew reached Maintenance Hangar. Return to Anchor 13; supply caches are optional."));
    if(IsEscortActive())return FText::FromString(FString::Printf(TEXT("Guide the ground crew to Maintenance Hangar. Stay close and clear the route (%ds remaining)."),FMath::CeilToInt(GetLucidityRemaining())));
    if(GetEscortState()==EBreakerSurvivorEscortState::Failed)return FText::FromString(TEXT("Escort interrupted. Regroup with the ground crew at Departures Terminal."));
    return FText::FromString(TEXT("Meet the ground crew in Departures Terminal and guide them to Maintenance Hangar."));
}
