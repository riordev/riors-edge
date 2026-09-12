#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Abilities/BreakerTankAbilities.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Classes/BreakerGritComponent.h"
#include "Combat/BreakerCombatComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Progression/BreakerExperience.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionLibrary.h"
#include "Progression/BreakerProgressionTree.h"
#include "Save/BreakerMissionContent.h"
#include "TimerManager.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerHoldHitCapRuntimeTest,
    "RiorsEdge.Abilities.HoldPaidHitCap", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerHoldHitCapRuntimeTest::RunTest(const FString& Parameters)
{
    for (bool bWall : { false, true })
    {
        UWorld::InitializationValues Init;
        Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
        auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
        if (!TestNotNull(TEXT("isolated Hold world"), World)) return false;
        GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
        World->InitializeActorsForPlay(FURL());
        const uint64 SavedFrame = GFrameCounter;
        ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); GFrameCounter = SavedFrame; };
        auto* Tank = World->SpawnActor<ABreakerCharacter>();
        if (!TestNotNull(TEXT("native Tank"), Tank)) return false;
        Tank->SetActorTickEnabled(false);
        Tank->GetBreakerMovement()->SetComponentTickEnabled(false);
        auto* ASC = Tank->GetAbilitySystemComponent();
        auto* Attributes = Tank->GetAttributes();
        auto* Combat = Tank->GetCombat();
        auto* Progression = Tank->GetProgression();
        ASC->InitAbilityActorInfo(Tank, Tank);
        ASC->AddAttributeSetSubobject(Attributes);
        Combat->BindAttributes(Attributes);
        Progression->BindAttributes(Attributes);
        if (!TestTrue(TEXT("actual Tank class"), Progression->ChoosePermanentClassById(EBreakerClassId::Tank))) return false;
        if (bWall)
        {
            // Restored benchmark/XP entitlement fixture, not a campaign run.
            Progression->AwardExperience(UBreakerExperienceLibrary::TotalXpToReachLevel(50, Progression->ExperienceCurve));
            FBreakerQuestFlagSet Flags;
            for (const auto& Mission : UBreakerMissionLibrary::GetMissions())
                for (const auto& Beat : Mission.Beats)
                    for (FName Flag : UBreakerMissionLibrary::BeatCompletionFlags(Beat)) Flags.Add(Flag);
            Flags.Add(TEXT("Quest.Finale.Seal"));
            Progression->SettleDoctrineEntitlement(Flags);
            TestEqual(TEXT("earned fixture wallet is eight"), Progression->GetUnspentPoints(EBreakerPointCurrency::DoctrinePoints), 8);
            const auto* Tree = UBreakerProgressionLibrary::GetTankBastionTree();
            FText Reason;
            // O272: six single points open the keystone's gate — Held Ground
            // (Wall's own travel) first, two whole pairs, a third travel —
            // then the commitment and Wall for one. Seven of the eight are
            // spent; the eighth is the honest remainder.
            for (const TCHAR* Node : { TEXT("Tank.Bastion.HeldGround"),
                TEXT("Tank.Bastion.LineOfSight"), TEXT("Tank.Bastion.ImmovableObject"),
                TEXT("Tank.Bastion.Footing"), TEXT("Tank.Bastion.Conversion"),
                TEXT("Tank.Bastion.Loud") })
                if (!TestTrue(FString::Printf(TEXT("purchase %s: %s"), Node, *Reason.ToString()), Progression->PurchaseNode(Tree, Node, Reason))) return false;
            TestEqual(TEXT("six invested opens the keystone gate"), Progression->GetTreeInvestment(Tree), 6);
            if (!TestTrue(TEXT("actual Bastion commitment"), Progression->CommitToBranch(Tree->TreeId, Reason))) return false;
            if (!TestTrue(TEXT("actual Wall purchase"), Progression->PurchaseNode(Tree, TEXT("Tank.Bastion.Wall"), Reason))) return false;
            TestEqual(TEXT("Wall walk leaves one of eight"), Progression->GetUnspentPoints(EBreakerPointCurrency::DoctrinePoints), 1);
        }
        auto* Grit = Tank->GetGrit();
        Grit->BindAttributes(Attributes);
        Grit->SetComponentTickEnabled(false);
        Grit->SetInCombat(true);
        for (int32 Second = 0; Second < 70; ++Second)
        {
            Grit->SetEnemyInProximity(true);
            Grit->AdvanceLoop(1.0f);
        }
        TestEqual(TEXT("ordinary combat entry and proximity bank 100 Grit"), Attributes->GetClassResource(), 100.0f);
        if (!TestTrue(TEXT("Tank Hold is actually unlocked"), Progression->IsAbilityUnlocked(TEXT("Tank.Hold")))) return false;
        const auto Handle = ASC->GiveAbility(FGameplayAbilitySpec(UBreakerAbility_Hold::StaticClass(), 1));
        if (!TestTrue(TEXT("actual paid Hold activates"), ASC->TryActivateAbility(Handle))) return false;
        if (!TestTrue(TEXT("paid Hold remains active"), ASC->FindAbilitySpecFromHandle(Handle)->IsActive())) return false;
        TestEqual(TEXT("Hold pays all 100 Grit"), Attributes->GetClassResource(), 0.0f);
        const float Maximum = Attributes->GetMaxHealth();
        auto Hit = [&](float Fraction)
        {
            Combat->RestoreVitals();
            FBreakerDamageRequest Request;
            Request.BaseDamage = Maximum * Fraction;
            Request.DamageFamily = EBreakerDamageFamily::TrueDamage;
            Request.bCanCritical = false;
            Request.bCanBeAvoided = false;
            Request.bBypassShield = true;
            return Combat->ReceiveDamage(Request).HealthDamage;
        };
        TestEqual(TEXT("large landed hit uses the purchased per-hit cap"), Hit(.6f), Maximum * (bWall ? .125f : .25f), .001f);
        TestEqual(TEXT("small landed hit is not reduced"), Hit(.05f), Maximum * .05f, .001f);
        if (bWall)
        {
            for (int32 Step = 0; Step < 400; ++Step)
            {
                ++GFrameCounter;
                World->Tick(LEVELTICK_All, .05f);
                if (!World->GetTimerManager().HasBeenTickedThisFrame()) World->GetTimerManager().Tick(.05f);
            }
            TestFalse(TEXT("real window timer ends Wall"), ASC->FindAbilitySpecFromHandle(Handle)->IsActive());
        }
        else ASC->CancelAbilityHandle(Handle);
        TestFalse(TEXT("Hold is ended"), ASC->FindAbilitySpecFromHandle(Handle)->IsActive());
        TestEqual(TEXT("ended Hold leaves the same large hit uncapped"), Hit(.6f), Maximum * .6f, .001f);
    }
    return true;
}
#endif
