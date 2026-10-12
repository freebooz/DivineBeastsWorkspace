#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "Contracts/DivineBeastsUIContracts.h"
#include "GamePlatformUITypes.h"
#include "Loading/GamePlatformLoadingScreenService.h"
#include "Types/GamePlatformDataLease.h"
#include "DivineBeastsUIClientSubsystem.generated.h"

class UDivineBeastsApplicationUIAdapter;
class UDivineBeastsUIViewModel;
class UGamePlatformHUDWidget;
class UGamePlatformLoadingScreenService;
class UGamePlatformToastWidget;
class UGamePlatformUILayerStack;
class UGamePlatformUIManagerSubsystem;
class UGamePlatformUIScreen;
class UGamePlatformUIScreenDefinition;
class ADivineBeastsWorldPlayerController;
class UCommonUIActionRouterBase;
class UGameInstance;
class IGamePlatformDataService;
struct FUIInputConfig;

/** 本地系统菜单整体可见性；设置子页切换不重复广播，世界控制器据此撤销鼠标拖动。 */
DECLARE_MULTICAST_DELEGATE_OneParam(FDivineBeastsSystemMenuVisibilityChangedNative, bool);

/** FDivineBeastsUIContractHandle（项目UI应用契约注册句柄）。 */
USTRUCT(BlueprintType)
struct DIVINEBEASTSUICLIENT_API FDivineBeastsUIContractHandle
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    FGuid Id;

    bool IsValid() const { return Id.IsValid(); }
};

DECLARE_MULTICAST_DELEGATE_OneParam(
    FDivineBeastsUIStateChangedNative,
    const FDivineBeastsUIViewState&);

/**
 * UDivineBeastsUIClientSubsystem（神兽联盟项目UI客户端子系统）。
 * 只管理项目UI契约/页面定义注册并复用GamePlatformUIManagerSubsystem。
 */
UCLASS()
class DIVINEBEASTSUICLIENT_API UDivineBeastsUIClientSubsystem final
    : public ULocalPlayerSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    /** 地图切换产生新控制器后重新消费真实业务快照；不登录、不推进业务、不使用Tick。 */
    virtual void PlayerControllerChanged(APlayerController* NewPlayerController) override;

    FDivineBeastsUIContractHandle RegisterApplicationContract(
        FName AdapterId,
        UObject* QuerySourceObject,
        UObject* CommandPortObject);

    bool UnregisterApplicationContract(
        const FDivineBeastsUIContractHandle& Handle);

    UFUNCTION(BlueprintPure, Category="DivineBeasts|UI")
    FDivineBeastsUIViewState GetViewState() const { return ViewState; }

    UFUNCTION(BlueprintPure, Category="DivineBeasts|UI")
    TArray<FName> GetRegisteredScreenIds() const;

    UFUNCTION(BlueprintPure, Category="DivineBeasts|UI")
    FName GetRecommendedPrimaryScreenId() const;

    /** 本地角色入口换页命令；仅空闲且已认证时接受，注销或流程离开入口后撤销偏好。 */
    UFUNCTION(BlueprintCallable, Category="DivineBeasts|UI|Characters")
    bool RequestCharacterEntryScreen(FName ScreenId);

    /** 仅处理已进入真实世界的ESC；前端返回false，保留原页面取消语义。游戏线程。 */
    UFUNCTION(BlueprintCallable, Category="DivineBeasts|UI|System")
    bool HandleEscapeAction();
    /** 从游戏菜单打开真实设备设置；重复或资源加载中返回false。 */
    UFUNCTION(BlueprintCallable, Category="DivineBeasts|UI|System")
    bool OpenSystemSettings();
    /** 返回游戏；取消本协调器在途请求并关闭所有系统子页，不暂停联网世界。 */
    UFUNCTION(BlueprintCallable, Category="DivineBeasts|UI|System")
    void CloseSystemMenu();
    /** 关闭设置子页回到原游戏菜单；没有子页时返回false。 */
    UFUNCTION(BlueprintCallable, Category="DivineBeasts|UI|System")
    bool CloseSystemSettings();
    FDivineBeastsSystemMenuVisibilityChangedNative& OnSystemMenuVisibilityChanged() { return SystemMenuVisibilityChanged; }

    UFUNCTION(BlueprintCallable, Category="DivineBeasts|UI")
    FGamePlatformUIAsyncRequest OpenScreen(
        FName ScreenId,
        UDivineBeastsUIViewModel* ViewModel);

    UFUNCTION(BlueprintCallable, Category="DivineBeasts|UI")
    bool CancelScreenOpen(FGuid RequestId);

    UFUNCTION(BlueprintCallable, Category="DivineBeasts|UI")
    bool CloseScreen(UGamePlatformUIScreen* Screen);

    UFUNCTION(BlueprintCallable, Category="DivineBeasts|UI")
    bool InstallRootLayoutClass(
        TSubclassOf<UGamePlatformUILayerStack> RootLayoutClass);

    UFUNCTION(BlueprintPure, Category="DivineBeasts|UI")
    UGamePlatformLoadingScreenService* GetLoadingScreenService() const;

    UFUNCTION(BlueprintCallable, Category="DivineBeasts|UI")
    bool AttachHUDWidget(UGamePlatformHUDWidget* Widget);

    UFUNCTION(BlueprintCallable, Category="DivineBeasts|UI")
    bool AttachToastWidget(UGamePlatformToastWidget* Widget);

    /**
     * 按界面身份创建项目 ViewModel。
     * P0 登录/加载页面返回专用 ViewModel，其余页面保持兼容通用 ViewModel。
     * 对象 Outer 为当前 LocalPlayer 子系统，生命周期随本地玩家结束而清理。
     */
    UDivineBeastsUIViewModel* CreateViewModel(FName ScreenId);

    void SubmitCommand(
        FDivineBeastsUICommand Command,
        TFunction<void(const FDivineBeastsUICommandResult&)> Completion);

    bool CancelCommand(const FGuid& RequestId);

    FDivineBeastsUIStateChangedNative& OnStateChanged()
    {
        return StateChanged;
    }

