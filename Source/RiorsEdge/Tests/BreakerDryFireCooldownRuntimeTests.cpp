#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Classes/BreakerMomentumComponent.h"
#include "Abilities/BreakerAbility_Sightline.h"
#include "Abilities/BreakerGameplayAbility.h"
#include "Combat/BreakerCombatComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Progression/BreakerExperience.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionLibrary.h"
#include "Save/BreakerMissionContent.h"
#include "Weapons/BreakerWeaponComponent.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerDryFireCooldownRuntimeTest,
    "RiorsEdge.Classes.Momentum.DryFirePaidCooldown", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerDryFireCooldownRuntimeTest::RunTest(const FString& Parameters)
{
    // O272: Dry Fire is a single rank — the grant and the one-second refund
    // land together. Without the node the cooldown is untouched; with it,
    // one buy shaves exactly one second.
    for (const bool bOwnsDryFire : { false, true })
    {
        UWorld::InitializationValues Init;
        Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
        auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
        if (!World) return false;
        GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
        World->InitializeActorsForPlay(FURL());
        const uint64 SavedFrame = GFrameCounter;
        ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); GFrameCounter = SavedFrame; };
        auto* Player = World->SpawnActor<ABreakerCharacter>();
        if (!Player) return false;
        Player->SetActorTickEnabled(false);
        auto* Movement = Player->GetBreakerMovement(); Movement->SetComponentTickEnabled(false);
        auto* ASC = Player->GetAbilitySystemComponent(); auto* Attributes = Player->GetAttributes();
        ASC->InitAbilityActorInfo(Player, Player); ASC->AddAttributeSetSubobject(Attributes);
        Player->GetCombat()->BindAttributes(Attributes);
        auto* Progression = Player->GetProgression(); Progression->BindAttributes(Attributes);
        if (!Progression->ChoosePermanentClassById(EBreakerClassId::Swift)) return false;
        Progression->AwardExperience(UBreakerExperienceLibrary::TotalXpToReachLevel(50, Progression->ExperienceCurve));
        FBreakerQuestFlagSet Flags;
        for (const auto& Mission : UBreakerMissionLibrary::GetMissions())
            for (const auto& Beat : Mission.Beats)
                for (FName Flag : UBreakerMissionLibrary::BeatCompletionFlags(Beat)) Flags.Add(Flag);
        Flags.Add(TEXT("Quest.Finale.Seal")); Progression->SettleDoctrineEntitlement(Flags);
        FText Reason;
        auto* Tree = UBreakerProgressionLibrary::GetSwiftFrenzyTree();
        // Dry Fire heads its own pair (O272): one legal buy, no entry purchases.
        if (bOwnsDryFire)
            if (!TestTrue(TEXT("legal single Dry Fire purchase"), Progression->PurchaseNode(Tree, TEXT("Swift.Frenzy.DryFire"), Reason))) return false;
        if (!Progression->IsAbilityUnlocked(TEXT("Swift.Sightline")))
            if (!TestTrue(TEXT("earned token unlocks Sightline"), Progression->SpendAbilityToken(TEXT("Swift.Sightline"), Reason))) return false;
        auto* Momentum = Player->GetMomentum(); Momentum->BindAttributes(Attributes); Momentum->BeginPlay(); Momentum->SetComponentTickEnabled(false);
        Movement->SetMovementMode(MOVE_Walking); Movement->Velocity = FVector(Movement->WalkSpeed, 0, 0);
        for (int32 Second = 0; Second < 40; ++Second)
        {
            Player->SetActorLocation(Player->GetActorLocation() + Movement->Velocity);
            Momentum->AdvanceLoop(1);
        }
        Movement->StopMovementImmediately();
        if (!TestEqual(TEXT("ordinary ground travel funds cast"), Momentum->GetMomentum(), 100.0f)) return false;
        auto* Weapon = Player->GetWeapon(); Weapon->BeginPlay(); Weapon->SetComponentTickEnabled(false);
        auto Advance = [&](float Seconds) { ++GFrameCounter; World->Tick(LEVELTICK_All, Seconds); };
        Advance(1);
        // Spend real ammunition down to the final round before starting the cooldown.
        for (int32 Attempt = 0; Attempt < 100 && Weapon->GetMagazineAmmo() > 1; ++Attempt)
        {
            Weapon->StartFire(); Weapon->StopFire(); Advance(.2f);
        }
        if (!TestEqual(TEXT("ordinary rifle magazine reaches one round"), Weapon->GetMagazineAmmo(), 1)) return false;
        const auto Ability = ASC->GiveAbility(FGameplayAbilitySpec(UBreakerAbility_Sightline::StaticClass(), 1));
        const float ResourceBefore = Momentum->GetMomentum();
        if (!TestTrue(TEXT("actual paid Sightline activates"), ASC->TryActivateAbility(Ability))) return false;
        TestTrue(TEXT("cast spends actual Momentum"), Momentum->GetMomentum() < ResourceBefore);
        FGameplayEffectQuery Query; Query.EffectDefinition = UBreakerAbilityCooldownEffect::StaticClass();
        const auto Cooldowns = ASC->GetActiveEffects(Query);
        if (!TestEqual(TEXT("paid cast creates one real cooldown"), Cooldowns.Num(), 1)) return false;
        auto Remaining = [&]() { const auto* Effect = ASC->GetActiveGameplayEffect(Cooldowns[0]); return Effect ? Effect->GetTimeRemaining(World->GetTimeSeconds()) : 0.0f; };
        const float Before = Remaining();
        Weapon->StartFire(); Weapon->StopFire();
        TestEqual(TEXT("actual final round consumed"), Weapon->GetMagazineAmmo(), 0);
        TestEqual(TEXT("one Dry Fire buy shaves exactly one second; none shaves nothing"), Before - Remaining(), bOwnsDryFire ? 1.0f : 0.0f, .001f);
        const float After = Remaining();
        Weapon->StartReload();
        TestEqual(TEXT("starting reload never shaves cooldown"), Remaining(), After, .001f);
        Advance(.25f);
        TestEqual(TEXT("reload time only ages cooldown normally"), Remaining(), After - .25f, .001f);
    }
    return true;
}
#endif
