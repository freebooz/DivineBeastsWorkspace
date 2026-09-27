# HeroAndSkinUnlocks（英雄与皮肤解锁）

平台层使用稳定 TargetType + TargetId：HeroDefinitionId → RequiredEntitlementId，SkinDefinitionId → RequiredEntitlementId。免费 Hero 可将 RequiredEntitlementId 留空。

客户端 `IsHeroUnlocked/IsSkinUnlocked`只根据 Snapshot 做展示投影；服务器授权必须再次向 PlayerData 检查 RequiredEntitlementId。

当前项目尚无真实 Hero Definition、Skin Definition、角色选择或皮肤应用运行代码，因此本轮只建立稳定映射与授权接口，不伪造完整选人/皮肤系统。