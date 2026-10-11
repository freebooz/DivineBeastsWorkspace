// 神兽联盟客户端环境飘落特效的UE编辑器专用真实Niagara验证。
// 第三层只在Editor测试中检查Niagara的保存资产与CPU脚本编译状态，不实现播放器或服务器逻辑。
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/App.h" // NullRHI（无图形设备）时FApp::CanEverRender为false，不能将GPU未初始化误判为Niagara资源损坏。
#include "NiagaraSystem.h"
#include "NiagaraScript.h"
#include "NiagaraCommon.h"
#include "NiagaraEffectType.h"
#include "UObject/UObjectGlobals.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDivineBeastsFallingFoliageNiagaraValidationTest,
    "DivineBeasts.Presentation.FallingFoliage.NiagaraAssets",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDivineBeastsFallingFoliageNiagaraValidationTest::RunTest(const FString&)
{
    // 只验证已发布的通用运动模板和四个项目外观变体；不覆盖任何资产。
    const TCHAR* SystemPaths[] = {
        TEXT("/GamePlatformVFX/Environment/FallingFoliage/Niagara/NS_GP_FallingFoliage_Base.NS_GP_FallingFoliage_Base"),
        TEXT("/DBAPresentationPack_Core/VFX/Environment/FallingFoliage/Niagara/NS_DBA_FallingFoliage_Peach.NS_DBA_FallingFoliage_Peach"),
        TEXT("/DBAPresentationPack_Core/VFX/Environment/FallingFoliage/Niagara/NS_DBA_FallingFoliage_Maple.NS_DBA_FallingFoliage_Maple"),
        TEXT("/DBAPresentationPack_Core/VFX/Environment/FallingFoliage/Niagara/NS_DBA_FallingFoliage_Bamboo.NS_DBA_FallingFoliage_Bamboo"),
        TEXT("/DBAPresentationPack_Core/VFX/Environment/FallingFoliage/Niagara/NS_DBA_FallingFoliage_Ginkgo.NS_DBA_FallingFoliage_Ginkgo"),
    };

    for (const TCHAR* Path : SystemPaths)
    {
        UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, Path);
        if (!TestNotNull(*FString::Printf(TEXT("Niagara资产必须真实存在：%s"), Path), System))
        {
            continue;
        }

        // 等待编辑器CPU脚本编译完成，不宣称由NullRHI取得GPU着色器结果。
        System->WaitForCompilationComplete(false, false);
        TestTrue(*FString::Printf(TEXT("Niagara脚本有效：%s"), Path), System->IsValid());
        // UE5.8 NiagaraSystem::IsReadyToRunInternal 在FApp::CanEverRender()==false
        // 时无条件返回false。NullRHI验证CPU脚本；真实GPU就绪须在有图形RHI的命令中核验。
        if (FApp::CanEverRender())
        {
            TestTrue(*FString::Printf(TEXT("Niagara运行数据已就绪：%s"), Path),
                System->IsReadyToRun());
        }
        else
        {
            AddInfo(FString::Printf(TEXT("NullRHI无GPU上下文：%s仅检查CPU脚本；GPU就绪另验。"), Path));
        }
        TestTrue(*FString::Printf(TEXT("Niagara必须有已配置发射器：%s"), Path),
            System->GetEmitterHandles().Num() > 0);
        TestNotNull(*FString::Printf(TEXT("环境性能类型已绑定：%s"), Path), System->GetEffectType());

        const UNiagaraScript* Spawn = System->GetSystemSpawnScript();
        const UNiagaraScript* Update = System->GetSystemUpdateScript();
        TestNotNull(*FString::Printf(TEXT("系统生成脚本：%s"), Path), Spawn);
        TestNotNull(*FString::Printf(TEXT("系统更新脚本：%s"), Path), Update);
        if (Spawn)
        {
            const auto Status = Spawn->GetLastCompileStatus();
            TestTrue(*FString::Printf(TEXT("生成脚本必须成功编译：%s"), Path),
                Status == ENiagaraScriptCompileStatus::NCS_UpToDate ||
                Status == ENiagaraScriptCompileStatus::NCS_UpToDateWithWarnings);
        }
        if (Update)
        {
            const auto Status = Update->GetLastCompileStatus();
            TestTrue(*FString::Printf(TEXT("更新脚本必须成功编译：%s"), Path),
                Status == ENiagaraScriptCompileStatus::NCS_UpToDate ||
                Status == ENiagaraScriptCompileStatus::NCS_UpToDateWithWarnings);
        }
    }
    return !HasAnyErrors();
}

#endif // WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
