#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Characters/BreakerFirstPersonArms.h"
#include "Combat/BreakerCombatComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Weapons/BreakerWeaponComponent.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerFirstPersonArmsRuntimeTest, "RiorsEdge.Presentation.FirstPersonArmsRuntime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerFirstPersonArmsRuntimeTest::RunTest(const FString& Parameters)
{
    USkeletalMesh* Crop = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Breaker/Characters/SKM_FirstPersonArms.SKM_FirstPersonArms"));
    USkeletalMesh* Manny = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"));
    if (!TestNotNull(TEXT("shipped cropped arms asset"), Crop) || !TestNotNull(TEXT("source mannequin"), Manny)) return false;
    TestEqual(TEXT("crop has one authored LOD"), Crop->GetLODNum(), 1);
    TestTrue(TEXT("crop retains source skeleton"), Crop->GetSkeleton() == Manny->GetSkeleton());
    UWorld::InitializationValues Init;
    Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!TestNotNull(TEXT("isolated registered world"), World)) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL());
    const uint64 InitialFrame = GFrameCounter;
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); GFrameCounter = InitialFrame; };
    ABreakerCharacter* Player = World->SpawnActor<ABreakerCharacter>();
    if (!TestNotNull(TEXT("real character"), Player)) return false;
    // Deliberately omit Character BeginPlay: this fixture never loads an owner's save.
    Player->GetBreakerMovement()->SetComponentTickEnabled(false);
    Player->GetAbilitySystemComponent()->InitAbilityActorInfo(Player, Player);
    Player->GetAbilitySystemComponent()->AddAttributeSetSubobject(Player->GetAttributes());
    Player->GetCombat()->BindAttributes(Player->GetAttributes());
    USceneComponent* Rig = NewObject<USceneComponent>(Player);
    Player->AddInstanceComponent(Rig); Rig->SetupAttachment(Player->GetRootComponent()); Rig->RegisterComponent();
    UStaticMeshComponent* Gun = NewObject<UStaticMeshComponent>(Player);
    Player->AddInstanceComponent(Gun); Gun->SetupAttachment(Rig);
    Gun->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
    Gun->SetCollisionProfileName(TEXT("NoCollision")); Gun->RegisterComponent();
    Gun->SetRelativeLocation(FVector(30, 8, -12));
    UBreakerFirstPersonArms* Arms = NewObject<UBreakerFirstPersonArms>(Player);
    Player->AddInstanceComponent(Arms); Arms->SetupAttachment(Rig); Arms->RegisterComponent();
    const FTransform OriginalGun = Gun->GetComponentTransform();
    const FVector Right(12, 8, -12), Left(35, -6, -12);
    if (!TestTrue(TEXT("real crop configures registered gun"), Arms->Configure(Gun, Right, Left))) return false;
    TestTrue(TEXT("arms owner-only"), Arms->bOnlyOwnerSee);
    TestFalse(TEXT("arms cast no shadow"), Arms->CastShadow);
    TestEqual(TEXT("arms have no query or physics collision"), Arms->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
    TestTrue(TEXT("gun attaches to firing wrist"), Gun->GetAttachParent() == Arms && Gun->GetAttachSocketName() == FName(TEXT("hand_r")));
    TestTrue(TEXT("attachment preserves inspected gun fit"), Gun->GetComponentTransform().Equals(OriginalGun, 0.01f));
    // Shipped configuration: the idle HOLDS. Configure samples the clip at
    // t=0 and pauses it, so the hands keep the pose the gun was fitted to; a
    // playing idle sways the wrist and the socketed gun with it. Two seconds
    // of animation tick must move the firing wrist by nothing.
    {
        const FVector WristAtZero = Arms->GetSocketTransform(TEXT("hand_r"), RTS_Component).GetLocation();
        Arms->TickAnimation(2.0f, false);
        Arms->RefreshBoneTransforms();
        const FVector WristAfter = Arms->GetSocketTransform(TEXT("hand_r"), RTS_Component).GetLocation();
        const float DriftCm = static_cast<float>((WristAfter - WristAtZero).Size());
        UE_LOG(LogTemp, Display, TEXT("[BreakerArms] idle hold: hand_r drift over 2.0 s = %.4f cm (t=0 %s, after %s)"),
            DriftCm, *WristAtZero.ToString(), *WristAfter.ToString());
        TestTrue(FString::Printf(TEXT("idle holds the fitted wrist (drift %.4f cm)"), DriftCm), DriftCm <= 0.01f);
        UAnimSingleNodeInstance* Held = Arms->GetSingleNodeInstance();
        if (TestNotNull(TEXT("configured arms carry a single-node instance"), Held))
        {
            TestFalse(TEXT("configured idle is paused, not playing"), Held->IsPlaying());
        }
    }
    Arms->DeactivateArms(Rig);
    TestFalse(TEXT("deactivation clears configuration"), Arms->IsConfigured());
    TestFalse(TEXT("deactivation hides arms"), Arms->IsVisible());
    TestTrue(TEXT("deactivation restores rig and fit"), Gun->GetAttachParent() == Rig && Gun->GetComponentTransform().Equals(OriginalGun, 0.01f));
    if (!TestTrue(TEXT("reconfiguration succeeds"), Arms->Configure(Gun, Right, Left))) return false;
    TestTrue(TEXT("reconfiguration does not accumulate fitting transforms"), Gun->GetComponentTransform().Equals(OriginalGun, 0.01f));
    UAnimSingleNodeInstance* Pose = Arms->GetSingleNodeInstance();
    if (!TestNotNull(TEXT("real single-node animation instance"), Pose)) return false;
    UAnimationAsset* Idle = Pose->GetCurrentAsset();
    UBreakerWeaponComponent* Weapon = Player->GetWeapon();
    Weapon->ResetAmmunition();
    const int32 FullMagazine = Weapon->GetMagazineAmmo();
    TestTrue(TEXT("normal ammunition initialized"), FullMagazine > 0 && Weapon->GetReserveAmmo() > 0);
    Weapon->StartReload(); Arms->UpdateWeaponPose(Weapon);
    TestFalse(TEXT("full magazine refuses actual reload"), Weapon->IsReloading());
    TestTrue(TEXT("refusal leaves idle animation"), Arms->GetSingleNodeInstance()->GetCurrentAsset() == Idle);
    Weapon->StartFire(); Weapon->StopFire();
    TestEqual(TEXT("actual firing spends one round"), Weapon->GetMagazineAmmo(), FullMagazine - 1);
    Weapon->StartReload();
    if (!TestTrue(TEXT("spent round allows actual reload"), Weapon->IsReloading())) return false;
    for (int32 Step = 0; Step < 4; ++Step) { ++GFrameCounter; World->Tick(LEVELTICK_All, 0.05f); }
    Arms->UpdateWeaponPose(Weapon);
    UAnimSequence* Reload = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/Characters/Mannequins/Anims/Rifle/MM_Rifle_Reload.MM_Rifle_Reload"));
    if (!TestNotNull(TEXT("authored reload animation"), Reload)) return false;
    TestTrue(TEXT("accepted reload selects authored clip"), Arms->GetSingleNodeInstance()->GetCurrentAsset() == Reload);
    TestTrue(TEXT("real authoritative reload clock advanced"), Weapon->GetReloadFraction() > 0);
    TestEqual(TEXT("pose samples ammunition clock"), Arms->GetSingleNodeInstance()->GetCurrentTime(), Weapon->GetReloadFraction() * Reload->GetPlayLength(), 0.001f);
    Weapon->EquipSlot(2); Arms->UpdateWeaponPose(Weapon);
    TestFalse(TEXT("actual weapon swap cancels reload"), Weapon->IsReloading());
    TestTrue(TEXT("cancelled reload restores idle"), Arms->GetSingleNodeInstance()->GetCurrentAsset() == Idle);
    Arms->DeactivateArms(Rig);
    TestTrue(TEXT("animated gun cleanly returns to rig"), Gun->GetAttachParent() == Rig);
    return true;
}
#endif
