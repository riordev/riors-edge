#include "Playtest/BreakerTellCapture.h"
#include "Abilities/BreakerSupportAbilities.h"
#include "AbilitySystemComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Camera/PlayerCameraManager.h"
#include "Characters/BreakerCharacter.h"
#include "Classes/BreakerChargeComponent.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerEnemy.h"
#include "Combat/BreakerModifierComponent.h"
#include "Combat/BreakerRangedEnemy.h"
#include "Engine/World.h"
#include "Game/BreakerPocketRift.h"
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

// ---------------------------------------------------------------------------
// -BreakerCaptureBlast: photograph a Volatile corpse's blast ring.
//
// The ring is the whole point of the change and a fuse is 1.2 seconds long, so
// without this it is unphotographable — and an unphotographed ring can be at
// the wrong height, the wrong scale, or under the floor, with nothing saying
// so. The fuse is STRETCHED for the capture only: the shipped 1.2s is shorter
// than the harness's own screenshot cadence, so a real fuse would be over
// before the first frame lands. Nothing else about the body is changed.
// ---------------------------------------------------------------------------
void BreakerScheduleBlastCapture(UWorld* World)
{
    if (!World || !FParse::Param(FCommandLine::Get(), TEXT("BreakerCaptureBlast"))) return;
    FTimerHandle Setup;
    World->GetTimerManager().SetTimer(Setup, FTimerDelegate::CreateWeakLambda(World, [World]()
    {
        auto* Controller = World->GetFirstPlayerController();
        auto* Player = Controller ? Cast<ABreakerCharacter>(Controller->GetPawn()) : nullptr;
        if (!Player || !Player->HasAuthority()) return;
        Player->ResumeFromMenu();
        for (TActorIterator<ABreakerEnemy> It(World); It; ++It) It->SetActorTickEnabled(false);

        FActorSpawnParameters Spawn;
        Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        // Far enough back that the whole 900 cm ring is in frame.
        auto* Body = World->SpawnActor<ABreakerEnemy>(
            Player->GetActorLocation() + Player->GetActorForwardVector() * 1400.0f,
            FRotator::ZeroRotator, Spawn);
        if (!Body) return;
        Body->ConfigureCrowdProbe();
        Body->DispatchBeginPlay();
        if (!Body->ConfigureWithExactModifiers({ EBreakerEnemyModifier::Volatile }))
        {
            UE_LOG(LogTemp, Error, TEXT("[BlastCapture] the body refused Volatile."));
            return;
        }
        auto* Fuse = Body->GetModifierComponent();
        if (!Fuse) return;
        Fuse->Params.VolatileFuseSeconds = 30.0f;   // capture only; see the note above

        FVector Eye; FRotator View;
        Controller->GetPlayerViewPoint(Eye, View);
        Controller->SetControlRotation((Body->GetActorLocation() - Eye).Rotation());
        if (Controller->PlayerCameraManager) Controller->PlayerCameraManager->UpdateCamera(.05f);

        // Kill it the way the game does, so the fuse lights through the real
        // death path rather than by poking the field.
        FBreakerDamageRequest Kill;
        Kill.BaseDamage = 1000000; Kill.bCanCritical = false; Kill.bBypassShield = true;
        Kill.bCanBeAvoided = false; Kill.DamageFamily = EBreakerDamageFamily::TrueDamage;
        Kill.SetInstigator(Player);
        Body->FindComponentByClass<UBreakerCombatComponent>()->ReceiveDamage(Kill);
        UE_LOG(LogTemp, Display, TEXT("[BlastCapture] fuse lit=%d remaining=%.1fs radius=%.0fcm"),
            Fuse->IsFuseLit() ? 1 : 0, Fuse->GetFuseRemainingSeconds(), Fuse->Params.VolatileOuterRadiusCm);
    }), 1.0f, false);
}

