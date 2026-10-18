# 导入角色与动画审查（2026-10-18）

本次范围为 Kachujin G Rosales 玩家模型、Arissa NPC、Vampire A Lusth 怪物的骨架、站立、移动、跳跃、攻击、施法和死亡链路。十二生肖外观资料当前全部引用 Kachujin 同一模型及 `ABP_DBA_Kachujin`，因此该模型的动画修复覆盖十二个外观入口。本次不修改服务器移动、伤害、角色胶囊或账号逻辑。

## 已确认原因与修正

1. 旧移动重定向只有 FK 骨链映射，缺少脚部 IK 目标和地面锁定，并以固定骨盆高度偏移处理不同身高。该方式不能保持左右脚在整个步态中的接地。
2. 直接把来源动画导入不同骨架时，来源骨骼平移进入了目标动画。Kachujin 和 Vampire 的旧攻击/施法采样甚至出现相同的头部高度，未体现两者原始骨长。现在先在每个目标的原始骨架上烘焙，再通过 Monolith 的 UE 原生 FBX 导入器导入动画；模型与骨架资产本身保持原始大小。
3. 行走来源每周期前进 184.495 cm、时长 1.033 s；跑步来源每周期前进 214.292 cm、时长 0.833 s。移动动画已去除前进根位移，Actor 位移继续由 CharacterMovement 负责。混合空间采样为 0/180/500 cm/s；跑步片段的播放倍率校准为 1.94439，使来源约 257.15 cm/s 的步幅对应 500 cm/s 采样。
4. 三个移动混合空间此前仍残留旧动画且缺少跑步采样。现已逐项读回确认全部使用原骨架 `StandingIdle_TargetRig`、`Walking_TargetRig`、`Running_TargetRig`，且三份运行插值数据均已烘焙。
5. 三份动画蓝图的死亡分支改为各自 `DyingBackwards_TargetRig`；跳跃分支仍引用各自 `MM_Jump`，没有用死亡片段替换跳跃。死亡身体最低点通过逐帧骨盆修正保持在地面上，未使用下落或物理穿地代替死亡动作。
6. Arissa 来源动作没有七节披风骨的死亡轨道，披风原先最多进入地面约 65 cm。已给披风链根补充死亡轨道；身体保持落地，披风主体穿插减小，但顶部受胸骨蒙皮权重影响仍有约 6–10 cm 的地面遮挡，武器附属网格仍有约 8–15 cm 穿插。这两项尚未达到完整布料/附属物碰撞验收，不能宣称全部模型细节已无穿插。

## 站立与原始尺寸

三套角色装配的 Mesh 缩放均为 `(1,1,1)`，Pitch/Roll 均为 0，朝向只绕竖直轴旋转。原骨架骨数为 Kachujin 75、Arissa 73、Vampire 99。派生 FBX 与对应原模型的骨骼集合一致；已检查的绑定位置最大偏差小于 0.0002 cm，绑定旋转最大偏差小于 0.00001°。

站立动画逐帧审查覆盖每套 181 帧：

| 模型 | 骨盆至头部与竖直方向夹角 | 双肩高度差 | 接地观察 |
| --- | --- | --- | --- |
| Kachujin | 3.09°–3.78° | 0.91–1.01 cm | 实体最低点 -0.01–0.05 cm |
| Arissa | 1.78°–2.35° | 1.01–1.11 cm | 身体起/中/末采样最低点约 0.04–0.08 cm；披风下摆略入地 |
| Vampire | 9.88°–10.75° | 2.05–2.25 cm | 实体最低点 -0.09–0.09 cm；弓身来自原模型的怪物姿态 |

上述角度描述自然站姿，不是要求脊柱完全成为数学直线。模型装配没有额外的俯仰或侧滚倾斜；吸血鬼的弓身应与人类站姿分别判断。脚趾骨并不位于鞋底/爪尖表面，因此接地同时检查了变形后网格最低点，未仅凭脚趾骨高度下结论。

## 真实资产与验证状态

- 公共资源：`/DBAContentPack_Common/Characters/KachujinGRosales/Animations/`、`/DBAContentPack_Common/Characters/NPC/Arissa/Animations/`。
- 怪物资源：`/DBAWorldPack_Village/Creatures/VampireALusth/Animations/`。
- 每套新增四段 `AS_DBA_{角色}_{StandingIdle,Walking,Running,DyingBackwards}_TargetRig`；攻击和施法在既有 AnimSequence 身份上重导，保留 Montage 引用。
- Monolith 0.23.0 在正式 `Game/DivineBeastsArena.uproject` 执行原生导入、图引用修改、混合空间烘焙、编译、保存及重载。三份 ABP 重新加载后编译均为 0 错误、0 警告。
- 24 个动画相关资产已从磁盘重载；随后改动的三份跑步速率也已单独保存、重载。21 段活动动画完成 UE 原生姿态采样，跳跃姿势的离地与蜷腿属于空中动作，未当作站立接地错误处理。
- 独立证据：`Saved/Validation/CharacterAnimationAudit_20261018/ReloadEvidence.json`、`PoseAudit.json`、`IdleVerticality.json`、`Inputs.before.json`。
- UI 颜色既有门禁已重新运行通过：`Saved/Validation/FoundationM0/213dfe73-073a-4135-9acc-148340de3852/UIColorRHI/`。
- 客户端 Cook/Stage 第一次被 Zen sponsor 进程登记失败阻断：`Saved/Validation/FoundationM0/1c21ea7c-a936-4dbc-88e5-39dbfa2229fb/Cook-FrontEndClient/`。已在无其他 Cook/UnrealPak 运行时重启已核实的 Zen 缓存进程；后续尝试遇到其他任务的 Foundation 构建锁。
- 真实客户端站立、行走、跑步、攻击、施法、死亡及双客户端远端显示尚待本轮新包验收。Monolith 动画离屏预览未可靠推进姿态，不能作为运行通过证据。

本轮尚未达到完整实机验收，不将蓝图编译、数值采样或 Blender 预览视为游戏内视觉通过。未执行 Git 提交。
