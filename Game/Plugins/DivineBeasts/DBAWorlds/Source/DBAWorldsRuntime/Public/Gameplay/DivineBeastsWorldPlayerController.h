#pragma once
#include "Framework/GamePlatformPlayerControllerBase.h"
#include "DivineBeastsWorldPlayerController.generated.h"
class AGamePlatformPlayerStateBase;
class UGamePlatformExperienceComponent;
class AGameStateBase;
class UDivineBeastsCharacterComponent;

/** 第三层双端控制器：事件驱动读取真实体验、Pawn及准备令牌；客户端不提交认证结论。
 * 输入在服务器Active后开放，本地观察镜头与角色网络朝向分离；EndPlay解除本世界订阅与鼠标捕获。
 * 原生输入帧消费只负责运动，不Tick轮询登录、资源或服务器业务状态。 */
UCLASS()
class DBAWORLDSRUNTIME_API ADivineBeastsWorldPlayerController : public AGamePlatformPlayerControllerBase
{
    GENERATED_BODY()
public:
    /** 双端控制器拥有一个平台交互发起组件；不创建第二份交互RPC或Pawn权威状态。 */
    ADivineBeastsWorldPlayerController();
    FSimpleMulticastDelegate OnLocalWorldFactsChanged;
    /** 本地ESC意图；UI客户端拥有订阅和菜单，共享世界模块不依赖客户端UI实现。 */
    FSimpleMulticastDelegate OnWorldEscapeRequested;
    /** I键只发出本地背包开关意图；世界双端控制器不依赖UI实现，不授予或消耗道具。 */
    FSimpleMulticastDelegate OnWorldInventoryRequested;
    /** 当前本地选中目标变化；只读展示意图，技能与交互必须另经服务器资格校验。 */
    FSimpleMulticastDelegate OnWorldTargetChanged;
    /** 当前世界内的弱选中引用；目标结束、换Pawn和旅行立即清空，不跨网络复制。 */
    AActor* GetSelectedWorldTarget() const { return SelectedWorldTarget.Get(); }
    /** 游戏线程设置本地界面暂停；true清除拖动/按键/技能并恢复光标，false仅在服务器Active时恢复世界输入。 */
    void SetWorldGameplayInputSuspended(bool bSuspended);
    /** 游戏线程幂等重算，只提交完整准备事实；缺组件/旧令牌时保持输入关闭。 */
    void RefreshLocalWorldFacts();
    /** 当前体验的本地Data租约真正Prepared才返回true；尚未复制/加载则false。 */
    bool AreLocalResourcesPrepared() const;
    /** 当前Controller拥有实际Pawn、复制身份匹配且输入接收器已创建才返回true。 */
    bool IsLocalPawnBound() const;
    /** 只读服务器Active和本地绑定事实，不能把本地键盘绑定当服务器授权。 */
    bool IsWorldGameplayActive() const;
    virtual void OnRep_PlayerState() override;
    virtual void OnRep_Pawn() override;
    virtual void ClientRestart_Implementation(APawn* NewPawn) override;
    /** 旅行真正开始前撤销本世界输入；菜单关闭不能在离开世界期间重新放开移动。 */
    virtual void PreClientTravel(const FString& PendingURL, ETravelType TravelType, bool bIsSeamlessTravel) override;
    /** 引擎失焦路径调用；清除本世界鼠标捕获和按住命令，重新聚焦不回放旧输入。 */
    virtual void FlushPressedKeys() override;
    /** 原生每帧输入消费（非业务状态轮询）：角色朝向与本地观察镜头分别更新。 */
    virtual void UpdateRotation(float DeltaTime) override;
protected:
    /** ListenServer本地拥有/失控也走同一事件门禁，不能只依赖客户端OnRep。 */
    virtual void OnPossess(APawn* NewPawn) override;
    virtual void OnUnPossess() override;
    virtual void SetupInputComponent() override;
    virtual void PostProcessInput(float DeltaTime, bool bGamePaused) override;
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    void BindGameState(AGameStateBase* State);
    TWeakObjectPtr<AGamePlatformPlayerStateBase> ObservedPlayer;
    TWeakObjectPtr<UGamePlatformExperienceComponent> ObservedExperience;
    /** 当前本地受控Pawn的角色定义Ready订阅；换Pawn及EndPlay先解绑，不使用永久轮询。 */
    TWeakObjectPtr<UDivineBeastsCharacterComponent> ObservedCharacter;
    bool bRefreshingFacts=false;
    /** 仅本控制器当前世界所有，换Pawn/退出清除；没有反射字段或持久化键位。 */
    uint8 MouseButtonMask = 0;
    bool bWorldGameplayInputSuspended = false;
    bool bWorldInputActive = false;
    bool bWorldTravelPending = false;
    /** 本控制器各持有至多一次引擎输入锁；只增减自有计数，不Reset清除其他系统锁。 */
    bool bOwnsWorldMoveInputLock = false;
    bool bOwnsWorldLookInputLock = false;
    bool bHasSavedCursorPosition = false;
    bool bResettingWorldInput = false;
    FIntPoint SavedCursorPosition = FIntPoint::ZeroValue;
    FRotator ObservationRotation = FRotator::ZeroRotator;
    TWeakObjectPtr<APawn> InputPawn;
    /** 单击候选只保存按下时的可见命中；拖动、组合键、失焦和菜单切换均取消。 */
    TWeakObjectPtr<AActor> PointerPressedTarget;
    TWeakObjectPtr<AActor> SelectedWorldTarget;
    bool bPointerClickPending = false;
    double PointerPressedAtSeconds = 0.0;
    float PointerTravelPixels = 0.f;
    /** 游戏线程可见射线命中；忽略自身，不透过遮挡或界面，不扫描世界Actor。 */
    AActor* FindPointerTarget() const;
    void SetSelectedWorldTarget(AActor* Target);
    /** 必须由目标EndPlay撤销，不能依赖Widget逐帧检查无效弱指针。 */
    UFUNCTION()
    void HandleSelectedTargetEndPlay(AActor* Actor, EEndPlayReason::Type Reason);
    /** 仅采样鼠标输入/悬停；不轮询任务、网络或角色业务状态。 */
    void RefreshPointerFeedback();
    void BeginWorldInteraction();
    void EndWorldInteraction();
    float ForwardAxis = 0.f;
    float StrafeAxis = 0.f;
    float KeyboardTurnAxis = 0.f;
    /** 每秒转向角度、滚轮每格厘米及镜头臂范围是项目默认值，不影响服务器运动权威。 */
    static constexpr float KeyboardTurnDegreesPerSecond = 120.f;
    static constexpr float ZoomStepCentimeters = 50.f;
    static constexpr float MinimumCameraDistanceCentimeters = 80.f;
    static constexpr float MaximumCameraDistanceCentimeters = 1800.f;
    /** 输入回调共用本地/Active/界面暂停门禁；失败立即丢弃本帧命令。 */
    bool CanProcessWorldInput() const;
    /** 镜头与角色转向另尊重其他系统拥有的Look锁；不影响允许的纯移动命令。 */
    bool CanProcessWorldLookInput() const;
    /** 当次桌面Slate命中必须是本地玩家真实游戏视口空白；禁用或未处理鼠标键的UI同样阻止捕获。 */
    bool IsPointerOverWorldViewport() const;
    /** 幂等申请/释放本控制器各一次输入锁；其它所有者的计数保留到其自行释放。 */
    void UpdateWorldInputLocks(bool bNeedsLock);
    /** 服务器事实和Pawn事件驱动更新门禁；换Pawn先清理旧Pawn的捕获及按住状态。 */
    void RefreshWorldInputPolicy();
    /** 清本控制器自有输入；不制动权威服务器角色、不取消其他玩家或世界的资源。 */
    void ResetWorldInput();
    /** 世界GameAndUI基线；暂停时只恢复光标，不覆盖UI客户端拥有的UIOnly焦点。 */
    void ApplyVisibleCursorMode();
    /** 仅视口已接收的鼠标按下可捕获；最后一个按键释放后回到保存的视口坐标。 */
    void SetCameraMouseButton(bool bRight, bool bPressed);
    void BeginLeftCameraDrag();
    void EndLeftCameraDrag();
    void BeginRightCameraDrag();
    void EndRightCameraDrag();
    /** 轴回调只保存本帧归一化键盘输入，PostProcessInput统一消费以支持双鼠标键前进。 */
    void SetForwardAxis(float Value);
    void SetStrafeAxis(float Value);
    void SetKeyboardTurnAxis(float Value);
    /** 鼠标轴是本帧角度增量，不乘DeltaTime；仅拖动时修改本地镜头，RMB另同步角色水平朝向。 */
    void TurnObservationCamera(float Value);
    void LookObservationCamera(float Value);
    /** 滚轮按格调整本地SpringArm厘米距离并钳制范围；保留引擎碰撞测试，不改角色碰撞。 */
    void ZoomObservationCamera(float Value);
    /** 跳跃只提交原生Character命令；释放和所有取消路径均StopJumping。 */
    void BeginWorldJump();
    void EndWorldJump();
    /** 只广播本地ESC请求，菜单创建、退出命令与界面焦点由客户端所有。 */
    void RequestWorldMenu();
    /** 仅真实本地且世界Active时发布背包页面意图；实际显示/关闭由客户端UI自行判断。 */
    void RequestWorldInventory();
};
