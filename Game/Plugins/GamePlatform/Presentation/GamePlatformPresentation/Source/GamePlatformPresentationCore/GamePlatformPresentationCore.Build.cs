// 平台双端中立表现契约构建规则：公开请求、定义校验及命中反馈参数供上层与客户端适配使用。
// Core/Data只持有身份、结果与资源契约；本模块不拥有播放实例，不加载具体VFX/SFX或项目内容。
using UnrealBuildTool;

// 模块身份与既有目录/导出宏保持；依赖仅指向平台基础层，不引入MOBA或项目。
public class GamePlatformPresentationCore : ModuleRules
{
    // 按宿主Target生成双端规则；Public依赖发布公开反射类型/返回值的真实包含与链接合同。
    public GamePlatformPresentationCore(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        // 配置资源DataAsset只提供平台中立数据；公开ValidateDefinition返回FGamePlatformResult。
        // 本模块直接消费Core导出结果符号，必须声明直接Public合同；不能只依赖Data的传递公开依赖。
        PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "GameplayTags", "GamePlatformCore", "GamePlatformData" });
    }
}
