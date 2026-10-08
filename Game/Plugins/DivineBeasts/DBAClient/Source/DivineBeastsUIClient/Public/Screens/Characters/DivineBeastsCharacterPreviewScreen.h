#pragma once
#include "Screens/DivineBeastsUIScreen.h"
#include "DivineBeastsCharacterPreviewScreen.generated.h"
class UBorder;

/** 项目客户端预览页共同行为：只在Monolith透明预览区域捕获鼠标，旋转通过具体页面ViewModel命令提交。
 * 控件树归UI内容包，手势归本地页面；无Tick、HTTP、权威角色动作或全局输入状态。
 */
UCLASS(Abstract, Blueprintable)
class DIVINEBEASTSUICLIENT_API UDivineBeastsCharacterPreviewScreen : public UDivineBeastsUIScreen
{
    GENERATED_BODY()
protected:
    virtual FReply NativeOnMouseButtonDown(const FGeometry&, const FPointerEvent&) override;
    virtual FReply NativeOnMouseButtonUp(const FGeometry&, const FPointerEvent&) override;
    virtual FReply NativeOnMouseMove(const FGeometry&, const FPointerEvent&) override;
    virtual void NativeOnMouseCaptureLost(const FCaptureLostEvent&) override;
    virtual void NativeOnDeactivated() override;
    /** 具体页重用既有只读ViewModel的本地预览命令；本抽象页不得自行绕过该命令端口。 */
    virtual void RotatePreviewFromDrag(float DeltaYawDegrees) PURE_VIRTUAL(UDivineBeastsCharacterPreviewScreen::RotatePreviewFromDrag, );
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UBorder> PreviewDragSurface;
private:
    /** 只保留捕获身份；所有取消路径清理，不跨页面或世界复用。 */
    bool bPreviewDragging=false;
    uint32 DragUserIndex=0;
    uint32 DragPointerIndex=0;
};
