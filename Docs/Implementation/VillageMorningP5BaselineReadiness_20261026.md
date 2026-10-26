# SSLM新手村清晨光雾P5：曝光新基线与隔离复核实施说明

日期：2026-10-26。范围：正式地图 `/DBAWorldPack_Village/Maps/L_Village_Start` 的**只读基线复核**及新的隔离原型；**不得把本文件理解为已完成P5或已经获得用户人工审核批准**。

## 1. 既有事实与阻断

- 历史P0～P4见 `Docs/Implementation/VillageMorningEnvironment_20261025.md`。Monolith v0.23.0、UE5.8的原生读写、四份Village隔离地图、20张历史截图、12轮历史性能采样与冷重载均有历史证据；不重做旧版、不覆盖历史。
- 旧作者参数 `Tools/Unreal/World/VillageMorningPresets.json` 及历史隔离地图均采用 Manual Exposure Bias **+2.5 EV**。
- 正式地图于2026-10-25经过单字段修正，将曝光补偿从 +2.5 EV 改为 **+1.0 EV**；证据见 `Docs/Implementation/VillageOvercastLighting_20261025.md`。该文件登记正式地图当时 SHA-256 为 `2fa3662054514dc65f6e97ecb50b05d85bca9481a5608325fd18ca2cb393012f`。这是**历史参考哈希**，再次执行应重新读取当前地图实际哈希，不以旧哈希强行覆盖其他工作。
- 2026-10-26通过远端Git仓库核对时，HWebCodex和Cwebcodex均返回“Tunnel-client has not been seen for 300 seconds”，因此本次**未取得新的本地UE/Monolith读写、截图、性能或保存证据**。

## 2. 本次隔离修正内容

仅在功能分支 `review/village-morning-p5-exposure-20261026` 内维护以下**文本脚本、参数与本说明**，不改动旧地图、正式地图、天气服务及已有美术资产：

1. `Tools/Unreal/World/ReviewVillageMorningEnvironmentUE.py`：增加 `select_review_scope('p5')`（P5审核范围选择），保留默认 `legacy`（历史范围）；新基线必须从真实正式地图采集，强制当前曝光回读为+1.0 EV，记录审核范围与正式包新哈希；已存在基线拒绝覆盖。新副本继续遵循已有非环境几何/出生点保护和保存后校验。
2. `Tools/Unreal/World/VillageMorningP5Presets.json`：沿用三套天气参数、质量分档，**只将固定曝光补偿改为+1.0 EV**，保留历史作者输入不变。
3. `Tools/Unreal/World/VillageMorningPIEReviewUE.py`：P5 PIE运行时读取和记录实际后处理曝光值，要求与P5清单一致。
4. `Tests/Architecture/ValidateVillageMorningP5Scope.py`：不依赖UE的静态测试，检查旧版与P5参数、独立路径、审核保护及曝光回读接口；**测试通过不等于UE运行通过**。

新隔离地图目录：`/Game/Development/Weather/VillageMorningP5/Maps/`

新证据目录：`Saved/Validation/VillageMorningEnvironmentP5_20261026/`

历史目录 `/Game/Development/Weather/VillageMorning/Maps/` 及 `Saved/Validation/VillageMorningEnvironment20261025/` 保持不变。

## 3. 恢复Monolith连接后的实际执行步骤

### S0. 工程身份、状态及静态检查

1. 本地修复HWebCodex/Cwebcodex连接，确认实际项目路径、分支、未提交文件、引擎版本、PIE状态、地图脏包，并保证没有其他编辑器任务正在写入同一地图。
2. 通过真实Monolith的 `monolith_status`、`monolith_discover`、`monolith_guide`、`describe_query` 获取当前版本/动作及 `editor.run_python` 的真实输入Schema；不得使用历史调用次数或文档代替。
3. 使用实际部署的功能分支脚本执行：
   `python Tests/Architecture/ValidateVillageMorningP5Scope.py`。
4. 若当前编辑器或程序未加载正确项目、工具未连接、项目有未保存修改或自动化脚本与地图不属于同一工作树，则停止，不尝试强制重启或清除编辑器现场。

### S1. 从当前正式地图采集全新P5基线（只读）

1. 通过Monolith原生查询确认 `/DBAWorldPack_Village/Maps/L_Village_Start` 已加载；现有关卡变化应优先和并行任务协调。先确认当前场景不在PIE，且没有地图/内容脏包。
2. 通过Monolith `run_python` 的实际 `execute_statement` 模式调用以下**Python主体**（参数名称必须由本机Schema确认，不能直接将下文当MCP JSON请求）：

