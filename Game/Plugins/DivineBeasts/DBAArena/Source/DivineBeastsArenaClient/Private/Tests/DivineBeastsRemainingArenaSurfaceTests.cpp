// SSLM竞技剩余两页的真实资产回归；只验证客户端显示合同，不伪造英雄授权、服务器比分或比赛结果。
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Blueprint/UserWidget.h"
#include "UI/Screens/DivineBeastsArenaScreenBase.h"
#include "UI/DivineBeastsArenaUIScreenCatalog.h"
#include "Components/Button.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/Package.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsRemainingArenaSurfaceTest,
    "DivineBeasts.Arena.UI.RemainingSurfaces",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDivineBeastsRemainingArenaSurfaceTest::RunTest(const FString&)
{
    for(const TCHAR* Name:{TEXT("HeroSelection"),TEXT("Scoreboard")})
    {
        const FString Package=FString(TEXT("/DBAArena/UI/Screens/WBP_DBA_UI_Arena_"))+Name;
        const FString Object=Package+TEXT(".WBP_DBA_UI_Arena_")+Name+TEXT("_C");
        UClass* Class=LoadClass<UUserWidget>(nullptr,*Object);
        if(!TestNotNull(TEXT("目录中的真实竞技蓝图"),Class))continue;
        TestTrue(TEXT("继承竞技组合而非公共世界页面"),Class->IsChildOf(UDivineBeastsArenaScreenBase::StaticClass()));
        TStrongObjectPtr<UUserWidget> Widget(NewObject<UUserWidget>(GetTransientPackage(),Class));
        Widget->Initialize();
        for(const TCHAR* Required:{TEXT("RosterText"),TEXT("ResultsList"),TEXT("CloseButton")})
            TestNotNull(FString::Printf(TEXT("竞技命名控件 %s"),Required),Widget->GetWidgetFromName(Required));
        // 从实际注册目录读取焦点，不能测试里自行指定一个存在的名字而掩盖目录/资产接线不一致。
        const FName SurfaceId(FCString::Strcmp(Name,TEXT("HeroSelection"))==0?TEXT("UI.Screen.ArenaHeroSelection"):TEXT("UI.Screen.Scoreboard"));
        const auto* Surface=FDivineBeastsArenaUIScreenCatalog::Find(SurfaceId);
        if(TestNotNull(TEXT("实际竞技页面目录"),Surface))
        {
            auto* Focus=Cast<UButton>(Widget->GetWidgetFromName(Surface->DefaultFocusWidgetName));
            if(TestNotNull(TEXT("初始焦点必须对应真实始终可用按钮"),Focus))
                TestTrue(TEXT("焦点可用且可聚焦"),Focus->GetIsEnabled()&&Focus->GetIsFocusable());
        }
    }
    return true;
}
#endif
