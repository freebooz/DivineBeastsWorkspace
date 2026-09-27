#pragma once

#include "CoreMinimal.h"

/**
 * FDivineBeastsServerRoleProfile（神兽联盟服务器角色Profile）。
 * 是Deploy/Server非秘密启动配置的校验值；不承载凭据，也不加载地图或联系控制面。
 */
struct DBASERVER_API FDivineBeastsServerRoleProfile
{
    int32 ProfileVersion = 0; // Profile结构版本；不等同于跨语言契约版本。
    FName ServerRoleId = NAME_None; // 必须属于Shared正式服务器角色集合。
    FName DefaultExperienceId = NAME_None; // 本角色未显式选择体验时使用的默认体验。
    TArray<FName> AllowedExperienceIds; // 此Profile允许启动的全部体验，逐项与Shared映射校验。
    FString WorldPackage; // 必须加载的地图包名；只校验身份，不负责载图。
    TArray<FSoftObjectPath> RequiredAssets; // Ready前必须存在于实际服务器制品中的软引用资源。
    TArray<FName> ArenaModeIds; // 仅MainArena可列出，且必须完整覆盖项目五种模式。
    FName InstancePolicy = NAME_None; // 非秘密实例生命周期策略，限制为已批准策略值。
    bool bRequireRequiredAssets = false; // 是否将清单缺失视为Ready硬阻断。
    bool bRequireWorldBeginPlay = false; // 是否必须观察到Profile指定世界真实BeginPlay。
    bool bRequireControlPlaneRegistration = false; // 是否必须先收到控制面注册确认。

    /** 按固定角色目录读取Profile，并验证唯一键、体验映射、地图和模式；失败返回中文原因。 */
    static bool TryLoadForRoleName(
        const FString& RoleName,
        FDivineBeastsServerRoleProfile& OutProfile,
        FString& OutError);

    /** 校验Profile中列出的每个软路径对应的包已随服务器制品存在；不伪造或生成资产。 */
    bool FindMissingRequiredAssets(TArray<FString>& OutMissingAssets) const;

private:
    static bool TryLoadFromFile(
        const FString& FilePath,
        FDivineBeastsServerRoleProfile& OutProfile,
        FString& OutError);
    bool Validate(FString& OutError) const;
};
