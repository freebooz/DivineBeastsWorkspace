#pragma once

#include "UI/Screens/DivineBeastsArenaScreenBase.h"
#include "DivineBeastsMatchmakingScreen.generated.h"

/** UDivineBeastsMatchmakingScreen（神兽联盟竞技匹配页面）：展示1v1～5v5模式并提交匹配意图。 */
UCLASS(Abstract, Blueprintable)
class DIVINEBEASTSARENACLIENT_API UDivineBeastsMatchmakingScreen
    : public UDivineBeastsArenaScreenBase
{
    GENERATED_BODY()
    friend class FDivineBeastsArenaModeSelectionRegressionTest;
    friend class FDivineBeastsArenaChallengeWidgetRegressionTest;

public:
    UDivineBeastsMatchmakingScreen()
    {
        ExpectedSurfaceId = TEXT("UI.Screen.Matchmaking");
    }

protected:
    virtual void BindUIEvents() override;
    virtual void UnbindUIEvents() override;
    virtual void RefreshInitialState() override;

private:
    /** 页签只改变本地呈现；所有状态/资格来自同一本人观察，不因切页重新报名。 */
    bool bChallengeTabSelected=false;
    FString LastDisplayedChallengeId;
    UFUNCTION() void SelectQuickMatchTab();
    UFUNCTION() void SelectChallengeTab();
    UFUNCTION() void SendChallenge();
    UFUNCTION() void AcceptChallenge();
    UFUNCTION() void DeclineChallenge();
    UFUNCTION() void CancelChallenge();
    UFUNCTION() void TargetPlayerIdChanged(const FText& Text);
};
