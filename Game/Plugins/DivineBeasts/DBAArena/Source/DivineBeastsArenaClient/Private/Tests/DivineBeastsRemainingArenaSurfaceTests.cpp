// SSLM竞技剩余两页的真实资产回归；只验证客户端显示合同，不伪造英雄授权、服务器比分或比赛结果。
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Blueprint/UserWidget.h"
#include "UI/Screens/DivineBeastsArenaScreenBase.h"
#include "UObject/StrongObjectPtr.h"
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
    }
    return true;
}
#endif
