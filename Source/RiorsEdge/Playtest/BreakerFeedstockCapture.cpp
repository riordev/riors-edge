#include "Playtest/BreakerFeedstockCapture.h"
#include "Characters/BreakerCharacter.h"
#include "Camera/PlayerCameraManager.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerEnemy.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "TimerManager.h"

void BreakerScheduleFeedstockCapture(UWorld* World)
{
    FString CaptureDirectory;
    if (!World || !FParse::Param(FCommandLine::Get(), TEXT("BreakerCaptureFeedstock"))
        || !FParse::Value(FCommandLine::Get(), TEXT("UserDir="), CaptureDirectory) || CaptureDirectory.IsEmpty()) return;
    FTimerHandle Timer;
    World->GetTimerManager().SetTimer(Timer, FTimerDelegate::CreateWeakLambda(World, [World]()
    {
        auto* Controller = World->GetFirstPlayerController();
        auto* Player = Controller ? Cast<ABreakerCharacter>(Controller->GetPawn()) : nullptr;
        if (!Player || !Player->HasAuthority()) return;
        Player->AddQuestFlag(TEXT("Quest.KessSalvage.Accepted"));
        for (TActorIterator<ABreakerEnemy> It(World); It; ++It)
        {
            if (It->IsDeadEnemy() || !It->Tags.Contains(TEXT("Fernhall.Outdoor.0"))) continue;
            It->SetActorLocation(Player->GetActorLocation() + Player->GetActorForwardVector() * 220);
            FBreakerDamageRequest Hit;
            Hit.BaseDamage = 1000000; Hit.bCanCritical = false; Hit.bCanBeAvoided = false; Hit.bBypassShield = true;
            Hit.SetInstigator(Player);
            It->FindComponentByClass<UBreakerCombatComponent>()->ReceiveDamage(Hit);
            FVector Eye; FRotator Facing; Controller->GetPlayerViewPoint(Eye, Facing);
            Controller->SetControlRotation((It->GetActorLocation() - FVector(0,0,80) - Eye).Rotation());
            if (Controller->PlayerCameraManager) Controller->PlayerCameraManager->UpdateCamera(.05f);
            // The visual fixture freezes the encounter after the real death event.
            Controller->SetPause(true);
            break;
        }
    }), 1.0f, false);
}
