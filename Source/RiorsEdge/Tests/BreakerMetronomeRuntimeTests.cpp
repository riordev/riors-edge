#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Abilities/BreakerSupportAbilities.h"
#include "Abilities/BreakerAbilityTags.h"
#include "Abilities/BreakerGameplayAbility.h"
#include "Abilities/BreakerAbilityDefinition.h"
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
#include "Progression/BreakerProgressionTree.h"
#include "Progression/BreakerExperience.h"
#include "Save/BreakerMissionContent.h"
#include "Weapons/BreakerWeaponComponent.h"
#include <limits>

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerMetronomeRuntimeTest, "RiorsEdge.Abilities.MetronomeRuntime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerMetronomeRuntimeTest::RunTest(const FString& Parameters)
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

    auto* Target = World->SpawnActor<AActor>();
    if (!TestNotNull(TEXT("actual damage receiver"), Target)) return false;
    auto* TargetCombat = NewObject<UBreakerCombatComponent>(Target);
    Target->AddInstanceComponent(TargetCombat); TargetCombat->RegisterComponent();
    auto* Health = NewObject<UBreakerAttributeSet>(Target);
    Health->ApplyMaxHealth(100000); Health->ApplyHealth(100000); TargetCombat->BindAttributes(Health);
    auto PeekDamage = [&](ABreakerCharacter* Player, EBreakerDamageDelivery Delivery = EBreakerDamageDelivery::Weapon)
    {
        FBreakerDamageRequest Hit; Hit.BaseDamage = 10; Hit.Delivery = Delivery;
        Player->GetCombat()->ApplyOutgoingModifiers(Hit);
        return Hit.BaseDamage;
    };
    auto Hit = [&](ABreakerCharacter* Player, EBreakerDamageDelivery Delivery = EBreakerDamageDelivery::Weapon, float Proc = 1, bool Dot = false)
    {
        FBreakerDamageRequest Request; Request.BaseDamage = 10; Request.Delivery = Delivery;
        Request.ProcCoefficient = Proc; Request.bIsDamageOverTime = Dot; Request.bCanCritical = false;
        Request.SetInstigator(Player); Player->GetCombat()->ApplyOutgoingModifiers(Request);
        return TargetCombat->ReceiveDamage(Request);
    };
    auto GrantCast = [&](ABreakerCharacter* Player, TSubclassOf<UGameplayAbility> Ability)
    {
        Player->GetAttributes()->ApplyClassResource(100);
        auto* ASC = Player->GetAbilitySystemComponent();
        const auto Handle = ASC->GiveAbility(FGameplayAbilitySpec(Ability, 1));
        TestTrue(TEXT("actual native ability cast"), ASC->TryActivateAbility(Handle));
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

    Advance(1);
    Second->SetActorLocation(FVector(2500, 0, 100));
    const auto FirstMetro = GrantCast(First, UBreakerAbility_Metronome::StaticClass());
    TestTrue(TEXT("actual successful weapon damage"), Hit(First).HealthDamage > 0);
    TestEqual(TEXT("caster first stack pays two flat"), PeekDamage(First), 12.0f);
    TestEqual(TEXT("ally has independent zero-stack ramp"), PeekDamage(Ally), 10.0f);
    TestTrue(TEXT("ally actual hit"), Hit(Ally).HealthDamage > 0);
    TestEqual(TEXT("recipient owns independent first stack"), PeekDamage(Ally), 12.0f);
    TestEqual(TEXT("weapon payload cannot increase ability damage"), PeekDamage(Ally, EBreakerDamageDelivery::Ability), 10.0f);
    Hit(Ally, EBreakerDamageDelivery::Ability);
    TestEqual(TEXT("baseline ability hit does not climb ramp"), PeekDamage(Ally), 12.0f);
    Hit(Ally, EBreakerDamageDelivery::Weapon, 1, true);
    TestEqual(TEXT("baseline DoT does not climb ramp"), PeekDamage(Ally), 12.0f);
    TargetCombat->DodgeChance = 1;
    TestTrue(TEXT("actual weapon damage can be dodged"), Hit(Ally).bDodged);
    TargetCombat->DodgeChance = 0;
    TestEqual(TEXT("dodged hit cannot climb ramp"), PeekDamage(Ally), 12.0f);
    FBreakerDamageRequest Melee; Melee.BaseDamage = 10; Melee.bCanCritical = false; Melee.SetInstigator(Ally);
    Melee.SourceTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Ability.Class.Caster.Cleave")));
    Melee.SourceTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Damage.Melee")));
    Ally->GetCombat()->ApplyOutgoingModifiers(Melee);
    TestTrue(TEXT("tagged weapon-delivered Cleave actually damages"), TargetCombat->ReceiveDamage(Melee).HealthDamage > 0);
    TestEqual(TEXT("weapon-delivered ability cannot climb baseline ramp"), PeekDamage(Ally), 12.0f);
    Ally->SetActorLocation(FVector(2000, 0, 100));
    Hit(Ally);
    TestEqual(TEXT("snapshot recipient retains buff outside cast radius"), PeekDamage(Ally), 14.0f);
    Second->SetActorLocation(FVector(200, 0, 100));
    Hit(Second);
    TestEqual(TEXT("late arrival cannot acquire snapshot buff"), PeekDamage(Second), 10.0f);
    Advance(23);
    TestEqual(TEXT("actual idle gap clears ramp"), PeekDamage(Ally), 10.0f);
    First->GetAbilitySystemComponent()->CancelAbilityHandle(FirstMetro);
    TestEqual(TEXT("cancel removes actual recipient payload"), PeekDamage(Ally), 10.0f);

    // Restored authored eight-point campaign fixture isolates purchased consumers.
    SettleCampaignFixture(Second->GetProgression());
    const auto* Tree = UBreakerProgressionLibrary::GetSupportConductorTree();
    FText Reason;
    for (const TCHAR* Node : { TEXT("Support.Conductor.Sustain"), TEXT("Support.Conductor.Sustain"),
        TEXT("Support.Conductor.Tempo") })
        if (!TestTrue(Node, Second->GetProgression()->PurchaseNode(Tree, Node, Reason))) return false;
    Ally->SetActorLocation(FVector(300, 0, 100));
    const auto SelfTempo = GrantCast(Second, UBreakerAbility_Metronome::StaticClass());
    for (int32 I = 0; I < 7; ++I) { Hit(Second); Hit(Ally); }
    TestEqual(TEXT("Tempo rank one extends only caster cap"), PeekDamage(Second), 24.0f);
    TestEqual(TEXT("Tempo rank one leaves ally baseline cap"), PeekDamage(Ally), 20.0f);
    Advance(23);
    TestEqual(TEXT("Tempo rank one extends only caster gap"), PeekDamage(Second), 24.0f);
    TestEqual(TEXT("Tempo rank one leaves ally baseline gap"), PeekDamage(Ally), 10.0f);
    Second->GetAbilitySystemComponent()->CancelAbilityHandle(SelfTempo);
    FGameplayTagContainer MetroCooldown;
    MetroCooldown.AddTag(GetDefault<UBreakerAbility_Metronome>()->GetAbilityDefinition()->CooldownTag);
    Second->GetAbilitySystemComponent()->RemoveActiveEffectsWithGrantedTags(MetroCooldown);
    for (const TCHAR* Node : { TEXT("Support.Conductor.Tempo"), TEXT("Support.Conductor.Counterpoint") })
        if (!TestTrue(Node, Second->GetProgression()->PurchaseNode(Tree, Node, Reason))) return false;
    const auto CounterMetro = GrantCast(Second, UBreakerAbility_Metronome::StaticClass());
    Hit(Ally, EBreakerDamageDelivery::Ability, .25f, true);
    TestEqual(TEXT("Counterpoint preserves fractional tick credit"), PeekDamage(Ally), 10.5f);
    Hit(Ally, EBreakerDamageDelivery::Ability, 0, true);
    TestEqual(TEXT("zero proc cannot climb ramp"), PeekDamage(Ally), 10.5f);
    for (int32 I = 0; I < 12; ++I) Hit(Ally);
    TestEqual(TEXT("Tempo rank two applies higher cap to ally"), PeekDamage(Ally), 26.0f);
    Advance(23);
    TestEqual(TEXT("Tempo ally gap lasts beyond baseline second"), PeekDamage(Ally), 26.0f);
    Advance(10);
    TestEqual(TEXT("Tempo extended gap still expires"), PeekDamage(Ally), 10.0f);
    Second->GetAbilitySystemComponent()->CancelAbilityHandle(CounterMetro);

    // A new independent owner can end without removing the other's contribution.
    auto* Third = MakePlayer(FVector(100, 0, 100), EBreakerClassId::Support);
    if (!TestNotNull(TEXT("independent second buff owner"), Third)) return false;
    auto* Fourth = MakePlayer(FVector(100, 100, 100), EBreakerClassId::Support);
    if (!TestNotNull(TEXT("independent third buff owner"), Fourth)) return false;
    const auto ThirdMetro = GrantCast(Third, UBreakerAbility_Metronome::StaticClass());
    const auto FourthMetro = GrantCast(Fourth, UBreakerAbility_Metronome::StaticClass());
    auto* FirstState = First->FindComponentByClass<UBreakerAbilityStateComponent>();
    TestTrue(TEXT("receiving Support has actual foreign buff window"), FirstState && FirstState->IsWindowActive(UBreakerAbility_Metronome::WindowKey()));
    if (FirstState) TestEqual(TEXT("receiving another caster buff creates no owned upkeep recipients"), FirstState->GetMaintainedBuffRecipientCount(), 0);
    Hit(Ally);
    TestEqual(TEXT("distinct Metronome sources add within flat bucket"), PeekDamage(Ally), 14.0f);
    Third->GetAbilitySystemComponent()->CancelAbilityHandle(ThirdMetro);
    TestEqual(TEXT("one source cancellation preserves another"), PeekDamage(Ally), 12.0f);
    FBreakerDamageRequest Lethal; Lethal.BaseDamage = 100000; Lethal.bCanCritical = false;
    Fourth->GetCombat()->ReceiveDamage(Lethal);
    Advance(2);
    TestFalse(TEXT("source death ends its active ability"), Fourth->GetAbilitySystemComponent()->FindAbilitySpecFromHandle(FourthMetro)->IsActive());
    TestEqual(TEXT("source death removes owned recipient payload"), PeekDamage(Ally), 10.0f);

    First->GetCombat()->PushWeaponFlatDamage(TEXT("Test.A"), 3);
    First->GetCombat()->PushWeaponFlatDamage(TEXT("Test.B"), 2);
    TestEqual(TEXT("distinct weapon flat keys sum"), PeekDamage(First), 15.0f);
    First->GetCombat()->PushWeaponFlatDamage(TEXT("Test.A"), 4);
    First->GetCombat()->PushWeaponFlatDamage(NAME_None, 100);
    First->GetCombat()->PushWeaponFlatDamage(TEXT("Test.Bad"), std::numeric_limits<float>::quiet_NaN());
    TestEqual(TEXT("key replacement and invalid refusal"), PeekDamage(First), 16.0f);
    TestEqual(TEXT("weapon flat helper excludes ability delivery"), PeekDamage(First, EBreakerDamageDelivery::Ability), 10.0f);
    FBreakerDamageRequest WeaponDot; WeaponDot.BaseDamage = 10; WeaponDot.bIsDamageOverTime = true;
    First->GetCombat()->ApplyOutgoingModifiers(WeaponDot);
    TestEqual(TEXT("weapon-delivered DoT receives no shot-only flat"), WeaponDot.BaseDamage, 10.0f);
    FBreakerDamageRequest Cleave; Cleave.BaseDamage = 10;
    Cleave.SourceTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Ability.Class.Caster.Cleave")));
    Cleave.SourceTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Damage.Melee")));
    First->GetCombat()->ApplyOutgoingModifiers(Cleave);
    TestEqual(TEXT("weapon-delivered Cleave receives no gun-only flat"), Cleave.BaseDamage, 10.0f);
    First->GetCombat()->PopWeaponFlatDamage(TEXT("Test.A"));
    TestEqual(TEXT("removing one key preserves another"), PeekDamage(First), 12.0f);
    First->GetCombat()->PopWeaponFlatDamage(TEXT("Test.B"));

    auto* Solo = MakePlayer(FVector(10000, 0, 100), EBreakerClassId::Support);
    if (!TestNotNull(TEXT("isolated Conduit owner"), Solo)) return false;
    SettleCampaignFixture(Solo->GetProgression());
    for (const TCHAR* Node : { TEXT("Support.Conductor.DownbeatDiscipline"), TEXT("Support.Conductor.DownbeatDiscipline"),
        TEXT("Support.Conductor.Rehearsal"), TEXT("Support.Conductor.Rehearsal"),
        TEXT("Support.Conductor.Sustain"), TEXT("Support.Conductor.Sustain") })
        if (!TestTrue(Node, Solo->GetProgression()->PurchaseNode(Tree, Node, Reason))) return false;
    if (!TestTrue(TEXT("actual Conductor branch commitment"), Solo->GetProgression()->CommitToBranch(Tree->TreeId, Reason))) return false;
    if (!TestTrue(TEXT("actual Downbeat keystone purchase"), Solo->GetProgression()->PurchaseNode(Tree, TEXT("Support.Conductor.Downbeat"), Reason))) return false;
    auto ResetCooldown = [&](TSubclassOf<UBreakerGameplayAbility> Ability)
    {
        FGameplayTagContainer Tags; Tags.AddTag(Ability->GetDefaultObject<UBreakerGameplayAbility>()->GetAbilityDefinition()->CooldownTag);
        Solo->GetAbilitySystemComponent()->RemoveActiveEffectsWithGrantedTags(Tags);
    };
    const auto EmptyConduit = GrantCast(Solo, UBreakerAbility_Conduit::StaticClass());
    Advance(2);
    TestEqual(TEXT("Conduit with zero buff holders adds no fabricated flat"), PeekDamage(Solo), 10.0f);
    Solo->GetAbilitySystemComponent()->CancelAbilityHandle(EmptyConduit);
    ResetCooldown(UBreakerAbility_Conduit::StaticClass());
    const auto SoloMetro = GrantCast(Solo, UBreakerAbility_Metronome::StaticClass());
    const auto SoloCadence = CastAura(Solo);
    auto* SoloState = Solo->FindComponentByClass<UBreakerAbilityStateComponent>();
    TestEqual(TEXT("two buffs on one person count one actual holder"), SoloState->GetMaintainedBuffRecipientCount(), 1);
    const auto OneConduit = GrantCast(Solo, UBreakerAbility_Conduit::StaticClass());
    Advance(2);
    TestEqual(TEXT("one holder contributes four flat"), PeekDamage(Solo), 14.0f);
    TestEqual(TEXT("actual Conduit doubles Cadence bonus above one"), Solo->GetWeapon()->GetReloadSpeedMultiplier(), 1.5f);
    Hit(Solo);
    TestEqual(TEXT("actual Conduit doubles Metronome stack payload"), PeekDamage(Solo), 18.0f);
    Ally->SetActorLocation(FVector(10200, 0, 100));
    Advance(2);
    TestEqual(TEXT("Cadence adds second unique living holder"), SoloState->GetMaintainedBuffRecipientCount(), 2);
    TestEqual(TEXT("second live holder adds four flat without double counting owner"), PeekDamage(Solo), 22.0f);
    Solo->GetAbilitySystemComponent()->CancelAbilityHandle(OneConduit);
    Advance(2);
    TestEqual(TEXT("Conduit removal restores ordinary stack payload"), PeekDamage(Solo), 12.0f);
    TestEqual(TEXT("Conduit removal restores ordinary tempo"), Solo->GetWeapon()->GetReloadSpeedMultiplier(), 1.25f);
    Solo->GetAbilitySystemComponent()->CancelAbilityHandle(SoloCadence);
    ResetCooldown(UBreakerAbility_Metronome::StaticClass());
    Solo->GetAttributes()->ApplyClassResource(100);
    TestTrue(TEXT("actual live Metronome reapplication"), Solo->GetAbilitySystemComponent()->TryActivateAbility(SoloMetro));
    TestEqual(TEXT("purchased Rehearsal preserves existing ramp on recast"), PeekDamage(Solo), 12.0f);
    Solo->GetAbilitySystemComponent()->CancelAbilityHandle(SoloMetro);
    ResetCooldown(UBreakerAbility_Metronome::StaticClass());
    Solo->GetAttributes()->ApplyClassResource(100);
    TestTrue(TEXT("fresh actual cast after explicit cancellation"), Solo->GetAbilitySystemComponent()->TryActivateAbility(SoloMetro));
    TestEqual(TEXT("canceled ramp is not preserved by Rehearsal"), PeekDamage(Solo), 10.0f);
    TestEqual(TEXT("canceled recast receives no Rehearsal refund"), Solo->GetAttributes()->GetClassResource(),
        100.0f - GetDefault<UBreakerAbility_Metronome>()->GetAbilityDefinition()->ResourceCost);
    Advance(320);
    TestFalse(TEXT("natural owned window expiration ends ability"), Solo->GetAbilitySystemComponent()->FindAbilitySpecFromHandle(SoloMetro)->IsActive());
    TestEqual(TEXT("expired Metronome leaves no damage payload"), PeekDamage(Solo), 10.0f);
    TestEqual(TEXT("expired owned recipients leave no Conduit count"), SoloState->GetMaintainedBuffRecipientCount(), 0);
    return true;
}
#endif
