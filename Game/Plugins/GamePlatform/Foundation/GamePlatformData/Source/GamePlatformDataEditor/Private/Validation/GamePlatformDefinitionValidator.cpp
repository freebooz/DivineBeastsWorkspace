#include "Validation/GamePlatformDefinitionValidator.h"
#include "Definitions/GamePlatformDefinitionBase.h"
#include "Validation/GamePlatformDefinitionValidation.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/StrongObjectPtr.h"
#include "Misc/DataValidation.h"

bool UGamePlatformDefinitionValidator::CanValidateAsset_Implementation(const FAssetData& InAssetData, UObject* InObject, FDataValidationContext& InContext) const
{
    return InObject && InObject->IsA<UGamePlatformPrimaryDataAsset>();
}

EDataValidationResult UGamePlatformDefinitionValidator::ValidateLoadedAsset_Implementation(const FAssetData& InAssetData, UObject* InAsset, FDataValidationContext& Context)
{
    auto Fail = [this, InAsset](const FGamePlatformResult& Result)
    {
        AssetFails(InAsset, FText::FromString(Result.Code.ToString() + TEXT(": ") + Result.Message));
        return EDataValidationResult::Invalid;
    };
    auto* Root = Cast<UGamePlatformDefinitionBase>(InAsset);
    if (!Root) return Fail(FGamePlatformResult::Failure(TEXT("InvalidDefinitionClass"), TEXT("平台主资产必须继承可校验的定义基类。")));
    IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
    // 编辑器验证允许等待源发现；运行请求绝不以此阻塞加载线程。
    Registry.SearchAllAssets(true);
    Registry.WaitForCompletion();
    struct FFrame
    {
        UGamePlatformDefinitionBase* Definition = nullptr;
        int32 NextChild = 0;
        bool bHasValidated = false;
    };
    TArray<FFrame> Stack;
    Stack.Add({Root, 0, false});
    TArray<TStrongObjectPtr<UGamePlatformDefinitionBase>> Pins;
    Pins.Emplace(Root);
    TSet<FPrimaryAssetId> Visiting;
    TSet<FPrimaryAssetId> Visited;
    while (!Stack.IsEmpty())
    {
        FFrame& Frame = Stack.Last();
        const FPrimaryAssetId Id = Frame.Definition->GetPrimaryAssetId();
        if (!Frame.bHasValidated)
        {
            FGamePlatformResult Result = Frame.Definition->ValidateDefinition();
            if (!Result.IsSuccess()) return Fail(Result);
            FSoftObjectPath Source;
            Result = ResolveUniqueGamePlatformDefinitionSource(Id, Source, Frame.Definition);
            if (!Result.IsSuccess()) return Fail(Result);
            Visiting.Add(Id);
            Frame.bHasValidated = true;
        }
        if (Frame.NextChild >= Frame.Definition->RequiredDefinitions.Num())
        {
            Visiting.Remove(Id); Visited.Add(Id); Stack.Pop(); continue;
        }
        const FPrimaryAssetId Child = Frame.Definition->RequiredDefinitions[Frame.NextChild++];
        if (Visiting.Contains(Child)) return Fail(FGamePlatformResult::Failure(TEXT("DependencyCycle"), FString::Printf(TEXT("必需定义存在循环：%s。"), *Child.ToString())));
        if (Visited.Contains(Child)) continue;
        if (Stack.Num() >= 128 || Pins.Num() >= 4096)
            return Fail(FGamePlatformResult::Failure(TEXT("DependencyGraphLimit"), TEXT("依赖图超过128层或4096个唯一节点。")));
        FSoftObjectPath Source;
        FGamePlatformResult Result = ResolveUniqueGamePlatformDefinitionSource(Child, Source, Root);
        if (!Result.IsSuccess()) return Fail(Result);
        // 仅编辑器验证同步取得源对象，不进入运行期服务、不注册长期资源需求。
        const FAssetData ChildData = Registry.GetAssetByObjectPath(Source);
        auto* ChildDefinition = Cast<UGamePlatformDefinitionBase>(ChildData.GetAsset());
        if (!ChildDefinition || ChildDefinition->GetPrimaryAssetId() != Child)
            return Fail(FGamePlatformResult::Failure(TEXT("DependencyLoadFailed"), TEXT("必需定义源对象无法加载，或类型/身份不匹配。")));
        Pins.Emplace(ChildDefinition);
        Stack.Add({ChildDefinition, 0, false});
    }
    AssetPasses(InAsset);
    return EDataValidationResult::Valid;
}
