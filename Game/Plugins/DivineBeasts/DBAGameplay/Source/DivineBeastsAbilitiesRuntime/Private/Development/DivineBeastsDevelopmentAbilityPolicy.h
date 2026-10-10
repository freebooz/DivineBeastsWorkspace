#pragma once

#include "CoreMinimal.h"
#include "Catalog/DivineBeastsHeroCatalog.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

/**
 * 开发技能覆盖的纯值策略，属于项目层双端运行模块，不拥有资产或用户状态。
 * 调用者负责提供配置、命令行与编译期资格快照；全部条件满足才选择同英雄的固定开发ID。
 * 不修改正式Hero Definition、不加载资源；Shipping/Test由调用点传入false并拒绝开发授权。
 */
namespace DivineBeasts::DevelopmentAbilities
{
    /** 双重显式启用，避免仅误加载开发配置就改变角色授权；纯值检查可用于自动化回归。 */
    inline bool IsEnabled(bool bEligibleBuild, bool bConfigured, bool bCommandLineOptIn)
    {
        return bEligibleBuild && bConfigured && bCommandLineOptIn;
    }

    /** 读取本进程只读启动资格；游戏线程使用，不缓存可变编辑器配置、不修改全局状态。 */
    inline bool IsEnabledForCurrentProcess()
    {
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
        bool bConfigured = false;
        if (GConfig)
        {
            GConfig->GetBool(TEXT("DivineBeasts.Abilities"), TEXT("bAllowDevelopmentAbilitySets"), bConfigured, GGameIni);
        }
        return IsEnabled(true, bConfigured, FParse::Param(FCommandLine::Get(), TEXT("DBADevelopmentSkills")));
#else
        return false;
#endif
    }

    /**
     * 空或错误的映射保留正式集合；只允许十二生肖各自的dba.abilityset.<hero>_dev@1。
     * 开发集合仍必须由既有数据服务加载、校验并原子授予，返回ID本身不代表授予成功。
     */
    inline FName ResolveSetId(FName HeroId, FName FormalSetId, bool bEnabled, const FString& ConfiguredId)
    {
        if (!bEnabled || !FDivineBeastsHeroCatalog::IsCoreHeroId(HeroId))
        {
            return FormalSetId;
        }
        const FString HeroSuffix = HeroId.ToString().RightChop(FString(TEXT("Hero.Zodiac.")).Len()).ToLower();
        const FString Expected = FString(TEXT("dba.abilityset.")) + HeroSuffix + TEXT("_dev@1");
        // 精确匹配而非任意路径/主资产ID，不能用配置给其他英雄或正式集合签发开发权限。
        return ConfiguredId == Expected ? FName(*Expected) : FormalSetId;
    }
}
