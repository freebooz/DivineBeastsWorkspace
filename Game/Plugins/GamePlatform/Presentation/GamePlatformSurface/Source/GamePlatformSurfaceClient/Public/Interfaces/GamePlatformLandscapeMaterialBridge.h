#pragma once
#include "CoreMinimal.h"
class ALandscapeProxy;

/**
 * 游戏线程：将Landscape已有MID复用到其Nanite组件的对应源组件槽。
 * 无状态，不创建材质/播放器/资源租约，不修改共享StaticMesh或碰撞。
 * 没有烘焙Nanite组件的地形成功并返回零槽；已有组件但源MID未就绪时失败。
 * 要求真实Nanite显示的调用方须额外核对非零槽数，不能把零槽成功作为Nanite验收证据。
 * OverrideMaterials和MID随所属World销毁，不跨世界缓存指针。
 */
GAMEPLATFORMSURFACECLIENT_API bool BindGamePlatformLandscapeNaniteMIDs(
    ALandscapeProxy& Landscape, int32& OutVerifiedSlots, FString& OutDiagnostic);