private:
#if WITH_DEV_AUTOMATION_TESTS
    /** 原生回归只注入同LocalPlayer的真实Router，验证交接/阻断/解绑，不伪造网络或平台输入。 */
    friend class FDivineBeastsWorldInputHandoffTest;
#endif
    /** 按本地玩家真实Router实例绑定，替换/退出精确撤销旧委托，不关闭其他UI来源。 */
    void ObserveWorldInputRouter(UCommonUIActionRouterBase* Router);
    void HandleWorldInputTreeChanged();
    void HandleWorldInputConfigChanged(FUIInputConfig Config);
    /** 仅已进入世界且所有流程/系统/平台活跃页与在途请求均为空时，发布平台GameAndUI基线。 */
    void SyncWorldInputBaseline();
    TWeakObjectPtr<UCommonUIActionRouterBase> WorldInputRouter;
    FDelegateHandle WorldInputTreeHandle;
    FDelegateHandle WorldInputConfigHandle;
    /** Router发布可同步重入，本组合根只校正当前世界空栈，退出前先撤资格。 */
    bool bPublishingWorldInputBaseline = false;
    bool bUIShuttingDown = false;
    /** 系统页与主流程页使用不同请求身份，迟到加载不能复活已关闭菜单。 */
    bool OpenSystemSurface(FName ScreenId);
    void HandleWorldEscapeRequested();
    void SetSystemMenuVisible(bool bVisible);
    TWeakObjectPtr<ADivineBeastsWorldPlayerController> BoundMenuController;
    FDelegateHandle WorldEscapeHandle;
    FDelegateHandle ApplicationDeactivateHandle;
    FDelegateHandle SlateActivationHandle;
    void HandleApplicationActivationChanged(bool bActive);
    TWeakObjectPtr<UGamePlatformUIScreen> ActiveSystemMenu;
    TWeakObjectPtr<UGamePlatformUIScreen> ActiveSystemSettings;
    FName OpeningSystemScreenId;
    FGuid OpeningSystemRequestId;
    bool bSystemMenuVisible = false;
    /** 关闭/换页重入资格；外部层栈通知若开启后继请求，旧关闭不得恢复后继的世界输入。 */
    uint64 SystemMenuGeneration = 0;
    FDivineBeastsSystemMenuVisibilityChangedNative SystemMenuVisibilityChanged;
    /** 读取项目默认主题编号并交给平台UI服务；空配置兼容旧页面，失败不阻断登录。 */
    void EnsureConfiguredTheme();
    UFUNCTION()
    void HandleThemeRequestFinished(FGuid RequestId, bool bSuccess, FText Reason);
    /** 只记录本组合根发起的请求，不取消设置页面或其他合法调用方的主题请求。 */
    FGuid PendingThemeRequestId;
    bool bDefaultThemeRequested = false;

    /** 本地玩家所属Instance租约异步装根；同代次Pending合并、Failed不自动重试，无同步读盘。 */
    bool EnsureDefaultRootLayout();
    void HandleDefaultRootLayoutLoaded(uint64 ExpectedGeneration,
        const FGamePlatformDataLease& Lease, const FGamePlatformResult& Result);
    /** 撤销请求代次，按需保留同LocalPlayer跨Travel的成功根租约；释放前摘下所有权。 */
    void ResetDefaultRootLayoutLoad(bool bKeepInstalledLease);
    void UpdateDefaultRootLayoutContext(ULocalPlayer* Player, APlayerController* Controller);
    bool IsRootLayoutContextCurrent(uint64 ExpectedGeneration) const;
    IGamePlatformDataService* ResolveRootLayoutDataService(UGameInstance* Instance) const;

    /** Idle等候控制器，Pending在途，Failed等待新上下文，Ready由平台持有根实例。 */
    enum class ERootLayoutLoadState : uint8 { Idle, Pending, Failed, Ready };
    ERootLayoutLoadState RootLayoutLoadState = ERootLayoutLoadState::Idle;
    FGamePlatformDataLease DefaultRootLayoutLease;
    TWeakObjectPtr<UGameInstance> RootLayoutLeaseInstance;
    TWeakObjectPtr<ULocalPlayer> RootLayoutPlayer;
    TWeakObjectPtr<APlayerController> RootLayoutController;
    TWeakObjectPtr<UGamePlatformUILayerStack> LeasedRootLayout;
    FSoftObjectPath RootLayoutRequestPath;
    uint64 RootLayoutLoadGeneration = 0;
    /** 原生/蓝图Construct及平台关闭通知可能同步重入；安装返回前不再请求或同步主页面。 */
    bool bInstallingRootLayout = false;
