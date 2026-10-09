#pragma once

#include "Components/GamePlatformComponentWidget.h"
#include "Components/GamePlatformPortraitWidget.h"
#include "GamePlatformPartyRosterWidget.generated.h"

/** FGamePlatformUIPartyMember（队伍成员显示状态）。
 * 生命进度为展示归一化数据，不允许由UI修改实际角色属性。
 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMUICLIENT_API FGamePlatformUIPartyMember
{
    GENERATED_BODY()

    /** 队伍显示成员唯一身份，不充当网络操作权限令牌。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Party")
    FName MemberId = NAME_None;

    /** 通用肖像、等级和头像状态，复用平台基础原子。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Party")
    FGamePlatformUIPortraitState Portrait;

    /** 0～1生命比例，非权威属性值。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Party")
    float HealthRatio = 0.0f;

    /** 0～1护盾比例，非权威属性值。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Party")
    float ShieldRatio = 0.0f;

    /** 团队就绪状态来自已确认客户端事实。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Party")
    bool bReady = false;

    /** 掉线状态由会话服务确定，UI不能猜测。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Party")
    bool bConnected = false;

    /** 仅展示队长标识，不执行队长权限操作。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Party")
    bool bLeader = false;
};

/** FGamePlatformUIPartyRosterState（队伍列表只读快照），可跨游戏复用。 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMUICLIENT_API FGamePlatformUIPartyRosterState
{
    GENERATED_BODY()

    /** 本地账号/会话来源身份，变更时旧快照失效。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Party")
    FGuid SourceScopeId;

    /** 来源递增版本，防止掉线重连后旧回调污染新状态。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Party")
    int64 Revision = -1;

    /** 队伍显示标识，不等于服务器组队资格。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Party")
    FName PartyId = NAME_None;

    /** 容量上限32项，符合单队展示而非好友列表语义。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Party")
    TArray<FGamePlatformUIPartyMember> Members;
};

/** UGamePlatformPartyRosterWidget（通用队友列表/组队HUD组件）。
 * MOBA层可在此之上附加阵营/比赛比分，但平台完全不认识竞技规则。
 */
UCLASS(Abstract, Blueprintable)
class GAMEPLATFORMUICLIENT_API UGamePlatformPartyRosterWidget
    : public UGamePlatformComponentWidget
{
    GENERATED_BODY()

public:
    /** 显式开始一个新会话作用域；退出时必须调用ClearPartySource。 */
    UFUNCTION(BlueprintCallable, Category="UI|Party")
    bool BindPartySource(FGuid InSourceScopeId);

    /** 断开会话来源并清空个人队伍数据，不向服务器发送解散请求。 */
    UFUNCTION(BlueprintCallable, Category="UI|Party")
    void ClearPartySource();

    /** 接受已认证的当前来源快照，失败时保留原状态。 */
    UFUNCTION(BlueprintCallable, Category="UI|Party")
    bool ApplyPartyRoster(const FGamePlatformUIPartyRosterState& InState);

    UFUNCTION(BlueprintPure, Category="UI|Party")
    FGamePlatformUIPartyRosterState GetPartyRoster() const { return State; }

    const FGamePlatformUIPartyRosterState& GetPartyRosterView() const { return State; }

protected:
    UFUNCTION(BlueprintImplementableEvent, Category="UI|Party",
        meta=(DisplayName="队伍列表显示状态已变化"))
    void BP_OnPartyRosterChanged(FGamePlatformUIPartyRosterState NewState);

private:
    UPROPERTY(Transient)
    FGamePlatformUIPartyRosterState State;

    /** 即使成员为空也保留作用域，只有切换或退出才清空。 */
    UPROPERTY(Transient)
    FGuid CurrentScopeId;
};
