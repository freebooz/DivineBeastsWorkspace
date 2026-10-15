// 平台客户端普通队伍协议边界：消费Shared/OpenAPI真源，先完整校验临时值再发布；无HTTP、账号缓存或权威规则。
#include "Party/GamePlatformPartyCodec.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
    /** 标识不是URL或凭据；拒绝控制字符、空白边界和超长值，不悄悄标准化改变幂等内容。 */
    bool Identifier(const FString& Value)
    {
        if (Value.IsEmpty() || Value.Len()>128 || Value.TrimStartAndEnd()!=Value) return false;
        for (const TCHAR C:Value) if (C<32 || C==127) return false;
        return true;
    }
    bool String(const TSharedPtr<FJsonObject>& Object, const TCHAR* Key, FString& Value)
    {
        // UE的TryGetStringField允许数字转字符串；协议身份和64位版本不允许这种隐式转换。
        return Object.IsValid() && Object->HasTypedField<EJson::String>(Key) && Object->TryGetStringField(Key, Value);
    }
    /** JSON布尔必须是真正的true/false；拒绝字符串和数值，避免不可信响应扩大按钮权限。 */
    bool Boolean(const TSharedPtr<FJsonObject>& Object, const TCHAR* Key, bool& Value)
    {
        return Object.IsValid() && Object->HasTypedField<EJson::Boolean>(Key) && Object->TryGetBoolField(Key, Value);
    }
    /** 十进制逐位溢出校验，不通过double、Atoi的容错截断或平台相关long。 */
    bool Revision(const TSharedPtr<FJsonObject>& Object, const TCHAR* Key, int64& Value)
    {
        FString Text;
        if (!String(Object,Key,Text) || Text.IsEmpty() || Text.Len()>19 || (Text.Len()>1 && Text[0]=='0')) return false;
        int64 Parsed=0;
        for (const TCHAR C:Text)
        {
            if (C<'0' || C>'9' || Parsed>(MAX_int64-(C-'0'))/10) return false;
            Parsed=Parsed*10+(C-'0');
        }
        Value=Parsed; return true;
    }
    bool Time(const TSharedPtr<FJsonObject>& Object, const TCHAR* Key, FDateTime& Value)
    {
        FString Text;
        return String(Object,Key,Text) && Text.Len()<=40 && Text.EndsWith(TEXT("Z")) &&
            FDateTime::ParseIso8601(*Text,Value) && Value.GetTicks()>0;
    }
    bool Parse(const FString& Body, TSharedPtr<FJsonObject>& Object)
    { return Body.Len()<=262144 && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Body),Object) && Object.IsValid(); }
    bool Request(const TSharedPtr<FJsonObject>& Object, FGamePlatformPartyRequest& Out)
    {
        return String(Object,TEXT("requestId"),Out.RequestId) && Identifier(Out.RequestId) &&
            String(Object,TEXT("kind"),Out.Kind) && (Out.Kind==TEXT("invitation") || Out.Kind==TEXT("application")) &&
            String(Object,TEXT("playerId"),Out.PlayerId) && Identifier(Out.PlayerId) &&
            String(Object,TEXT("state"),Out.State) &&
            (Out.State==TEXT("pending") || Out.State==TEXT("accepted") || Out.State==TEXT("declined") || Out.State==TEXT("cancelled")) &&
            Time(Object,TEXT("expiresAtUtc"),Out.ExpiresAtUtc);
    }
    bool Snapshot(const TSharedPtr<FJsonObject>& Object,FGamePlatformPartySnapshot& Next)
    {
        const TArray<TSharedPtr<FJsonValue>> *Members=nullptr,*Requests=nullptr;
        if (!Object.IsValid() || !Boolean(Object,TEXT("hasParty"),Next.bHasParty) ||
            !String(Object,TEXT("partyId"),Next.PartyId) || !String(Object,TEXT("leaderPlayerId"),Next.LeaderPlayerId) ||
            !Revision(Object,TEXT("revision"),Next.Revision) || !Boolean(Object,TEXT("rosterLocked"),Next.bRosterLocked) ||
            !Object->TryGetArrayField(TEXT("members"),Members) || Members->Num()>5 ||
            !Object->TryGetArrayField(TEXT("requests"),Requests) || Requests->Num()>64 ||
            !Time(Object,TEXT("serverTimeUtc"),Next.ServerTimeUtc)) return false;
        // 新准备合同必须成组出现；旧响应兼容为空，任何部分字段/类型错误均不覆盖缓存。
        if(Object->HasField(TEXT("activityId"))||Object->HasField(TEXT("readyCheckId"))||Object->HasField(TEXT("readyDeadlineUtc")))
        {
            if(!String(Object,TEXT("activityId"),Next.ActivityId)||!String(Object,TEXT("readyCheckId"),Next.ReadyCheckId)||!Object->HasField(TEXT("readyDeadlineUtc")))return false;
            if(!Next.ReadyCheckId.IsEmpty()){if(!Identifier(Next.ReadyCheckId)||!Identifier(Next.ActivityId)||!Time(Object,TEXT("readyDeadlineUtc"),Next.ReadyDeadlineUtc))return false;}
            else if(!Object->TryGetField(TEXT("readyDeadlineUtc"))->IsNull())return false;
        }
        if (!Next.bHasParty)
            return Next.PartyId.IsEmpty() && Next.LeaderPlayerId.IsEmpty() && Next.Revision==0 &&
                !Next.bRosterLocked && Members->IsEmpty() && Requests->IsEmpty();
        if (!Identifier(Next.PartyId) || !Identifier(Next.LeaderPlayerId) || Members->IsEmpty()) return false;
        TSet<FString> Ids;
        for (const auto& Value:*Members)
        {
            const TSharedPtr<FJsonObject>* Item=nullptr;
            FGamePlatformPartyMember Member;
            if (!Value.IsValid() || !Value->TryGetObject(Item) ||
                !String(*Item,TEXT("playerId"),Member.PlayerId) || !Identifier(Member.PlayerId) || Ids.Contains(Member.PlayerId) ||
                !String(*Item,TEXT("displayName"),Member.DisplayName) || Member.DisplayName.Len()>256 ||
                !Boolean(*Item,TEXT("ready"),Member.bReady) || !Boolean(*Item,TEXT("online"),Member.bOnline)) return false;
            Ids.Add(Member.PlayerId); Next.Members.Add(MoveTemp(Member));
        }
        if (!Ids.Contains(Next.LeaderPlayerId)) return false;
        Ids.Reset();
        for (const auto& Value:*Requests)
        {
            const TSharedPtr<FJsonObject>* Item=nullptr;
            FGamePlatformPartyRequest Pending;
            if (!Value.IsValid() || !Value->TryGetObject(Item) || !Request(*Item,Pending) || Ids.Contains(Pending.RequestId)) return false;
            Pending.PartyId=Next.PartyId; Pending.Revision=Next.Revision;
            Ids.Add(Pending.RequestId); Next.Requests.Add(MoveTemp(Pending));
        }
        return true;
    }
}

