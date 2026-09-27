# D09｜排错与安全恢复

每次先检查本次Saved/Validation/GamePlatformSession下的RunId和日志；不要用旧日志确认新运行。当前不存在可启动的完整Session连接服务，以下分别说明已实现内核和待接入路径。

- **未分配**：检查是否仅调用了内部状态Begin。它不会发送HTTP；Online/网关适配缺失是当前前置阻塞。禁止直接填写IP或模拟Assignment后显示玩家Ready。
- **目标未就绪**：SQL返回SESSION_TARGET_UNAVAILABLE时核对Boot、协议、Ready和healthy_until。真实联调只能由实际UE实例产生就绪；测试SQL中的Ready仅是夹具，禁止移用到运行服务。
- **端点不可达**：未来检查后端批准的外部游戏地址和本次进程监听，区分容器内地址。当前Store不返回端点，无ClientTravel调用，禁止把ping或端口探测当作准入完成。
- **票据过期/重放**：查session_reservations状态、截止和实际ConnectionID；错误材料、不同连接或到期会拒绝。查询同一操作状态再决定取消/新操作，不把Claimed改回Reserved。
- **预登录卡住**：当前没有PreLoginAsync适配。后续必须按真实待连接上下文记录一次性回调、截止、World退出；禁止超时后默认放行或把IP当唯一连接身份。
- **地图加载但无接入确认**：四事实未齐不能Ready。检查网络、Admission、世界与控制器分别由哪条可信事件提供。禁止直接调用Observe补齐测试事实。
- **错误实例或旧账号回调**：核对Scope/Operation/Attempt/AuthGeneration/ConnectionGeneration以及完整Binding。Stale或Invalid不应重试覆盖当前状态，也不应清除其他PIE实例。
- **重连耗尽**：自动重连预算尚未实现，禁止添加无限循环调用Begin。后续应由Online通用重试和Session总预算共同约束，新网络尝试必须新材料。
- **容量未释放**：查未过期Reserved/Claimed和authority_until；成功Commit应继续占位。Cancelled/Released或租约到期才释放，禁止直接减计数规避事务。
- **版本不符**：Reserve核对实例protocol_version；客户端状态核对完整Binding。不要跳过比较或更改锁定引擎版本。
- **证书错误**：真实HTTPS适配未接通，应修复CA、主机名和服务身份配置。隔离数据库测试的sslmode=disable不是可复制到游戏控制面的方案。
- **旧连接影响新连接**：查看释放调用的Reservation/Instance/Boot/Connection/Epoch，必须全部匹配当前绑定。不要按PlayerID单独清空新绑定。
- **VerifySession返回2**：表示完整接入尚未验证；查看result.json的NotVerified条目，不等于原生测试失败，更不能改脚本强行返回0。
- **PostgreSQL迁移语法失败**：查看本次backend.log中的SQL position和SQLSTATE。本轮已修正authorization参数关键字冲突；失败库由自身容器清理，禁止清空他人库后重试。
