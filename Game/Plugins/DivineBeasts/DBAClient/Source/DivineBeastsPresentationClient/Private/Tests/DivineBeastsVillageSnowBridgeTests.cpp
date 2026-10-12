// 神兽联盟Village客户端雪层桥的轻量原生自动化：只验证纯数值、世界身份及软引用配置。
// 不创建地图、加载Texture2D或假定网络权威；材质MID还须真实UE地图及Client Cook验收。
#include "Misc/AutomationTest.h"
#include "World/DivineBeastsVillageSnowWorldSubsystem.h"
#include "Settings/DivineBeastsVillageSnowSettings.h"
#include "Types/GamePlatformWeatherTypes.h"
#include <limits>

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDivineBeastsVillageSnowBridgeContractTest,
    "DivineBeasts.Presentation.VillageSnow.ClientBridgeContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDivineBeastsVillageSnowBridgeContractTest::RunTest(const FString& Parameters)
{
    (void)Parameters;

    // 真Village与PIE前缀路径必须识别；外部地图即使重名也不得启用项目雪资源。
    TestTrue(TEXT("Village正式世界应匹配"),
        UDivineBeastsVillageSnowWorldSubsystem::IsVillageWorldPackage(
            TEXT("/DBAWorldPack_Village/Maps/L_Village_Start")));
    TestTrue(TEXT("Village PIE前缀应匹配"),
        UDivineBeastsVillageSnowWorldSubsystem::IsVillageWorldPackage(
            TEXT("/DBAWorldPack_Village/Maps/UEDPIE_0_L_Village_Start")));
    TestFalse(TEXT("非项目地图不得匹配"),
        UDivineBeastsVillageSnowWorldSubsystem::IsVillageWorldPackage(
            TEXT("/Game/Development/Snow/Maps/L_Village_Start")));
    TestFalse(TEXT("旧Village子关卡不可误匹配"),
        UDivineBeastsVillageSnowWorldSubsystem::IsVillageWorldPackage(
            TEXT("/DBAWorldPack_Village/Maps/L_Village_Start_Test")));

    // 复用平台WeatherRuntime数值合同；非法NaN/过大雪量不能写到客户端MID。
    float Snow = -1.f;
    float Wetness = -1.f;
    FGamePlatformWeatherState LightSnow;
    LightSnow.Type = EGamePlatformWeatherType::LightSnow;
    LightSnow.SnowAmount = .23f;
    TestTrue(TEXT("小雪合法状态可转换"),
        UDivineBeastsVillageSnowWorldSubsystem::MakeClampedVisualState(
            LightSnow, Snow, Wetness));
    TestTrue(TEXT("小雪强度按服务器事实保留"),
        FMath::IsNearlyEqual(Snow, .23f));
    TestTrue(TEXT("未下雨的地表湿润为零"), FMath::IsNearlyZero(Wetness));

    FGamePlatformWeatherState Invalid = LightSnow;
    Invalid.SnowAmount = std::numeric_limits<float>::quiet_NaN();
    TestFalse(TEXT("NaN天气值必须拒绝"),
        UDivineBeastsVillageSnowWorldSubsystem::MakeClampedVisualState(
            Invalid, Snow, Wetness));

    const auto* Config = GetDefault<UDivineBeastsVillageSnowSettings>();
    TestNotNull(TEXT("项目客户端应包含Village雪资源配置"), Config);
    if (Config)
    {
        TestTrue(TEXT("宏观雪纹理使用合法软对象路径"), Config->SnowCoverageTexture.IsValid());
        TestTrue(TEXT("雪色贴图只存软引用"), Config->SnowAlbedoTexture.IsValid());
        TestTrue(TEXT("雪法线贴图只存软引用"), Config->SnowNormalTexture.IsValid());
        TestTrue(TEXT("Village苔藓材质通过项目第三层软引用配置"),
            Config->MossSnowRockMaterial.IsValid());
        TestTrue(TEXT("苔藓组件标记不为空"),
            !Config->MossSnowComponentTag.IsNone());
        TestTrue(TEXT("标签式岩石覆盖默认开启，但未标记网格保持原材质"),
            Config->bEnableTaggedMossSnowRocks);
    }
    return true;
}

#endif
