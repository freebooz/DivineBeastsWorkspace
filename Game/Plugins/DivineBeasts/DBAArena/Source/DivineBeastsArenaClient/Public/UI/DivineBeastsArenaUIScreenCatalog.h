#pragma once

#include "CoreMinimal.h"
#include "GamePlatformUITypes.h"

/** EDivineBeastsArenaUISurfaceKind（神兽联盟竞技UI表面类型）。 */
enum class EDivineBeastsArenaUISurfaceKind : uint8
{
    /** 可激活完整页面。 */
    Screen,

    /** 长驻竞技HUD。 */
    HUD
};

/**
 * FDivineBeastsArenaUISurfaceDescriptor（神兽联盟竞技UI表面描述）。
 *
 * 这里只保存稳定身份和软资源路径，不创建伪造 .uasset。
 * 实际 Widget Blueprint 必须由 Unreal Editor 创建后才能运行时打开。
 */
struct DIVINEBEASTSARENACLIENT_API FDivineBeastsArenaUISurfaceDescriptor
{
    FName SurfaceId = NAME_None;
    EDivineBeastsArenaUISurfaceKind Kind =
        EDivineBeastsArenaUISurfaceKind::Screen;
    EGamePlatformUILayer Layer = EGamePlatformUILayer::Screen;
    EGamePlatformUIInputMode InputMode =
        EGamePlatformUIInputMode::GameAndUI;
    FName DefaultFocusWidgetName = NAME_None;
    FString WidgetClassPath;
    FString MobileWidgetClassPath;
};

/**
 * FDivineBeastsArenaUIScreenCatalog（神兽联盟竞技UI目录）。
 *
 * 所有 Arena 专属 Screen/HUD 从公共 DBAClient 移入本目录，
 * 保证禁用 DBAArena 后公共客户端不携带竞技 UI 注册。
 */
class DIVINEBEASTSARENACLIENT_API FDivineBeastsArenaUIScreenCatalog
{
public:
    static const TArray<FDivineBeastsArenaUISurfaceDescriptor>& GetSurfaces();
    static const FDivineBeastsArenaUISurfaceDescriptor* Find(FName SurfaceId);
    static TArray<FName> GetScreenIds();
};
