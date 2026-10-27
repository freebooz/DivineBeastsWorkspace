// 项目层开发演示启动门禁：只读判定，不发起业务请求；Test/Shipping/专服均不得获得免登录资格。
#pragma once
#include "CoreMinimal.h"

/** 开发演示入口的显式启动策略；无需游戏实例全局单例，不修改正式ApplicationFlow。 */
struct DIVINEBEASTSAPPLICATIONFLOWCLIENT_API FDivineBeastsPluginDemoMode final
{
    /** 唯一允许的初始演示总览地图包路径，非资产加载成功保证。 */
    static const TCHAR* GetHubMapPackage();
    /** 仅开发客户端允许的电影镜头专项地图（无需正式登录，不暴露后端权限）。 */
    static const TCHAR* GetCameraReviewMapPackage();
    /** 纯策略判定供自动化测试；地图路径必须精确一致。 */
    static bool IsAllowedArguments(bool bDevelopment, bool bInteractiveClient,
        bool bExplicitOptIn, bool bConflictingDevelopmentMode, const FString& RequestedEntryMap);
    /** 只读运行时启动参数检测，需-DBAPluginDemo和-DBAPluginDemoMap双重确认。 */
    static bool IsEnabled();
    /** 真实uplugin名称的词法检查；目录存在性仍由Catalog核验，拒绝路径/空名。 */
    static bool IsValidPluginIdText(const FString& PluginId);
    /** 独立页直达只允许完整开发资格和精确Hub入口，不扩展镜头审核地图资格。 */
    static bool IsAllowedPluginTestArguments(bool bDevelopment, bool bInteractiveClient,
        bool bExplicitOptIn, bool bConflictingDevelopmentMode, const FString& RequestedEntryMap,
        const FString& PluginId);
    /** 只读DBAPluginTest启动意图；无参数/非法/缺资格返回None，未知插件由目录拒绝。 */
    static FName GetRequestedPluginTest(FString* OutError = nullptr);
};
