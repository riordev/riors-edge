#include "Combat/BreakerEntropyCapture.h"
#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerEnemy.h"
#include "Combat/BreakerStatusComponent.h"
#include "Combat/BreakerEntropy.h"
#include "Combat/BreakerZoneActor.h"
#include "Abilities/BreakerAbility_Rot.h"
#include "UI/BreakerEffectMath.h"
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
        if (FParse::Param(FCommandLine::Get(), TEXT("BreakerCaptureLingering")))
        {
            // Visual comparison only; purchased-node/cost delivery has its own
            // runtime test. Both footprints use the real zone presentation.
            const auto* Rot = GetDefault<UBreakerAbility_Rot>();
            const FVector Origin = Character->GetActorLocation();
            for (int32 Side = 0; Side < 2; ++Side)
            {
                FVector Center = Origin + FVector(1000, Side == 0 ? -550 : 550, 0);
                FHitResult Floor;
                FCollisionQueryParams Params(SCENE_QUERY_STAT(BreakerLingeringCapture), false, Character);
                if (World->LineTraceSingleByChannel(Floor, Center + FVector(0,0,500), Center - FVector(0,0,2000), ECC_GameTraceChannel2, Params))
                    Center = Floor.ImpactPoint;
                auto* Zone = World->SpawnActor<ABreakerZoneActor>(Center, FRotator::ZeroRotator);
                if (!Zone) continue;
                FBreakerZoneSpec Spec;
                Spec.RadiusCm = Rot->RadiusCm;
                Spec.Duration = 20; // O2 visual fixture lifetime, not ability tuning.
                Spec.ZoneColor = BreakerFX::ColorForStatusTag(FGameplayTag::RequestGameplayTag(TEXT("Status.Rot")), FLinearColor::White);
                Zone->ConfigureZone(Spec, Character);
                if (Side == 1) Zone->GrowRadiusOnce(Rot->LingeringRefreshGrowthCm);
                UE_LOG(LogTemp, Display, TEXT("[BreakerCapture] Lingering side=%d radius=%.1f"), Side, Zone->GetSpec().RadiusCm);
            }
            if (auto* Movement = Character->GetMovementComponent())
            { Movement->StopMovementImmediately(); Movement->SetComponentTickEnabled(false); }
            Character->SetActorLocation(Origin + FVector(0,0,1000));
            FVector Eye; FRotator View;
            Character->GetActorEyesViewPoint(Eye, View);
            Character->GetController()->SetControlRotation((Origin + FVector(1000,0,-100) - Eye).Rotation());
            return;
        }
        int32 Index = 0;
        AActor* SourceEnemy = nullptr;
        AActor* FocusEnemy = nullptr;
        FString Reaction;
        FParse::Value(FCommandLine::Get(), TEXT("BreakerCaptureReaction="), Reaction);
        const bool bReaction = !Reaction.IsEmpty();
        const bool bRift = FParse::Param(FCommandLine::Get(), TEXT("BreakerCaptureRift"));
        const bool bVoid = FParse::Param(FCommandLine::Get(), TEXT("BreakerCaptureVoid"));
        const bool bFeedback = FParse::Param(FCommandLine::Get(), TEXT("BreakerCaptureEntropyFeedback"));
        const int32 FocusIndex = bReaction || bFeedback || FParse::Param(FCommandLine::Get(), TEXT("BreakerCaptureEntropyRot")) ? 1 : 0;
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
            if (bRift)
            {
                Hit.Element = EBreakerElement::Rift;
                Hit.BaseDamage = Status->GetRiftThreshold() * (Index == 0 ? .6f : 1.01f)
                    / FMath::Max(.01f, 1 - Status->GetRiftResistancePercent() / 100);
            }
            if (bReaction && Index == 1)
            {
                Hit.Element = Reaction == TEXT("Tear") ? EBreakerElement::Void : EBreakerElement::Entropy;
                const float Threshold = Reaction == TEXT("Tear") ? Status->GetVoidThreshold() : Status->GetEntropyThreshold();
                const float Resistance = Reaction == TEXT("Tear") ? Status->GetVoidResistancePercent() : Status->GetEntropyResistancePercent();
                Hit.BaseDamage = Threshold * 1.01f / FMath::Max(.01f, 1 - Resistance / 100);
            }
            if (bVoid)
            {
                Hit.Element = EBreakerElement::Void;
                Hit.BaseDamage = Status->GetVoidThreshold() * (Index == 0 ? .6f : 1.01f)
                    / FMath::Max(.01f, 1 - Status->GetVoidResistancePercent() / 100);
            }
            Combat->ReceiveDamage(Hit);
            if (bReaction && Index == 1)
            {
                FBreakerDamageRequest Trigger = Hit;
                Trigger.Element = Reaction == TEXT("Wither") ? EBreakerElement::Void : EBreakerElement::Rift;
                Trigger.BaseDamage = 1; // Capture fixture: expose only the consumed earned budget.
                Combat->ReceiveDamage(Trigger);
            }
            if (bVoid)
            {
                FBreakerDamageRequest VoidHit = Hit;
                VoidHit.Element = EBreakerElement::Entropy;
                VoidHit.BaseDamage = Status->GetEntropyThreshold() * (Index == 0 ? .6f : 1.01f)
                    / FMath::Max(.01f, 1 - Status->GetEntropyResistancePercent() / 100);
                Combat->ReceiveDamage(VoidHit);
            }
            if (bFeedback && Index == 1)
            {
                FBreakerStatusApplicationSpec Bleed;
                Bleed.StatusTag = FGameplayTag::RequestGameplayTag(TEXT("Status.Bleed"));
                Bleed.Duration = 4; Bleed.TickInterval = .5f; Bleed.BaseDamagePerTick = 1;
                Bleed.ProcCoefficient = 0; // Visual fixture: distinguish simultaneous physical and Rot ticks.
                Status->ApplyStatus(Bleed, EBreakerDamageFamily::Physical, Character);
            }
            UE_LOG(LogTemp, Display, TEXT("[BreakerCapture] Elements target=%d entropy=%.2f void=%.2f rift=%.2f statuses=%d reaction=%s"),
                Index, Status->GetEntropyBuildup(), Status->GetVoidBuildup(), Status->GetRiftBuildup(),
                Status->GetActiveStatuses().Num(), *Reaction);
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
            if (bRift) { Hit.Element = EBreakerElement::Rift; Hit.BaseDamage = Status->GetRiftThreshold() * .6f; }
            Character->GetCombat()->ReceiveDamage(Hit);
            if (bVoid)
            {
                Hit.Element = EBreakerElement::Void;
                Hit.BaseDamage = Status->GetVoidThreshold() * .6f;
                Character->GetCombat()->ReceiveDamage(Hit);
            }
        }
    }), (FParse::Param(FCommandLine::Get(), TEXT("BreakerCaptureRift")) || FCString::Strstr(FCommandLine::Get(), TEXT("BreakerCaptureReaction="))) ? 5.8f : FParse::Param(FCommandLine::Get(), TEXT("BreakerCaptureVoid")) ? 5.5f : 1.0f, false);
#endif
}
