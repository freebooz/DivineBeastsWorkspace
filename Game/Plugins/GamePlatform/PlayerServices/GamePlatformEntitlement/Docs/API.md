# API（接口）

客户端公开接口只有 `GET /v1/entitlements`，玩家身份由 Gateway 的 `PlayerAuthenticator（玩家认证器）`解析，客户端不能在 URL 或 Body 指定任意 PlayerId。

PlayerData 内部接口包括 Snapshot、单项 Check、批量 Check、Grant、Revoke 和 Operation 查询。Grant/Revoke/Check 需要可信服务器上下文；普通客户端没有 Grant/Revoke API。

UE 客户端核心查询为 `HasEntitlement（是否拥有权益）`、`HasAny（任一拥有）`、`HasAll（全部拥有）`、`IsHeroUnlocked（英雄已解锁）`和 `IsSkinUnlocked（皮肤已解锁）`。