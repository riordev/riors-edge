#pragma once

#include "CoreMinimal.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Components/MeshComponent.h"
#include "Engine/Texture.h"

// ---------------------------------------------------------------------------
// The asset-free "this thing glows" material.
//
// The gym's dressing, the loot pickups and the rocket casing all use
// /Engine/BasicShapes/BasicShapeMaterial, which is LIT: a primitive painted
// with it is a coloured object that the sun shades. That is right for a crate
// and wrong for a round in flight or a muzzle spark, which must be brighter
// than their surroundings and must not go grey in shadow.
//
// /Engine/EngineMaterials/EmissiveMeshMaterial is the engine's stock unlit
// ADDITIVE surface. Two properties earn it its place here:
//   * unlit + additive means the primitive reads as light rather than as
//     paint, which is the whole difference between a tracer and a stick;
//   * additive translucency still depth-TESTS against the opaque scene, so a
//     round behind a wall is correctly hidden. That is the entire reason this
//     pass moved the tracer out of the HUD canvas.
//
// Its parameters were MEASURED, not guessed (temporary automation probe
// against UMaterialInterface::GetAll*ParameterInfo): one vector parameter
// named "Color" and one texture parameter named "LinearColor", which defaults
// to DefaultWhiteGrid_Low — a GRID. Left alone, every tracer in the game would
// have graph paper printed on it. WhiteSquareTexture is the override that
// makes the surface a flat multiple of Color.
//
// Colours still come from BreakerUI tokens; the intensity multiplier is what
// pushes them past 1.0 into bloom. Nothing here is teal.
// ---------------------------------------------------------------------------
namespace BreakerUI
{
    inline UMaterialInterface* LoadGlowBaseMaterial()
    {
        return LoadObject<UMaterialInterface>(
            nullptr, TEXT("/Engine/EngineMaterials/EmissiveMeshMaterial.EmissiveMeshMaterial"));
    }

    inline UTexture* LoadGlowWhiteTexture()
    {
        return LoadObject<UTexture>(
            nullptr, TEXT("/Engine/EngineResources/WhiteSquareTexture.WhiteSquareTexture"));
    }

    // Creates the dynamic instance on a mesh component and neutralises the
    // grid texture. Returns null when the engine content is missing, and every
    // caller must cope with that rather than assume.
    inline UMaterialInstanceDynamic* MakeGlowMaterial(UMeshComponent* Mesh)
    {
        if (!Mesh) return nullptr;
        UMaterialInterface* Base = LoadGlowBaseMaterial();
        if (!Base) return nullptr;
        UMaterialInstanceDynamic* Dynamic = UMaterialInstanceDynamic::Create(Base, Mesh);
        if (!Dynamic) return nullptr;
        if (UTexture* White = LoadGlowWhiteTexture())
        {
            Dynamic->SetTextureParameterValue(TEXT("LinearColor"), White);
        }
        Mesh->SetMaterial(0, Dynamic);
        return Dynamic;
    }

    // Additive materials have no alpha channel to fade: brightness IS the
    // colour magnitude, so a fade to nothing is a multiply toward black.
    inline void SetGlowColor(UMaterialInstanceDynamic* Dynamic, const FLinearColor& Color, float Intensity)
    {
        if (!Dynamic) return;
        Dynamic->SetVectorParameterValue(TEXT("Color"),
            FLinearColor(Color.R * Intensity, Color.G * Intensity, Color.B * Intensity, 1.0f));
    }
}
