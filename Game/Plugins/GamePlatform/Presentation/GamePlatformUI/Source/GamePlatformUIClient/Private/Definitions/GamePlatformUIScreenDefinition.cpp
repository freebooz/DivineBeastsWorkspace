#include "Definitions/GamePlatformUIScreenDefinition.h"

#include "Misc/PackageName.h"
#include "Screens/GamePlatformUIScreen.h"

#define LOCTEXT_NAMESPACE "GamePlatformUIScreenDefinition"

namespace
{
    bool IsSafeContentPath(const FSoftObjectPath& Path)
    {
        if (!Path.IsValid())
        {
            return false;
        }

        const FString FullPath = Path.ToString();
        if (FullPath.Contains(TEXT("://")) ||
            FullPath.Contains(TEXT("..")) ||
            FullPath.StartsWith(TEXT("file:"), ESearchCase::IgnoreCase))
        {
            return false;
        }

        const FString PackageName = Path.GetLongPackageName();
        return !PackageName.IsEmpty() &&
            !PackageName.StartsWith(TEXT("/Script/")) &&
            FPackageName::IsValidLongPackageName(PackageName);
    }

    bool IsSafeScreenClass(
        const TSoftClassPtr<UGamePlatformUIScreen>& ScreenClass)
    {
        if (ScreenClass.IsNull())
        {
            return false;
        }

        if (const UClass* LoadedClass = ScreenClass.Get())
        {
            return LoadedClass->IsChildOf(
                       UGamePlatformUIScreen::StaticClass()) &&
                   !LoadedClass->HasAnyClassFlags(CLASS_Abstract);
        }

        return IsSafeContentPath(ScreenClass.ToSoftObjectPath());
    }

    bool IsActivatableLayer(EGamePlatformUILayer Layer)
    {
        switch (Layer)
        {
        case EGamePlatformUILayer::Screen:
        case EGamePlatformUILayer::Modal:
        case EGamePlatformUILayer::System:
        case EGamePlatformUILayer::Loading:
        case EGamePlatformUILayer::Debug:
            return true;
        default:
            return false;
        }
    }

    bool RequiresExplicitFocus(
        EGamePlatformUILayer Layer,
        EGamePlatformUIInputMode InputMode)
    {
        if (InputMode == EGamePlatformUIInputMode::GameOnly)
        {
            return false;
        }

        return Layer == EGamePlatformUILayer::Screen ||
            Layer == EGamePlatformUILayer::Modal ||
            Layer == EGamePlatformUILayer::System;
    }
}

bool UGamePlatformUIScreenDefinition::ValidateDefinition(
    FText& OutReason) const
{
    if (ScreenId.IsNone())
    {
        OutReason = LOCTEXT("MissingScreenId", "ScreenId不能为空。");
        return false;
    }

    if (!IsSafeScreenClass(WidgetClass))
    {
        OutReason = LOCTEXT(
            "InvalidWidgetClass",
            "WidgetClass必须是有效的平台页面类或本地Cook内容软引用。");
        return false;
    }

    if (!IsActivatableLayer(Layer))
    {
        OutReason = LOCTEXT(
            "InvalidScreenLayer",
            "Screen Definition只能使用Screen/Modal/System/Loading/Debug可激活层。");
        return false;
    }

#if UE_BUILD_SHIPPING
    if (Layer == EGamePlatformUILayer::Debug)
    {
        OutReason = LOCTEXT(
            "DebugLayerShipping",
            "Shipping构建禁止注册Debug页面。");
        return false;
    }
#endif

    if (RequiresExplicitFocus(Layer, InputMode) &&
        DefaultFocusWidgetName.IsNone())
    {
        OutReason = LOCTEXT(
            "MissingFocus",
            "可交互页面必须声明DefaultFocusWidgetName。");
        return false;
    }

    if (RequiredTags.HasAny(BlockedTags))
    {
        OutReason = LOCTEXT(
            "ConflictingTags",
            "RequiredTags与BlockedTags不能包含同一标签。");
        return false;
    }

    for (const TSoftObjectPtr<UObject>& Asset : PreloadAssets)
    {
        if (!Asset.IsNull() &&
            !IsSafeContentPath(Asset.ToSoftObjectPath()))
        {
            OutReason = LOCTEXT(
                "UnsafePreloadAsset",
                "PreloadAssets包含非法、脚本类或外部资源路径。");
            return false;
        }
    }

    for (const TPair<FName, TSoftClassPtr<UGamePlatformUIScreen>>& Pair :
         PlatformWidgetVariants)
    {
        if (Pair.Key.IsNone() || !IsSafeScreenClass(Pair.Value))
        {
            OutReason = LOCTEXT(
                "InvalidPlatformVariant",
                "PlatformWidgetVariants包含空平台ID或非法页面类。");
            return false;
        }
    }

    OutReason = FText::GetEmpty();
    return true;
}

#undef LOCTEXT_NAMESPACE
