#pragma once

#include "CoreMinimal.h"
#include "Features/IModularFeature.h"
#include "Types/GamePlatformDataLease.h"
#include "Types/GamePlatformResult.h"

struct FStreamableHandle;
class UGameInstance;

/** FGamePlatformCharacterCreationHeroDescriptor（平台角色创建英雄描述）。 */
struct FGamePlatformCharacterCreationHeroDescriptor
{
    FName HeroDefinitionId = NAME_None;
    FName DisplayNameKey = NAME_None;
    FString ContentRevision;

};

/**
 * IGamePlatformCharacterCreationProvider（平台角色创建提供者）。
 * 仅提供本地Catalog/外观Schema与基础校验，不执行后端创建、最终资格或持久化。
 */
class IGamePlatformCharacterCreationProvider
    : public IModularFeature
{
public:
    virtual ~IGamePlatformCharacterCreationProvider() = default;

    static FName GetModularFeatureName()
    {
        static const FName Name(TEXT("GamePlatform.CharacterCreationProvider"));
        return Name;
    }

    virtual int32 GetCatalogRevision() const = 0;

    virtual void GetCreateableHeroes(
        TArray<FGamePlatformCharacterCreationHeroDescriptor>& OutHeroes) const = 0;

    virtual bool ValidateCreationDraft(
        FName HeroDefinitionId,
        const TMap<FString, FString>& AppearanceSelection,
        FString& OutError) const = 0;

    /**
     * 异步加载目标Hero Definition后执行完整草稿校验。
     * 用于真实Definition尚未驻留时避免UI把“资源未加载”误判成“用户输入非法”。
     * Completion只返回本地结构校验结果；最终资格、持久化与服务端业务规则仍由后端负责。
     */
    virtual TSharedPtr<FStreamableHandle> ValidateCreationDraftAsync(
        FName HeroDefinitionId,
        TMap<FString, FString> AppearanceSelection,
        TFunction<void(bool, FString)> Completion) const = 0;
    /**
     * 实例作用域草稿校验；调用者持有并显式释放数据租约，最终资格仍由后端拥有。
     * 默认拒绝尚未迁移的提供者；入口前置条件不满足直接拒绝；交给Data后的校验拒绝或接纳均可能延后一次通知，调用者须核对请求代次。
     */
    virtual FGamePlatformDataLease ValidateCreationDraftWithLease(
        UGameInstance& Instance, TWeakObjectPtr<UObject> WeakCaller, FName HeroDefinitionId,
        TMap<FString, FString> AppearanceSelection, TFunction<void(bool, FString)> Completion,
        FGamePlatformResult& OutResult) const
    {
        OutResult = FGamePlatformResult::Failure(TEXT("ScopedCreationProviderUnavailable"), TEXT("角色创建提供者尚未支持实例资源租约。"));
        return {};
    }
};
