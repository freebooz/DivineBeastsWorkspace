// SSLM项目Editor适配：把可信编目值转换到唯一平台Review合同；Client不包含此桥的Editor分支。
#pragma once
#include "CoreMinimal.h"
#if WITH_EDITOR
class UDivineBeastsDemoCatalogSubsystem;
namespace DivineBeasts::Demo
{
    /** 仅创建/绑定NotRun待审，不签收；原子返回本次对象集合，旧合同/缺真实索引失败保留输出为空。 */
    bool CreatePendingReviews(UDivineBeastsDemoCatalogSubsystem& Catalog, FName ScenarioId,
        const FGuid& RunId, const FString& IndexPath, TArray<TObjectPtr<UObject>>& OutReports, FString& OutError);
}
#endif