// ---------------------------------------------------------------------------
// THE POCKET TEAR. Placed 750 cm BEHIND each formation, which is the right
// place for it and the wrong place to photograph it from: the ordinary
// Fernhall route never gets past the fight to see one. So this walks the
// player to the nearest tear, faces it, and flares it on a loop — the frames
// then carry both states and the difference between them is the thing being
// judged.
// ---------------------------------------------------------------------------
void BreakerSchedulePocketRiftCapture(UWorld* World)
{
    // BOTH FORMS, because FParse::Param does not match a switch carrying a
    // value: -BreakerCapturePocketRift is the nearest tear, and
    // -BreakerCapturePocketRift=<n> is the nth. The first attempt only checked
    // Param and the =7 run silently photographed nothing at all.
    int32 Wanted = 0;
    const bool bIndexed = World && FParse::Value(FCommandLine::Get(),
        TEXT("BreakerCapturePocketRift="), Wanted);
    if (!World || (!bIndexed && !FParse::Param(FCommandLine::Get(), TEXT("BreakerCapturePocketRift")))) return;
    FTimerHandle Setup;
    World->GetTimerManager().SetTimer(Setup, FTimerDelegate::CreateWeakLambda(World, [World, Wanted]()
    {
        auto* Controller = World->GetFirstPlayerController();
        auto* Player = Controller ? Cast<ABreakerCharacter>(Controller->GetPawn()) : nullptr;
        if (!Player || !Player->HasAuthority()) return;
        Player->ResumeFromMenu();
        // The tears are behind live fights. Freezing the bodies keeps the frame
        // about the tear rather than about whatever reached the player first.
        for (TActorIterator<ABreakerEnemy> It(World); It; ++It) It->SetActorTickEnabled(false);

        // -BreakerCapturePocketRift=<n> picks the nth tear by distance, so the
        // yards past the first two are photographable at all: the harness has
        // no way to walk two seams, and the depot's tears are 200 m away.
        TArray<ABreakerPocketRift*> Tears;
        for (TActorIterator<ABreakerPocketRift> It(World); It; ++It) Tears.Add(*It);
        const FVector From = Player->GetActorLocation();
        Tears.Sort([From](const ABreakerPocketRift& A, const ABreakerPocketRift& B)
        {
            return FVector::DistSquared(A.GetActorLocation(), From)
                 < FVector::DistSquared(B.GetActorLocation(), From);
        });
        const int32 Found = Tears.Num();
        ABreakerPocketRift* Nearest = Tears.IsValidIndex(Wanted) ? Tears[Wanted] : nullptr;
        if (!Nearest)
        {
            UE_LOG(LogTemp, Error, TEXT("[PocketRiftCapture] no tear exists in this world."));
            return;
        }
        // In FRONT of it, which is the side a body walks out toward, and far
        // enough back that the whole 280 cm tear is in frame with ground under
        // it. The actor's forward is the walk direction, so standing along it
        // is standing where the player would be when the patrol arrives.
        const FVector Stand = Nearest->GetActorLocation()
            + Nearest->GetActorForwardVector() * 620.0f + FVector(0.0f, 0.0f, 120.0f);
        Player->SetActorLocation(Stand, false, nullptr, ETeleportType::TeleportPhysics);
        FVector Eye; FRotator View;
        Controller->GetPlayerViewPoint(Eye, View);
        Controller->SetControlRotation(
            (Nearest->GetActorLocation() + FVector(0.0f, 0.0f, 140.0f) - Eye).Rotation());
        if (Controller->PlayerCameraManager) Controller->PlayerCameraManager->UpdateCamera(0.05f);

        UE_LOG(LogTemp, Display,
            TEXT("[PocketRiftCapture] %d tears; nearest at %s with %d segments; standing at %s"),
            Found, *Nearest->GetActorLocation().ToString(), Nearest->SegmentCount(), *Stand.ToString());

        // On a loop, so consecutive frames land on different parts of the
        // flare rather than all four catching the same instant.
        TWeakObjectPtr<ABreakerPocketRift> Watched = Nearest;
        FTimerHandle Pulse;
        World->GetTimerManager().SetTimer(Pulse, FTimerDelegate::CreateWeakLambda(World, [Watched]()
        {
            if (ABreakerPocketRift* Tear = Watched.Get()) Tear->Flare();
        }), 2.5f, true, 1.5f);
    }), 1.0f, false);
}
