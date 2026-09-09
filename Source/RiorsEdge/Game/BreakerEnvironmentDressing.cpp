#include "Game/BreakerEnvironmentDressing.h"

#include "Materials/MaterialInterface.h"
#include "NaniteSceneProxy.h"

void BreakerConfigureDressingNanite(UStaticMeshComponent* Component, UStaticMesh* Mesh)
{
    if (!Component || !Mesh || !Mesh->IsNaniteEnabled()) return;
    for (const FStaticMaterial& Slot : Mesh->GetStaticMaterials())
    {
        // Use the engine's blend-mode support, including its translucency
        // configuration. Unsupported Nanite materials otherwise substitute
        // the default surface and lose the imported foliage appearance.
        const UMaterialInterface* Material = Slot.MaterialInterface;
        if (Material && Material->GetNaniteOverride()) Material = Material->GetNaniteOverride();
        if (Material && !Nanite::IsSupportedBlendMode(*Material))
        {
            Component->SetForceDisableNanite(true);
            return;
        }
    }
}
