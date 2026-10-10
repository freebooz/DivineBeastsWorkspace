#pragma once
#include "CoreMinimal.h"

class UPCGBasePointData;
class UPCGMetadata;

/**
 * PCG网格集合元数据操作（仅模块内部使用）。
 * 将稳定MeshSetId显式写入每个点；不负责资源加载或实际网格生成。
 * 失败时不写入生成结果，由上层按Fail-Safe规则丢弃输出。
 */
namespace GamePlatformPCGNodeMetadata
{
    bool AssignMeshSetId(UPCGBasePointData& PointData, FName MeshSetId);
    /**
     * 验证核心必需字段、显式必需字段、Schema注册身份和UE5.8真实Metadata类型。
     * 未经证明可稳定写入的Guid字段不在M0/M1通过名单中；失败返回false并由PCG图丢弃输入。
     */
    bool HasAllRequiredAttributes(const UPCGMetadata& Metadata, const TArray<FName>& ExplicitRequired);
}
