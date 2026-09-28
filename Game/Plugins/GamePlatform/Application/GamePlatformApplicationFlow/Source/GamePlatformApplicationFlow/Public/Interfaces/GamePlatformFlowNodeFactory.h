#pragma once

#include "Engine/GameInstance.h"
#include "Interfaces/GamePlatformCallbackFlowNode.h"
#include "UObject/UObjectGlobals.h"

/**
 * GamePlatformApplicationFlowNodes（游戏平台应用流程通用节点工厂）。
 *
 * 该命名空间只提供跨游戏稳定、无业务语义的节点构造能力。
 * 登录、角色、世界、竞技等领域规则不得下沉到这里。
 */
namespace GamePlatformApplicationFlowNodes
{
/**
 * 创建一个“等待外部 SubmitEvent（提交事件）”的通用流程节点。
 *
 * 职责：
 * - 进入节点后保持等待，不主动调用 Completion（完成回调）；
 * - 上层领域通过当前 FGamePlatformFlowNodeToken（流程节点令牌）调用 SubmitEvent 推进；
 * - 不解析 Payload（流程载荷）、不保存业务数据、不决定下一节点。
 *
 * 生命周期与线程：
 * - 必须在游戏线程调用，返回的新节点以传入 GameInstance（游戏实例）为 Outer；
 * - 平台执行器在成功、失败、取消、超时或 Shutdown（关闭）时统一调用 Finish；
 * - 节点本身不注册 Tick/Ticker/Delegate，也不持有 World/Actor/Widget。
 *
 * 性能：
 * 直接复用现有 UGamePlatformCallbackFlowNode（平台回调流程节点），
 * 不新增反射 UCLASS，不增加额外 GC 类型或 UHT 代码生成成本。
 */
inline UGamePlatformFlowNode* CreateAwaitEventFlowNode(UGameInstance& Owner)
{
    check(IsInGameThread());

    UGamePlatformCallbackFlowNode* Node =
        NewObject<UGamePlatformCallbackFlowNode>(&Owner);
    if (!Node)
    {
        return nullptr;
    }

    const bool bBound = Node->Bind(
        [](const FGamePlatformFlowContext& Context,
           FGamePlatformFlowCompletion Complete)
        {
            // 平台正常装配一定会提供有效上下文；这里仍做防御校验，避免无代次节点永久等待。
            if (!Context.GameInstance.IsValid() ||
                !Context.Handle.IsValid() ||
                Context.NodeId.IsNone() ||
                Context.NodeGeneration == 0)
            {
                Complete(FGamePlatformFlowNodeResult::Failure(
                    TEXT("AwaitEventContextInvalid"),
                    TEXT("外部事件等待节点缺少有效的GameInstance、流程句柄或节点代次。")));
                return;
            }

            // 正常路径刻意不保存也不调用 Completion。
            // SubmitEvent 直接进入平台执行器的单槽邮箱，与异步 Completion 共用同一代次保护。
            (void)Complete;
        },
        [](EGamePlatformFlowFinishReason Reason)
        {
            // 通用等待节点没有外部资源；Finish 仍显式存在以满足“每次开始必须清理一次”的统一契约。
            (void)Reason;
        });

    return bBound ? Node : nullptr;
}
}
