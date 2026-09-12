#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Abilities/BreakerAbilityComponent.h"
#include "Abilities/BreakerTankAbilities.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Classes/BreakerGritComponent.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerDamageLibrary.h"
#include "Combat/BreakerEnemy.h"
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
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerDetonationRuntimeTest,
    "RiorsEdge.Abilities.DetonationPaidSlotRelease", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerDetonationRuntimeTest::RunTest(const FString& Parameters)
{
    for (int32 Scenario = 0; Scenario < 3; ++Scenario)
    {
        UWorld::InitializationValues Init;
        Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
        auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
        if (!TestNotNull(TEXT("isolated Detonation world"), World)) return false;
        GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
        World->InitializeActorsForPlay(FURL());
        const uint64 SavedFrame = GFrameCounter;
        ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); GFrameCounter = SavedFrame; };
        auto* Tank = World->SpawnActor<ABreakerCharacter>();
        if (!Tank) return false;
        Tank->SetActorTickEnabled(false); Tank->GetBreakerMovement()->SetComponentTickEnabled(false);
        auto* ASC = Tank->GetAbilitySystemComponent();
        auto* Attributes = Tank->GetAttributes();
        auto* Combat = Tank->GetCombat();
        auto* Progression = Tank->GetProgression();
        ASC->InitAbilityActorInfo(Tank, Tank); ASC->AddAttributeSetSubobject(Attributes);
        // Capture neutral critical randomness in the fixture's base attributes,
        // so later condition recomposition preserves it; offensive pools stay real.
        Attributes->SetCriticalChance(0);
        Combat->BindAttributes(Attributes); Progression->BindAttributes(Attributes);
        if (!TestTrue(TEXT("actual Tank class"), Progression->ChoosePermanentClassById(EBreakerClassId::Tank))) return false;
        // Restored benchmark entitlement fixture, not a campaign acquisition claim.
        Progression->AwardExperience(UBreakerExperienceLibrary::TotalXpToReachLevel(50, Progression->ExperienceCurve));
        FBreakerQuestFlagSet Flags;
        for (const auto& Mission : UBreakerMissionLibrary::GetMissions())
            for (const auto& Beat : Mission.Beats)
                for (FName Flag : UBreakerMissionLibrary::BeatCompletionFlags(Beat)) Flags.Add(Flag);
        Flags.Add(TEXT("Quest.Finale.Seal")); Progression->SettleDoctrineEntitlement(Flags);
        TestEqual(TEXT("real entitlement eight"), Progression->GetUnspentPoints(EBreakerPointCurrency::DoctrinePoints), 8);
        const auto* Tree = UBreakerProgressionLibrary::GetTankDemolitionistTree();
        FText Reason;
        // O272: six single points open the keystone's gate — its own travel
        // first, two whole pairs, a third travel — then the commitment and
        // Detonation for one. Seven of the eight are spent; the eighth is the
        // honest remainder.
        for (const TCHAR* Node : { TEXT("Tank.Demolitionist.Fragmentation"),
            TEXT("Tank.Demolitionist.ShapedCharge"), TEXT("Tank.Demolitionist.BlastRadius"),
            TEXT("Tank.Demolitionist.Bootstraps"), TEXT("Tank.Demolitionist.KineticRecovery"),
            TEXT("Tank.Demolitionist.BracedForImpact") })
        {
            const bool bBought = Progression->PurchaseNode(Tree, Node, Reason);
            if (!TestTrue(FString::Printf(TEXT("purchase %s: %s"), Node, *Reason.ToString()), bBought)) return false;
        }
        TestEqual(TEXT("six invested opens the keystone gate"), Progression->GetTreeInvestment(Tree), 6);
        if (!TestTrue(TEXT("actual branch commitment"), Progression->CommitToBranch(Tree->TreeId, Reason))) return false;
        if (!TestTrue(TEXT("actual Detonation purchase"), Progression->PurchaseNode(Tree, TEXT("Tank.Demolitionist.Detonation"), Reason))) return false;
        TestEqual(TEXT("Detonation walk leaves one of eight"), Progression->GetUnspentPoints(EBreakerPointCurrency::DoctrinePoints), 1);
        auto* Grit = Tank->GetGrit(); Grit->BindAttributes(Attributes); Grit->SetComponentTickEnabled(false);
        Grit->SetInCombat(true);
        for (int32 Second = 0; Second < 70; ++Second) { Grit->SetEnemyInProximity(true); Grit->AdvanceLoop(1); }
        auto* Slots = Tank->GetAbilities(); Slots->RefreshGrants(); Slots->SetComponentTickEnabled(false);
        if (!TestEqual(TEXT("actual ultimate slot is Hold"), Slots->GetAbilityIdForSlot(EBreakerAbilitySlot::Ultimate), FName(TEXT("Tank.Hold")))) return false;
        if (!TestTrue(TEXT("first slot input casts paid Hold"), Slots->TryActivateSlot(EBreakerAbilitySlot::Ultimate))) return false;
        TestEqual(TEXT("first input pays all normal Grit"), Attributes->GetClassResource(), 0.0f);
        auto* Spec = ASC->FindAbilitySpecFromClass(UBreakerAbility_Hold::StaticClass());
        if (!Spec || !TestTrue(TEXT("actual granted Hold active"), Spec->IsActive())) return false;
        const auto Handle = Spec->Handle;
        float Ledger = 0;
        for (float Amount : { 20.0f, 10.0f })
        {
            FBreakerDamageRequest Hit; Hit.BaseDamage = Amount; Hit.DamageFamily = EBreakerDamageFamily::TrueDamage;
            Hit.bCanCritical = false; Hit.bCanBeAvoided = false;
            const auto Result = Combat->ReceiveDamage(Hit);
            Ledger += Result.HealthDamage + Result.ShieldDamage;
        }
        TestTrue(TEXT("actual accepted damage funds release ledger"), Ledger > 0);
        FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        auto* Enemy = World->SpawnActor<ABreakerEnemy>(FVector(300, 0, 0), FRotator::ZeroRotator, Spawn);
        if (!Enemy) return false;
        Enemy->SetActorTickEnabled(false);
        auto* EnemyHealth = FindObject<UBreakerAttributeSet>(Enemy, TEXT("Attributes"));
        auto* EnemyCombat = Enemy->FindComponentByClass<UBreakerCombatComponent>();
        if (!EnemyHealth || !EnemyCombat) return false;
        auto* EnemyASC = Enemy->GetAbilitySystemComponent();
        EnemyASC->InitAbilityActorInfo(Enemy, Enemy); EnemyASC->AddAttributeSetSubobject(EnemyHealth);
        EnemyHealth->ApplyMaxHealth(1000); EnemyHealth->ApplyHealth(1000); EnemyCombat->BindAttributes(EnemyHealth);
        FBreakerDamageRequest Control; Control.BaseDamage = Ledger * .7f; Control.bCanCritical = false;
        Control.SourceLocation = Tank->GetActorLocation(); Control.bHasSourceLocation = true; Control.SetInstigator(Tank);
        UBreakerDamageLibrary::FillSourcePools(Attributes, EBreakerDamageDelivery::Ability, Control);
        Combat->ApplyOutgoingModifiers(Control);
        const float Expected = EnemyCombat->ReceiveDamage(Control).HealthDamage;
        TestTrue(TEXT("mitigated positive control has a payable payload"), Expected > 0);
        EnemyCombat->RestoreVitals();
        const float Before = EnemyHealth->GetHealth();
        const float ResourceBeforeRelease = Attributes->GetClassResource();
        if (Scenario == 0)
        {
            TestEqual(TEXT("actual release retains neutral critical chance"), Attributes->GetCriticalChance(), 0.0f);
            if (!TestTrue(TEXT("second ultimate input releases"), Slots->TryActivateSlot(EBreakerAbilitySlot::Ultimate))) return false;
            TestEqual(TEXT("single release pays the actual ledger through mitigation"), Before - EnemyHealth->GetHealth(), Expected, .001f);
            TestEqual(TEXT("release does not spend resource again"), Attributes->GetClassResource(), ResourceBeforeRelease);
        }
        else if (Scenario == 1) ASC->CancelAbilityHandle(Handle);
        else
        {
            FBreakerDamageRequest Lethal; Lethal.BaseDamage = Attributes->GetMaxHealth() * 2;
            Lethal.DamageFamily = EBreakerDamageFamily::TrueDamage; Lethal.bCanCritical = false; Lethal.bCanBeAvoided = false;
            // Hold limits each hit, so actual repeated lethal pressure reaches death naturally.
            for (int32 Hit = 0; Hit < 10 && !Combat->IsDead(); ++Hit) Combat->ReceiveDamage(Lethal);
            TestTrue(TEXT("actual incoming damage kills caster"), Combat->IsDead());
        }
        const float After = EnemyHealth->GetHealth();
        if (Scenario != 0) TestEqual(TEXT("cancel or death pays nothing"), After, Before);
        TestFalse(TEXT("paid Hold has ended"), ASC->FindAbilitySpecFromHandle(Handle)->IsActive());
        for (int32 Step = 0; Step < 240; ++Step)
        {
            ++GFrameCounter; World->Tick(LEVELTICK_All, .05f);
            if (!World->GetTimerManager().HasBeenTickedThisFrame()) World->GetTimerManager().Tick(.05f);
        }
        TestEqual(TEXT("original expiry cannot replay a closed ledger"), EnemyHealth->GetHealth(), After);
    }
    return true;
}
#endif
