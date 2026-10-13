// 第一层CommonInput中立数据基类：只声明UI默认操作数据的端侧，不包含项目页面/按键配置。
// 平台BP_GP_UIInputData经Monolith继承此类；CommonInput正常读取其DataTable行，不另建输入路由。
#pragma once
#include "CommonInputBaseTypes.h"
#include "CommonUITypes.h"
#include "GamePlatformUIInputData.generated.h"

/** 客户端/编辑器UI输入数据：禁止原始CommonUIInputData默认的服务端加载行为。
 * 行内容仍由中立平台资产配置；不授予技能输入、不持有玩家或服务端权威。
 */
UCLASS(Blueprintable)
class GAMEPLATFORMUICLIENT_API UGamePlatformUIInputData : public UCommonUIInputData
{
    GENERATED_BODY()
public:
    /** 真实Cook依赖分析不应把纯UI输入数据交付Dedicated Server。 */
    virtual bool NeedsLoadForServer() const override { return false; }
};
