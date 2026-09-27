#pragma once
#include "Types/GamePlatformInputTypes.h"
class ULocalPlayer;
class UEnhancedInputComponent;

/** 全部接口仅游戏线程。同步Success表示受理/内存操作，准备、重建和磁盘结果分别查询快照。 */
class GAMEPLATFORMINPUTCLIENT_API IGamePlatformInputService
{
public:
    virtual ~IGamePlatformInputService() = default;
    /** 显式LocalPlayer，无第零玩家或进程全局替身；命令行与专服不提供捕获服务。 */
    static IGamePlatformInputService* Get(ULocalPlayer& LocalPlayer);
    /** 仅一个配置操作，先释放旧配置；不透明设置键须8..128个安全ASCII，PIE自动另加实例命名空间。 */
    virtual FGamePlatformInputProfileHandle PrepareInputProfile(const FPrimaryAssetId& ProfileId,const FString& LocalSettingsKey,FGamePlatformResult& OutResult) = 0;
    /** 取消加载或退出当前配置，先结束/解绑/释放上下文再释放Data租约；旧句柄不能释放新配置。 */
    virtual FGamePlatformResult ReleaseInputProfile(const FGamePlatformInputProfileHandle& Handle) = 0;
    /** Owner必须属于当前世界；0..100优先级；外部已加入同一IMC则拒绝接管。 */
    virtual FGamePlatformInputContextHandle AcquireInputContext(FName ContextName,int32 Priority,TWeakObjectPtr<UObject> Owner,FGamePlatformResult& OutResult) = 0;
    virtual FGamePlatformResult ReleaseInputContext(const FGamePlatformInputContextHandle& Handle) = 0;
    /** 当前本地控制器或受控Pawn的组件；相同组件重复绑定返回同一身份，精确移除自己的绑定。 */
    virtual FGamePlatformInputBindingHandle BindInputReceiver(UEnhancedInputComponent& Component,FGamePlatformResult& OutResult) = 0;
    virtual FGamePlatformResult UnbindInputReceiver(const FGamePlatformInputBindingHandle& Handle) = 0;
    /** 多来源掩码叠加，至少一个通道；弱拥有者失效撤销，最大64，不能清除他人的抑制。 */
    virtual FGamePlatformInputBlockHandle AcquireInputBlock(uint8 Channels,FName Reason,TWeakObjectPtr<UObject> Owner,FGamePlatformResult& OutResult) = 0;
    virtual FGamePlatformResult ReleaseInputBlock(const FGamePlatformInputBlockHandle& Handle) = 0;
    /** 焦点由主工程真实视口告知，插件不抢占输入模式；失焦先阻止再Flush，恢复需释放/轴回中。 */
    virtual void SetApplicationFocus(bool bHasFocus) = 0;
    /** 弱订阅者只能属于当前本地玩家世界；回调内禁止修改服务，调用者延后到安全点。 */
    virtual FGamePlatformInputSubscription SubscribeInputEvents(TWeakObjectPtr<UObject> Owner,TFunction<void(const FGamePlatformInputEvent&)> Callback) = 0;
    virtual bool UnsubscribeInputEvents(const FGamePlatformInputSubscription& Handle) = 0;
    virtual FGamePlatformInputSnapshot GetInputSnapshot() const = 0;
    /** 从原生用户设置枚举登记行；不会通过枚举自动激活。 */
    virtual TArray<FGamePlatformInputMapping> ListPlayerMappings() const = 0;
    virtual FGamePlatformInputRebindPreview PreviewRebind(FName RowName,int32 Slot,FKey Key) const = 0;
    /** 预检后在内存应用，原生重建回调之后才bMappingsApplied；不自动保存。 */
    virtual FGamePlatformResult ApplyRebind(FName RowName,int32 Slot,FKey Key) = 0;
    /** 仅本配置一行；None为本配置所有已登记行，不能清其他功能。 */
    virtual FGamePlatformResult ResetMappings(FName RowName) = 0;
    /** 实际写入完成/失败与内存生效分开；失败保留当前已应用键位。 */
    virtual FGamePlatformResult SaveInputPreferences() = 0;
    /** 限定本地世界拥有者、触点0..9和白名单语义；同动作首版只允许一个触摸源，拒绝冲突。 */
    virtual FGamePlatformInputTouchHandle BeginTouchInput(int32 PointerId,EGamePlatformInputSemantic Semantic,TWeakObjectPtr<UObject> Owner,FGamePlatformResult& OutResult) = 0;
    /** 向原生增强输入注入有限类型值，不直接调用语义消费者；非有限、越界或错维度拒绝。 */
    virtual FGamePlatformResult UpdateTouchInput(const FGamePlatformInputTouchHandle& Handle,const FInputActionValue& Value) = 0;
    virtual FGamePlatformResult EndTouchInput(const FGamePlatformInputTouchHandle& Handle) = 0;
};