bool GamePlatformPartyCodec::DecodeSnapshot(const FString& Body,FGamePlatformPartySnapshot& Out)
{
    TSharedPtr<FJsonObject> Object; FGamePlatformPartySnapshot Next;
    if (!Parse(Body,Object) || !Snapshot(Object,Next)) return false;
    Out=MoveTemp(Next); return true;
}

bool GamePlatformPartyCodec::DecodeObservation(const FString& Body,FGamePlatformPartyResult& Out)
{
    TSharedPtr<FJsonObject> Object;
    const TSharedPtr<FJsonObject>* View=nullptr;
    const TArray<TSharedPtr<FJsonValue>>* Inbox=nullptr;
    FGamePlatformPartyResult Next;
    FDateTime ServerTime;
    if (!Parse(Body,Object) || !String(Object,TEXT("scopeId"),Next.ObservationScopeId) ||
        !Next.ObservationScopeId.StartsWith(TEXT("party/")) || Next.ObservationScopeId.Len()>134 ||
        !String(Object,TEXT("cursor"),Next.ObservationCursor) || Next.ObservationCursor.IsEmpty() || Next.ObservationCursor.Len()>43 ||
        !Boolean(Object,TEXT("resyncRequired"),Next.bResyncRequired) || !Boolean(Object,TEXT("changed"),Next.bChanged) ||
        !Time(Object,TEXT("serverTimeUtc"),ServerTime) || !Object->TryGetObjectField(TEXT("snapshot"),View) ||
        !Snapshot(*View,Next.Snapshot) || !Object->TryGetArrayField(TEXT("invitations"),Inbox)) return false;
    TArray<FString> Parts;
    Next.ObservationCursor.ParseIntoArray(Parts,TEXT("-"),false);
    if (Parts.Num()!=2) return false;
    for (const FString& Part:Parts)
    {
        if (Part.IsEmpty() || Part.Len()>20 || (Part.Len()>1 && Part[0]=='0')) return false;
        uint64 Parsed=0;
        for (const TCHAR C:Part)
        {
            if (C<'0' || C>'9' || Parsed>(MAX_uint64-(C-'0'))/10) return false;
            Parsed=Parsed*10+(C-'0');
        }
    }
    FString InboxBody;
    if (!FJsonSerializer::Serialize(*Inbox,TJsonWriterFactory<>::Create(&InboxBody)) ||
        !DecodeInvitations(InboxBody,Next.Invitations)) return false;
    // 总体与嵌套时间允许编码时的微小间隔；统一采用最新服务器基准展示，不用客户端系统时钟。
    Next.Snapshot.ServerTimeUtc=ServerTime;
    Next.bSuccess=true;
    Out=MoveTemp(Next);
    return true;
}
bool GamePlatformPartyCodec::DecodeInvitations(const FString& Body,TArray<FGamePlatformPartyRequest>& Out)
{
    TArray<TSharedPtr<FJsonValue>> Values;
    if (Body.Len()>262144 || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Body),Values) || Values.Num()>20) return false;
    TArray<FGamePlatformPartyRequest> Next; TSet<FString> Ids;
    for (const auto& Value:Values)
    {
        const TSharedPtr<FJsonObject>* Item=nullptr; FGamePlatformPartyRequest Invite; double Count=0;
        if (!Value.IsValid() || !Value->TryGetObject(Item) || !String(*Item,TEXT("partyId"),Invite.PartyId) || !Identifier(Invite.PartyId) ||
            !String(*Item,TEXT("requestId"),Invite.RequestId) || !Identifier(Invite.RequestId) ||
            !String(*Item,TEXT("inviterPlayerId"),Invite.InviterPlayerId) || !Identifier(Invite.InviterPlayerId) ||
            !Revision(*Item,TEXT("revision"),Invite.Revision) || !Time(*Item,TEXT("expiresAtUtc"),Invite.ExpiresAtUtc) ||
            !(*Item)->HasTypedField<EJson::Number>(TEXT("memberCount")) ||
            !(*Item)->TryGetNumberField(TEXT("memberCount"),Count) || Count<1 || Count>5 || Count!=FMath::FloorToDouble(Count)) return false;
        // 请求身份属于队伍作用域，不能仅按可能重复的局部requestId去重。
        const FString Key=Invite.PartyId+TEXT("\n")+Invite.RequestId;
        if (Ids.Contains(Key)) return false;
        Ids.Add(Key); Invite.MemberCount=static_cast<int32>(Count); Invite.Kind=TEXT("invitation"); Invite.State=TEXT("pending");
        Next.Add(MoveTemp(Invite));
    }
    Out=MoveTemp(Next); return true;
}
bool GamePlatformPartyCodec::EncodeCommand(const FGamePlatformPartyCommand& Command,FString& Out)
{
    Out.Empty();
    static const TSet<FString> Actions={TEXT("invite"),TEXT("requestJoin"),TEXT("acceptRequest"),TEXT("declineRequest"),TEXT("cancelRequest"),TEXT("ready"),TEXT("transferLeader"),TEXT("leave"),TEXT("kick"),TEXT("disband")};
    if (!(Identifier(Command.PartyId)||(Command.PartyId.IsEmpty()&&Command.Action==TEXT("invite")&&Command.ExpectedRevision==0)) || !Identifier(Command.ClientRequestId) || Command.ExpectedRevision<0 || !Actions.Contains(Command.Action)) return false;
    const bool bTarget=Command.Action==TEXT("invite") || Command.Action==TEXT("kick") || Command.Action==TEXT("transferLeader");
    const bool bRequest=Command.Action==TEXT("invite") || Command.Action==TEXT("requestJoin") || Command.Action==TEXT("acceptRequest") || Command.Action==TEXT("declineRequest") || Command.Action==TEXT("cancelRequest");
    if ((bTarget && !Identifier(Command.TargetPlayerId)) || (bRequest && !Identifier(Command.MembershipRequestId))) return false;
    TSharedRef<FJsonObject> Object=MakeShared<FJsonObject>();
    Object->SetStringField(TEXT("partyId"),Command.PartyId); Object->SetStringField(TEXT("action"),Command.Action);
    Object->SetStringField(TEXT("clientRequestId"),Command.ClientRequestId);
    Object->SetStringField(TEXT("expectedRevision"),LexToString(Command.ExpectedRevision));
    if (bTarget) Object->SetStringField(TEXT("targetPlayerId"),Command.TargetPlayerId);
    if (bRequest) Object->SetStringField(TEXT("membershipRequestId"),Command.MembershipRequestId);
    if (Command.Action==TEXT("ready")) Object->SetBoolField(TEXT("ready"),Command.bReady);
    return FJsonSerializer::Serialize(Object,TJsonWriterFactory<>::Create(&Out));
}
bool GamePlatformPartyCodec::DecodeCommandResult(const FString& Body,const FGamePlatformPartyCommand& Command,FGamePlatformPartySnapshot& Out)
{
    TSharedPtr<FJsonObject> Object; FString RequestId,PartyId; int64 AffectedRevision=0;
    const TSharedPtr<FJsonObject>* View=nullptr; FGamePlatformPartySnapshot Next;
    if (!Parse(Body,Object) || !String(Object,TEXT("clientRequestId"),RequestId) || RequestId!=Command.ClientRequestId ||
        !String(Object,TEXT("affectedPartyId"),PartyId) || !(PartyId==Command.PartyId || (Command.PartyId.IsEmpty()&&Command.Action==TEXT("invite")&&Identifier(PartyId))) ||
        !Revision(Object,TEXT("affectedRevision"),AffectedRevision) || AffectedRevision<Command.ExpectedRevision ||
        !Object->TryGetObjectField(TEXT("snapshot"),View) || !Snapshot(*View,Next)) return false;
    if(Command.PartyId.IsEmpty()&&(!Next.bHasParty||Next.PartyId!=PartyId))return false;
    Out=MoveTemp(Next); return true;
}
