#pragma once

#include "CoreMinimal.h"
#include "Validation/GamePlatformValidationTypes.h"

/**
 * FGamePlatformValidationService（平台验证服务）。
 * Editor模块、命令行和DataValidation调用；所有入口在游戏线程串行调用，不支持并发改注册表。
 * 只拥有Editor/CI规则元数据与豁免，不持有Runtime业务状态；输出报告不能代替构建、Cook或人工验收。
 */
class GAMEPLATFORMDEVELOPERTOOLS_API FGamePlatformValidationService
{
public:
    /** 幂等建立内建规则并加载豁免；缺失豁免文件保留警告且不创造豁免。 */
    static void RegisterBuiltInRules();
    /** 模块关闭清空所有机制元数据；调用前必须停止验证任务，返回的引用随之失效。 */
    static void ResetForShutdown();

    /** 返回内建规则借用引用并按需初始化；调用方不得跨模块关闭保存引用。 */
    static const TArray<FGamePlatformValidationRule>& GetRules();
    /** 查找内建/扩展ID，缺失返回nullptr；引用仅在注册表不发生修改时有效。 */
    static const FGamePlatformValidationRule* FindRule(FName RuleId);

    /** 拷贝Provider生成规则；空/重复Provider或规则ID返回false并填写OutError，整批失败不部分注册。 */
    static bool RegisterRuleProvider(
        const TSharedRef<IGamePlatformValidationRuleProvider>& Provider,
        FString& OutError);
    /** 幂等注销本Provider扩展规则；不删除其他模块内建规则。 */
    static void UnregisterRuleProvider(FName ProviderId);
    /** 返回内建与扩展规则值快照，适用于一次验证任务，不暴露容器所有权。 */
    static TArray<FGamePlatformValidationRule> GetAllRulesSnapshot();
    /** 按GateStage与规则/Warning配置判定阻断；未知失败规则保持阻断，Passed/Skipped不阻断。 */
    static bool ShouldBlockResult(const FGamePlatformValidationResult& Result, FName GateStage);

    /** 精确匹配规则与目标并验证Now时刻的审批/过期；不支持模糊路径豁免。 */
    static bool IsAllowlisted(
        FName RuleId,
        const FString& Target,
        const TArray<FGamePlatformValidationAllowlistEntry>& Allowlist,
        const FDateTime& Now);

    /** 从工作空间正式Build/Rules文件重载；读取/结构失败返回false并给中文错误，旧豁免被清空。 */
    static bool ReloadAllowlist(FString& OutError);
    /** 使用当前UTC检查本次目标是否获有效豁免；空目标或过期审批不能默认通过。 */
    static bool IsTargetAllowlisted(FName RuleId, const FString& Target);
    /** 原地标记符合有效豁免的结果；证据保留原规则身份，不伪造未执行检查为实测通过。 */
    static void ApplyAllowlist(TArray<FGamePlatformValidationResult>& Results);
    /** 借用当前豁免列表；重载/关闭后引用失效，调用方需自行复制跨操作使用。 */
    static const TArray<FGamePlatformValidationAllowlistEntry>& GetAllowlist();

    /** 创建UTC时间与随机GUID组合的运行身份，不接受任意目录输入。 */
    static FString CreateRunId();
    /** 返回Saved/Validation正式报告根，不硬编码个人磁盘。 */
    static FString GetReportRoot();
    /** 校验RunId并限定输出于报告根；非法/越界返回false与OutError，不创建越界目录。 */
    static bool ResolveSafeReportDirectory(const FString& RunId, FString& OutDirectory, FString& OutError);
    /** 写入JSON/中文报告到受限目录；IO失败返回false，调用方不能宣称证据已交付。 */
    static bool WriteReports(const FGamePlatformValidationRunSummary& Summary, FString& OutDirectory, FString& OutError);

    /** 识别本项目已取消玩法标签的明确前缀，普通第三方技术词不应仅因同名被拒绝。 */
    static bool IsRemovedLegacyGameplayTag(const FString& TagText);
    /** 为报告屏蔽敏感值；返回可输出文本，不代表源值可持久化或公开。 */
    static FString MaskSensitiveValue(const FString& Value);

private:
    static TArray<FGamePlatformValidationRule> Rules;
    static TMap<FName, TArray<FGamePlatformValidationRule>> ExtensionRules;
    static TArray<FGamePlatformValidationAllowlistEntry> AllowlistEntries;
    static bool bAllowlistLoaded;
};
