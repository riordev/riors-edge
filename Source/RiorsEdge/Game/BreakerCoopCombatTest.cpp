#include "Game/BreakerCoopCombatTest.h"
#include "Game/BreakerGameInstance.h"
#include "Game/BreakerZoneBuilder.h"
#include "Game/BreakerWorldBasics.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
namespace
{
    bool BreakerCoopSessionLatched = false;
    FAutoConsoleCommandWithWorldAndArgs BreakerCoopHost(TEXT("Breaker.CoopHost"),
        TEXT("Start isolated nonsaving Swift combat test in Fernhall. No campaign travel or persistent progression."),
        FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args,UWorld* World)
        {
            if (!World || World->GetNetMode()==NM_Client) return;
            BreakerCoopSessionLatched=true;
            UGameplayStatics::OpenLevel(World,TEXT("/Game/Breaker/Maps/Lvl_Fernhall"),true,TEXT("listen?BreakerCoopCombat=1"));
        }));
    FAutoConsoleCommandWithWorldAndArgs BreakerCoopJoin(TEXT("Breaker.CoopJoin"),
        TEXT("Breaker.CoopJoin host:port -- join isolated nonsaving combat test; host must run CoopHost."),
        FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args,UWorld* World)
        {
            if (!World || Args.Num()!=1 || Args[0].Contains(TEXT("?")) || Args[0].Contains(TEXT("/"))) return;
            auto* PC=UGameplayStatics::GetPlayerController(World,0);if(!PC)return;
            BreakerCoopSessionLatched=true;
            PC->ClientTravel(Args[0]+TEXT("?BreakerCoopCombat=1"),TRAVEL_Absolute);
        }));
}
bool BreakerCoopCombatTest::IsEnabled(const UWorld* World)
{
    return BreakerCoopSessionLatched || FParse::Param(FCommandLine::Get(),TEXT("BreakerCoopCombatTest"))
        || (World && World->URL.HasOption(TEXT("BreakerCoopCombat=1")));
}

bool BreakerCoopCombatTest::EnsureLocalEnvironment(UWorld* World)
{
    if (!World || !IsEnabled(World) || !UBreakerGameInstance::IsFernhallMap(World)) return false;
    // Claim before spawning so reentrant callbacks cannot duplicate geometry.
    // Failed partial construction also remains claimed until world teardown.
    static TMap<TWeakObjectPtr<UWorld>, bool> Attempts;
    for (auto It=Attempts.CreateIterator(); It; ++It) if (!It.Key().IsValid()) It.RemoveCurrent();
    const TWeakObjectPtr<UWorld> Key(World);
    if (const bool* Complete=Attempts.Find(Key)) return *Complete;
    Attempts.Add(Key,false);
    FBreakerZoneMarkers Markers;
    const bool bBuilt=UBreakerZoneBuilder::BuildFernhallYard(World,Markers);
    if (bBuilt) UBreakerWorldBasics::EnsureWorldLighting(World);
    Attempts.FindChecked(Key)=bBuilt;
    if (bBuilt) { UE_LOG(LogTemp,Display,TEXT("[CoopTest] local environment ready: Fernhall geometry and lighting; no gameplay spawns")); }
    else { UE_LOG(LogTemp,Error,TEXT("[CoopTest] local environment failed; restart test after repairing Fernhall assets")); }
    return bBuilt;
}
