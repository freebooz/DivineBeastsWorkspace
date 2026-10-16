// SSLM开发电影摄像机演示界面：仅本地游戏视口，不引用正式登录UI或权威逻辑。
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DivineBeastsCameraReviewWidget.generated.h"

class ADivineBeastsCinematicCameraReviewActor;
class UButton;
class UHorizontalBox;
class UTextBlock;
class UVerticalBox;

/** 开发地图中由C++生成真实可点击按钮的轻量UMG界面；按键与按钮共用同一Actor操作。 */
UCLASS()
class DIVINEBEASTSDEMOCLIENT_API UDivineBeastsCameraReviewWidget final : public UUserWidget
{
    GENERATED_BODY()
public:
    /** Widget借用当前开发Actor，不持有服务器/账号/比赛对象。 */
    void BindReviewActor(ADivineBeastsCinematicCameraReviewActor* Actor);

    /** 中文状态消息，只更新已有TextBlock，不进行持续Tick轮询。 */
    void SetCameraStatus(const FString& Message);

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
private:
    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> StatusText;
    TWeakObjectPtr<ADivineBeastsCinematicCameraReviewActor> ReviewActor;

    UHorizontalBox* AddButtonRow(UVerticalBox& Parent);
    UButton* AddButton(UHorizontalBox& Row, FName Name, const FString& Label);

    UFUNCTION() void Choose24();
    UFUNCTION() void Choose35();
    UFUNCTION() void Choose50();
    UFUNCTION() void Choose85();
    UFUNCTION() void ApertureOpen();
    UFUNCTION() void ApertureClose();
    UFUNCTION() void FocusNear();
    UFUNCTION() void FocusFar();
    UFUNCTION() void ResetAll();
};