```python
import unreal as u
from pathlib import Path
import sys
root = Path(u.Paths.project_dir()).resolve().parent
world_tools = str(root / "Tools" / "Unreal" / "World")
if world_tools not in sys.path:
    sys.path.insert(0, world_tools)
import ReviewVillageMorningEnvironmentUE as author
print(author.select_review_scope("p5"))
author.audit("P0_Baseline.json")
```

3. 输出必须包含**实际**UE5.8版本、正式地图包名、唯一PostProcessVolume、Manual曝光补偿+1.0 EV、当前正式地图SHA-256、光源/雾组件及场景几何清单。若曝光不是1.0、override未生效、已经存在P5基线或正式地图被并发改写，则自动停止，交由人工确认基线差异。

### S2. 建立四份新的隔离副本，不覆盖20261025的旧图

每次调用编辑器Python前都必须在已加载模块上显式执行 `author.select_review_scope('p5')`。各副本必须先从**当前正式地图**引擎SaveAs得到，而不是从旧晨间原型直接复制。

```python
# 以下调用需在S1通过、作者模块已载入、UE5.8编辑器连接正常后分阶段执行。
author.select_review_scope("p5")
author.load_formal_and_copy("L_Village_Baseline_Overcast")
author.save_review("Baseline_Saved.json")

for short in ("Village_Morning_Clear", "Village_Morning_LightFog", "Village_Morning_Mist"):
    author.load_formal_and_copy("L_" + short)
    author.apply_preset(short)
```

实际执行时应按地图逐张保存、回读、确认非环境几何/出生点一致，再进行下一张。旧原型和正式关卡仅供只读对照。若同名P5目标地图已经存在，脚本拒绝覆盖，先人工处理版本冲突。

### S3. 冷重载、Nanite绑定与PIE原始图

1. 重启**受当前操作会话明确控制**的UE5.8独立编辑器进程，在新进程运行 `author.select_review_scope("p5"); author.cold_audit()`，证明四张P5地图保存后环境参数及非环境几何一致。
2. 基于**新的四份P5地图**通过Monolith原生组件反射重新获取LandscapeNanite真实来源组件与槽位对应关系，写入新证据目录下 `NativeNaniteBindings.json`。不得直接复制20261025旧地图绑定文件或推断组件数组顺序。
3. 使用 `VillageMorningPIEReviewUE.py`；每次审查初始化前调用 `author.select_review_scope("p5")`。若P5绑定文件缺失或版本不匹配必须停止。维持原有Clear天气、0.70风、八纹理及MID、100% ScreenPercentage和固定FOV75。
4. 在入口、桃林、湖岸、现有湖畔茅屋、低洼林地五机位生成四方案各五张**真实UE截图**，记录地图包、Camera ViewTarget、截图原图SHA、实际曝光及天气状态。湖心码头、完整村落中心和瀑布若尚未存在，继续明确为缺项，不伪造位置与功能。
5. 高/中/低分别确认体积雾开关、分辨率参数、截图质量；检查远景空气透视、清晨明快色彩、湖面水汽、树干层次以及镜头运动时的重投影残影。

### S4. 真实性能重测与P5门禁

1. 使用同一设备、分辨率、画质、天气、相机和运行入口，以四份**新P5地图**重测，不混用旧+2.5 EV基线。
2. 分别记录平均FPS、帧时间P95/P99、GPU时间、VolumetricFog GPU事件、设备显存采样、风场及相机移动结果，保留原始CSV与单帧ProfileGPU；不要把GPU整卡显存当作独占游戏显存。
3. 评价`Village_Morning_LightFog`（清晨薄雾）为优先候选，但只能依据新截图和数据提出建议，不得标记已被人工选定。
4. 人工审核通过前，禁止将清晨光雾覆盖 `/DBAWorldPack_Village/Maps/L_Village_Start`、切换正式天气主资产或提交任何正式地图修改。
5. 即使原型通过，也不能直接宣称Production Ready（生产就绪）；后续需按实际变更范围验证正式运行、生产Village装配、Cook剥离、NPC/技能可辨识度及联机相关边界。

## 4. 当前交付状态

- 本分支：只完成隔离作者工具、参数与静态测试的准备工作。
- Monolith重新连接与本地UE5.8现场回读：**未执行**。
- 新P5地图及新P5视觉、性能证据：**未生成**。
- P5人工签收与正式地图合入：**均未执行**。

此文档是恢复执行的明确工单，不是引擎运行记录。全部最终状态应按新的本机执行日志、地图文件、参数回读和人工签收重新判定。
