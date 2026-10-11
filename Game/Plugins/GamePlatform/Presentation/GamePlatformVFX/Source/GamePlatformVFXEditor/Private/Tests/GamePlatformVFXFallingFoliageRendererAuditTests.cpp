#if WITH_DEV_AUTOMATION_TESTS
// 平台VFX测试：确认真实自然飘落系统的Emitter与Renderer已启用。
// 不依赖神兽联盟专有材质、英雄、世界或界面，避免平台层反向依赖项目。
#include "Misc/AutomationTest.h"
#include "NiagaraSystem.h"
#include "NiagaraEmitter.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraRendererProperties.h"
#include "NiagaraSpriteRendererProperties.h"
#include "UObject/UObjectGlobals.h"

DEFINE_LOG_CATEGORY_STATIC(LogGPFallingFoliageRendererAudit, Log, All);

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformVFXFallingFoliageRendererAuditTest,
    "GamePlatform.VFX.Editor.FallingFoliage.RendererAudit",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformVFXFallingFoliageRendererAuditTest::RunTest(const FString&)
{
    UNiagaraSystem* System = LoadObject<UNiagaraSystem>(
        nullptr,
        TEXT("/GamePlatformVFX/Environment/FallingFoliage/Niagara/NS_GP_FallingFoliage_Base.NS_GP_FallingFoliage_Base"));
    if (!TestNotNull(TEXT("通用飘落系统资源必须存在"), System))
    {
        return false;
    }

    const auto& Handles = System->GetEmitterHandles();
    TestTrue(TEXT("至少存在一个Emitter发射器"), Handles.Num() > 0);

    int32 EnabledEmitters = 0;
    int32 Renderers = 0;
    for (const FNiagaraEmitterHandle& Handle : Handles)
    {
        const FVersionedNiagaraEmitterData* Data = Handle.GetEmitterData();
        if (!TestNotNull(TEXT("Emitter必须指向有效版本数据"), Data))
        {
            continue;
        }
        const bool bEnabled = Handle.GetIsEnabled();
        if (bEnabled)
        {
            ++EnabledEmitters;
        }
        int32 LocalEnabledRenderers = 0;
        for (UNiagaraRendererProperties* Renderer : Data->GetRenderers())
        {
            if (Renderer && Renderer->GetIsEnabled())
            {
                ++LocalEnabledRenderers;
                ++Renderers;
                if (const UNiagaraSpriteRendererProperties* Sprite = Cast<UNiagaraSpriteRendererProperties>(Renderer))
                {
                    // 记录真实Sprite模式用于判断落地后是否仍以竖直平面穿透地形。
                    UE_LOG(LogGPFallingFoliageRendererAudit, Display,
                        TEXT("Sprite Alignment=%d, Facing=%d, Enabled=%d"),
                        static_cast<int32>(Sprite->Alignment),
                        static_cast<int32>(Sprite->FacingMode), bEnabled ? 1 : 0);
                }
            }
        }
        UE_LOG(LogGPFallingFoliageRendererAudit, Display,
            TEXT("Emitter enabled=%d renderers=%d simulation=%d"),
            bEnabled ? 1 : 0, LocalEnabledRenderers, static_cast<int32>(Data->SimTarget));
        TestTrue(TEXT("已启用Emitter必须具有已启用的Renderer"),
            !bEnabled || LocalEnabledRenderers > 0);
    }
    TestTrue(TEXT("至少存在一个已启用的Emitter"), EnabledEmitters > 0);
    TestTrue(TEXT("至少存在一个有效Renderer"), Renderers > 0);
    return !HasAnyErrors();
}
#endif
