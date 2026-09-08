#include "Playtest/BreakerTellCapture.h"
#include "Abilities/BreakerSupportAbilities.h"
#include "AbilitySystemComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Camera/PlayerCameraManager.h"
#include "Characters/BreakerCharacter.h"
#include "Classes/BreakerChargeComponent.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerRangedEnemy.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Progression/BreakerExperience.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionLibrary.h"
#include "Save/BreakerMissionContent.h"
#include "TimerManager.h"

void BreakerScheduleTellCapture(UWorld* World)
{
    FString UserDirectory;
    if (!World || !FParse::Param(FCommandLine::Get(),TEXT("BreakerCaptureTell"))
        || !FParse::Value(FCommandLine::Get(),TEXT("UserDir="),UserDirectory) || UserDirectory.IsEmpty()) return;
    FTimerHandle Setup;
    World->GetTimerManager().SetTimer(Setup, FTimerDelegate::CreateWeakLambda(World,[World]()
    {
        auto* Controller=World->GetFirstPlayerController();
        auto* Player=Controller ? Cast<ABreakerCharacter>(Controller->GetPawn()) : nullptr;
        if (!Player || !Player->HasAuthority()) return;
        Player->ResumeFromMenu();
        auto* Progression=Player->GetProgression();
        Progression->DevForceClass(EBreakerClassId::Support);
        // Isolated earned-entitlement benchmark, not a claimed campaign playthrough.
        Progression->AwardExperience(UBreakerExperienceLibrary::TotalXpToReachLevel(50,Progression->ExperienceCurve));
        FBreakerQuestFlagSet Flags;
        for (const auto& Mission:UBreakerMissionLibrary::GetMissions())
            for (const auto& Beat:Mission.Beats)
                for (FName Flag:UBreakerMissionLibrary::BeatCompletionFlags(Beat)) Flags.Add(Flag);
        Flags.Add(TEXT("Quest.Finale.Seal")); Progression->SettleDoctrineEntitlement(Flags);
        FText Reason;
        auto Buy=[&](const UBreakerProgressionTree* Tree,FName Id,int32 Rank)
        {
            while (Progression->GetNodeRank(Id,EBreakerPointCurrency::DoctrinePoints)<Rank)
                if (!Progression->PurchaseNode(Tree,Id,Reason)) { UE_LOG(LogTemp,Error,TEXT("Tell capture purchase failed: %s"),*Reason.ToString()); return false; }
            return true;
        };
        if (!Buy(UBreakerProgressionLibrary::GetSupportMedicTree(),TEXT("Support.Medic.FieldDressing"),1)
            || !Buy(UBreakerProgressionLibrary::GetSupportWardenTree(),TEXT("Support.Warden.Painted"),2)
            || !Buy(UBreakerProgressionLibrary::GetSupportWardenTree(),TEXT("Support.Warden.Tell"),1)) return;
        auto* Charge=Player->GetCharge(); Charge->SetInCombat(true);
        for (int32 I=0;I<12;++I)
        {
            Charge->AdvanceLoop(1);
            FBreakerDamageRequest Hurt; Hurt.BaseDamage=Player->GetAttributes()->GetMaxHealth()*.25f;
            Hurt.DamageFamily=EBreakerDamageFamily::TrueDamage; Hurt.bCanCritical=false; Hurt.bCanBeAvoided=false;
            Player->GetCombat()->ReceiveDamage(Hurt);
            Player->GetCombat()->ApplyHealingAmount(Hurt.BaseDamage,Player,FGameplayTag()); Charge->AdvanceLoop(1);
        }
        for (TActorIterator<ABreakerEnemy> It(World);It;++It) It->SetActorTickEnabled(false);
        FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        auto* Target=World->SpawnActor<ABreakerRangedEnemy>(Player->GetActorLocation()+Player->GetActorForwardVector()*1500,FRotator::ZeroRotator,Spawn);
        if (!Target) return;
        Target->ConfigureCrowdProbe();
        FVector Eye; FRotator View; Controller->GetPlayerViewPoint(Eye,View);
        Controller->SetControlRotation((Target->GetActorLocation()-Eye).Rotation());
        if (Controller->PlayerCameraManager) Controller->PlayerCameraManager->UpdateCamera(.05f);
        if (!Progression->IsAbilityUnlocked(TEXT("Support.Mark")) && !Progression->SpendAbilityToken(TEXT("Support.Mark"),Reason)) return;
        auto* ASC=Player->GetAbilitySystemComponent();
        const auto Handle=ASC->GiveAbility(FGameplayAbilitySpec(UBreakerAbility_Mark::StaticClass(),1));
        if (!ASC->TryActivateAbility(Handle)) { UE_LOG(LogTemp,Error,TEXT("Tell capture paid Mark failed")); return; }
        const TWeakObjectPtr<ABreakerCharacter> WeakPlayer=Player;
        const TWeakObjectPtr<ABreakerRangedEnemy> WeakTarget=Target;
        const double Deadline=World->GetTimeSeconds()+5;
        const auto Timer=MakeShared<FTimerHandle>();
        World->GetTimerManager().SetTimer(*Timer,FTimerDelegate::CreateWeakLambda(World,[World,WeakPlayer,WeakTarget,Deadline,Timer]()
        {
            auto* Owner=WeakPlayer.Get(); auto* Enemy=WeakTarget.Get();
            if (!Owner || !Enemy || World->GetTimeSeconds()>Deadline)
            { World->GetTimerManager().ClearTimer(*Timer); UE_LOG(LogTemp,Warning,TEXT("Tell capture did not reach live windup")); return; }
            if (!UBreakerAbility_Mark::ShouldShowTell(Owner,Enemy)) return;
            auto* PC=Cast<APlayerController>(Owner->GetController());
            if (PC && PC->PlayerCameraManager) PC->PlayerCameraManager->UpdateCamera(.05f);
            World->GetTimerManager().ClearTimer(*Timer);
            if (PC) PC->SetPause(true);
            UE_LOG(LogTemp,Display,TEXT("Tell capture frozen during actual marked attack windup"));
        }),.02f,true);
    }),1.0f,false);
}
