#if WITH_DEV_AUTOMATION_TESTS
// 丑牛V6八图标真实主资产与HUD ViewModel合同：仅编辑器资产验证，不批准技能、不授予GAS权限。
#include "Misc/AutomationTest.h"
#include "Definitions/DivineBeastsAbilityUIProfile.h"
#include "ViewModels/Combat/DivineBeastsAbilityBarViewModel.h"
#include "Engine/Texture2D.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsOxV6ArtProfileNativeTest,
    "DivineBeasts.UI.Combat.OxV6.ArtProfileAndViewModel",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDivineBeastsOxV6ArtProfileNativeTest::RunTest(const FString&)
{
    const TCHAR* ProfilePath =
        TEXT("/DBAHeroPack_Ox/UI/Abilities/DA_DBA_Ox_PlaceholderUIProfile.DA_DBA_Ox_PlaceholderUIProfile");
    const TStrongObjectPtr<UDivineBeastsAbilityUIProfile> Profile(
        LoadObject<UDivineBeastsAbilityUIProfile>(nullptr, ProfilePath));
    if (!TestNotNull(TEXT("从项目牛英雄内容包加载唯一UIProfile原生资产"), Profile.Get()))
        return false;

    TestEqual(TEXT("主资产英雄身份不可变更"), Profile->HeroDefinitionId,
        FName(TEXT("Hero.Zodiac.Ox")));
    TestEqual(TEXT("唯一Profile的PrimaryAssetType正确"),
        Profile->GetPrimaryAssetId().PrimaryAssetType.ToString(),
        FString(TEXT("DivineBeastsAbilityUIProfile")));
    FString Error;
    TestTrue(TEXT("真实原生UIProfile所有字段通过作者门禁"), Profile->ValidateProfile(Error));

    struct FExpectedIcon
    {
        const TCHAR* Id; // V6稳定技能编号，不是占位AbilityId。
        const TCHAR* Name; // 必须与历史V6中文名称完全一致。
        const TCHAR* AssetName; // 牛包中的真正48px UE Texture2D。
    };
    static const FExpectedIcon Expected[] = {
        {TEXT("dba.ability.ox_eight_horn_strike@1"), TEXT("玄角撞"),
            TEXT("T_DBA_Ox_V6_01_XuanJiaoZhuang_Icon")},
        {TEXT("dba.ability.ox_eight_horn_break@1"), TEXT("裂角"),
            TEXT("T_DBA_Ox_V6_02_LieJiao_Icon")},
        {TEXT("dba.ability.ox_eight_ground_pulse@1"), TEXT("裂地踏"),
            TEXT("T_DBA_Ox_V6_03_LieDiTa_Icon")},
        {TEXT("dba.ability.ox_eight_mountain_break@1"), TEXT("不动玄门"),
            TEXT("T_DBA_Ox_V6_04_BuDongXuanMen_Icon")},
        {TEXT("dba.ability.ox_eight_horn_charge@1"), TEXT("沉步"),
            TEXT("T_DBA_Ox_V6_05_ChenBu_Icon")},
        {TEXT("dba.ability.ox_eight_stone_armor@1"), TEXT("镇山护壁"),
            TEXT("T_DBA_Ox_V6_06_ZhenShanHuBi_Icon")},
        {TEXT("dba.ability.ox_eight_seismic_stun@1"), TEXT("顶回"),
            TEXT("T_DBA_Ox_V6_07_DingHui_Icon")},
        {TEXT("dba.ability.ox_eight_steadfast_roar@1"), TEXT("立阵"),
            TEXT("T_DBA_Ox_V6_08_LiZhen_Icon")}
    };
    int32 OldPlaceholderCount = 0;
    for (const auto& Entry : Profile->Entries)
        if (Entry.AbilityId.ToString().StartsWith(TEXT("dba.ability.ox_placeholder_")))
            ++OldPlaceholderCount;
    TestEqual(TEXT("原六项牛英雄占位UI条目完整保留"), OldPlaceholderCount, 6);
    TestEqual(TEXT("6项已有占位与8项V6仅表现条目同属唯一Profile"),
        Profile->Entries.Num(), 14);

    for (const FExpectedIcon& Row : Expected)
    {
        const FName AbilityId(Row.Id);
        const FDivineBeastsAbilityUIEntry* UI = Profile->FindEntry(AbilityId);
        if (!TestNotNull(FString::Printf(TEXT("V6对应真实AbilityId有唯一UI映射：%s"), Row.Id), UI))
            continue;
        TestEqual(TEXT("中文显示名称符合V6设计"), UI->DisplayName.ToString(),
            FString(Row.Name));
        TestFalse(TEXT("真实图标提示说明不得空白"), UI->Description.ToString().TrimStartAndEnd().IsEmpty());
        const FString ExpectedPackage = FString::Printf(
            TEXT("/DBAHeroPack_Ox/UI/Abilities/V6/Textures/%s"), Row.AssetName);
        TestEqual(TEXT("V6技能图标路径属自己的英雄包"),
            UI->Icon.ToSoftObjectPath().GetLongPackageName(), ExpectedPackage);
        const FString ExpectedObject = ExpectedPackage + TEXT(".") + Row.AssetName;
        UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, *ExpectedObject);
        if (!TestNotNull(TEXT("实际UE纹理原生资产类型"), Texture))
            continue;
        // 无RHI的UE5.8编辑器自动化下GetSizeX/Y取的是平台尺寸，可能为0；
        // 这里要求真实导入的TextureSource尺寸。非编辑器编译则只验证运行纹理。
#if WITH_EDITORONLY_DATA
        TestEqual(TEXT("导入纹理源宽度确为48像素"),
            static_cast<int32>(Texture->Source.GetSizeX()), 48);
        TestEqual(TEXT("导入纹理源高度确为48像素"),
            static_cast<int32>(Texture->Source.GetSizeY()), 48);
#else
        TestEqual(TEXT("运行纹理确为48像素宽"), Texture->GetSizeX(), 48);
        TestEqual(TEXT("运行纹理确为48像素高"), Texture->GetSizeY(), 48);
#endif
        TestEqual(TEXT("运行纹理归UI资源分组"), Texture->LODGroup, TEXTUREGROUP_UI);
    }

    // Profile中的8张图只是表现数据；VM没有服务器Loadout/GAS授权时，技能栏不能出现伪槽。
    const TStrongObjectPtr<UDivineBeastsAbilityBarViewModel> VM(
        NewObject<UDivineBeastsAbilityBarViewModel>());
    TestTrue(TEXT("真实技能栏VM可构造"), VM.IsValid());
    if (VM.IsValid())
    {
        TestEqual(TEXT("未绑定权威Loadout时技能栏为空"), VM->GetSlots().Num(), 0);
        TestEqual(TEXT("未绑定权威Loadout时提示状态为空"), VM->GetSlotDetails().Num(), 0);
        TestFalse(TEXT("V6的美术UI数据不能直接获得技能施放权限"),
            VM->RequestActivateSlot(TEXT("Platform.Ability.Input.DivineBeasts.Primary")).IsSuccess());
    }
    return true;
}
#endif
