# D09｜排错与安全恢复

每次先检查本次Saved/Validation/GamePlatformSession下的RunId和日志；不要用旧日志确认新运行。Gateway世界进入与Session公共状态层已经存在，但完整UE Transport/Admission链尚未完成，以下按真实边界排错。

- **未分配**：先确认Online已认证、Gateway `/v1/divinebeasts/world-entry` 已装配WorldEntryPort，并核对当前selectedCharacterId、角色Revision和目标Experience。禁止客户端直连 `/internal/v1/gameservers/*`，也禁止直接填写IP或模拟Assignment后显示玩家Ready。
- **目标未就绪**：SQL返回SESSION_TARGET_UNAVAILABLE时核对Boot、协议、Ready和healthy_until。真实联调只能由实际UE实例产生就绪；测试SQL中的Ready仅是夹具，禁止移用到运行服务。
- **端点不可达**：检查Gateway返回的后端批准Endpoint和目标进程实际监听地址，区分容器内/公网地址。当前默认UE Transport仍未实现，禁止把ping、TCP端口可达或手工ClientTravel当作Admission完成。
- **票据过期/重放**：查session_reservations状态、截止和实际ConnectionID；错误材料、不同连接或到期会拒绝。查询同一操作状态再决定取消/新操作，不把Claimed改回Reserved。
- **预登录卡住**：当前没有PreLoginAsync适配。后续必须按真实待连接上下文记录一次性回调、截止、World退出；禁止超时后默认放行或把IP当唯一连接身份。
- **地图加载但无接入确认**：四事实未齐不能Ready。检查网络、Admission、世界与控制器分别由哪条可信事件提供。禁止直接调用Observe补齐测试事实。
- **错误实例或旧账号回调**：核对Scope/Operation/Attempt/AuthGeneration/ConnectionGeneration以及完整Binding。Stale或Invalid不应重试覆盖当前状态，也不应清除其他PIE实例。
- **重连耗尽**：当前只有显式Reconnect意图和单次操作Deadline，没有无限自动重连。禁止循环调用Begin；每个新网络尝试必须使用新的Operation/Attempt和可信材料，Uncertain状态必须先对账。
- **容量未释放**：查未过期Reserved/Claimed和authority_until；成功Commit应继续占位。Cancelled/Released或租约到期才释放，禁止直接减计数规避事务。
- **版本不符**：Reserve核对实例protocol_version；客户端状态核对完整Binding。不要跳过比较或更改锁定引擎版本。
- **控制面认证/证书错误**：GameServerControl HTTP已要求内部Bearer，Shipping服务器控制Provider要求TLS BaseURL。应修复Token、CA、主机名和服务身份配置，不能关闭校验或把凭据放到URL。
- **旧连接影响新连接**：查看释放调用的Reservation/Instance/Boot/Connection/Epoch，必须全部匹配当前绑定。不要按PlayerID单独清空新绑定。
- **VerifySession返回2**：表示完整接入尚未验证；查看result.json的NotVerified条目，不等于原生测试失败，更不能改脚本强行返回0。
- **PostgreSQL迁移语法失败**：查看本次backend.log中的SQL position和SQLSTATE。本轮已修正authorization参数关键字冲突；失败库由自身容器清理，禁止清空他人库后重试。
