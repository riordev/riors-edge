#include "Misc/AutomationTest.h"
#include "Engine/Font.h"
#include "Engine/FontFace.h"
#include "Fonts/CompositeFont.h"
#include "UI/BreakerTypeRoles.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerRoleFontTest, "RiorsEdge.UI.Type.ShippedRoleFonts",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerRoleFontTest::RunTest(const FString& Parameters)
{
    struct FRole { const TCHAR* Asset; TArray<TPair<FName, FName>> Faces; };
    const TArray<FRole> Roles = {
        {TEXT("F_BreakerDisplay"), {{TEXT("SemiBold"), TEXT("FF_BarlowCondensedSemiBold")}, {TEXT("Bold"), TEXT("FF_BarlowCondensedBold")}}},
        {TEXT("F_BreakerBody"), {{TEXT("Regular"), TEXT("FF_SourceSans3Regular")}, {TEXT("Medium"), TEXT("FF_SourceSans3Medium")}, {TEXT("SemiBold"), TEXT("FF_SourceSans3SemiBold")}}},
        {TEXT("F_BreakerMono"), {{TEXT("Regular"), TEXT("FF_SometypeMonoMedium")}, {TEXT("Medium"), TEXT("FF_SometypeMonoBold")}}}
    };
    for (const auto& Role : Roles)
    {
        const FString Path = FString::Printf(TEXT("/Game/Breaker/UI/Fonts/%s.%s"), Role.Asset, Role.Asset);
        const auto* Font = LoadObject<UFont>(nullptr, *Path);
        if (!TestNotNull(TEXT("shipped role asset, not engine fallback"), Font)) return false;
        TestEqual(TEXT("role uses runtime vector cache"), Font->FontCacheType, EFontCacheType::Runtime);
        const auto& Entries = Font->GetInternalCompositeFont().DefaultTypeface.Fonts;
        TestEqual(TEXT("exact authored weight count"), Entries.Num(), Role.Faces.Num());
        for (const auto& Expected : Role.Faces)
        {
            const auto* Entry = Entries.FindByPredicate([&](const FTypefaceEntry& Value) { return Value.Name == Expected.Key; });
            if (!TestNotNull(TEXT("preserved weight key"), Entry)) return false;
            const auto* Face = Cast<UFontFace>(Entry->Font.GetFontFaceAsset());
            if (!TestNotNull(TEXT("weight references imported font face"), Face)) return false;
            TestEqual(TEXT("weight resolves requested supplied family"), Face->GetFName(), Expected.Value);
            TestTrue(TEXT("face contains font data"), Entry->Font.HasFont());
        }
    }
    TestEqual(TEXT("display helper retains semibold weight"), BreakerDisplayFont(20).TypefaceFontName, FName(TEXT("SemiBold")));
    TestEqual(TEXT("body helper retains regular prose"), BreakerBodyFont(14).TypefaceFontName, FName(TEXT("Regular")));
    TestEqual(TEXT("numeric helper retains medium emphasis"), BreakerMonoFont(14, 0, true).TypefaceFontName, FName(TEXT("Medium")));
    return true;
}
#endif
