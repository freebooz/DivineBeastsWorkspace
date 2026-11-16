// SSLM开发目录纯结构验证；项目层，供Data定义校验与目录原子发布前调用。
// 不加载资产、不读取任意文件、不推断插件启用；临时索引仅在本次调用栈内存活。
#include "Catalog/DivineBeastsDemoCatalogValidation.h"
#include "Definitions/DivineBeastsDemoCatalogDefinition.h"
#include "Misc/PackageName.h"
#include <initializer_list>

namespace DivineBeasts::Demo
{
bool IsStableIdentityText(const FString& Text)
{
    if (Text.IsEmpty() || Text.Len() >= NAME_SIZE || Text.Equals(TEXT("None"), ESearchCase::IgnoreCase)) return false;
    const auto IsLetter = [](TCHAR C) { return (C >= TEXT('a') && C <= TEXT('z')) || (C >= TEXT('A') && C <= TEXT('Z')); };
    if (!IsLetter(Text[0])) return false;
    for (TCHAR C : Text)
        if (!IsLetter(C) && !(C >= TEXT('0') && C <= TEXT('9')) && C != TEXT('_') && C != TEXT('.') && C != TEXT('-')) return false;
    return true;
}
bool IsStableAssetPathText(const FString& Text)
{
    if (Text.IsEmpty() || Text.Len() >= 1024 || !Text.StartsWith(TEXT("/")) || Text.StartsWith(TEXT("/Content/")) || Text.Contains(TEXT("//"))) return false;
    int32 Dots = 0;
    for (TCHAR C : Text)
    {
        if (C == TEXT('.')) { ++Dots; continue; }
        if (C != TEXT('/') && C != TEXT('_') && !(C >= TEXT('0') && C <= TEXT('9')) &&
            !(C >= TEXT('A') && C <= TEXT('Z')) && !(C >= TEXT('a') && C <= TEXT('z'))) return false;
    }
    if (Dots != 1) return false;
    FString Package, Object; Text.Split(TEXT("."), &Package, &Object);
    return !Object.IsEmpty() && !Object.Contains(TEXT("/")) && FPackageName::IsValidLongPackageName(Package);
}
namespace
{
/** 稳定身份限制为ASCII标识，避免路径、空白或脚本文本混入命令身份。 */
bool IsIdentity(FName Identity)
{
    return !Identity.IsNone() && IsStableIdentityText(Identity.ToString());
}

/** 明确枚举值而非隐式默认分支；未知名称必须拒绝。 */
bool IsOneOf(FName Value, std::initializer_list<const TCHAR*> Allowed)
{
    for (const TCHAR* Candidate : Allowed) if (Value == FName(Candidate)) return true;
    return false;
}

/** 相对路径只用于责任定位，不允许路径逃逸/盘符/空段；客户端不据此读源码。 */
bool IsRepositoryPath(const FString& Path)
{
    if (Path.IsEmpty() || Path.StartsWith(TEXT("/")) || Path.Contains(TEXT("\\")) || Path.Contains(TEXT(":"))) return false;
    TArray<FString> Segments;
    Path.ParseIntoArray(Segments, TEXT("/"), false);
    for (const FString& Segment : Segments)
    {
        if (Segment.IsEmpty() || Segment == TEXT(".") || Segment == TEXT("..")) return false;
    }
    return true;
}

/** 可空软引用仅做结构检查，不用路径合法冒充资源存在或已加载。 */
bool IsAssetReference(const FSoftObjectPath& Path, bool bAllowNull)
{
    if (Path.IsNull()) return bAllowNull;
    const FString Package = Path.GetLongPackageName();
    return Path.IsValid() && FPackageName::IsValidLongPackageName(Package) && !Package.StartsWith(TEXT("/Content/"));
}

/** 集合内身份必须唯一且有效，避免数组首项选择以及FName大小写冲突。 */
bool AreUniqueIdentities(const TArray<FName>& Values, bool bAllowEmpty)
{
    if (!bAllowEmpty && Values.IsEmpty()) return false;
    TSet<FName> Seen;
    for (FName Value : Values)
    {
        if (!IsIdentity(Value) || Seen.Contains(Value)) return false;
        Seen.Add(Value);
    }
    return true;
}

/** 中文文本列表必须非空且无纯空白项；这些文本是操作说明，不执行。 */
bool HasDescriptions(const TArray<FString>& Values)
{
    if (Values.IsEmpty()) return false;
    for (const FString& Value : Values) if (Value.TrimStartAndEnd().IsEmpty()) return false;
    return true;
}

/** 统一具名拒绝；不记录用户参数，避免诊断泄露敏感输入。 */
FGamePlatformResult Reject(const TCHAR* Code, const FString& Message)
{
    return FGamePlatformResult::Failure(FName(Code), Message);
}

/** 仅构建本次校验的只读指针索引，重复/无效身份立即失败，不发布半目录。 */
template<typename EntryType, typename IdentityGetter>
FGamePlatformResult IndexEntries(const TArray<EntryType>& Entries, IdentityGetter GetIdentity, TMap<FName, const EntryType*>& OutIndex)
{
    for (const EntryType& Entry : Entries)
    {
        const FName Identity = GetIdentity(Entry);
        if (!IsIdentity(Identity)) return Reject(TEXT("Demo.InvalidIdentity"), TEXT("目录含空值或非法稳定身份。"));
        if (OutIndex.Contains(Identity)) return Reject(TEXT("Demo.DuplicateIdentity"), TEXT("目录含重复稳定身份：") + Identity.ToString());
        OutIndex.Add(Identity, &Entry);
    }
    return FGamePlatformResult::Success();
}

/** 参数默认值与范围必须匹配；整数限制在double可精确表达区间，防止范围比较丢精度。 */
bool IsParameterValid(const FDivineBeastsDemoParameterDefinition& Parameter)
{
    if (!IsIdentity(Parameter.ParameterId) || Parameter.Unit.TrimStartAndEnd().IsEmpty() ||
        Parameter.DefaultSource.TrimStartAndEnd().IsEmpty() ||
        !IsOneOf(Parameter.ApplyWhen, {TEXT("prepare"), TEXT("execute")})) return false;
    const auto& Value = Parameter.DefaultValue;
    if (Value.Type == EDivineBeastsDemoParameterType::Boolean) return true;
    if (Value.Type == EDivineBeastsDemoParameterType::Identifier) return IsIdentity(Value.IdentifierValue);
    if (Value.Type == EDivineBeastsDemoParameterType::AssetPath) return IsStableAssetPathText(Value.AssetPathValue.ToString());
    if (Value.Type != EDivineBeastsDemoParameterType::Integer && Value.Type != EDivineBeastsDemoParameterType::Number) return false;
    constexpr int64 MaximumExactInteger = 9007199254740991LL;
    if (Value.Type == EDivineBeastsDemoParameterType::Integer && (Value.IntegerValue < -MaximumExactInteger || Value.IntegerValue > MaximumExactInteger)) return false;
    const double Number = Value.Type == EDivineBeastsDemoParameterType::Integer ? static_cast<double>(Value.IntegerValue) : Value.NumberValue;
    return FMath::IsFinite(Number) && FMath::IsFinite(Parameter.Minimum) && FMath::IsFinite(Parameter.Maximum) &&
        Parameter.Minimum <= Number && Number <= Parameter.Maximum;
}
}

FGamePlatformResult ValidateCatalog(const UDivineBeastsDemoCatalogDefinition& Catalog)
{
    if (Catalog.ManualPageContractVersion < 0 || Catalog.ManualPageContractVersion > 1)
        return Reject(TEXT("Demo.ManualPageVersion"), TEXT("独立人工页合同版本不受支持。"));
    if (Catalog.Plugins.IsEmpty()) return Reject(TEXT("Demo.EmptyPlugins"), TEXT("目录必须登记真实插件，空目录不能激活。"));
    if (Catalog.SourceFingerprint.Len() != 64) return Reject(TEXT("Demo.InvalidFingerprint"), TEXT("目录缺少64位作者源SHA256。"));
    for (TCHAR C : Catalog.SourceFingerprint)
    {
        if (!(C >= TEXT('0') && C <= TEXT('9')) && !(C >= TEXT('a') && C <= TEXT('f')))
            return Reject(TEXT("Demo.InvalidFingerprint"), TEXT("作者源SHA256必须使用小写十六进制。"));
    }
    TMap<FName, const FDivineBeastsDemoPluginEntry*> Plugins;
    TMap<FName, const FDivineBeastsDemoFeatureEntry*> Features;
    TMap<FName, const FDivineBeastsDemoScenarioDefinition*> Scenarios;
    TMap<FName, const FDivineBeastsDemoReviewCaseDefinition*> Reviews;
    FGamePlatformResult Result = IndexEntries(Catalog.Plugins, [](const auto& Entry) { return Entry.PluginId; }, Plugins);
    if (!Result.IsSuccess()) return Result;
    Result = IndexEntries(Catalog.Features, [](const auto& Entry) { return Entry.FeatureId; }, Features);
    if (!Result.IsSuccess()) return Result;
    Result = IndexEntries(Catalog.Scenarios, [](const auto& Entry) { return Entry.ScenarioId; }, Scenarios);
    if (!Result.IsSuccess()) return Result;
    Result = IndexEntries(Catalog.ReviewCases, [](const auto& Entry) { return Entry.ReviewCaseId; }, Reviews);
    if (!Result.IsSuccess()) return Result;

    // 按作者数组顺序返回第一项错误，仅诊断有序；实际案例选择始终使用稳定ID。
    for (const auto& Plugin : Catalog.Plugins)
    {
        if (!IsRepositoryPath(Plugin.DescriptorPath) || !Plugin.DescriptorPath.EndsWith(TEXT(".uplugin")) ||
            !IsOneOf(Plugin.ArchitectureLayer, {TEXT("Platform"), TEXT("Moba"), TEXT("Project")}) ||
            !IsOneOf(Plugin.PluginKind, {TEXT("code"), TEXT("content_pack")}) ||
            !IsOneOf(Plugin.FeatureAuditStatus, {TEXT("unreviewed"), TEXT("partial"), TEXT("complete")}) ||
            (Plugin.PluginKind == TEXT("content_pack") && Plugin.ArchitectureLayer != TEXT("Project")))
            return Reject(TEXT("Demo.InvalidPlugin"), TEXT("插件字段或三层归属不合法。"));
        if (Catalog.ManualPageContractVersion == 1)
        {
            const FString ExpectedPackage = TEXT("/DBAUIPack_Core/Development/PluginDemos/UI/Plugins/WBP_DBA_PluginTest_") + Plugin.PluginId.ToString();
            const FName ExpectedSource = Plugin.DescriptorPath.StartsWith(TEXT("Game/Plugins/ThirdParty/")) ? FName(TEXT("third_party")) : FName(TEXT("first_party"));
            if (Plugin.TestPageAsset.ToString() != ExpectedPackage + TEXT(".") + FPackageName::GetShortName(ExpectedPackage) + TEXT("_C") || Plugin.SourceKind != ExpectedSource)
                return Reject(TEXT("Demo.ManualPageIdentity"), TEXT("独立页与插件身份/来源不一致，禁止跨插件页面别名。"));
        }
    }
    for (const auto& Feature : Catalog.Features)
    {
        if (!Plugins.Contains(Feature.PluginId) || Feature.Title.IsEmptyOrWhitespace() || Feature.Purpose.TrimStartAndEnd().IsEmpty() ||
            !IsRepositoryPath(Feature.OwnerPath) || !AreUniqueIdentities(Feature.ScenarioIds, true) ||
            !IsOneOf(Feature.ImplementationStatus, {TEXT("unreviewed"), TEXT("unimplemented"), TEXT("partial"), TEXT("implemented")}) ||
            !IsOneOf(Feature.IntegrationStatus, {TEXT("unregistered"), TEXT("registered"), TEXT("blocked"), TEXT("wired"), TEXT("launchable")}))
            return Reject(TEXT("Demo.InvalidFeature"), TEXT("功能字段或插件引用不合法。"));
        if ((Feature.ImplementationStatus == TEXT("implemented") && Feature.ImplementationEvidence.IsEmpty()) ||
            (IsOneOf(Feature.IntegrationStatus, {TEXT("wired"), TEXT("launchable")}) && Feature.ScenarioIds.IsEmpty()))
            return Reject(TEXT("Demo.MissingEvidence"), TEXT("功能实现缺少依据，或已接线却没有案例。"));
        for (const auto& Evidence : Feature.ImplementationEvidence)
        {
            if (!IsRepositoryPath(Evidence.Path) || Evidence.Symbol.TrimStartAndEnd().IsEmpty())
                return Reject(TEXT("Demo.InvalidEvidence"), TEXT("实现依据必须提供合法仓库路径与具体符号。"));
        }
        for (FName ScenarioId : Feature.ScenarioIds)
        {
            const auto* const* Scenario = Scenarios.Find(ScenarioId);
            if (!Scenario || ((*Scenario)->FeatureId != Feature.FeatureId && !(*Scenario)->RelatedFeatureIds.Contains(Feature.FeatureId)))
                return Reject(TEXT("Demo.MissingReference"), TEXT("功能与案例的双向关系不完整。"));
        }
    }
    for (const auto& Scenario : Catalog.Scenarios)
    {
        if (!IsIdentity(Scenario.AdapterId) || Scenario.Title.IsEmptyOrWhitespace() ||
            !IsOneOf(Scenario.TemplateId, {TEXT("UIWidgetGallery"), TEXT("PresentationLab"), TEXT("GameplaySandbox"), TEXT("ServiceDiagnostics"), TEXT("AssetGallery")}) ||
            !IsOneOf(Scenario.RunMode, {TEXT("local"), TEXT("fixture"), TEXT("network"), TEXT("editor")}) ||
            !AreUniqueIdentities(Scenario.Targets, false) || !AreUniqueIdentities(Scenario.RequiredPlugins, false) ||
            !AreUniqueIdentities(Scenario.Operations, false) || !AreUniqueIdentities(Scenario.ReviewCaseIds, false) ||
            !AreUniqueIdentities(Scenario.RelatedFeatureIds, true) || !HasDescriptions(Scenario.Preconditions))
            return Reject(TEXT("Demo.InvalidScenario"), TEXT("案例字段、操作能力或必需合同缺失。"));
        if (!FMath::IsFinite(Scenario.PrepareTimeoutSeconds) || Scenario.PrepareTimeoutSeconds <= 0.0 ||
            !FMath::IsFinite(Scenario.StopTimeoutSeconds) || Scenario.StopTimeoutSeconds <= 0.0)
            return Reject(TEXT("Demo.InvalidTimeout"), TEXT("准备和清理超时必须是有限正秒数。"));
        for (FName Target : Scenario.Targets)
        {
            if (!IsOneOf(Target, {TEXT("Game"), TEXT("Client"), TEXT("Editor")}) || (Scenario.RunMode == TEXT("editor") && Target != TEXT("Editor")))
                return Reject(TEXT("Demo.InvalidTarget"), TEXT("案例声明了不允许的启动目标。"));
        }
        for (const auto& Label : Scenario.OperationLabels)
        {
            const FString Text = Label.Value.ToString();
            if (!Scenario.Operations.Contains(Label.Key) || Text.TrimStartAndEnd().IsEmpty() || Text.Len() > 80)
                return Reject(TEXT("Demo.InvalidOperationLabels"), TEXT("操作标签必须属于已声明操作、非空且不超过80字符。"));
        }
        for (FName Operation : Scenario.Operations)
        {
            if (!IsIdentity(Operation)) return Reject(TEXT("Demo.InvalidOperation"), TEXT("操作身份不合法。"));
            if (!IsOneOf(Operation, {TEXT("Execute"), TEXT("Pause"), TEXT("Resume"), TEXT("Stop"), TEXT("Reset")}) && !Scenario.OperationLabels.Contains(Operation))
                return Reject(TEXT("Demo.MissingOperationLabel"), TEXT("具名操作缺少中文标签；执行仍需适配编译白名单。"));
        }
        for (FName PluginId : Scenario.RequiredPlugins)
            if (!Plugins.Contains(PluginId)) return Reject(TEXT("Demo.MissingReference"), TEXT("案例依赖未登记插件。"));
        if (!IsAssetReference(Scenario.MapAsset, true) || !IsAssetReference(Scenario.WidgetAsset, true))
            return Reject(TEXT("Demo.InvalidAssetPath"), TEXT("地图或控件软引用格式非法。"));
        for (const FSoftObjectPath& Asset : Scenario.DefinitionAssets)
            if (!IsAssetReference(Asset, false)) return Reject(TEXT("Demo.InvalidAssetPath"), TEXT("定义软引用格式非法。"));
        TSet<FName> Parameters;
        for (const auto& Parameter : Scenario.Parameters)
        {
            if (Parameters.Contains(Parameter.ParameterId) || !IsParameterValid(Parameter))
                return Reject(TEXT("Demo.InvalidParameter"), TEXT("参数身份、类型或默认范围非法。"));
            Parameters.Add(Parameter.ParameterId);
        }
        TArray<FName> Involved = Scenario.RelatedFeatureIds;
        Involved.Add(Scenario.FeatureId);
        if (!AreUniqueIdentities(Involved, false)) return Reject(TEXT("Demo.DuplicateIdentity"), TEXT("主功能不能重复登记为关联功能。"));
        TSet<FName> Covered;
        for (FName ReviewId : Scenario.ReviewCaseIds)
        {
            const auto* const* Review = Reviews.Find(ReviewId);
            if (!Review || (*Review)->ScenarioId != Scenario.ScenarioId || !Involved.Contains((*Review)->FeatureId))
                return Reject(TEXT("Demo.MissingReference"), TEXT("案例检查用例缺失或属于其他功能/案例。"));
            Covered.Add((*Review)->FeatureId);
        }
        for (FName FeatureId : Involved)
        {
            const auto* const* Feature = Features.Find(FeatureId);
            if (!Feature || !(*Feature)->ScenarioIds.Contains(Scenario.ScenarioId) || !Scenario.RequiredPlugins.Contains((*Feature)->PluginId) || !Covered.Contains(FeatureId))
                return Reject(TEXT("Demo.MissingReference"), TEXT("案例缺少双向功能关系、所有者依赖或独立检查用例。"));
        }
    }
    for (const auto& Review : Catalog.ReviewCases)
    {
        const auto* const* Scenario = Scenarios.Find(Review.ScenarioId);
        if (!Scenario || !(*Scenario)->ReviewCaseIds.Contains(Review.ReviewCaseId) || !Features.Contains(Review.FeatureId) ||
            Review.Version < 1 || !HasDescriptions(Review.Steps) || Review.ExpectedResult.TrimStartAndEnd().IsEmpty() ||
            !AreUniqueIdentities(Review.RequiredEvidence, false))
            return Reject(TEXT("Demo.InvalidReviewCase"), TEXT("检查用例身份、版本、步骤或证据不完整。"));
        for (FName Evidence : Review.RequiredEvidence)
            if (!IsOneOf(Evidence, {TEXT("log"), TEXT("screenshot"), TEXT("state_dump"), TEXT("performance"), TEXT("asset_manifest"), TEXT("network_trace")}))
                return Reject(TEXT("Demo.InvalidReviewCase"), TEXT("检查用例证据类型未知。"));
    }
    TSet<FName> Maps;
    for (const auto& Route : Catalog.LegacyMapRoutes)
    {
        const FName Package(*Route.MapAsset.GetLongPackageName());
        if (!IsAssetReference(Route.MapAsset, false) || Maps.Contains(Package) || !AreUniqueIdentities(Route.ScenarioIds, false) ||
            (Route.ScenarioIds.Num() > 1 && !Route.bRequiresSelection))
            return Reject(TEXT("Demo.AmbiguousLegacyRoute"), TEXT("旧地图路由无效、重复或缺少一对多选择要求。"));
        Maps.Add(Package);
        for (FName ScenarioId : Route.ScenarioIds)
        {
            const auto* const* Scenario = Scenarios.Find(ScenarioId);
            if (!Scenario || FName(*(*Scenario)->MapAsset.GetLongPackageName()) != Package)
                return Reject(TEXT("Demo.MissingReference"), TEXT("旧地图路由与目标案例地图不匹配。"));
        }
    }
    return FGamePlatformResult::Success();
}
}
