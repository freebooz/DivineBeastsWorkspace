#include "Interfaces/GamePlatformLandscapeMaterialBridge.h"
#include "LandscapeProxy.h"
#include "LandscapeComponent.h"
#include "LandscapeNaniteComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

bool BindGamePlatformLandscapeNaniteMIDs(ALandscapeProxy& Landscape, int32& OutVerifiedSlots, FString& OutDiagnostic)
{
    check(IsInGameThread());
    OutVerifiedSlots = 0;
    OutDiagnostic.Reset();
    // IsNaniteEnabled只在WITH_EDITOR内声明；客户端以真实烘焙Nanite组件为准。
    if (!Landscape.HasNaniteComponents()) return true;
    TInlineComponentArray<ULandscapeNaniteComponent*> Components(&Landscape);
    if (Components.IsEmpty())
    {
        OutDiagnostic = TEXT("Nanite地形缺少已构建组件");
        return false;
    }
    // 先完整检查，避免数据缺失时只更新部分槽。引用关系由引擎真实烘焙源组件数组给出。
    for (ULandscapeNaniteComponent* Nanite : Components)
    {
        const auto& Sources = Nanite->GetSourceLandscapeComponents();
        if (Sources.IsEmpty() || Nanite->GetNumMaterials() != Sources.Num())
        {
            OutDiagnostic = TEXT("Nanite材质槽与源地形组件合同不一致");
            return false;
        }
        for (const auto& Source : Sources)
        {
            if (!IsValid(Source) || !IsValid(Cast<UMaterialInstanceDynamic>(Source->GetMaterialInstance(0, true))))
            {
                OutDiagnostic = TEXT("源地形MID尚未就绪");
                return false;
            }
        }
    }
    for (ULandscapeNaniteComponent* Nanite : Components)
    {
        const auto& Sources = Nanite->GetSourceLandscapeComponents();
        for (int32 Slot = 0; Slot < Sources.Num(); ++Slot)
        {
            UMaterialInstanceDynamic* Mid = Cast<UMaterialInstanceDynamic>(Sources[Slot]->GetMaterialInstance(0, true));
            if (Nanite->GetMaterial(Slot) != Mid) Nanite->SetMaterial(Slot, Mid);
            if (Nanite->GetMaterial(Slot) != Mid)
            {
                OutDiagnostic = TEXT("Nanite实际材质回读不匹配");
                return false;
            }
            ++OutVerifiedSlots;
        }
    }
    return OutVerifiedSlots > 0;
}
