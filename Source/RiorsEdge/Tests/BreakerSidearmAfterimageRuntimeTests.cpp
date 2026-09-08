#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Abilities/BreakerGunsmithAbilities.h"
#include "Abilities/BreakerAbilityComponent.h"
#include "Abilities/BreakerAbilityDefinition.h"
#include "Classes/BreakerScrapComponent.h"
#include "Combat/BreakerZoneActor.h"
#include "EngineUtils.h"
#include "Abilities/BreakerAbility_CadenceBreak.h"
#include "Abilities/BreakerAbilityStateComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Classes/BreakerGritComponent.h"
#include "Classes/BreakerMomentumComponent.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerEnemy.h"
#include "Components/SphereComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PawnMovementComponent.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Progression/BreakerClassDefinition.h"
#include "Progression/BreakerExperience.h"
#include "Progression/BreakerProgressionLibrary.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionNode.h"
#include "Progression/BreakerProgressionTree.h"
#include "Save/BreakerMissionContent.h"
#include "Items/BreakerEquipmentComponent.h"
#include "Items/BreakerLootLibrary.h"
#include "Progression/BreakerCoreWheelMath.h"
#include "Weapons/BreakerWeaponComponent.h"
#include "Weapons/BreakerWeaponDefinition.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
    struct FBreakerSidearmAfterimageFixture
    {
        UWorld* World = nullptr;
        ABreakerCharacter* Player = nullptr;
        AActor* Target = nullptr;
        UBreakerProgressionTree* CoreTree = nullptr;
        uint64 SavedFrame = GFrameCounter;
        FBreakerSidearmAfterimageFixture(EBreakerClassId Class)
        {
            UWorld::InitializationValues Init; Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
            World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
            if (!World) return;
            GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); World->InitializeActorsForPlay(FURL());
            Player=World->SpawnActor<ABreakerCharacter>(); Player->bRefuseSavesForPendingCharacter=true; Player->SetActorTickEnabled(false);
            Player->GetBreakerMovement()->SetComponentTickEnabled(false); Player->GetBreakerMovement()->SetMovementMode(MOVE_Walking);
            auto* ASC=Player->GetAbilitySystemComponent(); auto* Attr=Player->GetAttributes();
            ASC->InitAbilityActorInfo(Player,Player); ASC->AddAttributeSetSubobject(Attr); Player->GetCombat()->BindAttributes(Attr);
            auto* Progression=Player->GetProgression(); Progression->BindAttributes(Attr); Progression->ChoosePermanentClassById(Class);
            auto* Definition=DuplicateObject<UBreakerClassDefinition>(Progression->ClassDefinition,Player);
            auto* Tree=NewObject<UBreakerProgressionTree>(Definition); Tree->TreeId=TEXT("Test.Afterimage.Delivery"); Tree->Currency=EBreakerPointCurrency::CorePoints;
            Definition->BranchTrees.Add(Tree); Progression->ClassDefinition=Definition; CoreTree=Tree;
            auto* Node=NewObject<UBreakerProgressionNode>(Tree); Node->NodeId=TEXT("Test.Afterimage.Delivery.Rule"); Node->Currency=Tree->Currency;
            Node->GrantedTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Progression.Node.Core.Afterimage"))); Tree->Nodes.Add(Node);
            Progression->AwardExperience(UBreakerExperienceLibrary::TotalXpToReachLevel(5,Progression->ExperienceCurve));
            FText Reason; Progression->PurchaseNode(Tree,Node->NodeId,Reason);
            Player->GetScrap()->BindAttributes(Attr); Player->GetScrap()->SetComponentTickEnabled(false);
            Player->GetMomentum()->BindAttributes(Attr); Player->GetMomentum()->SetComponentTickEnabled(false);
            UBreakerAbilityStateComponent::FindOrAdd(Player)->SetComponentTickEnabled(false);
            auto* Weapon=Player->GetWeapon(); Weapon->WeaponDefinition=DuplicateObject<UBreakerWeaponDefinition>(Weapon->GetActiveDefinition(),Weapon);
            Weapon->WeaponDefinition->HipSpreadDegrees=Weapon->WeaponDefinition->AimSpreadDegrees=0; Weapon->WeaponDefinition->BleedChance=0;
            Weapon->ResetAmmunition();
            // Bind the same resource notifications as Character BeginPlay,
            // without starting saves/missions in this isolated delivery fixture.
            Weapon->OnReloadCompleted.AddDynamic(Player->GetScrap(), &UBreakerScrapComponent::NotifyReloadCompleted);
            Weapon->OnMagazineEmptied.AddDynamic(Player->GetScrap(), &UBreakerScrapComponent::NotifyMagazineEmptied);
            Target=World->SpawnActor<AActor>(); auto* Body=NewObject<USphereComponent>(Target); Target->AddInstanceComponent(Body); Target->SetRootComponent(Body);
            Body->SetSphereRadius(60); Body->SetCollisionEnabled(ECollisionEnabled::QueryOnly); Body->SetCollisionResponseToAllChannels(ECR_Ignore);
            Body->SetCollisionResponseToChannel(ECC_GameTraceChannel2,ECR_Block); Body->RegisterComponent(); AimTarget();
            auto* Sink=NewObject<UBreakerCombatComponent>(Target); Target->AddInstanceComponent(Sink); Sink->RegisterComponent();
            auto* Health=NewObject<UBreakerAttributeSet>(Target); Health->ApplyMaxHealth(100000); Health->ApplyHealth(100000); Sink->BindAttributes(Health);
        }
        ~FBreakerSidearmAfterimageFixture() { if (World) { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); } GFrameCounter=SavedFrame; }
        void AimTarget() { FVector Eye; FRotator Aim; Player->GetActorEyesViewPoint(Eye,Aim); Target->SetActorLocation(Eye+Aim.Vector()*800); }
        void Tick(float Seconds)
        {
            auto* State=UBreakerAbilityStateComponent::FindOrAdd(Player);
            for (int32 I=0; I<FMath::CeilToInt(Seconds/.01f); ++I)
            { ++GFrameCounter; World->Tick(LEVELTICK_All,.01f); State->TickComponent(.01f,LEVELTICK_All,nullptr); Player->GetScrap()->AdvanceLoop(.01f); }
        }
        float Shot()
        {
            auto* W=Player->GetWeapon();
            const float CriticalMultiplier=Player->GetAttributes()->GetCriticalMultiplier();
            W->StartFire(); W->StopFire();
            const auto& Result=W->GetLastShot().DamageResult;
            // These real hits retain their ordinary critical rolls. Compare
            // the flat lane on the same pre-critical basis for each result;
            // progression recomposition must not change what is measured.
            return Result.HealthDamage / (Result.bCritical ? CriticalMultiplier : 1.0f);
        }
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerSidearmAfterimageTest, "RiorsEdge.Abilities.AfterimageSidearmRig",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerSidearmAfterimageTest::RunTest(const FString&)
{
    for (int32 Scenario = 0; Scenario < 10; ++Scenario)
    {
        FBreakerSidearmAfterimageFixture F(EBreakerClassId::Gunsmith); if (!F.Player) return false;
        auto* Player = F.Player; auto* ASC = Player->GetAbilitySystemComponent(); auto* P = Player->GetProgression();
        auto* Weapon = Player->GetWeapon(); auto* Abilities = Player->GetAbilities(); auto* Scrap = Player->GetScrap();
        FText Reason;
        TestTrue(TEXT("Actual class starter unlocks Sidearm Rig"), P->IsAbilityUnlocked(TEXT("Gunsmith.SidearmRig")));
        const auto Slot = EBreakerAbilitySlot::ClassAbilityOne;
        if (!TestTrue(TEXT("Equip through real unlocked loadout"), P->EquipAbility(Slot, TEXT("Gunsmith.SidearmRig"), Reason))) return false;
        Abilities->RefreshGrants();
        if (Scenario == 7)
        {
            P->AwardExperience(UBreakerExperienceLibrary::TotalXpToReachLevel(40, P->ExperienceCurve));
            FBreakerQuestFlagSet Flags;
            for (const auto& Mission : UBreakerMissionLibrary::GetMissions()) for (const auto& Beat : Mission.Beats)
                for (FName Flag : UBreakerMissionLibrary::BeatCompletionFlags(Beat)) Flags.Add(Flag);
            P->SettleDoctrineEntitlement(Flags);
            if (!P->CommitToBranch(UBreakerProgressionLibrary::GetGunsmithArmoryTree()->TreeId, Reason)) return false;
            for (const TCHAR* Id : {TEXT("Gunsmith.Armory.Chambered"),TEXT("Gunsmith.Armory.Chambered"),
                TEXT("Gunsmith.Armory.ColdBarrel"),TEXT("Gunsmith.Armory.ColdBarrel"),TEXT("Gunsmith.Armory.RigDiscipline")})
                if (!TestTrue(FString::Printf(TEXT("Actual ranked Rig Discipline path %s"),Id), P->PurchaseNode(UBreakerProgressionLibrary::GetGunsmithArmoryTree(),Id,Reason))) return false;
        }
        Weapon->StartFire(); F.Tick(.4f); Weapon->StopFire(); Weapon->StartReload();
        for (int32 Step=0; Step<1000 && Weapon->IsReloading(); ++Step) F.Tick(.01f);
        F.Tick(.5f);
        TestTrue(TEXT("Real firing/reload earns ordinary Scrap"), Scrap->GetScrap() > 0);
        const float Baseline = F.Shot(); F.Tick(.3f);
        // Chambered made the first baseline shot free; spend one real round
        // before activation so Rig Discipline must survive the first reload.
        if (Scenario == 7) { F.Shot(); F.Tick(.3f); }
        const float Multiplier = Player->GetAttributes()->GetDamageMultiplier();
        const float Flat = GetDefault<UBreakerAbility_SidearmRig>()->FlatBonusDamage * Multiplier;
        TestEqual(TEXT("Actual starter quote is zero Scrap"), Abilities->GetCost(Slot), 0.0f);
        const float Before = Scrap->GetScrap();
        if (!TestTrue(TEXT("Native equipped Rig activates"), Abilities->TryActivateSlot(Slot))) return false;
        TestEqual(TEXT("Zero-cost cast does not invent Scrap payment"), Scrap->GetScrap(), Before, .001f);
        TestTrue(TEXT("Native cooldown applied"), Abilities->GetCooldownRemaining(Slot) > 0);
        TestFalse(TEXT("Cooldown/active cast blocks duplicate activation"), Abilities->TryActivateSlot(Slot));
        auto* Spec = ASC->FindAbilitySpecFromClass(UBreakerAbility_SidearmRig::StaticClass()); if (!Spec) return false;
        const auto Handle = Spec->Handle;
        const int32 FullPierce = Weapon->GetShotChannels().PierceCount;
        TestTrue(TEXT("Actual Rig adds authored Pierce"), FullPierce >= 1);
        TestEqual(TEXT("Actual full damage contribution"), F.Shot(), Baseline + Flat, .01f); F.Tick(.3f);
        if (Scenario == 8)
        {
            FBreakerDamageRequest Kill; Kill.BaseDamage=100000; Kill.DamageFamily=EBreakerDamageFamily::TrueDamage; Kill.bCanCritical=false;
            auto* Enemy=F.World->SpawnActor<AActor>(); Kill.SetInstigator(Enemy); Player->GetCombat()->ReceiveDamage(Kill);
            TestTrue(TEXT("Actual death interrupts active Rig"),Player->GetCombat()->IsDead()); Player->GetCombat()->RestoreVitals();
            TestFalse(TEXT("Death closes active GAS Rig"),ASC->FindAbilitySpecFromHandle(Handle)->IsActive());
            TestFalse(TEXT("Death closes ordinary Rig permission"),UBreakerAbilityStateComponent::FindOrAdd(Player)->IsWindowActive(UBreakerAbility_SidearmRig::WindowKey()));
            TestEqual(TEXT("Revival cannot regain active Rig contribution"),F.Shot(),Baseline,.01f); continue;
        }
        if (Scenario == 9)
        {
            ASC->ClearAbility(Handle);
            TestFalse(TEXT("Ability removal closes ordinary Rig permission"),UBreakerAbilityStateComponent::FindOrAdd(Player)->IsWindowActive(UBreakerAbility_SidearmRig::WindowKey()));
            TestEqual(TEXT("Ability removal discards contribution"),F.Shot(),Baseline,.01f); continue;
        }
        if (Scenario == 2)
        {
            ASC->CancelAbilityHandle(Handle);
            TestEqual(TEXT("Explicit cancellation removes full contribution"), F.Shot(), Baseline, .01f);
            continue;
        }
        if (Scenario == 5)
        {
            F.Tick(121);
            TestTrue(TEXT("Nominal HUD timer cannot end actual rig"), ASC->FindAbilitySpecFromHandle(Handle)->IsActive());
            TestEqual(TEXT("Idle rig remains full beyond nominal HUD time"), F.Shot(), Baseline + Flat, .01f); F.Tick(.3f);
        }
        if (Scenario == 6) F.Tick(11);
        if (Scenario == 0 || Scenario == 7)
        {
            for (int32 Round=0; Round<100 && Weapon->GetMagazineAmmo()>1; ++Round) { F.Shot(); F.Tick(.15f); }
            const float FinalRound = F.Shot();
            TestEqual(TEXT("Final accepted magazine round keeps full source snapshot"), FinalRound, Baseline + Flat, .01f);
            if (Scenario == 7)
            {
                TestTrue(TEXT("Rig Discipline survives first reload with shots remaining"), ASC->FindAbilitySpecFromHandle(Handle)->IsActive());
                for(int32 Step=0;Step<1000 && Weapon->IsReloading();++Step) F.Tick(.01f);
                for(int32 Shot=0;Shot<100 && ASC->FindAbilitySpecFromHandle(Handle)->IsActive();++Shot)
                { TestEqual(TEXT("Budget-ending shot remains full"),F.Shot(),Baseline+Flat,.01f); F.Tick(.15f); }
            }
        }
        else Weapon->StartReload();
        if (!TestFalse(TEXT("Actual event closes ordinary Rig"), ASC->FindAbilitySpecFromHandle(Handle)->IsActive())) return false;
        TestFalse(TEXT("Ordinary window permission ended"), UBreakerAbilityStateComponent::FindOrAdd(Player)->IsWindowActive(UBreakerAbility_SidearmRig::WindowKey()));
        TestEqual(TEXT("Half Pierce uses existing final count floor"), Weapon->GetShotChannels().PierceCount, FullPierce - 1);
        const float TailStart = F.World->GetTimeSeconds();
        if (Scenario == 6)
        {
            if (!TestTrue(TEXT("Ready cooldown permits real recast over tail"), Abilities->TryActivateSlot(Slot))) return false;
            TestEqual(TEXT("Successful recast replaces half with full Pierce"),Weapon->GetShotChannels().PierceCount,FullPierce);
            TestFalse(TEXT("Refused duplicate does not erase replacement"),Abilities->TryActivateSlot(Slot));
            ASC->CancelAbilityHandle(Handle); continue;
        }
        for(int32 Step=0;Step<1000 && Weapon->IsReloading();++Step) F.Tick(.01f);
        if (Scenario == 3)
        {
            if (!P->RespecCore(Reason)) return false;
            TestEqual(TEXT("Respec removes tail with actual floor recomposition"),F.Shot(),Baseline*Player->GetAttributes()->GetDamageMultiplier()/Multiplier,.01f);
            if (!P->PurchaseNode(F.CoreTree,TEXT("Test.Afterimage.Delivery.Rule"),Reason)) return false;
            F.Tick(.2f); TestEqual(TEXT("Rebuy cannot revive ended tail"),F.Shot(),Baseline,.01f);
        }
        else if (Scenario == 4)
        {
            FBreakerDamageRequest Kill; Kill.BaseDamage=100000; Kill.DamageFamily=EBreakerDamageFamily::TrueDamage; Kill.bCanCritical=false;
            auto* Enemy=F.World->SpawnActor<AActor>(); Kill.SetInstigator(Enemy); Player->GetCombat()->ReceiveDamage(Kill);
            TestTrue(TEXT("Hostile hit kills"),Player->GetCombat()->IsDead()); Player->GetCombat()->RestoreVitals();
            TestEqual(TEXT("Revival cannot restore dead owner's tail"),F.Shot(),Baseline,.01f);
        }
        else
        {
            if (!TestTrue(TEXT("Native reload completes within tail for actual shot"),F.World->GetTimeSeconds()-TailStart<2)) return false;
            TestEqual(TEXT("Actual rifle receives half flat contribution"),F.Shot(),Baseline+Flat*.5f,.01f);
        }
        // Idempotent finish must retain its first event's deadline.
        Player->GetCombat()->FinishWindowOutgoingModifier(UBreakerAbility_SidearmRig::OutgoingModifierKey());
        Weapon->FinishWindowShotChannelBonus(UBreakerAbility_SidearmRig::OutgoingModifierKey());
        F.Tick(FMath::Max(.2f,2.05f-(F.World->GetTimeSeconds()-TailStart)));
        TestEqual(TEXT("Original event plus two seconds expires tail"),F.Shot(),Baseline,.01f);
    }
    return true;
}
#endif