#if WITH_DEV_AUTOMATION_TESTS
    /** 仅测试Data延后完成与最终Viewport边界；生产生命周期方法、代次和释放路径保持原样。 */
    friend struct FDivineBeastsUIResourceLeaseTestAccess;
    IGamePlatformDataService* RootDataServiceForTests = nullptr;
    FSoftObjectPath RootLayoutPathForTests;
    TFunction<bool(TSubclassOf<UGamePlatformUILayerStack>)> RootInstallerForTests;
#endif

    void RegisterDefaultScreenDefinitions();
    void UnregisterDefaultScreenDefinitions();
    void HandleViewStateChanged(const FDivineBeastsUIViewState& NewState);
    void PullInitialState();
    void SyncLoadingService();
    /** 根据只读项目状态同步唯一主页面；只在状态/RootLayout变化时执行，不使用 Tick。 */
    void SyncPrimaryScreen();
    /** 角色入口页面只驱动客户端三维预览；不改变流程节点或服务器世界。 */
    void SyncCharacterPreview();

    /** 平台页面成功打开事件；只接管由本协调器发起的当前主页面请求。 */
    UFUNCTION()
    void HandlePrimaryScreenOpened(
        FGuid RequestId,
        FName ScreenId,
        UGamePlatformUIScreen* Screen);

    /** 平台页面打开失败事件；保留原页面并显示资源错误，不改写业务状态。 */
    UFUNCTION()
    void HandlePrimaryScreenOpenFailed(
        FGuid RequestId,
        FName ScreenId,
        FText Reason);

    /** 当前主页面被关闭时清理弱引用；后续状态事件可重新选择页面。 */
    UFUNCTION()
    void HandlePrimaryScreenClosed(FName ScreenId);

    void DetachContract();

    /** 项目UI到ApplicationFlow的内部单向适配器；不暴露给蓝图。 */
    UPROPERTY(Transient)
    TObjectPtr<UDivineBeastsApplicationUIAdapter> ApplicationAdapter = nullptr;

    UPROPERTY(Transient)
    TObjectPtr<UGamePlatformUIManagerSubsystem> PlatformUI = nullptr;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UGamePlatformUIScreenDefinition>> RegisteredDefinitions;

    UPROPERTY(Transient)
    TWeakObjectPtr<UObject> QuerySourceObject;

    UPROPERTY(Transient)
    TWeakObjectPtr<UObject> CommandPortObject;

    FName ContractAdapterId = NAME_None;
    FDivineBeastsUIContractHandle ContractHandle;
    FDelegateHandle QueryStateHandle;

    /** 当前由项目主路由持有的页面；弱引用不延长 Widget 生命周期。 */
    TWeakObjectPtr<UGamePlatformUIScreen> ActivePrimaryScreen;
    /** 换页期间保留旧页弱引用，加载失败时仍提供可见错误反馈。 */
    TWeakObjectPtr<UGamePlatformUIScreen> ReplacingPrimaryScreen;
    FName ActivePrimaryScreenId = NAME_None;
    /** 只描述当前本地玩家显示创建或选择页，不属于业务角色选择或持久化资料。 */
    FName CharacterEntryScreenPreference = NAME_None;

    /** 正在异步打开的主页面身份与请求。 */
    FName OpeningPrimaryScreenId = NAME_None;
    FGuid OpeningPrimaryRequestId;

    /** 防止关闭旧页时的同步回调重入主路由。 */
    bool bSynchronizingPrimaryScreen = false;

    FDivineBeastsUIViewState ViewState;
    FGamePlatformLoadingToken LoadingToken;
    FDivineBeastsUIStateChangedNative StateChanged;
};
