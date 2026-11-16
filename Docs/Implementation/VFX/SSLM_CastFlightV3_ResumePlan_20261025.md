# SSLM 火冰双掌与1号键前摇接线续作计划

目标：续接现有四套CastFlight V3（施法/飞行第三版）资源，不重复创建，不以编译通过冒充美术通过。用户已授权1号键火法视觉替换和本次执行期间连接恢复。

## 已核实基线
- 十二生肖Primary（主输入）实际授权均为DirectedMissile（定向飞弹），表现样式DBA.Development.DirectedMissile。
- DirectedMissileFlight（1号键飞行定义）已由上次作业指向NS_DBA_FireOrbFlight_V3；须独立冷加载再次确认。
- 前摇目前仅一个右手请求，Cast语义复用Fireball。数字2不能跟着本次修改。
- 火冰单掌共享CPU运动模块存在负SubImageIndex（序列帧索引）；先以旧SimCache证明失败，再用非负相位与原生模拟回归。

## 实施顺序
- [x] 恢复自有UE5.8编辑器连接，只读核实项目、进程、资产及输入绑定。
- [x] 检出负动画帧索引并通过Monolith原生动作修复，备份旧模块；四系统编译通过。
- [x] 添加1号键双掌真实请求/参数/去重/取消与重入回归，已观察旧实现只有右掌的真实失败。
- [x] 在既有施法WorldSubsystem（世界子系统）中增加仅DirectedMissile使用的两个掌心请求账本，分别附着实际左右掌；保持现有动画、真实施法窗口、请求服务和取消逻辑。
- [x] 新增独立DirectedMissileCast（定向飞弹前摇定义），参数白名单仅CastDurationSec（施法时长）、ElapsedAtStartSec（延迟偏移）、HandSideSign（左右旋向）；目录只改变1号Cast解析，不改2号或寒冰地面法阵。
- [x] 完整Editor增量构建及16项生命周期原生回归通过；四套Niagara和四个定义独立冷加载通过；已生成Alpha修复后12张真实PIE隔离截图，抽样帧索引全部有效。
- [x] 已在SSLM_CastFlightV3_ResumeResult_20261025.md逐项记录：1号真实按键端到端、连续视频、多人性能与母图质量人工验收仍未完成，脚本轨迹和测试提供者不冒充正式运行验证。

## 连接恢复合同
只读请求遇到超时/连接重置最多重连3次（退避1、2、4秒）；写入中断先按路径、哈希、状态查询是否已完成，禁止盲目重放或重新创建。UE编辑器进程不存在才恢复本任务自有进程；不得停止别人的编辑器、扩大端口访问或绕过附件/安全拒绝。无需用户重复授权。本轮结束后不声称后台持续运行。

## 受影响文件与回退
DivineBeastsDevelopmentCastingWorldSubsystem.h/.cpp（施法表现账本与双掌请求）、DivineBeastsDevelopmentMagicPresentation.cpp（1号前摇独立映射）、DivineBeastsDevelopmentCastingLifecycleTests.cpp（生命周期回归）。新定义仅在DBAPresentationPack_Core项目内容包内生成。源改动可按本次精确差异回退；飞行原绑定保留Saved/Validation/CastFlightV3/Slot1Rollback。正式GA、技能授予、伤害/冷却、服务器时序、弹道与碰撞不改，不提交不推送。