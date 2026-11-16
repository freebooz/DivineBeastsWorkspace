// SSLM editor automation adapter. Formatting remains owned by GlooPrint.
#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "GlooPrintProjectBatchLibrary.generated.h"

/** 编辑器批量入口：只复用GlooPrint算法，不参与游戏运行。 */
UCLASS()
class UGlooPrintProjectBatchLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    /** 输出必须在当前工程Saved内；逐资产备份、校验并保存。DryRun只清点，不写资产。 */
    UFUNCTION(BlueprintCallable, Category="GlooPrint|Editor")
    static FString StartProjectBatch(const FString& OutputDirectory, bool bDryRun=true, int32 MaxAssets=0);

    /** 新UE进程只读核对磁盘哈希、逻辑和布局；不要求Slate或修改资产。 */
    UFUNCTION(BlueprintCallable, Category="GlooPrint|Editor")
    static FString AuditSavedBatch(const FString& ReportFile);
};
