# GamePlatformWorld 实施进度与断点

日期：2026-09-21。依据用户本轮《世界系统工程师》20节要求，直接实施，不创建替代宿主，不提交或清理其他任务文件。

## 现场与边界

- 正式工作树 `E:/poject/feebooz/DivineBeastsWorkspace`，main；UE源码为5.8.0、CL0。现场未提交代码包含Online、Session及Loading工作，按所有者保留。
- 正式规范为 `Game/Plugins/插件开发规范.md` 与 `Docs/Architecture/解决方案总体规划.md`；`OverallPlan.md`不存在。
- Session目前没有公开服务或可消费的真实快照，只有内部状态内核；SessionWorld真实联调保持未执行，World不包含Session私有头，不补造会话分配。
- 三个历史空插件描述保留原位；正式UE目标仍有扫描阻断。当前Foundation无真实地图资产，不伪造二进制。

## 实施计划

1. 读取Core/Data/Flow/Loading真实公开接口；World只依赖Core、Data、Loading及引擎公开接口。以World私有子系统承担世界生命周期，不新建全局管理器。
2. 定义World/Region数据及只读上下文、区域句柄和流送请求。逻辑身份复用LogicalId；定义版本/必需依赖复用Data基类。编辑器模块实际验证地图和区域引用。
3. 先为世界类型、代次、区域确定性和就绪聚合编写原生失败用例；实现共享生产算法，再接UWorld/Data/弱提供者，覆盖撤销和跨世界隔离。
4. 依据UE5.8公开源码实现World Partition临时流送源与已有LevelStreaming薄适配。操作成功与资源持有分离；取消撤销本请求，不卸载外部需求。
5. 使用Loading公开任务工厂注册WorldReadiness任务，主工程显式FoundationWorld装配。Session缺失路径明确失败，不以同名地图当网络身份。
6. 交付受控资产脚本、验证脚本、README和12份审查文档；执行可用算法/脚本检查与正式构建，记录退出码与未执行项。

## 审查重点

- Provider销毁与区域退出同轮发生；事件发布不能使迭代重入失效。
- 旧世界租约回调、流送完成、观察者事件不得操作新ContextGeneration。
- Loading成功后世界失效必须能被只读Ready复核拒绝。
- 客户端不能用开发装配绕过真实联网Session验证。
- 流送源所有权与引擎其他用户需求不能混淆；不得卸载他人资源。

## 验证状态

源码与测试实施中；UE构建、资产、Cook、Stage、进程、双PIE和人工审查均未执行。本文件后续增量记录，不将计划项当通过。
