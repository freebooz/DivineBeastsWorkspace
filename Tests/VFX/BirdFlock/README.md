# 飞鸟与环境群落生命周期回归

## 当前验证状态（2026-10-11）

本目录保存真实 UE 集成脚本与复核输入。两个 Python 脚本的 AST 语法检查已通过，使用的解释器是
`D:/UnrealEngine-5.8.0-release/Engine/Binaries/ThirdParty/Python3/Win64/python.exe`。

本轮没有启动、关闭编辑器，没有构建或运行 PIE，因此 **C++ 编译、PIE 执行、异步取消重入、GPU 画面及帧时均尚未由本目录用例验收**。没有生成“运行通过”JSON。脚本仅在真正执行后生成报告。

完整缺陷因果、代码位置与剩余问题见 `Docs/Implementation/BirdFlockAudit_20261011.md`。

## 真实集成脚本

| 文件 | 宿主显式开关 | 自动断言 |
| --- | --- | --- |
| `RunBirdFlockOwnershipReview.py` | `-BirdOwnedIntegrationReview` | 同 Actor 双组件默认路径独立；新 ID + 513 拒绝且保留旧绑定；有效换 ID；另一 Owner 占用同名拒绝；失败候选的 Stop 不伤及他人；Count=0；Actor EndPlay 清理 |
| `RunFaunaOwnershipReview.py` | `-FaunaOwnedIntegrationReview` | 上述共用生命周期；ButterflyAzure/FishSilver；Seed=-1、1000001 拒绝；Count=512 与 Seed=1000000、0 接纳 |

先完成当前源码的真实 UE 5.8 Editor 构建和项目资源登记，在独占的 PIE 验证宿主中执行对应文件。宿主必须已经有唯一真实 LocalPlayer 和它的项目表现子系统，并且具有正式鸟种/群落 Definition 的 Data 注册配置。脚本不会建立或结束 PIE，也不会生成、修改、保存资产。

示例在已运行宿主的 Python 控制台执行文件（按实际工程路径）：

```python
exec(compile(open(r"E:/work/2026/DivineBeastsWorkspace/Tests/VFX/BirdFlock/RunBirdFlockOwnershipReview.py", encoding="utf-8").read(),
             r"E:/work/2026/DivineBeastsWorkspace/Tests/VFX/BirdFlock/RunBirdFlockOwnershipReview.py", "exec"),
     {"__file__": r"E:/work/2026/DivineBeastsWorkspace/Tests/VFX/BirdFlock/RunBirdFlockOwnershipReview.py", "__name__": "__main__"})
```

Fauna 脚本使用相同方式换文件名；宿主应带它自己的显式开关。也可通过已批准的编辑器 Python 文件执行入口运行。

脚本只创建自己的瞬态 Actor 和真实场景组件，以唯一 UUID 前缀隔离显式注册；完成或异常都销毁该 Actor。缺少 PIE 或存在多个 LocalPlayer 时明确失败，不能以编辑器世界或自行拼接的状态机充当运行环境。报告输出：

- `Saved/Validation/BirdFlockAudit/OwnershipReview.json`
- `Saved/Validation/BirdFlockAudit/FaunaOwnershipReview.json`

`GetRequestedBirdCount`/`GetRequestedFaunaCount` 是项目意图数量。脚本不以它们证明 Niagara 已生成对应粒子，也不以 Loading 证明资源已完成播放。

UE 5.8 的 `PyWrapperObject.cpp:992–1022` 确认 `set_editor_properties` 在同一次 PreEdit/PostEdit 通知之间写入这一批属性。故“换 ID 同时给非法数量/种子”的输入没有经过一次中间合法 Apply，不会误测成两个独立操作。

## 必须由真实宿主补充的交互/异步用例

| 用例 | 真实输入与观察点 | 期望 |
| --- | --- | --- |
| 播放取消回调内 Stop | 等待一个正式 Bird/Fauna 实例 Playing；在公共 VFX completion handler 收到旧实例取消时调用它的 Owner/token Stop；外层更改数量 | 旧请求完成一次；当前 Desired 为 0；外层不能重新写回或再提交；无孤立平台实例 |
| 播放取消回调内再次 Apply | 在同一旧实例取消回调将数量改为另一合法值；例如外层 4、内层 9 | 内层 9 保持当前意图，外层不能覆盖；只保留当前 RequestId |
| 首次预载诊断回调撤销 | 在 OnLogicalPreloadRequested 内停止本次 Owner/token 或 StopAll | 本次 Pending 句柄可识别并取消；迟到 Data 回执不能发布目录或恢复实例 |
| 同世界 Region/Experience 改变 | 保持 UWorld 指针不变，更新有效 ProjectContext 的 RegionId 或 ExperienceId | 存活组件意图保留；旧 RequestId 先失效并取消；新目录 Active 后使用新 RequestId 重放 |
| 挂起期排队与删除 | 上一行的取消通知中，停止 A，新增 B，或把 C 的数量修改 | 新 Context 恢复 A 仍不存在，B/C 只按当前意图各提交一次；鸟与 Fauna 跨类型回调同样受门闩约束 |
| 嵌套 Context 更新 | 在旧目录取消通知中再次调用 UpdateProjectContext | 外层识别更新 token 被取代；仅最外层解除暂停；恢复依据最终 Context |
| 世界与账号退出 | 在 WorldCleanup/ResetForAccountSwitch 取消通知中尝试 Start；随后等待迟到预载完成 | 清理段拒绝重建；Desired、Pending、请求均清空；不会在退出后的世界复活 |
| 多组件/流送/分屏 | 两个 Level 实例含同名 Actor；每个 Actor 多组件；再增加第二 LocalPlayer | 自动组件路径不同；显式重复 ID 被拒绝；现行场景共享环境只归首个 LocalPlayer；其他 Owner 无权停止 |
| 异步失败重试观察 | Data 已受理后让实际 VFX 播放失败，随后相同参数 Apply | 记录真实失败终态和项目 RequestId 行为；这是本轮仍需补齐的恢复策略，不预设“已修复” |
| 视觉与预算 | 正式资产下检查远近摄像机、出生视距外、分屏、数量 0/1/32/512、两类各 16 个请求 | 检查可见数量/布局/变形/距离恢复和帧时；不要只看意图数量 |

已有 C++ Automation `DivineBeasts.Presentation.Activation.FailureCancellationAndOldGeneration` 位于项目模块 `Private/Tests`，继续作为 ContentPack 失败/取消/迟到代次的独立回归入口。它的 Transient 账本夹具不证明真实资产成功，也不能替代上表的 PIE/VFX 回调验证。
