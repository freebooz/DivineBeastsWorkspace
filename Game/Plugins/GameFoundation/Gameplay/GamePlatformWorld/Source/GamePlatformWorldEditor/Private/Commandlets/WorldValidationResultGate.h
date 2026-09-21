#pragma once

#include "EditorValidatorSubsystem.h"
#include "Logging/TokenizedMessage.h"
#include "Validation/GamePlatformWorldDefinitionValidator.h"

namespace GamePlatformWorldValidation
{
/** 本次扫描的非空集合必须全部执行且通过World验证器；统计与逐对象证据必须一致。
 * 仅编辑器游戏线程使用。警告计数单独报告，不把警告本身等价为Invalid。
 * 返回false涵盖失败、跳过、未验证、截断、缺失专用验证器及集合身份不匹配。
 */
inline bool PassesResultGate(const FValidateAssetsResults& Results, const TArray<FString>& SelectedObjectPaths)
{
    const int32 Count = SelectedObjectPaths.Num();
    if (Count <= 0 || Results.NumRequested != Count || Results.NumChecked != Count || Results.NumValid != Count
        || Results.NumInvalid != 0 || Results.NumSkipped != 0 || Results.NumUnableToValidate != 0
        || Results.NumExternalObjects != 0 || Results.NumWarnings < 0 || Results.bAssetLimitReached
        || Results.AssetsDetails.Num() != Count)
    {
        return false;
    }
    const auto* Statistics = Results.ValidatorStatistics.Find(UGamePlatformWorldDefinitionValidator::StaticClass()->GetClassPathName());
    if (!Statistics || Statistics->AssetsValidated != Count) { return false; }
    TSet<FString> UniquePaths;
    for (const FString& Path : SelectedObjectPaths)
    {
        if (Path.IsEmpty() || UniquePaths.Contains(Path)) { return false; }
        UniquePaths.Add(Path);
        const auto* Details = Results.AssetsDetails.Find(Path);
        if (!Details || Details->Result != EDataValidationResult::Valid || !Details->ValidationErrors.IsEmpty()) { return false; }
    }
    for (const auto& Message : Results.ValidatorMessages)
    {
        if (Message->GetSeverity() <= EMessageSeverity::Error) { return false; }
    }
    return true;
}
}
