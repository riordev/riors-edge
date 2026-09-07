#include "Characters/BreakerFirstPersonArms.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Weapons/BreakerWeaponComponent.h"

UBreakerFirstPersonArms::UBreakerFirstPersonArms()
{
    SetOnlyOwnerSee(true);
    SetCastShadow(false);
    SetCollisionProfileName(TEXT("NoCollision"));
    SetGenerateOverlapEvents(false);
    SetCanEverAffectNavigation(false);
    VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
    SetVisibility(false);
}

bool UBreakerFirstPersonArms::Configure(UStaticMeshComponent* Gun,
    const FVector& FiringGrip, const FVector& SupportGrip)
{
    if (!Gun || !Gun->GetStaticMesh()) return false;
    USkeletalMesh* Arms = LoadObject<USkeletalMesh>(nullptr,
        TEXT("/Game/Breaker/Characters/SKM_FirstPersonArms.SKM_FirstPersonArms"));
    RifleIdle = LoadObject<UAnimSequence>(nullptr,
        TEXT("/Game/Characters/Mannequins/Anims/Rifle/MF_Rifle_Idle_ADS.MF_Rifle_Idle_ADS"));
    RifleReload = LoadObject<UAnimSequence>(nullptr,
        TEXT("/Game/Characters/Mannequins/Anims/Rifle/MM_Rifle_Reload.MM_Rifle_Reload"));
    if (!Arms || !RifleIdle || !RifleReload || RifleIdle->IsValidAdditive()
        || RifleReload->IsValidAdditive()) return false;
    SetSkeletalMesh(Arms);
    if (GetBoneIndex(TEXT("hand_r")) == INDEX_NONE || GetBoneIndex(TEXT("hand_l")) == INDEX_NONE)
        return false;
    SetRelativeTransform(FTransform::Identity);
    PlayAnimation(RifleIdle, true);
    SetPosition(0.0f, false);
    TickAnimation(0.0f, false);
    RefreshBoneTransforms();
    const FName RightGrip = DoesSocketExist(TEXT("HandGrip_R")) ? FName(TEXT("HandGrip_R")) : FName(TEXT("hand_r"));
    const FName LeftGrip = DoesSocketExist(TEXT("HandGrip_L")) ? FName(TEXT("HandGrip_L")) : FName(TEXT("hand_l"));
    const FVector Right = GetSocketTransform(RightGrip, RTS_Component).GetLocation();
    const FVector Left = GetSocketTransform(LeftGrip, RTS_Component).GetLocation();
    const FVector SourceSpan = Left - Right;
    const FVector TargetSpan = SupportGrip - FiringGrip;
    if (SourceSpan.Size() < 1.0f || TargetSpan.Size() < 1.0f) return false;
    const FQuat Facing = FRotator(0.0f, -90.0f, 0.0f).Quaternion();
    // O2 first-person shoulder cant around the fixed grip line. Both contact
    // points remain fixed while the support elbow drops below the camera.
    const FQuat ShoulderCant(TargetSpan.GetSafeNormal(), FMath::DegreesToRadians(20.0f));
    const FQuat Alignment = ShoulderCant * FQuat::FindBetweenNormals(
        Facing.RotateVector(SourceSpan).GetSafeNormal(), TargetSpan.GetSafeNormal()) * Facing;
    const float Scale = TargetSpan.Size() / SourceSpan.Size();
    SetRelativeTransform(FTransform(Alignment, FiringGrip - Alignment.RotateVector(Right) * Scale, FVector(Scale)));
    // Preserve the inspected gun fit, then let the actual firing wrist carry it
    // through reload. No frame-by-frame re-fitting that would erase animation.
    HeldGun = Gun;
    Gun->AttachToComponent(this, FAttachmentTransformRules::KeepWorldTransform, TEXT("hand_r"));
    bConfigured = true;
    bShowingReload = false;
    SetVisibility(true);
    UE_LOG(LogTemp, Display, TEXT("[BreakerArms] cropped rifle arms ready; scale %.3f right %s left %s"),
        Scale, *Right.ToString(), *Left.ToString());
    return true;
}

void UBreakerFirstPersonArms::DeactivateArms(USceneComponent* Rig)
{
    if (HeldGun && Rig)
        HeldGun->AttachToComponent(Rig, FAttachmentTransformRules::KeepWorldTransform);
    HeldGun = nullptr;
    bConfigured = false;
    bShowingReload = false;
    SetVisibility(false);
}

void UBreakerFirstPersonArms::UpdateWeaponPose(const UBreakerWeaponComponent* Weapon)
{
    if (!bConfigured || !Weapon) return;
    const float ReloadFraction = Weapon->GetReloadFraction();
    // Remote clients currently replicate the state without its timer. Stay in
    // the stable rifle pose when the authoritative progress clock is absent.
    const bool bReloading = Weapon->IsReloading() && FMath::IsFinite(ReloadFraction) && ReloadFraction >= 0.0f;
    if (bReloading != bShowingReload)
    {
        bShowingReload = bReloading;
        PlayAnimation(bReloading ? RifleReload.Get() : RifleIdle.Get(), !bReloading);
    }
    if (bReloading)
    {
        // The ammunition system owns timing, including reload-speed affixes and
        // cancellation. A refused request never changes this pose.
        SetPosition(FMath::Clamp(ReloadFraction, 0.0f, 1.0f) * RifleReload->GetPlayLength(), false);
        if (UAnimSingleNodeInstance* Instance = GetSingleNodeInstance()) Instance->SetPlaying(false);
        TickAnimation(0.0f, false);
        RefreshBoneTransforms();
    }
    // MM_Rifle_Fire is mesh-space additive. Playing it as an absolute single-node
    // clip would expose reference-pose arms in packaged builds. The shared recoil
    // spring supplies the shot motion until a native additive layer is authored.
}
