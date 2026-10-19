// 第三层UI原稿颜色回归：读取真实纹理源像素、导入设置及Slate浮点渲染结果。
// 仅编辑器开发测试；不修改资产，不连接后端，不读取/操作人工游戏窗口。
// 浮点线性目标关闭Slate输出Gamma，避免离屏PNG的额外sRGB编码污染判定；源PNG按sRGB解释。
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Materials/MaterialInterface.h"
#include "ImageCore.h"
#include "RenderingThread.h"
#include "Slate/WidgetRenderer.h"
#include "Widgets/Images/SImage.h"
#include "UObject/StrongObjectPtr.h"
#include "TextureResource.h"
#include "UObject/Package.h"
#include "Blueprint/UserWidget.h"
#include "ImageUtils.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "RHI.h"
#include "ShaderCompiler.h"
#include "CommonInputSettings.h"
#include "ICommonInputModule.h" // CommonUI读取的是模块持有的运行设置实例，不是类默认对象。

namespace DivineBeastsUIColorTests
{
    // 以源像素的sRGB解码结果计算双线性期望值；不能拿错误导入设置作为期望颜色。
    FLinearColor SampleDesign(const FImage& Image, const FVector2f UV)
    {
        const float X=UV.X*Image.SizeX-.5f, Y=UV.Y*Image.SizeY-.5f;
        const int32 X0=FMath::Clamp(FMath::FloorToInt(X),0,Image.SizeX-1);
        const int32 Y0=FMath::Clamp(FMath::FloorToInt(Y),0,Image.SizeY-1);
        const auto Pixel=[&](int32 PX,int32 PY) { return FLinearColor::FromSRGBColor(*static_cast<const FColor*>(Image.GetPixelPointer(PX,PY))); };
        return FMath::Lerp(FMath::Lerp(Pixel(X0,Y0),Pixel(FMath::Min(X0+1,Image.SizeX-1),Y0),FMath::Frac(X)),
            FMath::Lerp(Pixel(X0,FMath::Min(Y0+1,Image.SizeY-1)),Pixel(FMath::Min(X0+1,Image.SizeX-1),FMath::Min(Y0+1,Image.SizeY-1)),FMath::Frac(X)),FMath::Frac(Y));
    }

