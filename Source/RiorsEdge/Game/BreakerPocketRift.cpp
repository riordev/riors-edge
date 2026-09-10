#include "Game/BreakerPocketRift.h"

#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Game/BreakerPocketRiftMath.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UI/BreakerGlowMaterial.h"
#include "UI/BreakerUIStyle.h"

ABreakerPocketRift::ABreakerPocketRift()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickGroup = TG_PostUpdateWork;
    // Cosmetic, and the whole file says so: nothing here is replicated and
    // nothing downstream asks it a question.
    bReplicates = false;
    SetCanBeDamaged(false);

    Pivot = CreateDefaultSubobject<USceneComponent>(TEXT("Pivot"));
    SetRootComponent(Pivot);

    // CONSTRUCTOR IS THE ONLY PLACE SetupAttachment WORKS. It is a no-op at
    // runtime and fails silently — the Volatile blast ring shipped invisible
    // once for exactly that.
    TearRoot = CreateDefaultSubobject<USceneComponent>(TEXT("TearRoot"));
    TearRoot->SetupAttachment(Pivot);
    // The actor is dropped ON the floor, so the tear's own centre lifts to half
    // its height: the bottom point touches the ground it is torn in.
    TearRoot->SetRelativeLocation(FVector(0.0f, 0.0f, TearHeightCm * 0.5f));

    Bloom = CreateDefaultSubobject<UPointLightComponent>(TEXT("Bloom"));
    Bloom->SetupAttachment(Pivot);
    Bloom->SetRelativeLocation(FVector(0.0f, 0.0f, TearHeightCm * 0.5f));
    Bloom->SetLightColor(BreakerUI::TealUnwritten);
    Bloom->SetAttenuationRadius(LightAttenuationCm);
    Bloom->SetIntensity(IdleLightIntensity);
    // Five of these stand in the yard permanently. A shadow-casting dynamic
    // light each is the expensive primitive, and the tear is meant to be
    // noticed rather than to relight the level.
    Bloom->SetCastShadows(false);
}

void ABreakerPocketRift::BeginPlay()
{
    Super::BeginPlay();
    if (!TearRoot) return;

    UStaticMesh* Segment = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (!Segment)
    {
        UE_LOG(LogTemp, Error, TEXT("[PocketRift] no cube mesh; the tear will not draw."));
        return;
    }

    // THE OUTLINE, laid once. The shape never changes; the SCALE and the glow
    // are what move, so rebuilding these per frame would be work for an
    // identical answer.
    auto AddSegment = [&](const FVector& From, const FVector& To)
    {
        const FVector Delta = To - From;
        const float Length = static_cast<float>(Delta.Size());
        if (Length <= KINDA_SMALL_NUMBER) return;
        UStaticMeshComponent* Mesh = NewObject<UStaticMeshComponent>(this);
        if (!Mesh) return;
        Mesh->SetStaticMesh(Segment);
        Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Mesh->SetCastShadow(false);
        // REGISTER THEN ATTACH, in that order — SetupAttachment does nothing at
        // runtime and says nothing about it.
        Mesh->RegisterComponent();
        Mesh->AttachToComponent(TearRoot, FAttachmentTransformRules::KeepRelativeTransform);
        Mesh->SetRelativeRotation(FRotationMatrix::MakeFromX(Delta).Rotator());
        Mesh->SetRelativeLocation(From + Delta * 0.5f);
        // The engine cube is 100 cm on a side, so every extent is cm/100. The
        // dash fraction leaves a gap between segments: a broken edge reads as
        // something torn, a solid one reads as a moulding.
        Mesh->SetRelativeScale3D(FVector(Length * SegmentDashFraction / 100.0f,
            SegmentThicknessCm / 100.0f, SegmentThicknessCm / 100.0f));
        if (!TearMaterial)
        {
            // The MID needs a mesh component as its outer, and the first one
            // built is as good an owner as any. Made once, shared by the rest.
            TearMaterial = BreakerUI::MakeGlowMaterial(Mesh);
        }
        if (TearMaterial) Mesh->SetMaterial(0, TearMaterial);
        Segments.Add(Mesh);
    };

    for (int32 Step = 0; Step < TearSteps; ++Step)
    {
        for (const bool bRight : { false, true })
        {
            AddSegment(
                BreakerPocketRift::RaggedEdgePoint(Step, TearSteps, TearWidthCm, TearHeightCm, bRight, EdgeWander),
                BreakerPocketRift::RaggedEdgePoint(Step + 1, TearSteps, TearWidthCm, TearHeightCm, bRight, EdgeWander));
        }
    }
    // Three cross-fractures, so the inside is not merely an empty outline. They
    // stop short of the edges on purpose: a tear that is fully barred reads as
    // a grate, and a body has to be able to walk out through it.
    //
    // SLANTED AND LOPSIDED, from the same hash. Three level bars across a
    // symmetric outline is an ICON — the first capture drew exactly that and
    // read as a logo painted on the air rather than as a split in the world.
    constexpr float Heights[] = { 0.28f, 0.47f, 0.71f };
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(Heights); ++Index)
    {
        const float Height = Heights[Index];
        const float Reach = BreakerPocketRift::TearHalfWidth(Height, TearWidthCm) * 0.62f;
        const float Z = (Height - 0.5f) * TearHeightCm;
        const float Left = -Reach * (0.30f + 0.70f * BreakerPocketRift::EdgeNoise(Index * 31 + 5));
        const float Right = Reach * (0.30f + 0.70f * BreakerPocketRift::EdgeNoise(Index * 31 + 17));
        const float Tilt = (BreakerPocketRift::EdgeNoise(Index * 31 + 23) * 2.0f - 1.0f) * 22.0f;
        AddSegment(FVector(0.0f, Left, Z - Tilt), FVector(0.0f, Right, Z + Tilt));
    }

    if (!TearMaterial)
    {
        // Content, and content can be absent. Say so rather than drawing a
        // black shape and letting the capture argue about the colour.
        UE_LOG(LogTemp, Error, TEXT("[PocketRift] no glow material; the tear will draw as a bare mesh."));
    }
    else
    {
        BreakerUI::SetGlowColor(TearMaterial, BreakerUI::TealUnwritten, IdleGlow);
    }
}

void ABreakerPocketRift::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    Age += DeltaSeconds;
    FlareRemaining = FMath::Max(0.0f, FlareRemaining - DeltaSeconds);

    const float Boost = BreakerPocketRift::FlareBoost(FlareRemaining, FlareSeconds);
    // UNIFORM. A non-uniform scale on a parent whose children are rotated
    // shears them, and every segment on this shape is rotated.
    const float Scale = BreakerPocketRift::IdleScale(Age, IdleHz, IdleAmplitude) + FlareWiden * Boost;
    if (TearRoot) TearRoot->SetRelativeScale3D(FVector(Scale));
    if (TearMaterial)
    {
        BreakerUI::SetGlowColor(TearMaterial, BreakerUI::TealUnwritten, IdleGlow + FlareGlow * Boost);
    }
    if (Bloom) Bloom->SetIntensity(IdleLightIntensity + FlareLightIntensity * Boost);
}

void ABreakerPocketRift::Flare()
{
    FlareRemaining = FlareSeconds;
}

int32 ABreakerPocketRift::SegmentCount() const
{
    return Segments.Num();
}
