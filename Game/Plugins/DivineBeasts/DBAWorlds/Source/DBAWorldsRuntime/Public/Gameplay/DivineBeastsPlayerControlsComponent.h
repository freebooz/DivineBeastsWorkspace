#pragma once
#include "Components/ActorComponent.h"
#include "DivineBeastsPlayerControlsComponent.generated.h"
class APlayerController;
class APawn;
class UInputComponent;
DECLARE_MULTICAST_DELEGATE_OneParam(FDivineBeastsWorldJournalRequested, FName);
/** SSLM唯一的本地操控组件。由实际Controller原生输入/镜头钩子调用；不Tick、不复制、不授予服务器资格。 */
UCLASS()
class DBAWORLDSRUNTIME_API UDivineBeastsPlayerControlsComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UDivineBeastsPlayerControlsComponent();
    /** 游戏线程绑定当前控制器输入一次；更换InputComponent后允许重新绑定。 */
    void BindInput();
    /** 在控制器Super::PostProcessInput后消费本帧运动，不查询网络业务。 */
    void ProcessInput(float DeltaTime, bool bGamePaused);
    /** 在控制器Super::UpdateRotation后更新独立观察镜头。 */
    void UpdateObservationRotation(float DeltaTime);
    /** 在引擎最终镜头更新后，仅真实视图变化才广播。 */
    void NotifyFinalCameraView(float DeltaSeconds);
    /** 本地事实由所属世界适配器事件投影；true不代替服务器资格。 */
    void SetGameplayActive(bool bActive);
    /** 旅行/重启成对清理；旧按住状态不能跨世界恢复。 */
    void SetTravelPending(bool bPending);
    /** UI持有焦点期间暂停并清除本组件输入，不覆盖其它系统输入锁。 */
    void SetWorldGameplayInputSuspended(bool bSuspended);
    /** 引擎失焦/换Pawn调用，清除捕获、轴、跳跃、技能及旧点击候选。 */
    void ResetWorldInput();
    /** 控制器退出时幂等结束本地状态及订阅。 */
    void ShutdownControls();
    /** 平台输入语义适配。Axis为归一化移动或已换算的角度增量；手柄/触控直接观察。 */
    void ApplySemanticMoveLook(FVector2D Axis, bool bMove, bool bDirectLook);
    /** 当前真实Enhanced Profile/Receiver拥有移动及视角轴；切换时清理旧输入，未准备时沿用原生轴。 */
    void SetSemanticInputOwned(bool bMoveOwned, bool bLookOwned);
    void SetSemanticInputOwned(bool bOwned) { SetSemanticInputOwned(bOwned, bOwned); }
    AActor* GetSelectedWorldTarget() const { return SelectedWorldTarget.Get(); }
    FSimpleMulticastDelegate OnLocalWorldFactsChanged;
    /** 输入接收器创建/ClientRestart完成通知，供本地适配器重采样一次，不进行轮询。 */
    FSimpleMulticastDelegate OnInputReceiverChanged;
    /** 本地ESC意图；UI客户端拥有订阅和菜单，共享世界模块不依赖客户端UI实现。 */
    FSimpleMulticastDelegate OnWorldEscapeRequested;
    /** I键只发出本地背包开关意图；世界双端控制器不依赖UI实现，不授予或消耗道具。 */
    FSimpleMulticastDelegate OnWorldInventoryRequested;
    FDivineBeastsWorldJournalRequested OnWorldJournalRequested;
    /** 当前本地目标选择/显式重选通知；只读展示意图，技能与交互必须另经服务器资格校验。 */
    FSimpleMulticastDelegate OnWorldTargetChanged;
    /** 最终本地镜头位置、旋转或FOV真实变化后通知；共享世界模块不依赖UI。 */
    FSimpleMulticastDelegate OnFinalCameraViewChanged;
    /** 本地血条显示切换意图；偏好与UI显示归LocalPlayer客户端，不修改战斗数据。 */
    FSimpleMulticastDelegate OnWorldHealthBarsToggleRequested;
protected:
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    friend class FDivineBeastsSharedPlayerControlsTest;

    friend class FDivineBeastsWorldTargetSelectionTest; // 隔离回归使用真实选择入口，不新增测试运行接口。
    friend class FDivineBeastsWorldTabTargetCycleTest;
    friend class FDivineBeastsWorldPortalClickTest;
    /** 最终镜头缓存只用于抑制相同视图事件，不作为权威角色姿态或网络同步源。 */
    FVector LastNotifiedCameraLocation = FVector::ZeroVector;
    FRotator LastNotifiedCameraRotation = FRotator::ZeroRotator;
    float LastNotifiedCameraFOV = -1.f;
    void RequestWorldHealthBarsToggle();
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
    /** TAB按次查询100米内可见的全部可选单位；按距离稳定循环，复用鼠标选择通知与服务器资格校验。 */
    void RequestNextWorldTarget();
    void SetSelectedWorldTarget(AActor* Target);
    /** 已核对短按、同一命中及视口遮挡后的单击入口；交互仍由平台执行服务器校验。 */
    void CommitWorldPointerClick(AActor* Target);
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
    /** C/M/L/K/P发出角色、地图任务、技能书及小队意图；不直接创建页面或读取业务服务。 */
    void RequestWorldJournal(FName Section);
    bool bGameplayActive = false;
    bool bSemanticMoveOwned = false;
    bool bSemanticLookOwned = false;
    void ApplyObservationDelta(float Horizontal, float Vertical, bool bDirectLook);
    FVector2D SemanticMoveAxis = FVector2D::ZeroVector;
    bool bSemanticMoveReceived = false;
    TWeakObjectPtr<UInputComponent> BoundInputComponent;
    APlayerController* Controller() const;
};
