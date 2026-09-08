#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Abilities/BreakerGunsmithAbilities.h"
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
    struct FBreakerMachinistAfterimageFixture
    {
        UWorld* World = nullptr;
        ABreakerCharacter* Player = nullptr;
        AActor* Target = nullptr;
        UBreakerProgressionTree* CoreTree = nullptr;
        uint64 SavedFrame = GFrameCounter;
        FBreakerMachinistAfterimageFixture(EBreakerClassId Class)
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
            Progression->AwardExperience(UBreakerExperienceLibrary::TotalXpToReachLevel(40,Progression->ExperienceCurve));
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
        ~FBreakerMachinistAfterimageFixture() { if (World) { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); } GFrameCounter=SavedFrame; }
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerMachinistAfterimageTest, "RiorsEdge.Abilities.AfterimageMachinistDamage",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerMachinistAfterimageTest::RunTest(const FString&)
{
    for (int32 Scenario = 0; Scenario < 4; ++Scenario)
    {
        FBreakerMachinistAfterimageFixture F(EBreakerClassId::Gunsmith); if (!F.Player) return false;
        auto* Player = F.Player; auto* Progression = Player->GetProgression(); auto* Weapon = Player->GetWeapon();
        auto* ASC = Player->GetAbilitySystemComponent(); auto* Scrap = Player->GetScrap();
        if (!TestTrue(TEXT("Afterimage bought with earned Core point"), Progression->HasNodeTag(FGameplayTag::RequestGameplayTag(TEXT("Progression.Node.Core.Afterimage"))))) return false;
        FBreakerQuestFlagSet Flags;
        // Restored campaign completion at level 40 supplies the normal eight
        // Doctrine points; this fixture does not claim to replay the campaign.
        for (const auto& Mission : UBreakerMissionLibrary::GetMissions()) for (const auto& Beat : Mission.Beats)
            for (FName Flag : UBreakerMissionLibrary::BeatCompletionFlags(Beat)) Flags.Add(Flag);
        Progression->SettleDoctrineEntitlement(Flags);
        FText Reason;
        if (!TestTrue(TEXT("Commit through actual Armory branch selection"),
            Progression->CommitToBranch(UBreakerProgressionLibrary::GetGunsmithArmoryTree()->TreeId, Reason))) return false;
        for (const TCHAR* Id : { TEXT("Gunsmith.Armory.Chambered"), TEXT("Gunsmith.Armory.Chambered"),
            TEXT("Gunsmith.Armory.ColdBarrel"), TEXT("Gunsmith.Armory.ColdBarrel"),
            TEXT("Gunsmith.Armory.RigDiscipline"), TEXT("Gunsmith.Armory.Machinist") })
        {
            const bool bPurchased = Progression->PurchaseNode(UBreakerProgressionLibrary::GetGunsmithArmoryTree(), Id, Reason);
            if (!TestTrue(FString::Printf(TEXT("Paid %s: %s"), Id, *Reason.ToString()), bPurchased)) return false;
        }
        if (Scenario == 2)
        {
            const auto Cost = BreakerCoreRespecCost(Progression->GetCharacterLevel());
            auto* Equipment = Player->GetEquipment();
            for (int32 Seed = 1; Seed <= 100 && !Equipment->GetForgeWallet().CanAfford(Cost); ++Seed)
            {
                const auto Item = UBreakerLootLibrary::RollItem(TEXT("Test.Machinist.Salvage"), EBreakerEquipSlot::Boots, EBreakerItemRarity::Standard, 40, Seed);
                if (!Equipment->AddToBackpack(Item) || !Equipment->SalvageFromBackpack(Item.ItemId)) return false;
            }
            if (!TestTrue(TEXT("Ordinary salvage funds real Core respec"), Equipment->GetForgeWallet().CanAfford(Cost))) return false;
        }
        // Actual shots and completed reloads earn the ultimate. Chambered's
        // first free shot is respected: hold long enough to spend real rounds.
        for (int32 Reload = 0; Reload < 30 && Scrap->GetScrap() < 100; ++Reload)
        {
            Weapon->StartFire(); F.Tick(.4f); Weapon->StopFire(); Weapon->StartReload();
            for (int32 Step = 0; Step < 1000 && Weapon->IsReloading(); ++Step) F.Tick(.01f);
            F.Tick(.5f);
        }
        if (!TestEqual(TEXT("Normal rifle/reload loop earns full Scrap"), Scrap->GetScrap(), 100.0f, .001f)) return false;
        F.Tick(.5f); F.AimTarget(); const float Baseline = F.Shot(); F.Tick(.5f);
        const float SourceMultiplier = Player->GetAttributes()->GetDamageMultiplier();
        // Added damage enters before the ordinary source pool, including the
        // earned-point floor. The tail halves that contribution, not the pool.
        const float Flat = Weapon->GetScaledBaseDamage() * GetDefault<UBreakerAbility_FieldAssembly>()->MachinistFlatDamageFraction * SourceMultiplier;
        const auto Handle = ASC->GiveAbility(FGameplayAbilitySpec(UBreakerAbility_FieldAssembly::StaticClass(), 1));
        const float Before = Scrap->GetScrap();
        if (!TestTrue(TEXT("Actual ultimate activation pays"), ASC->TryActivateAbility(Handle))) return false;
        TestEqual(TEXT("Actual ultimate spends full bar once"), Before - Scrap->GetScrap(), 100.0f, .001f);
        auto* State = UBreakerAbilityStateComponent::FindOrAdd(Player);
        const float Duration = State->GetWindowRemaining(UBreakerAbility_FieldAssembly::WindowKey());
        TestTrue(TEXT("Machinist ordinary window exists"), Duration > 0);
        TestEqual(TEXT("Actual full rider on rifle"), F.Shot(), Baseline + Flat, .01f);
        if (Scenario == 1)
        {
            ASC->CancelAbilityHandle(Handle); F.Tick(.5f);
            TestEqual(TEXT("Explicit cancellation removes rider immediately"), F.Shot(), Baseline, .01f);
            continue;
        }
        F.Tick(Duration + .03f);
        TestFalse(TEXT("Normal Assembly permission ends before tail"), State->IsWindowActive(UBreakerAbility_FieldAssembly::WindowKey()));
        TestFalse(TEXT("Ordinary ultimate GAS ends before tail"), ASC->FindAbilitySpecFromHandle(Handle)->IsActive());
        int32 LiveAuras = 0; for (TActorIterator<ABreakerZoneActor> It(F.World); It; ++It)
            if (It->GetOwner() == Player && !It->IsActorBeingDestroyed()) ++LiveAuras;
        TestEqual(TEXT("Aura retains ordinary expiry"), LiveAuras, 0);
        TestEqual(TEXT("Actual half flat rider during tail"), F.Shot(), Baseline + Flat * .5f, .01f);
        if (Scenario == 2)
        {
            if (!Progression->RespecCore(Reason)) return false;
            F.Tick(.5f); TestEqual(TEXT("Core respec removes existing tail and its spent-point floor"),
                F.Shot(), Baseline * Player->GetAttributes()->GetDamageMultiplier() / SourceMultiplier, .01f);
            if (!Progression->PurchaseNode(F.CoreTree, TEXT("Test.Afterimage.Delivery.Rule"), Reason)) return false;
            F.Tick(.5f); TestEqual(TEXT("Rebuy cannot resurrect revoked tail"), F.Shot(), Baseline, .01f);
        }
        else if (Scenario == 3)
        {
            auto* Enemy = F.World->SpawnActor<AActor>(); FBreakerDamageRequest Kill; Kill.BaseDamage = 100000;
            Kill.DamageFamily = EBreakerDamageFamily::TrueDamage; Kill.bCanCritical = false; Kill.SetInstigator(Enemy);
            Player->GetCombat()->ReceiveDamage(Kill); TestTrue(TEXT("Hostile damage really kills"), Player->GetCombat()->IsDead());
            Player->GetCombat()->RestoreVitals(); F.Tick(.5f);
            TestEqual(TEXT("Revival cannot retain dead owner's tail"), F.Shot(), Baseline, .01f);
        }
        F.Tick(2.05f); TestEqual(TEXT("Tail expires normally"), F.Shot(), Baseline, .01f);
    }
    return true;
}
#endif
