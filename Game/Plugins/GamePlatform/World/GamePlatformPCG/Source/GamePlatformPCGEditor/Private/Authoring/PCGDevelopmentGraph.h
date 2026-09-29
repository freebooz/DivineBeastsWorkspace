#pragma once
#include "CoreMinimal.h"
class UPCGGraph;
class UStaticMesh;

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
}
