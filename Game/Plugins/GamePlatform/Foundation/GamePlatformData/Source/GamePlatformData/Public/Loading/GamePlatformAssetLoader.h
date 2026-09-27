#pragma once

#include "Engine/AssetManager.h"

/**
 * FGamePlatformAssetLoader（游戏平台资产加载器）。
 *
 * 职责：
 * 为平台层和上层插件提供统一、极薄的 Soft Reference（软引用）异步加载入口，
 * 避免各业务模块直接复制 UAssetManager / FStreamableManager 调用代码。
 *
 * 边界：
 * - 本类型只负责非 Primary Asset Definition（主资产定义）的普通软资源加载；
 * - 需要版本、依赖、生命周期和可诊断租约的 Definition 必须使用 IGamePlatformDataService；
 * - 不保存全局业务状态，不建立第二套 AssetManager，也不拥有调用方返回的 FStreamableHandle。
 *
 * 线程：
 * 调用方应在游戏线程发起加载；完成委托由引擎流式加载系统调度。
 * 回调中的 UObject 生命周期保护仍由调用方负责。
 *
 * 性能：
 * 该薄封装不复制资源列表、不创建额外 UObject、不增加 Tick 或轮询。
 * 调用方必须保存返回的 FStreamableHandle，并在取消/生命周期结束时按领域规则释放，
 * 避免重复请求同一批资源和无意义的流式加载抖动。
 */
struct GAMEPLATFORMDATA_API FGamePlatformAssetLoader
{
    /**
     * 提交一组软资源的异步加载请求。
     * 空列表直接返回无效句柄，由调用方按自己的“无需加载”语义处理，避免向流式管理器提交空任务。
     */
    static TSharedPtr<FStreamableHandle> RequestAsyncLoad(
        const TArray<FSoftObjectPath>& Assets,
        FStreamableDelegate Completion);

    /**
     * 取消调用方持有的普通软资源加载请求。
     * 无效句柄视为已经结束并保持幂等；有效句柄转交引擎取消，使尚未触发的完成回调不再执行。
     * 本函数不重置调用方的共享指针，调用方仍须在自身生命周期边界显式释放句柄。
     */
    static void Cancel(const TSharedPtr<FStreamableHandle>& Handle);
};
