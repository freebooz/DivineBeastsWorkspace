// 项目双端草稿适配：只读定义和外观Schema；不拥有后端资格，资源租约归应用流程。
#include "Creation/DivineBeastsCharacterCreationProvider.h"

#include "Catalog/DivineBeastsHeroCatalog.h"
#include "Definitions/DivineBeastsHeroDefinition.h"
#include "Engine/AssetManager.h"

class FDivineBeastsCharacterCreationProvider final
    : public IDivineBeastsCharacterCreationProvider
{
public:
    virtual int32 GetCatalogRevision() const override
    {
        return FDivineBeastsHeroCatalog::CatalogRevision;
    }

    virtual void GetCreateableHeroes(
        TArray<FGamePlatformCharacterCreationHeroDescriptor>& OutHeroes) const override
    {
        OutHeroes.Reset();
        for (const FDivineBeastsCoreHeroCatalogEntry& Entry :
             FDivineBeastsHeroCatalog::GetCoreEntries())
        {
            const bool bHasRegisteredDefinition =
                FDivineBeastsHeroCatalog::GetDefinitionAssetPath(
                    Entry.HeroDefinitionId).IsValid();
#if UE_BUILD_SHIPPING
            // Shipping必须存在真实、已注册并可Cook的Definition资产；不允许使用开发占位回退。
            if (!bHasRegisteredDefinition)
            {
                continue;
            }
#endif
            FGamePlatformCharacterCreationHeroDescriptor Descriptor;
            Descriptor.HeroDefinitionId = Entry.HeroDefinitionId;
            Descriptor.DisplayNameKey = Entry.DisplayNameKey;
            Descriptor.ContentRevision = bHasRegisteredDefinition
                ? FString::Printf(
                    TEXT("Catalog.%d"),
                    FDivineBeastsHeroCatalog::CatalogRevision)
                : FDivineBeastsHeroCatalog::GetDevelopmentFallbackContentRevision();
            OutHeroes.Add(MoveTemp(Descriptor));
        }
    }

    virtual bool ValidateCreationDraft(
        FName HeroDefinitionId,
        const TMap<FString, FString>& AppearanceSelection,
        FString& OutError) const override
    {
        if (!FDivineBeastsHeroCatalog::IsCoreHeroId(HeroDefinitionId))
        {
            OutError = TEXT("HeroDefinitionId不在十二生肖核心Catalog中。");
            return false;
        }

        const FSoftObjectPath DefinitionPath =
            FDivineBeastsHeroCatalog::GetDefinitionAssetPath(HeroDefinitionId);
        if (!DefinitionPath.IsValid())
        {
#if UE_BUILD_SHIPPING
            OutError = TEXT("Hero Definition资产尚未注册到AssetManager，Shipping禁止使用占位回退。");
            return false;
#else
            // 开发阶段真实Definition尚未生成时，只允许默认空外观草稿；颜色和Mannequin外观由内容包生成器决定。
            if (!AppearanceSelection.IsEmpty())
            {
                OutError = TEXT("开发占位Definition只允许默认外观；请先生成真实Hero Definition资产再提交自定义外观。");
                return false;
            }
            return true;
#endif
        }

        if (const UDivineBeastsHeroDefinition* Definition =
            Cast<UDivineBeastsHeroDefinition>(DefinitionPath.ResolveObject()))
        {
            return Definition->AppearanceSchema.ValidateSelection(
                AppearanceSelection,
                OutError);
        }

        // Definition尚未载入时只允许空外观选择；非空选择必须等待Definition加载后再验证。
        if (!AppearanceSelection.IsEmpty())
        {
            OutError = TEXT("Hero Definition尚未载入，无法验证AppearanceSelection。");
            return false;
        }
        return true;
    }

    virtual TSharedPtr<FStreamableHandle> ValidateCreationDraftAsync(
        FName HeroDefinitionId,
        TMap<FString, FString> AppearanceSelection,
        TFunction<void(bool, FString)> Completion) const override
    {
        if (!FDivineBeastsHeroCatalog::IsCoreHeroId(HeroDefinitionId))
        {
            if (Completion)
            {
                Completion(false, TEXT("HeroDefinitionId不在十二生肖核心Catalog中。"));
            }
            return nullptr;
        }

        return FDivineBeastsHeroCatalog::RequestDefinition(
            HeroDefinitionId,
            [HeroDefinitionId,
             AppearanceSelection = MoveTemp(AppearanceSelection),
             Completion = MoveTemp(Completion)](
                UDivineBeastsHeroDefinition* Definition) mutable
            {
                if (!Completion)
                {
                    return;
                }
                if (!Definition || Definition->DefinitionId != HeroDefinitionId)
                {
                    Completion(false, TEXT("Hero Definition加载失败或身份不匹配。"));
                    return;
                }

                FString Error;
                if (!Definition->IsProjectDefinitionValid(Error))
                {
                    Completion(false, MoveTemp(Error));
                    return;
                }
                const bool bValid = Definition->AppearanceSchema.ValidateSelection(
                    AppearanceSelection,
                    Error);
                Completion(bValid, MoveTemp(Error));
            });
    }

    virtual FGamePlatformDataLease ValidateCreationDraftWithLease(
        UGameInstance& Instance, TWeakObjectPtr<UObject> WeakCaller, FName HeroDefinitionId,
        TMap<FString, FString> AppearanceSelection, TFunction<void(bool, FString)> Completion,
        FGamePlatformResult& OutResult) const override
    {
        if (!Completion)
        {
            OutResult = FGamePlatformResult::Failure(TEXT("InvalidCompletion"), TEXT("草稿校验必须提供完成回调。"));
            return {};
        }
        return FDivineBeastsHeroCatalog::AcquireDefinitionResources(Instance, HeroDefinitionId,
            EGamePlatformDataLifetime::Instance, WeakCaller,
            [HeroDefinitionId, AppearanceSelection = MoveTemp(AppearanceSelection), Completion = MoveTemp(Completion)]
            (UDivineBeastsHeroDefinition* Definition, const FGamePlatformDataLease&, const FGamePlatformResult& Result) mutable
            {
                if (!Result.IsSuccess() || !Definition || Definition->DefinitionId != HeroDefinitionId)
                { Completion(false, TEXT("真实英雄定义加载失败或身份不匹配。")); return; }
                FString Error;
                const bool bValid = Definition->IsProjectDefinitionValid(Error) &&
                    Definition->AppearanceSchema.ValidateSelection(AppearanceSelection, Error);
                Completion(bValid, MoveTemp(Error));
            }, OutResult);
    }

    virtual bool TryGetZodiacIdentity(
        FName HeroDefinitionId,
        EDivineBeastsZodiacIdentity& OutZodiacIdentity) const override
    {
        return FDivineBeastsHeroCatalog::TryGetZodiacIdentity(
            HeroDefinitionId,
            OutZodiacIdentity);
    }
};

TUniquePtr<IDivineBeastsCharacterCreationProvider>
CreateDivineBeastsCharacterCreationProvider()
{
    return MakeUnique<FDivineBeastsCharacterCreationProvider>();
}
