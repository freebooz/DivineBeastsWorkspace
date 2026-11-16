// SSLM项目层开发编目值合同；仅开发客户端/编辑器，调用方为Data定义、目录与只读UI。
// 值自行拥有字符串/数组，不拥有世界、业务权威或资源租约；共享资产加载后只读，变更通过新版本发布。
#pragma once

#include "CoreMinimal.h"
#include "Types/GamePlatformResult.h"
#include "UObject/SoftObjectPath.h"
#include "DivineBeastsDemoCatalogTypes.generated.h"

/** 只读插件身份与功能范围审计记录；内容包归Project，不是第四层。 */
USTRUCT(BlueprintType)
struct DIVINEBEASTSDEMOCLIENT_API FDivineBeastsDemoPluginEntry
{
    GENERATED_BODY()
    /** 真实uplugin稳定名称。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|Demo")
    FName PluginId;
    /** 仓库相对描述路径，仅作定位，不作为运行时文件访问指令。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|Demo")
    FString DescriptorPath;
    /** Platform/Moba/Project三层值，内容包固定Project。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|Demo")
    FName ArchitectureLayer = TEXT("Project");
    /** code或content_pack，区分代码插件与纯内容。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|Demo")
    FName PluginKind = TEXT("code");
    /** unreviewed/partial/complete；complete只表示范围审计，不表示功能完成。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|Demo")
    FName FeatureAuditStatus = TEXT("unreviewed");
    /** first_party/third_party仅描述来源，不增加第四架构层。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|Demo")
    FName SourceKind = TEXT("first_party");
    /** 本插件独立开发页的软类路径；由唯一作者清单派生，不据此直接同步加载或绕过UIManager。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|Demo")
    FSoftObjectPath TestPageAsset;
};

/** 人工核读实现依据；只定位文件与符号，不能代替实际运行。 */
USTRUCT(BlueprintType)
struct DIVINEBEASTSDEMOCLIENT_API FDivineBeastsDemoImplementationEvidence
{
    GENERATED_BODY()
    /** 仓库内源码/契约文件相对路径。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|Demo")
    FString Path;
    /** 真实类型、方法签名或文档定位，禁止填虚构API。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|Demo")
    FString Symbol;
};

/** 独立功能定义；实现与接入状态分离，运行/人工结论不写入资产。 */
USTRUCT(BlueprintType)
struct DIVINEBEASTSDEMOCLIENT_API FDivineBeastsDemoFeatureEntry
{
    GENERATED_BODY()
    /** 不依赖中文名称或地图的功能稳定身份。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|Demo")
    FName FeatureId;
    /** 拥有此功能的已登记插件身份。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|Demo")
    FName PluginId;
    /** 中文功能名，可本地化。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|Demo")
    FText Title;
    /** 中文目的与验证边界。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|Demo")
    FString Purpose;
    /** unreviewed/unimplemented/partial/implemented，必须有真实依据。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|Demo")
    FName ImplementationStatus = TEXT("unreviewed");
    /** unregistered/registered/blocked/wired/launchable；不表示运行成功。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|Demo")
    FName IntegrationStatus = TEXT("unregistered");
    /** 维护责任的仓库相对目录或文件，不在运行时据此执行命令。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|Demo")
    FString OwnerPath;
    /** 人工审计源码依据，implemented时不可为空。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|Demo")
    TArray<FDivineBeastsDemoImplementationEvidence> ImplementationEvidence;
    /** 主/关联案例身份；未接线功能允许空，不作为完成。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|Demo")
    TArray<FName> ScenarioIds;
};

/** 参数有效载荷的封闭类型；作者JSON通过显式映射转换，未知值不可默认成功。 */
UENUM(BlueprintType)
enum class EDivineBeastsDemoParameterType : uint8
{
    /** 作者boolean，仅读取bBooleanValue。 */
    Boolean UMETA(DisplayName="布尔"),
    /** 作者integer，仅读取IntegerValue并校验精度/范围。 */
    Integer UMETA(DisplayName="整数"),
    /** 作者number，仅读取有限NumberValue。 */
    Number UMETA(DisplayName="实数"),
    /** 作者identifier，仅读取非空稳定IdentifierValue。 */
    Identifier UMETA(DisplayName="标识"),
    /** 作者asset_path；真实挂载顶层对象，不是控制台/磁盘路径。 */
    AssetPath UMETA(DisplayName="资源路径")
};

/** 自包含类型化参数；Type仅选择一个有效载荷，其余字段不解释，不允许执行脚本字符串。 */
USTRUCT(BlueprintType)
struct DIVINEBEASTSDEMOCLIENT_API FDivineBeastsDemoParameterValue
{
    GENERATED_BODY()
    /** 显式参数类型；JSON封闭类型一对一映射，非法枚举值明确拒绝。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|Demo")
    EDivineBeastsDemoParameterType Type = EDivineBeastsDemoParameterType::Number;
    /** boolean载荷。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|Demo")
    bool bBooleanValue = false;
    /** integer载荷，不允许用浮点近似整数身份。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|Demo")
    int64 IntegerValue = 0;
    /** number载荷，必须有限；单位由参数定义说明。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|Demo")
    double NumberValue = 0.0;
    /** identifier载荷，稳定ASCII身份，不是控制台命令。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|Demo")
    FName IdentifierValue;
    /** asset_path载荷，只做软身份；加载/所有权由适配与中央Data服务验证。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|Demo")
    FSoftObjectPath AssetPathValue;
};

/** 参数编辑合同；先校验后显式提交，不代表业务数值已改变。 */
USTRUCT(BlueprintType)
struct DIVINEBEASTSDEMOCLIENT_API FDivineBeastsDemoParameterDefinition
{
    GENERATED_BODY()
    /** 案例内唯一参数身份。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|Demo")
    FName ParameterId;
    /** 中文单位或none，禁止猜测秒/毫秒等单位。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|Demo")
    FString Unit;
    /** 开发默认值，不能冒充玩家事实。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|Demo")
    FDivineBeastsDemoParameterValue DefaultValue;
    /** 数值下界；仅integer/number生效。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|Demo")
    double Minimum = 0.0;
    /** 数值上界；必须有序且包含默认值。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|Demo")
    double Maximum = 0.0;
    /** 默认值的定义/接口来源。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|Demo")
    FString DefaultSource;
    /** 是否允许用户修改，默认只读。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|Demo")
    bool bWritable = false;
    /** prepare或execute，参数只在声明阶段显式应用。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|Demo")
    FName ApplyWhen = TEXT("execute");
};

/** 可执行案例合同；地图可共享，预检/准备/清理与证据独立。 */
USTRUCT(BlueprintType)
struct DIVINEBEASTSDEMOCLIENT_API FDivineBeastsDemoScenarioDefinition
{
    GENERATED_BODY()
    /** 唯一案例身份，不能等于旧地图路径。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|Demo")
    FName ScenarioId;
    /** 主功能身份。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|Demo")
    FName FeatureId;
    /** 关联功能各自有独立审核用例。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|Demo")
    TArray<FName> RelatedFeatureIds;
    /** 中文案例标题。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|Demo")
    FText Title;
    /** 通过游戏实例注册的真实适配器身份；定义本身不加载模块。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|Demo")
    FName AdapterId;
    /** UIWidgetGallery/PresentationLab/GameplaySandbox/ServiceDiagnostics/AssetGallery。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|Demo")
    FName TemplateId;
    /** local/fixture/network/editor；需持续向用户显示。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|Demo")
    FName RunMode;
    /** 运行演示客户端允许Game/Client/Editor；Server仅是外部受控依赖。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|Demo")
    TArray<FName> Targets;
    /** 包含主/关联功能所有者的真实依赖集合。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|Demo")
    TArray<FName> RequiredPlugins;
    /** 中文可核验前提；不得包含自动执行的任意脚本。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|Demo")
    TArray<FString> Preconditions;
    /** 可空地图软引用，是否可加载由真实预检决定。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|Demo")
    FSoftObjectPath MapAsset;
    /** 可空控件软引用，UI归真实项目内容所有者。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|Demo")
    FSoftObjectPath WidgetAsset;
    /** 可空定义软引用，由Data服务加载；此值不拥有租约。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|Demo")
    TArray<FSoftObjectPath> DefinitionAssets;
    /** 实际支持Execute/Pause/Resume/Stop/Reset，不支持者不可展示为可操作。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|Demo")
    TArray<FName> Operations;
    /** 具名操作中文标签；只映射已声明稳定ID，不是脚本/反射调用。旧内置控制可省略。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|Demo")
    TMap<FName, FText> OperationLabels;
    /** 准备超时秒数，作者必须明确有限正值，零表示未配置并拒绝激活。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|Demo")
    double PrepareTimeoutSeconds = 0.0;
    /** 清理超时秒数，必须有限正值；超时不能伪装清理成功。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|Demo")
    double StopTimeoutSeconds = 0.0;
    /** 有界类型化参数，不包含生产凭据。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|Demo")
    TArray<FDivineBeastsDemoParameterDefinition> Parameters;
    /** 每个主/关联功能独立检查合同身份。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|Demo")
    TArray<FName> ReviewCaseIds;
};

/** 中立检查合同，不含Editor类型、审核人或人工通过状态。 */
USTRUCT(BlueprintType)
struct DIVINEBEASTSDEMOCLIENT_API FDivineBeastsDemoReviewCaseDefinition
{
    GENERATED_BODY()
    /** 稳定检查身份。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|Demo")
    FName ReviewCaseId;
    /** 合同版本，从1开始；变化后历史报告不得计入当前通过。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|Demo")
    int32 Version = 1;
    /** 该检查对应的案例。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|Demo")
    FName ScenarioId;
    /** 该检查独立核验的功能。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|Demo")
    FName FeatureId;
    /** 中文人工操作步骤，禁止自动执行文本。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|Demo")
    TArray<FString> Steps;
    /** 预期可为成功或明确拒绝；不改写实际原始结果。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|Demo")
    FString ExpectedResult;
    /** log/screenshot/state_dump/performance/asset_manifest/network_trace；不能为空。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|Demo")
    TArray<FName> RequiredEvidence;
};

/** 旧地图式身份的兼容表；一对多必须要求用户选择。 */
USTRUCT(BlueprintType)
struct DIVINEBEASTSDEMOCLIENT_API FDivineBeastsDemoLegacyMapRoute
{
    GENERATED_BODY()
    /** 旧地图软引用。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|Demo")
    FSoftObjectPath MapAsset;
    /** 同图显式候选案例，不按数组首项选择。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|Demo")
    TArray<FName> ScenarioIds;
    /** 多案例必须true；空候选无效。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|Demo")
    bool bRequiresSelection = false;
};

/** 运行预检投影；默认不可启动，原因来自实际验证，不将文件存在等同运行成功。 */
USTRUCT(BlueprintType)
struct DIVINEBEASTSDEMOCLIENT_API FDivineBeastsDemoPreflightResult
{
    GENERATED_BODY()
    /** 仅所有必需条件满足时由目录设为true。 */
    UPROPERTY(BlueprintReadOnly, Category="DivineBeasts|Demo") bool bCanStart = false;
    /** 默认NotExecuted；失败码供程序分支，中文诊断禁止包含敏感信息。 */
    UPROPERTY(BlueprintReadOnly, Category="DivineBeasts|Demo") FGamePlatformResult Result;
    /** 逐项阻塞原因，保留缺插件/资源/服务差别。 */
    UPROPERTY(BlueprintReadOnly, Category="DivineBeasts|Demo") TArray<FText> BlockingReasons;
};
