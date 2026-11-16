#include "GlooPrintManual.h"
#include "GlooPrintSettings.h"
namespace GlooPrintManual {
FDelegateHandle AddSettingsChanged(FSimpleDelegate Handler){check(IsInGameThread());return GetMutableDefault<UGlooPrintSettings>()->OnChanged.Add(MoveTemp(Handler));}
void RemoveSettingsChanged(FDelegateHandle Handle){check(IsInGameThread());GetMutableDefault<UGlooPrintSettings>()->OnChanged.Remove(Handle);}
void ReadSettings(FSettingsSnapshot& V){const auto* S=GetDefault<UGlooPrintSettings>();V.bEnabled=S->bFormattingEnabled;V.WireStyle=uint8(S->WireStyle);V.HorizontalSpacing=S->HorizontalSpacing;V.VerticalSpacing=S->VerticalSpacing;V.CommentPadding=S->CommentPadding;}
bool ApplySettings(const FSettingsSnapshot& V,FString& E){
 check(IsInGameThread());E.Reset();if(V.WireStyle>2||!FMath::IsFinite(V.HorizontalSpacing)||!FMath::IsFinite(V.VerticalSpacing)||!FMath::IsFinite(V.CommentPadding)||V.HorizontalSpacing<16||V.HorizontalSpacing>2048||V.VerticalSpacing<16||V.VerticalSpacing>2048||V.CommentPadding<8||V.CommentPadding>512){E=TEXT("GlooPrint设置超出供应商范围。");return false;}
 auto* S=GetMutableDefault<UGlooPrintSettings>();S->bFormattingEnabled=V.bEnabled;S->WireStyle=EGlooPrintWireStyle(V.WireStyle);S->HorizontalSpacing=V.HorizontalSpacing;S->VerticalSpacing=V.VerticalSpacing;S->CommentPadding=V.CommentPadding;return S->NotifyChanged();
}
bool RestoreSettings(const FSettingsSnapshot& O,const FSettingsSnapshot& Expected,FString& E){FSettingsSnapshot Current;ReadSettings(Current);if(!(Current==Expected)){E=TEXT("GlooPrint设置已有外部后继修改，拒绝覆盖。");return false;}return ApplySettings(O,E);}
}
