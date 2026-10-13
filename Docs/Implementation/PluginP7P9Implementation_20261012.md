# 第三批 P7–P9：表现、玩家服务、生肖内容与交付门禁

项目：DivineBeastsWorkspace（神兽联盟工作空间）；UE5.8；2026-10-12；状态：执行中。

## 执行范围及并发边界

本轮遵照用户明确指令转入第三批，不继续开发P5/P6玩法、不将其48项原生测试当作第三批的完成证据。已有构建作业wc_job_eDvlxJI_S-K_h2GH仅观察、复用适用产物并核对新旧输入，不重复发起、不终止。现有角色工作室、选角UI、桃花VFX、英雄定义和DefaultGame.ini已有修改，本轮不覆盖。固定在用户指定主工作区增量实施，采用路径级所有权、读版本围栏和隔离开发资产，不创建缺少当前真实UE资产的平行工作树。

## 分步计划及完成合同

- [ ] P7-0：读取规范/插件/资源与历史审核入口，建立输入哈希基线和本轮证据目录。
- [ ] P7-1：建立可打开、可重复运行的音效审核入口，覆盖2D/世界位置/附着、衰减/并发及停止；复用GamePlatformSFX既有执行器，测试音频与正式游戏音频品质分开记录，不复制混音器。代码优先回归测试，真实音频资产由UE导入。
- [ ] P7-2：复用鸟群/Fauna/落叶/Weather/Snow/PCG Gold既有场景，执行真实资产回读与专项原生测试，缺失或未运行时阻断，不伪称图形/声音/性能已通过。UI资产创建/修改仍只经Monolith；保护本轮外正在编辑的UI。
- [ ] P8-1：建立一个共享HeroShowcase（英雄展示）关卡，按12个已有Appearance、材质、骨骼、图标与肖像资产逐项校验，真实UE重新加载并核对12个展示对象；明确Manny/Quinn原型与最终生肖三维美术区别。
- [ ] P8-2：对Inventory/Equipment/Progression/Entitlement/LiveOps/CommerceUI（背包/装备/成长/权益/运营/商业界面）执行适用原生测试，缺DLL时定向构建后重试；不调用真实支付、生产账户或伪造服务端余额。适用时采用仓库已有业务后端沙箱测试。
- [ ] P9-1：全64插件逐项绑定真实入口、适用G0–G5及缺口；复用GamePlatformDeveloperTools的ReviewCase/Report，不新增第二个审核系统。建立测试结果/输入哈希/实际资产/人工签审分离的外部聚合门禁及回归测试。
- [ ] P9-2：执行当前结构、模块、资源、自动化和可获得的构建/发布检查；不让旧证据、零测试、仅文件存在或单机检查替代当前全功能验收。
- [ ] P9-3：输出能由人工打开的审核地图、声音/模型/特效清单、逐插件状态和未完成项。人工审核只能由用户实际操作确认，自动流程保持待审核；不给不存在的签名、设备预算或完整R4成熟度。

## 数据和目录归属

- 平台代码仍在现有GamePlatformSFX/UI/Presentation/Weather/Surface/PCG和PlayerServices各插件，不放生肖身份。
- 具体生肖外观只从既有DBAHeroPack_*与DBAContentPack_Common读取；原型资产保留。
- 新演示资产放在/Game/Development/Foundation/PluginReviews之下，利用已存在的Foundation发布排除，不改当前生产地图、不意外扩大Cook范围。
- 工具与校验：Tools/Unreal/PluginReviews、Tests/Assets、Build/Validation中职责明确的维护脚本。
- 本轮原始证据：Saved/Validation/PluginP7P9_20261012；正式报告：Docs/Implementation/PluginP7P9ExecutionResult_20261012.md。

## 已记录的实施裁决

- Ruling：补建原规划且已被天气作者脚本/配置引用的DBASFXPack_Core（项目独立音效内容包），不新建音效执行插件。原因：现有3份WAV只在Village SourceArt，不能直接进入服务器地图内容，也不能挤入VFX内容包。完成真实导入后内容包从17变18，总插件从64变65，代码插件仍47；台账和数量门禁以实际登记N计算，不隐藏数量变更。若导入未成功则标记未完成，不能宣称非空内容交付。
- Ruling：玩家服务在Editor/Client显式装配以便真实使用和演示；服务端仅启用共享/ServerOnly的Entitlement、Equipment和Progression，CommerceUI/LiveOps/SFX保持客户端专属，不发生生产支付或账本修改。
- Ruling：PCG原生回归中的TArray自身引用Add导致Editor断言；先复制插槽值再构造重复身份，保留重复身份应被拒绝的原断言，属于测试夹具修复而非跳过生产规则。

## 不可外推的验收边界

编译成功不等于包体生成；原生测试不等于目标设备效果；审核图存在不等于完整视觉验收；各玩家服务单元测试不等于真实后端事务闭环；生肖原型不等于12套最终3A英雄；P5/P6尚未完成的生产竞技和联机准入继续独立标记。人工通过必须有实际审核者与观察证据，软件不得代签。
