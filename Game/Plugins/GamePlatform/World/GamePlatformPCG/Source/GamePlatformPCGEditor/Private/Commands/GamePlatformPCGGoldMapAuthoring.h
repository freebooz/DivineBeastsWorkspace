#pragma once

#include "CoreMinimal.h"

/**
 * 金标准PCG地图原生创作/独立重开入口。
 * 仅UE5.8编辑器模块调用，不修改正式Village地图；不执行PCG Generate/Cook。
 */
namespace GamePlatformPCGGoldMap
{
    bool Create(FString& Error);
    /** 只对已生成的唯一GoldLevel修复非碰撞生成空间；校验全部Actor后备份并原位保存。 */
    bool Repair(FString& Error);
    bool Verify(FString& Error);
    /**
     * 从已保存的真实GoldLevel独立加载Editor地图，对固定样例进行二维空间遮罩/优先级探针。
     * 仅验证Mask数值输入与确定性；不启动PCG Generate、不修改地图、不代替G01～G16或Cook。
     */
    bool ProbeSpatial(FString& Error);
    /**
     * 仅在独立UE编辑器命令行对GoldLevel的3份批准网格图调用官方PCG生成，并核对管理实例。
     * 带有界等待、只针对新建Managed Resource的立即清理；从不SaveMap、Cook或改变权威状态。
     */
    bool PreviewGenerated(FString& Error);
}
