#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Abilities/BreakerSupportAbilities.h"
#include "Abilities/BreakerAbilityTags.h"
#include "Abilities/BreakerGameplayAbility.h"
#include "Abilities/BreakerAbilityStateComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Classes/BreakerChargeComponent.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerZoneActor.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionLibrary.h"
#include "Progression/BreakerExperience.h"
#include "Save/BreakerMissionContent.h"
#include "Weapons/BreakerWeaponComponent.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerCadenceAuraRuntimeTest, "RiorsEdge.Abilities.CadenceAuraRuntime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerCadenceAuraRuntimeTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Init;
    Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!TestNotNull(TEXT("isolated aura world"), World)) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL());
    const uint64 InitialFrame = GFrameCounter;
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); GFrameCounter = InitialFrame; };
    auto MakePlayer = [&](FVector Location, EBreakerClassId Class)
    {
        FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        auto* Player = World->SpawnActor<ABreakerCharacter>(Location, FRotator::ZeroRotator, Spawn);
        if (!Player) return Player;
        auto* ASC = Player->GetAbilitySystemComponent();
        ASC->InitAbilityActorInfo(Player, Player); ASC->AddAttributeSetSubobject(Player->GetAttributes());
        Player->GetCombat()->BindAttributes(Player->GetAttributes());
        Player->GetProgression()->BindAttributes(Player->GetAttributes());
        TestTrue(TEXT("actual permanent class selection"), Player->GetProgression()->ChoosePermanentClassById(Class));
        Player->GetBreakerMovement()->SetComponentTickEnabled(false);
        Player->FindComponentByClass<UBreakerChargeComponent>()->BindAttributes(Player->GetAttributes());
        Player->GetAttributes()->ApplyClassResource(100);
        return Player;
    };
    auto* First = MakePlayer(FVector(0, 0, 100), EBreakerClassId::Support);
    auto* Second = MakePlayer(FVector(0, 100, 100), EBreakerClassId::Support);
    auto* Ally = MakePlayer(FVector(250, 0, 100), EBreakerClassId::Swift);
    if (!First || !Second || !Ally) return false;
    auto CastAura = [&](ABreakerCharacter* Player)
    {
        // Native ability grant isolates aura delivery, not story acquisition.
        auto* ASC = Player->GetAbilitySystemComponent();
        const auto Handle = ASC->GiveAbility(FGameplayAbilitySpec(UBreakerAbility_Cadence::StaticClass(), 1));
        TestTrue(TEXT("actual Cadence cast commits"), ASC->TryActivateAbility(Handle));
        return Handle;
    };
    auto Advance = [&](int32 Steps)
    {
        const double Before = World->GetTimeSeconds();
        for (int32 I = 0; I < Steps; ++I)
        {
            // Character BeginPlay would load owner saves. Drive the real native
            // clocks explicitly where this isolated world omits actor startup.
            TArray<UBreakerAbilityStateComponent*> States;
            for (TActorIterator<ABreakerCharacter> It(World); It; ++It)
                if (auto* State = It->FindComponentByClass<UBreakerAbilityStateComponent>())
                { State->SetComponentTickEnabled(false); States.Add(State); }
            TArray<TWeakObjectPtr<ABreakerZoneActor>> Zones;
            for (TActorIterator<ABreakerZoneActor> It(World); It; ++It)
            { It->SetActorTickEnabled(false); Zones.Add(*It); }
            ++GFrameCounter;
            World->Tick(LEVELTICK_All, .05f);
            for (auto* State : States) State->AdvanceTime(.05f);
            for (const auto& Held : Zones)
                if (auto* Zone = Held.Get()) if (!Zone->IsActorBeingDestroyed()) Zone->AdvanceZone(.05f);
            if (!World->GetTimerManager().HasBeenTickedThisFrame()) World->GetTimerManager().Tick(.05f);
        }
        TestTrue(TEXT("actual world clock advances through fixture"), World->GetTimeSeconds() >= Before + Steps * .05 - .001);
    };
    auto SettleCampaignFixture = [&](UBreakerProgressionComponent* Progression)
    {
        // Restored completed-campaign state tests purchased consumers, not a playthrough.
        Progression->AwardExperience(UBreakerExperienceLibrary::TotalXpToReachLevel(50, FBreakerExperienceCurve()));
        FBreakerQuestFlagSet Flags;
        for (const auto& Mission : UBreakerMissionLibrary::GetMissions())
            for (const auto& Beat : Mission.Beats)
                for (FName Flag : UBreakerMissionLibrary::BeatCompletionFlags(Beat)) Flags.Add(Flag);
        Flags.Add(TEXT("Quest.Finale.Seal"));
        Progression->SettleDoctrineEntitlement(Flags);
        TestEqual(TEXT("actual authored benchmark settlement pays eight"), Progression->GetUnspentPoints(EBreakerPointCurrency::DoctrinePoints), 8);
    };
    const auto FirstCast = CastAura(First);
    auto* AllyState = Ally->FindComponentByClass<UBreakerAbilityStateComponent>();
    if (!TestNotNull(TEXT("aura creates recipient state"), AllyState)) return false;
    TestEqual(TEXT("actual ally receives reload tempo"), Ally->GetWeapon()->GetReloadSpeedMultiplier(), 1.25f);
    TestEqual(TEXT("actual caster receives self-first tempo"), First->GetWeapon()->GetSwapSpeedMultiplier(), 1.25f);
    const auto SecondCast = CastAura(Second);
    TestEqual(TEXT("two actual supports cannot multiply tempo"), Ally->GetWeapon()->GetReloadSpeedMultiplier(), 1.25f);
    First->GetAbilitySystemComponent()->CancelAbilityHandle(FirstCast);
    TestEqual(TEXT("one caster cancellation preserves other's tempo"), Ally->GetWeapon()->GetReloadSpeedMultiplier(), 1.25f);
    TestTrue(TEXT("one caster cancellation preserves other's HUD window"), AllyState->IsWindowActive(UBreakerAbility_Cadence::WindowKey()));
    // UE activates pre-first-tick PendingTimerSet entries only at the END of
    // its first tick. Start the real timer clock before testing steady polling.
    Advance(1);
    Ally->SetActorLocation(FVector(2000, 0, 100)); Advance(2);
    TestEqual(TEXT("leaving actual aura removes only recipient tempo"), Ally->GetWeapon()->GetReloadSpeedMultiplier(), 1.0f);
    TestFalse(TEXT("leaving actual aura closes matching HUD"), AllyState->IsWindowActive(UBreakerAbility_Cadence::WindowKey()));
    Ally->SetActorLocation(FVector(250, 0, 100)); Advance(2);
    TestEqual(TEXT("re-entry restores actual tempo"), Ally->GetWeapon()->GetReloadSpeedMultiplier(), 1.25f);
    AllyState->ExtendWindow(UBreakerAbility_Cadence::WindowKey(), 2.0f);
    Advance(160);
    TestTrue(TEXT("extended recipient remains visibly buffed after footprint expires"), AllyState->IsWindowActive(UBreakerAbility_Cadence::WindowKey()));
    TestEqual(TEXT("extension retains real payload too"), Ally->GetWeapon()->GetReloadSpeedMultiplier(), 1.25f);
    Advance(45);
    TestFalse(TEXT("extended window eventually expires"), AllyState->IsWindowActive(UBreakerAbility_Cadence::WindowKey()));
    TestEqual(TEXT("expiry removes actual payload"), Ally->GetWeapon()->GetReloadSpeedMultiplier(), 1.0f);
    TestFalse(TEXT("ability ends after its last extended recipient"), Second->GetAbilitySystemComponent()->FindAbilitySpecFromHandle(SecondCast)->IsActive());
    auto* SectionCaster = MakePlayer(FVector(0, 0, 100), EBreakerClassId::Support);
    if (!SectionCaster) return false;
    auto* Progression = SectionCaster->GetProgression();
    SettleCampaignFixture(Progression);
    FText Failure;
    if (!TestTrue(TEXT("actual Section purchase"), Progression->PurchaseNode(UBreakerProgressionLibrary::GetSupportConductorTree(), TEXT("Support.Conductor.Section"), Failure))) return false;
    Ally->SetActorLocation(FVector(650, 0, 100));
    CastAura(SectionCaster);
    TestEqual(TEXT("purchased Section reaches beyond base radius"), Ally->GetWeapon()->GetReloadSpeedMultiplier(), 1.25f);
    SectionCaster->SetActorLocation(FVector(1000, 0, 100));
    Ally->SetActorLocation(FVector(1650, 0, 100)); Advance(2);
    TestEqual(TEXT("Section aura keeps pace with moving caster"), Ally->GetWeapon()->GetReloadSpeedMultiplier(), 1.25f);
    FBreakerDamageRequest Lethal; Lethal.BaseDamage = 100000; Lethal.bCanCritical = false;
    SectionCaster->GetCombat()->ReceiveDamage(Lethal);
    TestTrue(TEXT("real source death"), SectionCaster->GetCombat()->IsDead());
    TestEqual(TEXT("source death removes allied payload immediately"), Ally->GetWeapon()->GetReloadSpeedMultiplier(), 1.0f);
    TestFalse(TEXT("source death removes owned HUD"), AllyState->IsWindowActive(UBreakerAbility_Cadence::WindowKey()));
    First->SetActorLocation(FVector(4000, 0, 100));
    Second->SetActorLocation(FVector(4000, 200, 100));
    Ally->SetActorLocation(FVector(250, 0, 100));
    auto* BatonCaster = MakePlayer(FVector(0, 0, 100), EBreakerClassId::Support);
    auto* ReceivingSupport = MakePlayer(FVector(300, 200, 100), EBreakerClassId::Support);
    if (!BatonCaster || !ReceivingSupport) return false;
    auto* BatonProgression = BatonCaster->GetProgression();
    SettleCampaignFixture(BatonProgression);
    for (const TCHAR* Id : {TEXT("Support.Conductor.DownbeatDiscipline"), TEXT("Support.Conductor.DownbeatDiscipline"),
        TEXT("Support.Conductor.Rehearsal"), TEXT("Support.Conductor.Rehearsal"),
        TEXT("Support.Conductor.Conducting"), TEXT("Support.Conductor.DetachedBaton")})
        if (!TestTrue(Id, BatonProgression->PurchaseNode(UBreakerProgressionLibrary::GetSupportConductorTree(), Id, Failure))) return false;
    const auto BatonCast = CastAura(BatonCaster);
    auto* OutsideSwift = MakePlayer(FVector(4000, 400, 100), EBreakerClassId::Swift);
    if (!OutsideSwift) return false;
    auto ApplyCooldown = [&](ABreakerCharacter* Player)
    {
        auto* ASC = Player->GetAbilitySystemComponent();
        auto Spec = ASC->MakeOutgoingSpec(UBreakerAbilityCooldownEffect::StaticClass(), 1, ASC->MakeEffectContext());
        Spec.Data->SetSetByCallerMagnitude(BreakerAbilityTags::Data_AbilityCooldown.GetTag(), 8.0f);
        return ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
    };
    const auto AllyCooldown = ApplyCooldown(Ally);
    const auto OutsideCooldown = ApplyCooldown(OutsideSwift);
    TestTrue(TEXT("real recipient cooldown effect applies"), AllyCooldown.IsValid());
    TestTrue(TEXT("real outside control cooldown effect applies"), OutsideCooldown.IsValid());
    BatonCaster->SetActorLocation(FVector(2000, 0, 100));
    Advance(22);
    const FActiveGameplayEffect* AllyEffect = Ally->GetAbilitySystemComponent()->GetActiveGameplayEffect(AllyCooldown);
    const FActiveGameplayEffect* OutsideEffect = OutsideSwift->GetAbilitySystemComponent()->GetActiveGameplayEffect(OutsideCooldown);
    if (!TestNotNull(TEXT("recipient cooldown still active for measurement"), AllyEffect)
        || !TestNotNull(TEXT("outside cooldown still active for measurement"), OutsideEffect)) return false;
    TestEqual(TEXT("Conducting actually shaves a non-Support ally beyond ordinary world time"),
        OutsideEffect->GetTimeRemaining(World->GetTimeSeconds()) - AllyEffect->GetTimeRemaining(World->GetTimeSeconds()), .25f, .01f);
    Advance(108);
    TestEqual(TEXT("stationary Baton keeps original ally buffed"), Ally->GetWeapon()->GetReloadSpeedMultiplier(), 1.25f);
    TestEqual(TEXT("Conducting and Downbeat self tail expires outside stationary aura"), BatonCaster->GetWeapon()->GetReloadSpeedMultiplier(), 1.0f);
    TestTrue(TEXT("caster maintains Charge source while buffing distant allies"), BatonCaster->FindComponentByClass<UBreakerChargeComponent>()->HasAnyBuffActive());
    TestFalse(TEXT("receiving Support does not earn caster upkeep credit"), ReceivingSupport->FindComponentByClass<UBreakerChargeComponent>()->HasAnyBuffActive());
    Ally->GetCombat()->ReceiveDamage(Lethal);
    TestTrue(TEXT("actual recipient death"), Ally->GetCombat()->IsDead());
    TestEqual(TEXT("recipient death removes tempo immediately"), Ally->GetWeapon()->GetReloadSpeedMultiplier(), 1.0f);
    TestEqual(TEXT("other living recipient keeps same caster's buff"), ReceivingSupport->GetWeapon()->GetReloadSpeedMultiplier(), 1.25f);
    BatonCaster->GetAbilitySystemComponent()->CancelAbilityHandle(BatonCast);
    TestEqual(TEXT("Baton cancellation cleans remaining allied payload"), ReceivingSupport->GetWeapon()->GetReloadSpeedMultiplier(), 1.0f);
    TestFalse(TEXT("Baton cancellation clears caster upkeep source"), BatonCaster->FindComponentByClass<UBreakerChargeComponent>()->HasAnyBuffActive());
    return true;
}
#endif
