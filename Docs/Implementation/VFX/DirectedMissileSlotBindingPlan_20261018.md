# 定向飞弹与治疗技能槽绑定执行记录

目标：数字1及按钮1释放定向飞弹，原治疗移到数字5及按钮5，服务器真实恢复生命。保留数字2火焰弹、3寒冰箭、4奥术飞弹。

架构：平台GAS/Combat负责权威授权、弹体和治疗；项目能力CDO只增加表现语义配置。项目表现目录把新语义解析到公共表现包原生Niagara，沿用平台唯一播放器、预载和取消链。治疗保留原Primary逻辑ID及20%最大生命、10秒冷却合同。UI按授权变化通知更新。

技术：UE5.8、C++/GAS、Monolith Editor Python/Niagara、现有十二生肖AbilitySet及UIProfile。依据：本对话用户“请此技能绑定到角色按钮1技能，原来的治疗技能绑定到按键5上，治疗要能加血量”。本次绑定授权不代表美术人工验收通过。

- [x] 精确修改项目表现语义和作者/核验脚本；新增定向飞弹内容身份，保留现有2—4技能与治疗身份。
- [x] 增量完整Editor构建；通过Monolith制作运行版Niagara及12套授权、UI映射，保存前备份。
- [x] 独立重载检查1/5映射、三尾60度相差、真实资源引用；运行服务器治疗与技能回归。
- [x] 记录原生截图/视频、构建和人工/GPU/联机验收边界；实际Client/Server Cook及容器验证通过。

决定：定向飞弹复用Fireball的单发权威行为，由新CDO配置独立表现语义，不增加执行器或枚举行为；出生与取消均保持现有服务器规则。
决定：运行版拥有独立System/模块，复用已打磨的几何材质；历史审核System不改写，避免审核截图与资产失配。飞行显示覆盖20米/1200厘米每秒的最大行程，并由真实命中/取消结束。

执行方式：本会话按executing-plans逐项执行，不另开实现任务。未提交的其他Combat修改保留。

实际验证：完整Editor目标`202be652-3a50-4565-8e68-d6ba908b5405`有效；Client/Server完整增量目标`2effa18d-5359-4f71-97bf-28f83a8eb12f`通过。原生测试`Native-06d6cdb1-cc16-4f25-9b8f-e265887f1de7`恰好一项成功，真实加载鼠英雄AbilitySet，经平台输入1生成单个权威飞弹；输入5将生命50%恢复到70%，验证上限、10秒冷却和沉默期间拒绝且不消耗冷却。十二生肖保存资产独立重载通过，治疗均补齐Ability.Spell标签。

发布验证：`Saved/Validation/CommonSkillVFX/DirectedMissileSlotBinding/CookBinding.json`读取真实Client/Server IoStore，核对85项权威技能/集合/数值包、12项英雄UIProfile、10项客户端运行VFX包；三项独立Niagara作者Module按引擎IsEditorOnly规则正常剥离，编译结果随System发布。审核资产及服务器VFX泄漏为零。Cook不等于正在运行的服务器已替换。

媒体：同目录`DirectedMissileRuntime_Native.png`、`DirectedMissileRuntime_Native.mp4`和`DirectedMissileRuntime_3xSlow.mp4`为固定镜头原生运行资产预览；视频取飞行阶段22帧，未把编辑器移动预览当成联机操作或命中视频。人工视觉、真实Village/MainArena准入、四档多人GPU仍未验收。本次未主动启动前端或重启其它任务的服务。
