// 项目客户端预览鼠标输入适配；透明区域由Monolith布局，不拦截两侧选择与底部表单。
#include "Screens/Characters/DivineBeastsCharacterPreviewScreen.h"
#include "Screens/Characters/DivineBeastsPreviewDragState.h"
#include "Components/Border.h"
#include "Input/Reply.h"
#include "InputCoreTypes.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Application/SlateUser.h"

FReply UDivineBeastsCharacterPreviewScreen::NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event)
{
    if (PreviewDragSurface && Event.GetEffectingButton()==EKeys::LeftMouseButton && !Event.IsTouchEvent() &&
        PreviewDragSurface->GetCachedGeometry().IsUnderLocation(Event.GetScreenSpacePosition()))
    {
        bPreviewDragging=true; DragUserIndex=Event.GetUserIndex(); DragPointerIndex=Event.GetPointerIndex();
        return FReply::Handled().CaptureMouse(TakeWidget());
    }
    return Super::NativeOnMouseButtonDown(Geometry,Event);
}
FReply UDivineBeastsCharacterPreviewScreen::NativeOnMouseMove(const FGeometry& Geometry, const FPointerEvent& Event)
{
    FDivineBeastsPreviewDragState State{bPreviewDragging,DragUserIndex,DragPointerIndex};
    if (State.Owns(Event.GetUserIndex(),Event.GetPointerIndex()))
    {
        // 鼠标事件推动本地预览；没有逐帧轮询输入，捕获只限按住左键的当前用户／指针。
        if (!Event.IsMouseButtonDown(EKeys::LeftMouseButton)) { bPreviewDragging=false; return FReply::Handled().ReleaseMouseCapture(); }
        RotatePreviewFromDrag(State.YawForMove(Event.GetUserIndex(),Event.GetPointerIndex(),Event.GetCursorDelta().X));
        return FReply::Handled();
    }
    return Super::NativeOnMouseMove(Geometry,Event);
}
FReply UDivineBeastsCharacterPreviewScreen::NativeOnMouseButtonUp(const FGeometry& Geometry, const FPointerEvent& Event)
{
    if (bPreviewDragging && Event.GetEffectingButton()==EKeys::LeftMouseButton &&
        Event.GetUserIndex()==DragUserIndex && Event.GetPointerIndex()==DragPointerIndex)
    { bPreviewDragging=false; return FReply::Handled().ReleaseMouseCapture(); }
    return Super::NativeOnMouseButtonUp(Geometry,Event);
}
void UDivineBeastsCharacterPreviewScreen::NativeOnMouseCaptureLost(const FCaptureLostEvent& Event)
{
    bPreviewDragging=false; DragUserIndex=0; DragPointerIndex=0;
    Super::NativeOnMouseCaptureLost(Event);
}
void UDivineBeastsCharacterPreviewScreen::NativeOnDeactivated()
{
    // 只释放本页面持有的捕获，不清空别的窗口、本地玩家或新页面的输入所有权。
    if (bPreviewDragging && HasMouseCapture() && FSlateApplication::IsInitialized())
    { if (const TSharedPtr<FSlateUser> User=FSlateApplication::Get().GetUser(DragUserIndex)) { User->ReleaseCapture(DragPointerIndex); } }
    bPreviewDragging=false; DragUserIndex=0; DragPointerIndex=0;
    Super::NativeOnDeactivated();
}
