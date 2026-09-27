# GamePlatformCore（游戏平台核心）API说明

> 只列稳定 Public（公开）契约。Private（私有）解析算法和 ResultPolicy（结果策略）不属于跨插件 API。

## 1. FGamePlatformId（平台逻辑身份）

头文件：`Types/GamePlatformId.h`。

主要接口：`IsValid()`、`ToString()`、`TryParse()`、`TryCreate()`、比较与 `GetTypeHash()`。

`TryCreate(Namespace, Name, LogicalVersion, OutId)` 适合从分离字段安全构造；失败时 OutId 恢复默认无效值。

## 2. FGamePlatformErrorCode（平台结构化错误码）

头文件：`Types/GamePlatformErrorCode.h`。

主要接口：`IsValid()`、`ToString()`、`ToName()`、`TryParse()`、`TryCreate()`、比较与 `GetTypeHash()`。

示例：

```cpp
FGamePlatformErrorCode ErrorCode;
if (FGamePlatformErrorCode::TryCreate(
        TEXT("Data.Asset"),
        TEXT("Missing_Definition"),
        ErrorCode))
{
    // 规范值：data.asset.missing_definition
}
```

## 3. FGamePlatformVersion（平台版本）

头文件：`Types/GamePlatformVersion.h`。主要接口：`IsValid()`、`ToString()`、`TryParse()`、`Compare()`。Compare 只比较数值，不判断兼容。

## 4. FGamePlatformVersionRange（平台版本兼容区间）

头文件：`Types/GamePlatformVersionRange.h`。

主要接口：`Inclusive(Minimum, Maximum)`、`IsValid()`、`Contains(Candidate)`、`ToString()`。

默认构造的 Range（区间）未配置，`Contains` 返回 false。

## 5. FGamePlatformResult（平台统一结果）

原有兼容接口保持：`Success()`、`Failure(FName,FString)`、`Cancelled(FString)`、`Unsupported(FName,FString)`、`IsSuccess()`。

新增结构化错误码接口：

```cpp
static FGamePlatformResult Failure(
    const FGamePlatformErrorCode& ErrorCode,
    FString Message);

static FGamePlatformResult Unsupported(
    const FGamePlatformErrorCode& ErrorCode,
    FString Message);

bool TryGetStructuredCode(
    FGamePlatformErrorCode& OutErrorCode) const;
```

既有裸错误码调用继续可用；只有符合 `domain.code` 的 Code 才能通过 `TryGetStructuredCode` 解析。

## 6. LogGamePlatformCore（平台核心日志分类）

头文件：`GamePlatformCore.h`。调用方不得记录 Token（令牌）、Secret（密钥）、密码或未脱敏玩家输入。