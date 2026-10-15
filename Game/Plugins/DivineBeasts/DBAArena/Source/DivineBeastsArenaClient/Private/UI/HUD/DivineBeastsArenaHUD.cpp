// 项目竞技HUD消费MOBA复制投影；缺少淘汰/生命合同不通过死亡次数或0血条伪造。阶段时间仅显示。
#include "UI/HUD/DivineBeastsArenaHUD.h"
#include "ViewModels/GamePlatformArenaViewModel.h"
#include "Framework/GamePlatformArenaPlayerState.h"
#include "Catalog/DivineBeastsArenaModeCatalog.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"
#include "CommonNumericTextBlock.h"

void UDivineBeastsArenaHUD::RefreshInitialState()
{
    Super::RefreshInitialState();const auto* VM=GetArenaViewModel();
    auto Text=[this](const FString& Name,const FText& Value,bool bShow=true)
    {if(auto* T=Cast<UTextBlock>(GetWidgetFromName(FName(*Name)))){T->SetText(Value);T->SetVisibility(bShow?ESlateVisibility::HitTestInvisible:ESlateVisibility::Collapsed);}};
    if(!VM)return;
    const auto* Mode=FDivineBeastsArenaModeCatalog::Find(VM->ArenaModeId);
    Text(TEXT("ModeText"),FText::FromString(Mode?FString::Printf(TEXT("%d 对 %d · 竞技场"),Mode->TeamSize,Mode->TeamSize):TEXT("竞技状态待同步")));
    const auto* Self=GetOwningPlayerState<AGamePlatformArenaPlayerState>();const FName MyTeam=Self?Self->TeamId:NAME_None;
    int32 Allied=0,Enemy=0;
    for(const auto& Row:VM->Scoreboard)
    {
        if(MyTeam.IsNone())break;
        const bool bAlly=Row.TeamId==MyTeam;int32& Index=bAlly?Allied:Enemy;if(Index>=5)continue;++Index;
        Text(FString::Printf(TEXT("%s%d"),bAlly?TEXT("AllyRow"):TEXT("EnemyRow"),Index),FText::FromString(FString::Printf(TEXT("%s · %d / %d / %d"),*Row.PlayerId,Row.Kills,Row.Deaths,Row.Assists)));
    }
    for(int32 Index=Allied+1;Index<=5;++Index)Text(FString::Printf(TEXT("AllyRow%d"),Index),FText(),false);
    for(int32 Index=Enemy+1;Index<=5;++Index)Text(FString::Printf(TEXT("EnemyRow%d"),Index),FText(),false);
    Text(TEXT("EliminationStateText"),FText::FromString(TEXT("生命/状态由公共战斗HUD展示；淘汰事实尚未接入，死亡次数不等于淘汰")));
    if(auto* Counter=Cast<UCommonNumericTextBlock>(GetWidgetFromName(TEXT("MatchTimeText"))))
    {
        // 复制阶段通知设置一次线性视觉端点；动画不查询VM，也不据显示0开始/结束比赛。
        const float Remaining=static_cast<float>(FMath::Max(0.0,VM->RemainingPhaseSeconds));
        Counter->EaseOutInterpolationExponent=1.0f;Counter->SetNumericType(ECommonNumericType::Seconds);Counter->SetCurrentValue(Remaining);
        if(Remaining>0)Counter->InterpolateToValue(0,Remaining,0);
    }
    else Text(TEXT("MatchTimeText"),FText::AsTimespan(FTimespan::FromSeconds(FMath::Max(0.0,VM->RemainingPhaseSeconds))));
}
void UDivineBeastsArenaHUD::OnViewModelStateChanged(int32 Revision,int32 Generation)
{Super::OnViewModelStateChanged(Revision,Generation);RefreshInitialState();}
void UDivineBeastsArenaHUD::NativeDestruct()
{if(auto* Counter=Cast<UCommonNumericTextBlock>(GetWidgetFromName(TEXT("MatchTimeText"))))Counter->SetCurrentValue(0);Super::NativeDestruct();}