    // 从真实保存页面生成颜色正确的布局证据：线性浮点目标无Slate输出Gamma，PNG编码仅一次。
    // 这不是编辑资产的替代工具；仅测试未激活页面，无账号数据、网络、设备设置写入和世界操作。
    bool SavePagePreview(const TCHAR* ClassPath, const TCHAR* FileName, const FIntPoint Size,
        FAutomationTestBase& Test, const bool bVerifyCover)
    {
        UClass* Class=LoadClass<UUserWidget>(nullptr,ClassPath);
        if(!Class)return false;
        TStrongObjectPtr<UUserWidget> Widget(NewObject<UUserWidget>(GetTransientPackage(),Class));
        if(!Widget->Initialize())return false;
        TStrongObjectPtr<UTextureRenderTarget2D> Target(NewObject<UTextureRenderTarget2D>());
        Target->ClearColor=FLinearColor::Transparent;
        Target->InitCustomFormat(Size.X,Size.Y,PF_FloatRGBA,true);
        Target->UpdateResourceImmediate(true);
        auto* Renderer=new FWidgetRenderer(false,true);
        // CommonInput模块通过NewObject拥有独立的运行设置；加载CDO不会初始化控件实际使用的对象。
        // 必须与CommonActivatableWidget读取同一实例，且保留真实Back行和全部RHI颜色断言。
        UCommonInputSettings& InputSettings=ICommonInputModule::GetSettings();
        InputSettings.LoadData();
        Test.AddInfo(FString::Printf(TEXT("颜色预览使用CommonInput运行实例：%s"),*InputSettings.GetPathName()));
        Test.TestNotNull(TEXT("颜色页渲染前已加载真实返回动作表"),InputSettings.GetDefaultBackAction().DataTable.Get());
        const auto SlateWidget=Widget->TakeWidget();
        Renderer->DrawWidget(Target.Get(),SlateWidget,FVector2D(Size),0.f);
        FlushRenderingCommands();
        if(GShaderCompilingManager)GShaderCompilingManager->FinishAllCompilation();
        Renderer->DrawWidget(Target.Get(),SlateWidget,FVector2D(Size),0.f);
        FlushRenderingCommands();
        bool bColorsValid=true;
        if(bVerifyCover)
        {
            // 选取不被LOGO或表单覆盖的三处背景。直接比较最终页面GPU像素，覆盖原稿到显示的完整路径。
            UTexture2D* Cover=LoadObject<UTexture2D>(nullptr,TEXT("/DBAUIPack_Core/UI/Textures/Nature/T_DBA_NatureLoginBackdrop"));
            FImage Source;
            TArray<FFloat16Color> Pixels;
            bColorsValid=Cover&&Cover->Source.GetMipImage(Source,0)
                &&Target->GameThread_GetRenderTargetResource()->ReadFloat16Pixels(Pixels)&&Pixels.Num()==Size.X*Size.Y;
            if(bColorsValid)
            {
                const float Scale=FMath::Max(float(Size.X)/Source.SizeX,float(Size.Y)/Source.SizeY);
                const FVector2f Crop((Source.SizeX*Scale-Size.X)*.5f,(Source.SizeY*Scale-Size.Y)*.5f);
                for(const FVector2f Sample:{FVector2f(.90f,.10f),FVector2f(.08f,.45f),FVector2f(.92f,.70f)})
                {
                    const int32 X=FMath::FloorToInt(Sample.X*Size.X),Y=FMath::FloorToInt(Sample.Y*Size.Y);
                    const FVector2f UV((X+.5f+Crop.X)/(Source.SizeX*Scale),(Y+.5f+Crop.Y)/(Source.SizeY*Scale));
                    const FLinearColor Expected=SampleDesign(Source,UV);
                    const FLinearColor Actual=Pixels[Y*Size.X+X].GetFloats();
                    bColorsValid&=Test.TestTrue(*FString::Printf(TEXT("%s 最终背景颜色误差≤0.008，actual=%s expected=%s"),FileName,*Actual.ToString(),*Expected.ToString()),
                        FMath::Abs(Actual.R-Expected.R)<=.008f&&FMath::Abs(Actual.G-Expected.G)<=.008f&&FMath::Abs(Actual.B-Expected.B)<=.008f);
                }
            }
        }
        const FString Path=FPaths::ProjectSavedDir()/TEXT("Validation/UIColorRHI")/FileName;
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path),true);
        TUniquePtr<FArchive> Writer(IFileManager::Get().CreateFileWriter(*Path));
        const bool bSaved=Writer&&FImageUtils::ExportRenderTarget2DAsPNG(Target.Get(),*Writer);
        if(Writer)Writer->Close();
        BeginCleanup(Renderer);
        return bSaved&&bColorsValid;
    }

    // 仅生成瞬态Slate图像和16×16浮点目标；同步GPU后读取中心像素并按引擎延迟清理释放渲染器。
    bool RenderLinear(UObject* Resource, const FVector2f UV, FLinearColor& OutColor)
    {
        TStrongObjectPtr<UTextureRenderTarget2D> Target(NewObject<UTextureRenderTarget2D>());
        Target->ClearColor=FLinearColor::Transparent;
        Target->InitCustomFormat(16,16,PF_FloatRGBA,true);
        Target->UpdateResourceImmediate(true);
        FSlateBrush Brush;
        Brush.DrawAs=ESlateBrushDrawType::Image;
        Brush.ImageSize=FVector2D(16,16);
        Brush.TintColor=FSlateColor(FLinearColor::White);
        Brush.SetResourceObject(Resource);
        Brush.SetUVRegion(FBox2f(UV,UV));
        const auto Widget=SNew(SImage).Image(&Brush).ColorAndOpacity(FLinearColor::White);
        FWidgetRenderer* Renderer=new FWidgetRenderer(false,true);
        // 首次绘制建立材质句柄；再次绘制使用已编译资源。测试必须在有RHI的编辑器运行。
        Renderer->DrawWidget(Target.Get(),Widget,FVector2D(16,16),0.f);
        FlushRenderingCommands();
        if(GShaderCompilingManager)GShaderCompilingManager->FinishAllCompilation();
        Renderer->DrawWidget(Target.Get(),Widget,FVector2D(16,16),0.f);
        FlushRenderingCommands();
        TArray<FFloat16Color> Pixels;
        const bool bRead=Target->GameThread_GetRenderTargetResource()->ReadFloat16Pixels(Pixels);
        BeginCleanup(Renderer);
        if(!bRead||Pixels.Num()!=256)return false;
        OutColor=Pixels[8*16+8].GetFloats();
        return true;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsUIColorPipelineTest,
    "DivineBeasts.UI.Color.SourceToSlate",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsUIColorPipelineTest::RunTest(const FString&)
{
    // NullRHI没有真实GPU结果，必须报失败而不是跳过后把“通过”写入交付记录。
    if(GUsingNullRHI)
    {
        AddError(TEXT("颜色回归必须使用真实RHI；NullRHI不能作为渲染颜色证据。"));
        return false;
    }
    // 离屏编辑器没有本地玩家子系统替我们加载CommonUI动作数据；真实设置页仍要使用正式输入定义。
    // 在模块实际持有的设置实例上初始化；不注入替代数据、不吞ensure、不依赖测试执行顺序。
    ICommonInputModule::GetSettings().LoadData();
    // 本轮三个真实魔法图标必须加入发布用同一RHI门禁，不沿用只覆盖旧九张纹理的报告。
    for(const TCHAR* Name:{TEXT("Menu"),TEXT("Ability"),TEXT("Minimap"),TEXT("PlayerFrame"),TEXT("LoginPanel"),TEXT("Panel"),TEXT("Logo"),TEXT("Cover"),TEXT("Zodiac"),TEXT("Fireball"),TEXT("Frostbolt"),TEXT("ArcaneMissiles")})
    {
        const bool bMagicIcon=FCString::Strcmp(Name,TEXT("Fireball"))==0||FCString::Strcmp(Name,TEXT("Frostbolt"))==0||FCString::Strcmp(Name,TEXT("ArcaneMissiles"))==0;
        const FString Path=bMagicIcon ? FString::Printf(TEXT("/DBAUIPack_Core/UI/Abilities/MagicTests/T_DBA_Dev%s"),Name)
            : FCString::Strcmp(Name,TEXT("Logo"))==0
            ? TEXT("/DBAUIPack_Core/UI/Textures/DBA_MythicLogo")
            : FCString::Strcmp(Name,TEXT("Zodiac"))==0
                ? TEXT("/DBAUIPack_Core/UI/Textures/Zodiac/T_DBA_ZodiacChoiceAtlas_V1")
            : FCString::Strcmp(Name,TEXT("Cover"))==0
                ? TEXT("/DBAUIPack_Core/UI/Textures/Nature/T_DBA_NatureLoginBackdrop")
                : FString::Printf(TEXT("/DBAUIPack_Core/UI/Textures/Nature/T_DBA_Nature%sReference"),Name);
        UTexture2D* Texture=LoadObject<UTexture2D>(nullptr,*Path);
        if(!TestNotNull(*Path,Texture))continue;
        TestTrue(TEXT("原稿彩色纹理必须启用sRGB"),Texture->SRGB);
        TestTrue(TEXT("原稿必须无损UI压缩"),Texture->CompressionSettings==TC_EditorIcon);
        TestTrue(TEXT("原稿必须固定无Mip"),Texture->MipGenSettings==TMGS_NoMipmaps);
        TestEqual(TEXT("不得调整亮度"),Texture->AdjustBrightness,1.f);
        TestEqual(TEXT("不得调整饱和度"),Texture->AdjustSaturation,1.f);
        TestEqual(TEXT("不得调整RGB曲线"),Texture->AdjustRGBCurve,1.f);
        TestEqual(TEXT("不得调整色相"),Texture->AdjustHue,0.f);
        FImage Source;
        if(!TestTrue(TEXT("真实源像素可读"),Texture->Source.GetMipImage(Source,0)))continue;
        if(!TestEqual(TEXT("原稿源格式为BGRA8"),Source.Format,ERawImageFormat::BGRA8))continue;
        TArray<FVector2f> Samples{FVector2f(.5f,.5f)};
        if(bMagicIcon)
        {
            // 中心能量核与两侧有色过渡均核对，不能以近黑背景像素掩盖重复Gamma编码。
            Samples.Add(FVector2f(.4f,.45f)); Samples.Add(FVector2f(.6f,.55f));
        }
        if(FCString::Strcmp(Name,TEXT("Zodiac"))==0)
        {
            // 新图集必须覆盖十二格，而非仅核对整体中心；不以旧原稿测试报告放行新图。
            Samples.Reset();
            for(float V:{1.f/6.f,.5f,5.f/6.f})
            for(float U:{.125f,.375f,.625f,.875f}) { Samples.Add(FVector2f(U,V)); }
        }
        for(const FVector2f UV:Samples)
        {
            FLinearColor Actual;
            if(!TestTrue(TEXT("实际GPU纹理渲染回读成功"),DivineBeastsUIColorTests::RenderLinear(Texture,UV,Actual)))continue;
            const auto Expected=DivineBeastsUIColorTests::SampleDesign(Source,UV);
            // 允许8位导入/双线性采样/半浮点误差；大幅Gamma偏差必须失败，不能只比较资产标签。
            TestTrue(*FString::Printf(TEXT("%s 原稿到Slate误差≤0.008，actual=%s expected=%s"),Name,*Actual.ToString(),*Expected.ToString()),
                FMath::Abs(Actual.R-Expected.R)<=.008f&&FMath::Abs(Actual.G-Expected.G)<=.008f&&FMath::Abs(Actual.B-Expected.B)<=.008f);
        }
    }
    // 原稿面板材质还经过自定义HLSL与九宫格UV裁切，单查纹理设置不能覆盖这一环节。
    auto* Texture=LoadObject<UTexture2D>(nullptr,TEXT("/DBAUIPack_Core/UI/Textures/Nature/T_DBA_NaturePanelReference"));
    auto* Material=LoadObject<UMaterialInterface>(nullptr,TEXT("/DBAUIPack_Core/UI/Materials/Nature/M_DBA_NaturePanelNineSlice"));
    FImage Source;
    FLinearColor Actual;
    if(TestNotNull(TEXT("实际面板材质"),Material)&&Texture&&Texture->Source.GetMipImage(Source,0)
        &&TestTrue(TEXT("实际面板材质GPU回读"),DivineBeastsUIColorTests::RenderLinear(Material,FVector2f(.5f,.6f),Actual)))
    {
        const auto Expected=DivineBeastsUIColorTests::SampleDesign(Source,FVector2f(.5f,.565f));
        TestTrue(*FString::Printf(TEXT("面板材质颜色无额外Gamma/乘色，actual=%s expected=%s"),*Actual.ToString(),*Expected.ToString()),
            FMath::Abs(Actual.R-Expected.R)<=.008f&&FMath::Abs(Actual.G-Expected.G)<=.008f&&FMath::Abs(Actual.B-Expected.B)<=.008f);
        TestTrue(TEXT("实心面板内部不透出场景"),Actual.A>=.99f);
    }
    TestTrue(TEXT("线性浮点设置页面证据成功导出"),DivineBeastsUIColorTests::SavePagePreview(
        TEXT("/DBAUIPack_Core/UI/Screens/WBP_DBA_UI_Settings.WBP_DBA_UI_Settings_C"),TEXT("SettingsLinearVerified.png"),FIntPoint(960,540),*this,false));
    for(const FIntPoint Size:{FIntPoint(960,540),FIntPoint(1920,1080),FIntPoint(1280,1024),FIntPoint(2560,1080)})
    {
        const FString FileName=FString::Printf(TEXT("Login%d_%d_LinearVerified.png"),Size.X,Size.Y);
        TestTrue(TEXT("登录页等比铺满且最终颜色正确"),DivineBeastsUIColorTests::SavePagePreview(
            TEXT("/DBAUIPack_Core/UI/Screens/WBP_DBA_UI_Login.WBP_DBA_UI_Login_C"),*FileName,Size,*this,true));
    }
    return true;
}

// 新增九份公共页复用上方既有线性浮点RHI/单次PNG编码链路，不调用Monolith已知错误的widget截图。
// 只构造未激活的真实资产测试实例，无账号/后端/权威状态；图像供集中人工核验，不自动判定布局或艺术品质。
// 本公共模块不加载竞技或开发中心资产，防止为截图使公共UI反向依赖可选模块。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsRemainingPagesRHIReportTest,
    "DivineBeasts.UI.RemainingSurfaces.RHISavedPages",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsRemainingPagesRHIReportTest::RunTest(const FString&)
{
    if (GUsingNullRHI)
    {
        AddError(TEXT("剩余页画面证据必须使用真实RHI，不能将NullRHI或未执行视为通过。"));
        return false;
    }
    // 使用明确的资产路径集合名，避免Unity编译时遮蔽其他测试文件的页面映射。
    const TCHAR* PageAssetPaths[] = {
        TEXT("Screens/WBP_DBA_UI_Boot"), TEXT("Screens/WBP_DBA_UI_Quest"),
        TEXT("Screens/WBP_DBA_UI_Social"), TEXT("Screens/WBP_DBA_UI_LiveOps"),
        TEXT("HUD/WBP_DBA_UI_OpenWorldHUD"), TEXT("HUD/WBP_DBA_UI_VillageMainHUD"),
        TEXT("HUD/WBP_DBA_UI_TutorialGuidance"), TEXT("HUD/WBP_DBA_UI_TrainingControls"),
        TEXT("Notifications/WBP_DBA_UI_Toast")};
    for (const TCHAR* Page : PageAssetPaths)
    {
        const FString Name = FPaths::GetCleanFilename(Page);
        const FString ClassPath = FString(TEXT("/DBAUIPack_Core/UI/")) + Page + TEXT(".") + Name + TEXT("_C");
        const FString FileName = FString(TEXT("RemainingPages/")) + Name + TEXT("_1920x1080_RenderEvidence.png");
        TestTrue(*FString::Printf(TEXT("%s真实RHI页面导出"), *Name),
            DivineBeastsUIColorTests::SavePagePreview(*ClassPath, *FileName, FIntPoint(1920, 1080), *this, false));
    }
    return true;
}
/** 小队操作说明使用真实保存页面，复用线性RHI回读；不伪造账号、成员或匹配状态。
 * 验证目的为资产可构造、GPU可绘制和文件可导出，交互与网络仍由各自定向回归验证。
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsPartyPagesRHIReportTest,
    "DivineBeasts.UI.Party.RHISavedPages",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsPartyPagesRHIReportTest::RunTest(const FString&)
{
    if(GUsingNullRHI){AddError(TEXT("小队操作界面证据必须使用真实RHI。"));return false;}
    for(const TCHAR* Name:{TEXT("WBP_DBA_PartyPanel"),TEXT("WBP_DBA_PartyInviteModal"),TEXT("WBP_DBA_PartyReadyCheckModal")})
    {
        const FString ClassPath=FString(TEXT("/DBAUIPack_Core/UI/Social/"))+Name+TEXT(".")+Name+TEXT("_C");
        const FString FileName=FString(TEXT("Party/"))+Name+TEXT(".png");
        TestTrue(TEXT("真实小队页面线性浮点RHI导出"),DivineBeastsUIColorTests::SavePagePreview(
            *ClassPath,*FileName,FIntPoint(960,720),*this,false));
    }
    return true;
}
/** 真实保存加载页的原生RHI证据；只验证布局和绑定资产可绘制，实际进度由另项事件回归验证。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsLoadingPageRHIReportTest,
    "DivineBeasts.UI.LoadingTravel.RHISavedPage",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsLoadingPageRHIReportTest::RunTest(const FString&)
{
    if (GUsingNullRHI) { AddError(TEXT("加载页证据必须使用真实RHI。")); return false; }
    return TestTrue(TEXT("真实加载页面线性浮点RHI导出"), DivineBeastsUIColorTests::SavePagePreview(
        TEXT("/DBAUIPack_Core/UI/Screens/WBP_DBA_UI_LoadingTravel.WBP_DBA_UI_LoadingTravel_C"),
        TEXT("LoadingTravel_1920x1080.png"), FIntPoint(1920,1080), *this, false));
}
#endif
