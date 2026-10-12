#pragma once

#include "CoreMinimal.h"

/**
 * 平台PCG金标准地图原生创作/独立重开内部入口，仅UE5.8编辑器模块游戏线程调用。
 * 创作/修复只写固定Gold开发资源，不执行PCG Generate/Cook；预览只拥有临时生成资源。
 */
namespace GamePlatformPCGGoldMap
{
    /**
     * 校验调用方显式提供的已注册包目录，无项目默认值；不访问或写入资产。
     * 接受首尾空白、正反分隔符及单个可选尾斜杠；拒绝空值、非法包名与未挂载根。
     * 成功输出唯一尾斜杠并清空Error；失败清空OutBlueprintRoot并返回中文原因。
     */
    bool NormalizeBlueprintRoot(const FString& BlueprintRoot, FString& OutBlueprintRoot, FString& Error);
    /** 显式目录提供11项放置器蓝图；预检全部依赖成功后创建固定Gold地图，已有地图绝不覆盖。 */
    bool Create(const FString& BlueprintRoot, FString& Error);
    /** 显式目录校验已有GoldLevel的全部Actor后备份并原位修复非碰撞生成空间；失败原因写入Error。 */
    bool Repair(const FString& BlueprintRoot, FString& Error);
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
