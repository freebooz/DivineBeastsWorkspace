#include "Panels/Combat/DivineBeastsCombatPanelBase.h"

void UDivineBeastsCombatPanelBase::NotifyCombatPresentationChanged()
{
    // 仅在真实数据变化后通知UI。循环代次不会代表任何服务器身份。
    PresentationRevision = PresentationRevision >= MAX_int32 ? 1 : PresentationRevision + 1;
    BP_OnCombatPresentationChanged(PresentationRevision);
}
