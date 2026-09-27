# EntitlementModel（权益模型）

Entitlement 表示 Hero（英雄）、Skin（皮肤）、Cosmetic（外观）或 Feature（功能许可）的长期/临时授权，不表示 Inventory Item（背包物品）、Equipment Instance（装备实例）、Currency（货币）、比赛临时状态或当前技能状态。

一个玩家同一 EntitlementId 可以存在多个 Grant。Grant 记录包含来源、有效起止时间、授予时间、撤销时间与撤销原因；Revoke 只标记历史，不删除。

有效规则由后端可信 UTC 判断：未撤销、已到 StartsAt、且尚未到 ExpiresAt。只要至少一个来源仍有效，聚合后的 Effective Entitlement 就为 Active。