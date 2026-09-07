#include "Combat/BreakerEntropyCapture.h"
#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerEnemy.h"
#include "Combat/BreakerStatusComponent.h"
#include "Combat/BreakerEntropy.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PawnMovementComponent.h"
#include "GameFramework/Controller.h"
#include "TimerManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

void BreakerStartEntropyCapture(ABreakerCharacter* Character)
{
#if !UE_BUILD_SHIPPING
    if (!Character || !Character->GetWorld()) return;
    FTimerHandle Timer;
    Character->GetWorld()->GetTimerManager().SetTimer(Timer,
        FTimerDelegate::CreateWeakLambda(Character, [Character]()
    {
        UWorld* World = Character->GetWorld();
        if (!World || !Character->GetController()) return;
        Character->GetCombat()->RestoreVitals();
        Character->GetController()->SetControlRotation(FRotator::ZeroRotator);
        int32 Index = 0;
        AActor* SourceEnemy = nullptr;
        AActor* FocusEnemy = nullptr;
        const bool bFeedback = FParse::Param(FCommandLine::Get(), TEXT("BreakerCaptureEntropyFeedback"));
        const int32 FocusIndex = bFeedback || FParse::Param(FCommandLine::Get(), TEXT("BreakerCaptureEntropyRot")) ? 1 : 0;
        const bool bSympathetic = FParse::Param(FCommandLine::Get(), TEXT("BreakerCaptureSympathetic"));
        for (TActorIterator<ABreakerEnemy> It(World); It && Index < 2; ++It)
        {
            ABreakerEnemy* Enemy = *It;
            if (!Enemy || Enemy->IsDeadEnemy() || Enemy->IsEliteOrBetter()) continue;
            auto* Status = Enemy->FindComponentByClass<UBreakerStatusComponent>();
            auto* Combat = Enemy->FindComponentByClass<UBreakerCombatComponent>();
            if (!Status || !Combat) continue;
            Enemy->SetActorTickEnabled(false);
            if (auto* Movement = Enemy->GetMovementComponent())
            { Movement->StopMovementImmediately(); Movement->SetComponentTickEnabled(false); }
            Enemy->SetActorLocation(Character->GetActorLocation() + FVector(1000, Index == 0 ? -180 : 180, 0));
            Enemy->SetActorRotation(FRotator(0, 180, 0));
            FBreakerDamageRequest Hit;
            Hit.Element = EBreakerElement::Entropy; Hit.ElementalFraction = 1;
            Hit.bCanCritical = false; Hit.SetInstigator(Character);
            Hit.BaseDamage = Status->GetEntropyThreshold() * (Index == 0 ? .6f : 1.01f)
                / FMath::Max(.01f, 1 - Status->GetEntropyResistancePercent() / 100);
            // Visual fixture for the real fading meter; purchased-node delivery
            // is exercised separately by SympatheticRuntime.
            if (bSympathetic && Index == 0)
            {
                Hit.ElementBuildupFlat = BreakerEntropy::SympatheticFlatBuildup();
                Hit.ElementBuildupFadeSeconds = BreakerEntropy::SympatheticFadeSeconds();
            }
            Combat->ReceiveDamage(Hit);
            if (bFeedback && Index == 1)
            {
                FBreakerStatusApplicationSpec Bleed;
                Bleed.StatusTag = FGameplayTag::RequestGameplayTag(TEXT("Status.Bleed"));
                Bleed.Duration = 4; Bleed.TickInterval = .5f; Bleed.BaseDamagePerTick = 1;
                Bleed.ProcCoefficient = 0; // Visual fixture: distinguish simultaneous physical and Rot ticks.
                Status->ApplyStatus(Bleed, EBreakerDamageFamily::Physical, Character);
            }
            UE_LOG(LogTemp, Display, TEXT("[BreakerCapture] Entropy target=%d buildup=%.2f threshold=%.2f rot=%d"),
                Index, Status->GetEntropyBuildup(), Status->GetEntropyThreshold(),
                Status->HasStatus(FGameplayTag::RequestGameplayTag(TEXT("Status.Rot"))));
            SourceEnemy = Enemy;
            if (Index == FocusIndex) FocusEnemy = Enemy;
            ++Index;
        }
        if (FocusEnemy)
        {
            FVector Eye; FRotator View;
            Character->GetActorEyesViewPoint(Eye, View);
            Character->GetController()->SetControlRotation((FocusEnemy->GetActorLocation() + FVector(0, 0, 30) - Eye).Rotation());
        }
        if (auto* Status = Character->FindComponentByClass<UBreakerStatusComponent>())
        {
            FBreakerDamageRequest Hit;
            Hit.Element = EBreakerElement::Entropy; Hit.ElementalFraction = 1;
            Hit.BaseDamage = Status->GetEntropyThreshold() * .6f;
            if (bSympathetic)
            {
                Hit.ElementBuildupFlat = BreakerEntropy::SympatheticFlatBuildup();
                Hit.ElementBuildupFadeSeconds = BreakerEntropy::SympatheticFadeSeconds();
            }
            Hit.bCanCritical = false; Hit.SetInstigator(SourceEnemy);
            Character->GetCombat()->ReceiveDamage(Hit);
        }
    }), 1.0f, false);
#endif
}
