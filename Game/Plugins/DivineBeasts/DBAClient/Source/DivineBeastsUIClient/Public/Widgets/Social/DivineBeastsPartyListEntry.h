// 项目公共小队列表条目：视觉树由Monolith资产拥有；C++只投影纯文本和选择状态，不构造视觉控件或联网。
#pragma once
#include "Components/GamePlatformComponentWidget.h"
#include "Blueprint/IUserObjectListEntry.h"
#include "DivineBeastsPartyListEntry.generated.h"

/** 单次授权快照派生的列表项；Key只索引本地选择，不能替代服务端授权。 */
UCLASS()
class DIVINEBEASTSUICLIENT_API UDivineBeastsPartyListItem : public UObject
{
    GENERATED_BODY()
public:
    /** 当前快照内稳定键，换账号/刷新列表时丢弃。 */
    UPROPERTY() FString Key;
    /** 纯文本主标题，不解析富文本标记。 */
    UPROPERTY() FText Title;
    /** 只读说明或人数/状态，不包含内部票据。 */
    UPROPERTY() FText Detail;
};

/** 供公共/竞技项目列表复用的真实行父类，不引用任何竞技类型或内容。 */
UCLASS(Abstract,Blueprintable)
class DIVINEBEASTSUICLIENT_API UDivineBeastsPartyListEntry : public UGamePlatformComponentWidget, public IUserObjectListEntry
{
    GENERATED_BODY()
protected:
    virtual void NativeOnListItemObjectSet(UObject* ListItemObject) override;
    virtual void NativeOnEntryReleased() override;
    virtual void NativeOnItemSelectionChanged(bool bIsSelected) override;
};
