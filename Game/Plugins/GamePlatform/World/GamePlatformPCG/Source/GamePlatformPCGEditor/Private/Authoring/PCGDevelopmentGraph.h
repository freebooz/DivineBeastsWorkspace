#pragma once
#include "CoreMinimal.h"
class UPCGGraph;
class UStaticMesh;

namespace GamePlatformPCGEditor
{
    /** 只创建四个原生节点并精确连接Out→In及output.Out；不执行图、不创建世界对象，调用者持有Outer。 */
    UPCGGraph* CreateDevelopmentGraph(UObject* Outer, FName Name, UStaticMesh* Mesh,
        bool bStaticCollision, FString& Error);
}
