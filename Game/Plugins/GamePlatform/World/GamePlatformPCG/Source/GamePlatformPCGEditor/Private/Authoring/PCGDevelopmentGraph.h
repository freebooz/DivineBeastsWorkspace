#pragma once
#include "CoreMinimal.h"
class UPCGGraph;
class UStaticMesh;
class UGamePlatformPCGProfileDefinition;
class UGamePlatformPCGMeshSetDefinition;

namespace GamePlatformPCGEditor
{
    /** 只创建四个原生节点并精确连接Out→In及output.Out；不执行图、不创建世界对象，调用者持有Outer。 */
    UPCGGraph* CreateDevelopmentGraph(UObject* Outer, FName Name, UStaticMesh* Mesh,
        bool bStaticCollision, FString& Error);

    /**
     * 创建1.0 Foundation Template（基础模板）合同图；使用真实UPCGGraph/节点/连线并标记bIsTemplate。
     * 仅支持M0/M1批准模板ID，不创建M2+模板，不执行图，不绑定项目资源。
     */
    UPCGGraph* CreateFoundationTemplateGraph(UObject* Outer, FName Name, FName TemplateId, FString& Error);
    /**
     * 为已加载、已租约的项目MeshSet构建可实际生成网格的Graph Instance。
     * 只做编辑器图创作，不执行Generate或同步加载软资源；调用者负责外层资产保存及关联Profile。
     * 初期限定Scatter/Biome/Crop/Assembly/InterfaceBand，不冒充道路多MeshSet选择已实现。
     */
    UPCGGraph* CreateFoundationRealizedGraph(UObject* Outer, FName Name,
        const UGamePlatformPCGProfileDefinition& Profile,
        const UGamePlatformPCGMeshSetDefinition& MeshSet, FString& Error);

    /** 创建M0/M1公共Foundation Subgraph（基础子图）；只使用官方/批准节点，不执行世界生成。 */
    UPCGGraph* CreateFoundationSubgraphGraph(UObject* Outer, FName Name, FName SubgraphId, FString& Error);
}
