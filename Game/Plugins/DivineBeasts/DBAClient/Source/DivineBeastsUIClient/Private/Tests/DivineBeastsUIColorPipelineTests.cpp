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
#include "Components/Image.h"
#include "Panels/Combat/DivineBeastsPlayerStatusPanel.h"
#include "Components/GamePlatformCastProgressWidget.h"
#include "Components/ProgressBar.h"
#include "Panels/Combat/DivineBeastsAbilityBarPanel.h"
#include "Definitions/DivineBeastsAbilityUIProfile.h"
#include "Blueprint/WidgetTree.h"
#include "Components/RetainerBox.h"

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
        FAutomationTestBase& Test, const bool bVerifyCover, const bool bVerifySkillBarBottom = false)
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
        if (bVerifySkillBarBottom)
        {
            // 检查真实HUD合成最后一行有连续底板主体，不能用少量半透明散点冒充贴底。
            TArray<FFloat16Color> Pixels;
            int32 SolidBottomPixels = 0;
            const bool bRead = Target->GameThread_GetRenderTargetResource()->ReadFloat16Pixels(Pixels)
                && Pixels.Num() == Size.X * Size.Y;
            if (bRead)
            {
                for (int32 X = Size.X / 2 - 364; X < Size.X / 2 + 364; ++X)
                    if (Pixels[(Size.Y - 1) * Size.X + X].GetFloats().A > .1f) ++SolidBottomPixels;
            }
            bColorsValid &= Test.TestTrue(*FString::Printf(TEXT("%s技能栏真实可见主体贴底，底行有效像素%d"),
                FileName, SolidBottomPixels), bRead && SolidBottomPixels >= 400);
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
    // 新导入的加载条四张原稿都必须进入同一真实RHI颜色门禁；遮罩以线性值解释，彩色图以sRGB解释。
    // 半透明前景在不同Slate目标上可采用直通或预乘RGB，二者都必须还原到相同源色且有实际Alpha输出。
    struct FLoadingBarProbe { const TCHAR* Name; int32 Row; bool bColor; };
    const FLoadingBarProbe LoadingBarProbes[] = {
        {TEXT("BG"),1,true}, {TEXT("FG"),8,true}, {TEXT("Mask"),3,false}, {TEXT("Frame"),1,true}};
    for(const FLoadingBarProbe& Probe:LoadingBarProbes)
    {
        const FString Path=FString::Printf(TEXT("/DBAUIPack_Core/UI/Loading/Textures/T_DBA_UI_LoadingBar_%s_1200x20"),Probe.Name);
        UTexture2D* LoadingTexture=LoadObject<UTexture2D>(nullptr,*Path);
        if(!TestNotNull(*Path,LoadingTexture))continue;
        // 离屏编辑器进程可能先给Texture2D分配占位的运行表面尺寸；实际导入尺寸以下方持久化Source和GPU采样核对。
        TestTrue(TEXT("加载条颜色角色与sRGB一致"),LoadingTexture->SRGB==Probe.bColor);
        TestTrue(TEXT("加载条保持无损UI压缩和无Mip"),LoadingTexture->CompressionSettings==TC_EditorIcon
            &&LoadingTexture->MipGenSettings==TMGS_NoMipmaps&&LoadingTexture->LODGroup==TEXTUREGROUP_UI);
        FImage LoadingSource;
        if(!TestTrue(TEXT("加载条真实源像素可读"),LoadingTexture->Source.GetMipImage(LoadingSource,0)))continue;
        if(!TestTrue(TEXT("加载条源像素格式和尺寸正确"),LoadingSource.Format==ERawImageFormat::BGRA8
            &&LoadingSource.SizeX==1200&&LoadingSource.SizeY==20))continue;
        for(const int32 Column:{100,600,1100})
        {
            const FVector2f UV((Column+.5f)/1200.f,(Probe.Row+.5f)/20.f);
            const FColor Raw=*static_cast<const FColor*>(LoadingSource.GetPixelPointer(Column,Probe.Row));
            const FLinearColor Expected=Probe.bColor ? FLinearColor::FromSRGBColor(Raw)
                : FLinearColor(Raw.R/255.f,Raw.G/255.f,Raw.B/255.f,Raw.A/255.f);
            FLinearColor Actual;
            if(!TestTrue(TEXT("加载条实际GPU纹理渲染回读成功"),DivineBeastsUIColorTests::RenderLinear(LoadingTexture,UV,Actual)))continue;
            const auto MaxRGBError=[&Expected](const FLinearColor& Value)
            {
                return FMath::Max3(FMath::Abs(Value.R-Expected.R),FMath::Abs(Value.G-Expected.G),FMath::Abs(Value.B-Expected.B));
            };
            const bool bColorMatches=MaxRGBError(Actual)<=.012f
                ||(Actual.A>.01f&&MaxRGBError(Actual/Actual.A)<=.012f);
            TestTrue(*FString::Printf(TEXT("加载条%s像素(%d,%d)原稿到Slate颜色一致，actual=%s expected=%s"),
                Probe.Name,Column,Probe.Row,*Actual.ToString(),*Expected.ToString()),Actual.A>.1f&&bColorMatches);
        }
    }
    // 经人工批准的技能栏13张彩色图及13张独立Alpha全部进入原生Slate门禁。
    // 每张图选取左、中、右三个非透明且颜色非极值的真实源像素，避免只采到黑背景或白高光。
    for (const TCHAR* Part : {TEXT("SkillBar_Center"), TEXT("SkillBar_EndLeft_V01"),
        TEXT("SkillBar_EndRight_V01"), TEXT("SkillSlot_BG"), TEXT("FunctionSlot_BG"),
        TEXT("Icon_Inventory"), TEXT("Icon_Party"), TEXT("Icon_Settings"),
        TEXT("CastBar_BG"), TEXT("CastBar_FG"), TEXT("Momentum_Track"),
        TEXT("Momentum_Gradient"), TEXT("Momentum_Bubble")})
    {
        for (const bool bMask : {false, true})
        {
            const FString SkillPath = FString::Printf(TEXT("/DBAUIPack_Core/UI/Textures/SkillBar/T_DBA_UI_%s%s"),
                Part, bMask ? TEXT("_Alpha") : TEXT(""));
            UTexture2D* SkillTexture = LoadObject<UTexture2D>(nullptr, *SkillPath);
            if (!TestNotNull(*SkillPath, SkillTexture)) continue;
            TestTrue(TEXT("技能栏Color与线性Alpha角色正确"), SkillTexture->SRGB == !bMask);
            TestTrue(TEXT("技能栏无损UI、无Mip及Clamp寻址"), SkillTexture->CompressionSettings == TC_EditorIcon
                && SkillTexture->MipGenSettings == TMGS_NoMipmaps && SkillTexture->LODGroup == TEXTUREGROUP_UI
                && SkillTexture->AddressX == TA_Clamp && SkillTexture->AddressY == TA_Clamp);
            FImage SkillSource;
            if (!TestTrue(TEXT("技能栏导入源BGRA8可读取"), SkillTexture->Source.GetMipImage(SkillSource, 0)
                && SkillSource.Format == ERawImageFormat::BGRA8)) continue;
            for (int32 Third = 0; Third < 3; ++Third)
            {
                FIntPoint Probe(-1, -1);
                float BestScore = -1.f;
                for (int32 Y = 0; Y < SkillSource.SizeY; ++Y)
                for (int32 X = Third * SkillSource.SizeX / 3; X < (Third + 1) * SkillSource.SizeX / 3; ++X)
                {
                    const FColor Raw = *static_cast<const FColor*>(SkillSource.GetPixelPointer(X, Y));
                    if (Raw.A < 64) continue;
                    const float Midtone = (Raw.R + Raw.G + Raw.B) / (3.f * 255.f);
                    const float Score = (1.f - FMath::Abs(Midtone - .5f)) * (Raw.A / 255.f);
                    if (Score > BestScore) { BestScore = Score; Probe = FIntPoint(X, Y); }
                }
                if (!TestTrue(TEXT("技能栏每个区域有真实可见样本"), Probe.X >= 0)) continue;
                const FColor Raw = *static_cast<const FColor*>(SkillSource.GetPixelPointer(Probe.X, Probe.Y));
                const FLinearColor Expected = bMask ? FLinearColor(Raw.R / 255.f, Raw.G / 255.f, Raw.B / 255.f, Raw.A / 255.f)
                    : FLinearColor::FromSRGBColor(Raw);
                FLinearColor Actual;
                const FVector2f UV((Probe.X + .5f) / SkillSource.SizeX, (Probe.Y + .5f) / SkillSource.SizeY);
                if (!TestTrue(TEXT("技能栏真实GPU/Slate回读成功"), DivineBeastsUIColorTests::RenderLinear(SkillTexture, UV, Actual))) continue;
                const auto RGBError = [&Expected](const FLinearColor& Color)
                { return FMath::Max3(FMath::Abs(Color.R - Expected.R), FMath::Abs(Color.G - Expected.G), FMath::Abs(Color.B - Expected.B)); };
                // Slate目标允许直通或预乘储存；必须还原到相同原稿颜色，Alpha也须独立一致。
                TestTrue(*FString::Printf(TEXT("技能栏%s%s区域%d颜色与Alpha一致 actual=%s expected=%s"), Part,
                    bMask ? TEXT("_Alpha") : TEXT(""), Third, *Actual.ToString(), *Expected.ToString()),
                    FMath::Abs(Actual.A - Expected.A) <= .012f && (RGBError(Actual) <= .012f
                        || (Actual.A > .01f && RGBError(Actual / Actual.A) <= .012f)));
            }
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

/**
 * 真实槽位整树的最终GPU颜色门禁，覆盖UImage之后的Retainer/父控件乘色。
 * 与同张运行Texture的白乘色Slate直绘比较，不能用图标指针相等或裸Texture测试放行。
 * 所有对象均为已保存Widget的瞬态测试实例，不修改资产、不伪造GAS授权或人工验收。
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsHeroAbilitySlotRGBTest,
    "DivineBeasts.UI.Color.HeroAbilitySlotRGB",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsHeroAbilitySlotRGBTest::RunTest(const FString&)
{
    if (GUsingNullRHI) { AddError(TEXT("技能图标最终颜色必须使用真实RHI。")); return false; }
    UClass* Class = LoadClass<UDivineBeastsAbilityBarPanel>(nullptr,
        TEXT("/DBAUIPack_Core/UI/Combat/WBP_DBA_UI_AbilityBar.WBP_DBA_UI_AbilityBar_C"));
    if (!TestNotNull(TEXT("已保存技能栏"), Class)) return false;
    ICommonInputModule::GetSettings().LoadData();
    struct FIconProbe { const TCHAR* Hero; const TCHAR* Input; const TCHAR* Widget; const TCHAR* Ability; };
    const FIconProbe Probes[] = {
        {TEXT("Goat"), TEXT("Primary"), TEXT("PrimaryAbilitySlot"), TEXT("directedmissile")},
        {TEXT("Goat"), TEXT("Slot1"), TEXT("ActiveAbilitySlot1"), TEXT("fireball")},
        {TEXT("Goat"), TEXT("Slot2"), TEXT("ActiveAbilitySlot2"), TEXT("frostbolt")},
        {TEXT("Goat"), TEXT("Slot4"), TEXT("UltimateAbilitySlot"), TEXT("arcanemissiles")},
        {TEXT("Rat"), TEXT("Primary"), TEXT("PrimaryAbilitySlot"), TEXT("directedmissile")},
        {TEXT("Rabbit"), TEXT("Primary"), TEXT("PrimaryAbilitySlot"), TEXT("directedmissile")},
        {TEXT("Rooster"), TEXT("Primary"), TEXT("PrimaryAbilitySlot"), TEXT("directedmissile")},
        {TEXT("Rooster"), TEXT("Slot1"), TEXT("ActiveAbilitySlot1"), TEXT("fireball")},
        {TEXT("Rooster"), TEXT("Slot2"), TEXT("ActiveAbilitySlot2"), TEXT("frostbolt")},
        {TEXT("Rooster"), TEXT("Slot4"), TEXT("UltimateAbilitySlot"), TEXT("arcanemissiles")},
        {TEXT("Rooster"), TEXT("Slot3"), TEXT("MovementAbilitySlot"), TEXT("primary")},
        {TEXT("Rooster"), TEXT("Slot5"), TEXT("DefenseAbilitySlot"), TEXT("dba.ability.rooster_eight_wing_guard@1")},
        {TEXT("Rooster"), TEXT("Slot6"), TEXT("ControlAbilitySlot"), TEXT("dba.ability.rooster_eight_sonic_silence@1")},
        {TEXT("Rooster"), TEXT("Slot7"), TEXT("SignatureAbilitySlot"), TEXT("dba.ability.rooster_eight_first_light_heal@1")}};
    bool bPassed = true;
    for (const auto& Probe : Probes)
    {
        TStrongObjectPtr<UDivineBeastsAbilityBarPanel> Bar(
            NewObject<UDivineBeastsAbilityBarPanel>(GetTransientPackage(), Class));
        if (!TestTrue(TEXT("真实技能栏初始化成功"), Bar->Initialize())) return false;
        auto* Slot = Cast<UGamePlatformSlotWidget>(Bar->GetWidgetFromName(Probe.Widget));
        if (!TestNotNull(TEXT("真实技能槽"), Slot)) return false;
        // 父Widget初始化已递归初始化子UserWidget；再次Initialize返回false并不代表构造失败。
        if (!TestNotNull(TEXT("父技能栏已初始化子槽位WidgetTree"), Slot->WidgetTree.Get())) return false;
        auto* Icon = Cast<UImage>(Slot->GetWidgetFromName(TEXT("AbilityIcon")));
        if (!TestNotNull(TEXT("真实技能图像"), Icon)) return false;
        const FString ProfilePath = FString::Printf(
            TEXT("/DBAHeroPack_%s/UI/Abilities/DA_DBA_%s_PlaceholderUIProfile.DA_DBA_%s_PlaceholderUIProfile"),
            Probe.Hero, Probe.Hero, Probe.Hero);
        const auto* Profile = LoadObject<UDivineBeastsAbilityUIProfile>(nullptr, *ProfilePath);
        if (!TestNotNull(TEXT("已保存当前英雄Profile"), Profile)) return false;
        const FName AbilityId(*((FString(Probe.Ability).StartsWith(TEXT("dba."))) ? FString(Probe.Ability)
            : FString::Printf(TEXT("dba.ability.%s_placeholder_%s@1"), *FString(Probe.Hero).ToLower(), Probe.Ability)));
        const auto* Entry = Profile->FindEntry(AbilityId);
        if (!TestNotNull(TEXT("实际授权ID图标条目"), Entry)) return false;
        UTexture2D* Texture = Entry->Icon.LoadSynchronous();
        if (!TestNotNull(TEXT("真实当前角色Texture2D"), Texture)) return false;
        // TakeWidget先完成槽位构造，随后与运行时相同由事件投影当前图标，避免测构造占位图。
        const auto SlateWidget = Slot->TakeWidget();
        FGamePlatformUISlotState State;
        State.SlotId = FName(*(FString(TEXT("Platform.Ability.Input.DivineBeasts.")) + Probe.Input));
        State.ContentId = AbilityId; State.Icon = Texture; State.bEnabled = false; State.bPending = true;
        Bar->ApplyAbilitySlots({State});
        bPassed &= TestTrue(TEXT("原图UImage必须白色乘色"), Icon->GetColorAndOpacity().Equals(FLinearColor::White));
        bPassed &= TestTrue(TEXT("原图画刷必须白色乘色"), Icon->GetBrush().TintColor.GetSpecifiedColor().Equals(FLinearColor::White));
        TStrongObjectPtr<UTextureRenderTarget2D> Target(NewObject<UTextureRenderTarget2D>());
        Target->ClearColor = FLinearColor::Transparent;
        constexpr int32 Side = 96;
        Target->InitCustomFormat(Side, Side, PF_FloatRGBA, true);
        Target->UpdateResourceImmediate(true);
        auto* Renderer = new FWidgetRenderer(false, true);
        const auto Draw = [&]()
        {
            // 仅显式请求测试离屏渲染；不据此声称运行时失效通知或业务状态已验证。
            TArray<UWidget*> Widgets;
            Slot->WidgetTree->GetAllWidgets(Widgets);
            for (auto* Widget : Widgets)
                if (auto* Retainer = Cast<URetainerBox>(Widget)) Retainer->RequestRender();
            Renderer->DrawWidget(Target.Get(), SlateWidget, FVector2D(Side, Side), 0.f);
            FlushRenderingCommands();
        };
        Draw();
        if (GShaderCompilingManager) GShaderCompilingManager->FinishAllCompilation();
        Draw();
        TArray<FFloat16Color> Pixels;
        const bool bRead = Target->GameThread_GetRenderTargetResource()->ReadFloat16Pixels(Pixels)
            && Pixels.Num() == Side * Side;
        if (!TestTrue(TEXT("槽位整树最终RHI像素可回读"), bRead)) { BeginCleanup(Renderer); return false; }
        const FGeometry Geometry = Icon->GetCachedGeometry();
        const FVector2D Extent = Geometry.GetLocalSize();
        if (!TestTrue(TEXT("实际技能图标有有效绘制尺寸"), Extent.X > 1. && Extent.Y > 1.))
            { BeginCleanup(Renderer); return false; }
        int32 Compared = 0;
        for (const FVector2D UV : {FVector2D(.25, .25), FVector2D(.5, .3), FVector2D(.7, .4),
            FVector2D(.3, .6), FVector2D(.5, .6), FVector2D(.7, .65)})
        {
            const FVector2D Absolute = Geometry.LocalToAbsolute(Extent * UV);
            const int32 X = FMath::FloorToInt(Absolute.X), Y = FMath::FloorToInt(Absolute.Y);
            if (!TestTrue(TEXT("采样点位于槽位目标中"), X >= 0 && X < Side && Y >= 0 && Y < Side)) continue;
            const FVector2D Local = Geometry.AbsoluteToLocal(FVector2D(X + .5, Y + .5));
            FLinearColor Expected;
            if (!TestTrue(TEXT("同张运行Texture白乘色直绘参考有效"), DivineBeastsUIColorTests::RenderLinear(Texture,
                FVector2f(Local.X / Extent.X, Local.Y / Extent.Y), Expected))) continue;
            if (Expected.A < .98f) continue; // 半透明边界与底板合成不作为RGB滤色判定点。
            const FLinearColor Actual = Pixels[Y * Side + X].GetFloats();
            ++Compared;
            bPassed &= TestTrue(*FString::Printf(TEXT("%s %s 实际槽位保留原图RGB actual=%s reference=%s"),
                Probe.Hero, Probe.Input, *Actual.ToString(), *Expected.ToString()),
                FMath::Abs(Actual.R - Expected.R) <= .035f && FMath::Abs(Actual.G - Expected.G) <= .035f
                && FMath::Abs(Actual.B - Expected.B) <= .035f);
        }
        bPassed &= TestTrue(TEXT("至少五个不透明内部点实际比较RGB"), Compared >= 5);
        const FString Path = FPaths::ProjectSavedDir() / TEXT("Validation/UIColorRHI/HeroAbilitySlots") /
            FString::Printf(TEXT("%s_%s.png"), Probe.Hero, Probe.Input);
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
        TUniquePtr<FArchive> Writer(IFileManager::Get().CreateFileWriter(*Path));
        bPassed &= TestTrue(TEXT("真实槽位颜色证据导出"), Writer && FImageUtils::ExportRenderTarget2DAsPNG(Target.Get(), *Writer));
        if (Writer) Writer->Close();
        BeginCleanup(Renderer);
    }
    return bPassed;
}

/** 酉鸡本轮16张原稿的原生RHI颜色与原尺寸审核页；不授予GAS或连接服务器。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsRoosterV6SourceColorTest,
    "DivineBeasts.UI.Color.RoosterV6Sources",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsRoosterV6SourceColorTest::RunTest(const FString&)
{
    if (GUsingNullRHI) { AddError(TEXT("酉鸡颜色必须以真实RHI验证")); return false; }
    const TCHAR* Names[] = {TEXT("GoldenPeck"), TEXT("DawnFan"), TEXT("MorningWave"), TEXT("DawnDomain"),
        TEXT("SwiftStep"), TEXT("FeatherWard"), TEXT("SonicBreak"), TEXT("MorningBeacon")};
    bool bPassed = true;
    for (int32 Index = 0; Index < 8; ++Index)
    for (const int32 Size : {48, 64})
    {
        const FString Path = FString::Printf(TEXT("/DBAHeroPack_Rooster/UI/%s/Textures/T_DBA_Rooster_V6_%02d_%s%s"),
            Size == 48 ? TEXT("Abilities/V6") : TEXT("Review/V6"), Index + 1, Names[Index], Size == 48 ? TEXT("") : TEXT("_64"));
        UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, *Path);
        if (!TestNotNull(TEXT("酉鸡真实原尺寸纹理"), Texture)) { bPassed = false; continue; }
        FImage Source;
        if (!TestTrue(TEXT("酉鸡导入源像素与原生尺寸正确"), Texture->Source.GetMipImage(Source, 0)
            && Source.SizeX == Size && Source.SizeY == Size && Source.Format == ERawImageFormat::BGRA8)) { bPassed = false; continue; }
        for (const FVector2f UV : {FVector2f(.3f, .4f), FVector2f(.5f, .5f), FVector2f(.7f, .6f)})
        {
            FLinearColor Actual;
            if (!TestTrue(TEXT("酉鸡真实GPU回读"), DivineBeastsUIColorTests::RenderLinear(Texture, UV, Actual))) { bPassed = false; continue; }
            const auto Expected = DivineBeastsUIColorTests::SampleDesign(Source, UV);
            bPassed &= TestTrue(*FString::Printf(TEXT("%s RGB误差≤0.008 actual=%s expected=%s"), *Path, *Actual.ToString(), *Expected.ToString()),
                FMath::Abs(Actual.R - Expected.R) <= .008f && FMath::Abs(Actual.G - Expected.G) <= .008f && FMath::Abs(Actual.B - Expected.B) <= .008f);
        }
    }
    bPassed &= TestTrue(TEXT("酉鸡真实审核Widget原生RHI导出"), DivineBeastsUIColorTests::SavePagePreview(
        TEXT("/DBAHeroPack_Rooster/UI/Review/WBP_DBA_Rooster_SkillIconReviewV6.WBP_DBA_Rooster_SkillIconReviewV6_C"),
        TEXT("RoosterV6_Review64_Runtime48.png"), FIntPoint(1064, 274), *this, false));
    return bPassed;
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
/** 验证已保存蓝图消费状态通知、钳制游标和清空旧来源；只使用瞬态只读视图数据。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsSkillBarEventProjectionTest,
    "DivineBeasts.UI.SkillBar.EventProjection",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsSkillBarEventProjectionTest::RunTest(const FString&)
{
    UClass* Class = LoadClass<UDivineBeastsPlayerStatusPanel>(nullptr,
        TEXT("/DBAUIPack_Core/UI/Combat/WBP_DBA_UI_SkillBarMomentum.WBP_DBA_UI_SkillBarMomentum_C"));
    if (!TestNotNull(TEXT("气势控件从已保存资产加载"), Class)) return false;
    TStrongObjectPtr<UDivineBeastsPlayerStatusPanel> Panel(
        NewObject<UDivineBeastsPlayerStatusPanel>(GetTransientPackage(), Class));
    if (!TestTrue(TEXT("气势控件初始化"), Panel->Initialize())) return false;
    UImage* Bubble = Cast<UImage>(Panel->GetWidgetFromName(TEXT("MomentumBubble")));
    if (!TestNotNull(TEXT("真实气势游标绑定存在"), Bubble)) return false;
    TestEqual(TEXT("未知初始状态隐藏游标"), Bubble->GetRenderOpacity(), 0.f);
    FDivineBeastsPlayerStatusViewData State;
    State.MaxMomentum = 100.;
    for (const double Value : {0., 50., 100., 150., -20.})
    {
        State.Momentum = Value;
        Panel->ApplyStatus(State);
        const double Expected = FMath::Clamp(Value / State.MaxMomentum, 0., 1.) * 527.;
        TestTrue(TEXT("真实蓝图事件将游标投影到轨道并钳制边界"),
            FMath::IsNearlyEqual(Bubble->GetRenderTransform().Translation.X, Expected, .001));
        TestEqual(TEXT("有效状态显示游标"), Bubble->GetRenderOpacity(), 1.f);
    }
    // 同一通知重复投递不漂移；无Pawn的来源刷新必须清空旧显示，恢复快照可以重新显现。
    Panel->ApplyStatus(State);
    TestEqual(TEXT("重复快照不累加位移"), Bubble->GetRenderTransform().Translation.X, 0.);
    Panel->RefreshStatusSourceFromOwningPawn();
    TestEqual(TEXT("旧来源清空后隐藏游标"), Bubble->GetRenderOpacity(), 0.f);
    State.Momentum = 50.;
    Panel->ApplyStatus(State);
    TestTrue(TEXT("恢复快照立即投影到中点"),
        FMath::IsNearlyEqual(Bubble->GetRenderTransform().Translation.X, 263.5, .001));
    State.MaxMomentum = 0.;
    Panel->ApplyStatus(State);
    TestEqual(TEXT("非法容量不能留下可见游标"), Bubble->GetRenderOpacity(), 0.f);
    return true;
}

/** 已批准技能栏资产在三种真实RHI画布上检查底边；无账号、Pawn或虚构技能授权。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsSkillBarRHIReportTest,
    "DivineBeasts.UI.SkillBar.RHIBottomFlush",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsSkillBarRHIReportTest::RunTest(const FString&)
{
    if (GUsingNullRHI) { AddError(TEXT("技能栏验收必须使用真实RHI。")); return false; }
    bool bPassed = true;
    for (const FIntPoint Size : {FIntPoint(1280, 720), FIntPoint(1920, 1080), FIntPoint(2560, 1440)})
    {
        const FString Name = FString::Printf(TEXT("SkillBar/CombatHUD_%dx%d.png"), Size.X, Size.Y);
        bPassed &= TestTrue(TEXT("技能栏底部贴齐且线性浮点RHI导出成功"), DivineBeastsUIColorTests::SavePagePreview(
            TEXT("/DBAUIPack_Core/UI/Combat/WBP_DBA_UI_CombatHUD.WBP_DBA_UI_CombatHUD_C"),
            *Name, Size, *this, false, true));
    }
    return bPassed;
}

/** 实际合成HUD中的3像素施法前景，防止Alpha=0导致数值/曲线测试通过而屏幕完全不可见。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsCastBarRHIProgressTest,
    "DivineBeasts.UI.SkillBar.RHICastProgress",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsCastBarRHIProgressTest::RunTest(const FString&)
{
    if (GUsingNullRHI) { AddError(TEXT("施法前景必须使用真实RHI验证")); return false; }
    UClass* Class = LoadClass<UUserWidget>(nullptr,
        TEXT("/DBAUIPack_Core/UI/Combat/WBP_DBA_UI_CombatHUD.WBP_DBA_UI_CombatHUD_C"));
    if (!TestNotNull(TEXT("实际HUD类"), Class)) return false;
    TStrongObjectPtr<UUserWidget> HUD(NewObject<UUserWidget>(GetTransientPackage(), Class));
    if (!HUD->Initialize()) return false;
    auto* CastBar = Cast<UGamePlatformCastProgressWidget>(HUD->GetWidgetFromName(TEXT("CastBar")));
    auto* Progress = CastBar ? Cast<UProgressBar>(CastBar->GetWidgetFromName(TEXT("CastProgressBar"))) : nullptr;
    if (!TestNotNull(TEXT("HUD内实际施法前景"), Progress)) return false;
    TestTrue(TEXT("原稿前景保持白色不透明乘色"), Progress->GetFillColorAndOpacity().Equals(FLinearColor::White));
    ICommonInputModule::GetSettings().LoadData();
    const auto SlateWidget = HUD->TakeWidget();
    constexpr int32 Width = 1920, Height = 1080;
    TStrongObjectPtr<UTextureRenderTarget2D> Target(NewObject<UTextureRenderTarget2D>());
    Target->ClearColor = FLinearColor::Transparent;
    Target->InitCustomFormat(Width, Height, PF_FloatRGBA, true);
    Target->UpdateResourceImmediate(true);
    auto* Renderer = new FWidgetRenderer(false, true);
    FGamePlatformUICastProgressState State;
    State.SourceScopeId = FGuid::NewGuid(); State.CastInstanceId = TEXT("RHI.ConfirmedCast");
    State.TotalSeconds = 2.; State.RemainingSeconds = 2.; State.bActive = true; State.Revision = 1;
    CastBar->BindCastSource(State.SourceScopeId);
    CastBar->ApplyCastSnapshot(State);
    const auto Draw = [&](TArray<FFloat16Color>& Pixels)
    {
        Renderer->DrawWidget(Target.Get(), SlateWidget, FVector2D(Width, Height), 0.f);
        FlushRenderingCommands();
        return Target->GameThread_GetRenderTargetResource()->ReadFloat16Pixels(Pixels)
            && Pixels.Num() == Width * Height;
    };
    TArray<FFloat16Color> Baseline;
    if (!Draw(Baseline)) { BeginCleanup(Renderer); return false; }
    if (GShaderCompilingManager) GShaderCompilingManager->FinishAllCompilation();
    if (!Draw(Baseline)) { BeginCleanup(Renderer); return false; }
    const FGeometry Geometry = Progress->GetCachedGeometry();
    const FVector2D Origin = Geometry.LocalToAbsolute(FVector2D::ZeroVector);
    const FVector2D Extent = Geometry.GetLocalSize();
    TestTrue(TEXT("施法前景保持554×3实际尺寸"), Extent.Equals(FVector2D(554., 3.), .01));
    bool bPassed = true;
    for (const float Fraction : { .25f, .75f })
    {
        State.RemainingSeconds = State.TotalSeconds * (1. - Fraction); ++State.Revision;
        CastBar->ApplyCastSnapshot(State);
        TArray<FFloat16Color> Pixels;
        if (!Draw(Pixels)) { bPassed = false; break; }
        int32 VisibleFill = 0;
        const int32 Y = FMath::Clamp(FMath::FloorToInt(Origin.Y + Extent.Y * .5), 0, Height - 1);
        for (int32 LocalX = 0; LocalX < FMath::FloorToInt(Extent.X); ++LocalX)
        {
            const int32 X = FMath::Clamp(FMath::FloorToInt(Origin.X + LocalX + .5), 0, Width - 1);
            const auto Before = Baseline[Y * Width + X].GetFloats();
            const auto After = Pixels[Y * Width + X].GetFloats();
            if ((After.G + After.B) - (Before.G + Before.B) > .1f) ++VisibleFill;
        }
        bPassed &= TestTrue(*FString::Printf(TEXT("施法%.0f%%的真实可见前景宽度%d"), Fraction * 100, VisibleFill),
            FMath::Abs(VisibleFill - Extent.X * Fraction) < 12.);
        const FString Path = FPaths::ProjectSavedDir() / TEXT("Validation/UIColorRHI/SkillBar") /
            FString::Printf(TEXT("CastProgress_%02d.png"), FMath::RoundToInt(Fraction * 100));
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
        TUniquePtr<FArchive> Writer(IFileManager::Get().CreateFileWriter(*Path));
        bPassed &= Writer && FImageUtils::ExportRenderTarget2DAsPNG(Target.Get(), *Writer);
        if (Writer) Writer->Close();
    }
    State.bActive = false; State.RemainingSeconds = 0.; ++State.Revision;
    CastBar->ApplyCastSnapshot(State);
    TestEqual(TEXT("真实终态隐藏施法条"), CastBar->GetVisibility(), ESlateVisibility::Collapsed);
    CastBar->ClearCastSource();
    BeginCleanup(Renderer);
    return bPassed;
}
#endif
