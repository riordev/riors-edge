#include "Misc/AutomationTest.h"
#include "UI/BreakerCoreLayoutPreview.h"
#include "UI/BreakerCoreBoardLayout.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerCorePreviewLayoutTest,"RiorsEdge.UI.CorePreviewLayout",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerCorePreviewLayoutTest::RunTest(const FString& Parameters)
{
    auto* Tree=BreakerCoreLayoutPreview::Create();
    const auto Overview=BreakerCoreBoard::Build(Tree);
    TestEqual(TEXT("Native preview contains all nodes"),Overview.Centers.Num(),187);
    TestEqual(TEXT("Preview has distinct identity"),Tree->TreeId,FName(TEXT("Preview.Core.Structure")));
    for(const UBreakerProgressionNode* Node:Tree->Nodes)
    {
        TestTrue(TEXT("Preview is inert"),Node->Effects.IsEmpty()&&Node->GrantedTags.IsEmpty()&&Node->GrantedAbilityIds.IsEmpty());
        TestFalse(TEXT("Every preview node has readable name"),Node->DisplayName.IsEmpty());
    }
    for(FName Name:{FName(TEXT("Precision")),FName(TEXT("Loadout"))})
    {
        const auto Focus=BreakerCoreBoard::Build(Tree,Name);
        // Representative usable board areas at720/1080. Fit-scaled markers
        // retain28px faces plus rank pips; names live in the unscaled detail rail.
        for(const FVector2D View:{FVector2D(800,420),FVector2D(1100,660)})
        for(const float GameUIScale:{.667f,1.0f})
        {
            const float Fit=GameUIScale*FMath::Min(View.X/Focus.Size.X,View.Y/Focus.Size.Y);
            const FVector2D Extent(14/Fit,24/Fit);
            for(const auto& Edge:Focus.Edges)
            {
                const FVector2D A=Focus.Centers[Edge.A], B=Focus.Centers[Edge.B];
                const FVector2D Segment=B-A;
                for(const auto& Node:Focus.Centers)
                {
                    if(Node.Key==Edge.A || Node.Key==Edge.B) continue;
                    const float Along=FMath::Clamp(static_cast<float>(FVector2D::DotProduct(Node.Value-A,Segment)/Segment.SizeSquared()),0.0f,1.0f);
                    TestTrue(TEXT("Authored edge avoids every unrelated marker"),
                        FVector2D::Distance(Node.Value,A+Segment*Along)>14/Fit);
                }
            }
            for(const auto& Point:Focus.Centers)
            {
                const FVector2D P=Point.Value;
                TestTrue(TEXT("Fit-readable marker and pips remain inside board"),P.X>=Extent.X&&P.Y>=Extent.Y
                    &&P.X+Extent.X<=Focus.Size.X&&P.Y+Extent.Y<=Focus.Size.Y);
                for(const auto& Other:Focus.Centers) if(Point.Key.LexicalLess(Other.Key))
                    TestFalse(TEXT("Fit-readable marker bounds do not overlap"),FMath::Abs(P.X-Other.Value.X)<Extent.X*2
                        &&FMath::Abs(P.Y-Other.Value.Y)<Extent.Y*2);
            }
        }
    }
    return true;
}
#endif
