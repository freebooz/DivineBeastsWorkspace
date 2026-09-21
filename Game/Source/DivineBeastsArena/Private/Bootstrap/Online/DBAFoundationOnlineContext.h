#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Interfaces/IGamePlatformOnlineService.h"
#include "DBAFoundationOnlineContext.generated.h"

class UGameInstance;

/** 仅项目显式开发入口的操作选择；平台插件和流程资产不导入本枚举。 */
enum class EDBAOnlineOperation : uint8 { ValidateConfiguration, ProbeService, Login, ReadProfile, Ready };

/** 项目在线联调装配；不复制认证状态机，全部请求通过本实例公开Online门面。
 * 凭据来自本RunId受控短期文件，只在开发进程内短暂持有，不是记住登录或正式账号管理。
 */
UCLASS(Transient)
class UDBAFoundationOnlineContext final : public UObject
{
    GENERATED_BODY()
public:
    /** 游戏线程初始化非秘密配置并核对本次A/B测试凭据；失败不登录、不退回单机。 */
    FGamePlatformResult Initialize(UGameInstance& Owner, const FString& InRunId);
    /** 游戏线程异步执行探测/登录/读取；所有回调弱捕获节点，Root关闭不复活旧操作。 */
    FGamePlatformOnlineRequestHandle Begin(EDBAOnlineOperation Operation,
        TFunction<void(FGamePlatformResult)> Completion);
    /** 真实认证主体与已验证资料一致才为真；不以HTTP已发出代替认证。 */
    bool IsAuthenticatedWithProfile() const;
    /** 只读脱敏展示；不返回凭据/报文，也不保存旧HUD或定义指针。 */
    FString GetDiagnostics() const;
    /** 实际地图与数据屏障已通过后启动显式联调练习；未设置开关时只显示结果。 */
    void OnFoundationReady();
    /** 先失效本上下文再取消请求、退出并释放凭据；销毁时远端撤销只能报告未确认，不能伪称成功。 */
    void Shutdown();
private:
    enum class EExercise : uint8 { Idle, Update, Refresh, LogoutA, LoginA, ReadA, LogoutForSwitch,
        LoginB, ReadB, LogoutB, Completed, Failed };
    IGamePlatformOnlineService* Service() const;
    FGamePlatformOnlineLoginRequest LoginRequest(bool bAccountB) const;
    FGamePlatformOnlineRequestOptions Options() const;
    bool TickExercise(float DeltaSeconds);
    void Observe(const FGamePlatformOnlineResult& Result);
    void FailExercise(const FGamePlatformResult& Result);
    TWeakObjectPtr<UGameInstance> Instance;
    FString RunId;
    FString AccountA;
    FString PasswordA;
    FString PlayerA;
    FString AccountB;
    FString PasswordB;
    FString PlayerB;
    FString ExpectedDisplayName;
    FString LastOperation;
    FGamePlatformResult LastResult;
    double LastElapsedSeconds = 0.0;
    int64 UpdatedRevision = -1;
    FGamePlatformOnlineRequestHandle ExerciseRequest;
    FTSTicker::FDelegateHandle ExerciseTicker;
    EExercise Exercise = EExercise::Idle;
    uint64 Generation = 0;
    bool bConfigured = false;
    bool bExerciseRequested = false;
};
