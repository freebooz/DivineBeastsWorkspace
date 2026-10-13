# P0 材质资源缺项与并行冲突检查（初版）

## 已确认（源代码与正式文档）

1. Surface（平台环境表面）存在七个MF（材质函数）、两个M（母版）和一个MPC（参数集合）的稳定资产路径合同，但本轮没有完成UE真实资产存在性扫描，不能直接断定资产缺失或齐全。
2. `Tools/Unreal/Weather/AuthorWeatherSurfaceAssets.py`（天气Surface作者脚本）、`Tools/Unreal/Surface/AuthorMossSnowPBRAssets.py`（苔藓雪PBR作者脚本）和贴花作者脚本已存在，应优先复用而不是重复创建。
3. `GamePlatformVFX`（VFX插件）的文档记载真实Niagara与材质资产，但本轮未重复UE加载核验，不沿用旧验证状态。
4. Surface现有两个文档描述了不同叠加顺序；已在`MaterialLayerCompositionPolicy.md`（材质层规则）统一为遮罩/覆盖语义，但尚未改动真实母材质图。
5. 单一Runner存在其他已运行与排队任务；在当前Editor未运行的情况下，批量UE资产制作会与其他任务争抢唯一工程/编辑器状态。本轮暂不修改已有.uasset和Village正式关卡。

## 缺项矩阵（准确含义）

- `Undetermined`（尚未确定）：真实MF/M/MI/MPC/ML数量、加载/引用/图形编译状态，需要UE Asset Registry与保存回读。
- `NotYetAuthored`（本轮尚未新增）：201项候选中的新材质函数、材质层、领域母版和演示。这不意味着项目历史资产都不存在。
- `Blocked`（环境阻断）：UI Monolith真实资产操作、完整UE GPU编译、客户端/专用服务器Cook、人工视觉。
- `ReadyForReview`（可审查）：201项能力的中文台账、归属分类、参数/覆盖约定与迁移/回滚规则。

## 消除冲突原则

禁止对同一个.uasset多会话写入、覆盖既有材质父级、清理未确认引用的Redirector（资源重定向器）、覆盖旧村落共享材质或重建已有UI/VFX机制。每批次应取得具体资产清单、工程锁定身份与回退证明。
