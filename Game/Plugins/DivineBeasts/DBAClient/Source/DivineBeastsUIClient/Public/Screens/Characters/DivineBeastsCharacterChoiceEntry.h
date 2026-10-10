#pragma once

#include "Core/GamePlatformWidgetBase.h"
#include "DivineBeastsCharacterChoiceEntry.generated.h"

class UButton;
class UTextBlock;
class UBorder;
class UDivineBeastsCharacterChoiceEntry;

/** 项目本地选择意图；页面必须重新核对ViewModel快照，不代表后端选择已生效。 */
DECLARE_MULTICAST_DELEGATE_OneParam(FDivineBeastsChoiceRequested, UDivineBeastsCharacterChoiceEntry*);

/**
 * 项目客户端英雄磁贴／角色卡片的只读显示投影。
 * 继承平台事件驱动控件基类；样式和控件树仅由Monolith内容资产提供。
 * 页面拥有条目与身份映射；本类型不访问HTTP、不持有业务档案，不启用Tick。
 */
UCLASS(Blueprintable)
class DIVINEBEASTSUICLIENT_API UDivineBeastsCharacterChoiceEntry : public UGamePlatformWidgetBase
{
    GENERATED_BODY()
public:
    /** 跨项目语义化样式默认绑定；具体外观由DBAUIPack_Core资源主题统一决定。 */
    UDivineBeastsCharacterChoiceEntry();
    /** 游戏线程刷新显示身份和文案；空身份拒绝点击。Identity可在条目复用时替换。 */
    void ConfigureChoice(const FString& Identity, const FText& Title, const FText& Subtitle, const FText& Emblem);
    /** 只改变本地选中边框，不推进应用流程或保存角色选择。 */
    void SetChoiceSelected(bool bSelected);
    /** 忙碌、资格失效和页面退出时关闭输入；不会改变服务器资格。 */
    void SetChoiceEnabled(bool bEnabled);
    /** 取消并清除显示身份；已排队的旧点击不能产生新的有效请求。 */
    void ResetChoice();
    /** 返回本次显示身份；仅供页面重查最新快照后组装用户命令。 */
    const FString& GetChoiceIdentity() const { return ChoiceIdentity; }
    /** CommonUI焦点路由使用真实按钮；没有视觉树时返回空。 */
    UWidget* GetChoiceFocusTarget() const;
    /** 提交本地选择意图；无身份、禁用或已取消时不广播。 */
    void RequestChoice();
    /** 页面激活期间订阅，失活／重建时由页面注销。 */
    FDivineBeastsChoiceRequested OnChoiceRequested;
protected:
    virtual void BindUIEvents() override;
    virtual void UnbindUIEvents() override;
    /** Monolith命名绑定合同；蓝图编译必须验证这些控件真实存在。 */
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> ChoiceButton;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> ChoiceTitle;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> ChoiceSubtitle;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> ChoiceEmblem;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UBorder> SelectedFrame;
private:
    UFUNCTION() void HandleButtonClicked();
    /** 瞬态显示身份与输入栅栏；不序列化，也不替代业务权威状态。 */
    FString ChoiceIdentity;
    bool bChoiceEnabled = false;
};
