# 英雄技能图标导入后的接线与验收

本流程落实用户2026-10-19指定的顺序：真实纹理导入 → 唯一Ability UI Profile → 既有技能栏ViewModel → 64/48小尺寸 → 新手村/战斗HUD原生显示。只读美术预览由用户明确选择，不等于V6玩法授权或联机验收。

## 当前实现入口

- `Tools/Unreal/UI/CompleteImportedHeroIconProfilesMonolith.py`：在Monolith `editor.run_python` 内以 `PIPELINE_MODE=inspect/apply/reload/verify` 执行。`PROFILE_PLAN` 指定一次导入的结构化清单；默认读取本轮 `Saved/Validation/HeroSkillIconWorkflow20261019/IntegrationPlan.json`。默认只读，不凭目录存在自动注册英雄。
- `Tools/Unreal/UI/AuditImportedHeroSkillIconSmallSizes.py --plan <IntegrationPlan.json>`：校验64与48的真实PNG及SHA、去重与近似，生成原尺寸对照图及无交互UISpec。源图和Runtime纹理保持原有RGB，不加锐化/调色。
- Monolith `ui.build_ui_from_spec`：在各英雄自己的 `/DBAHeroPack_*/UI/Review/WBP_DBA_*_SkillIconReviewV6` 创建、编译并保存审查Widget，保持内容包边界。每份29个控件，包含16个Image；无Button、无输入、无技能授予。
- `Tools/Unreal/UI/PreviewImportedHeroIconsInVillageMonolith.py`：用户授权后，在单机PIE内以 `PREVIEW_MODE=open/show/capture/cleanup` 执行；`PREVIEW_HERO` 选择已核验英雄。只读打开新手村，使用 `GameModeBase`，实例化已保存的真实战斗HUD和审查层，使用原生 `Shot SHOWUI` 截图，结束后移除本次UI与相机并停止PIE。

这些均为第三层编辑器制作工具，不是新增运行时播放器、VM、资产管理器或授权来源。UI蓝图和Profile写操作必须由Monolith进入真实编辑器执行，不能用普通文件工具伪造uasset。

## 清单合同

`IntegrationPlan.json` 的 `heroes` 每项包含：`hero`（稳定英文英雄名）、`heroId`（现有Hero定义ID）、`profilePath`（唯一Profile包名）、`profileFile`（工作空间相对uasset位置）及8个 `entries`。条目字段 `abilityId` 必须匹配现有稳定ID；`displayNameZh` 沿用批准名称；`descriptionZh` 标明候选展示边界；`texturePath` 指向真实已导入资源；`png48/png64` 与 `sha48/sha64` 描述实际源PNG。路径必须位于工作空间，不能按中文名称推导资源。

`ProtectedFiles.json` 保存本次运行前已有纹理和权威资产SHA，Profile允许增补但原条目必须逐项保留。`OriginalEntries_*.json` 是接线前表现快照；仅需修改的Profile另保留二进制备份。遇到同ID不同显示或图标映射时停止，禁止静默覆盖。

## 既有ViewModel接线

复用 `UDivineBeastsAbilityBarViewModel::HandleLoadoutChanged` 的真实快照与订阅链，根据 `DivineBeastsAbilityUIProfile:HeroDefinitionId` 查询唯一主资产，再通过GamePlatformData两阶段异步租约加载Profile和图标。`RefreshSlots` 用已授权的 `Grant.AbilityId` 调用Profile的 `FindEntry`；新条目只补充只读显示。

初轮仅增补Profile，没有改变VM C++、Gameplay或服务器资产。后续用户追加所选角色图标要求后，VM/技能栏增加下文的显式只读模式；普通无Loadout且未启用美术模式的VM仍保持0槽位。原生回归使用已有Profile校验、图标投影、资源切换/取消及资格事件测试；不得把类名或静态文字当作事件链已验证。

## 小尺寸与显示边界

源图审核分别使用原生64与48 PNG；HUD显示使用正式48像素Texture2D，同时设定64和48逻辑像素Image，以检查实际双线性放大和原尺寸效果。保存控件的真实布局尺寸、白色乘色及运行时DPI；不通过修改源色、字号或整页缩放修正显示。

技术去重通过不能替代辨识度签收；近似对及人工意见保留。禁止已知重复Gamma的Monolith widget截图路径。只读Standalone预览明确无Village/MainArena外服目标、不授予技能；联机前端另外核对真实客户端与两个角色服务器的构建输入、Cook/Stage、PID、准入及复制合同。

## 本轮范围与记录

首轮已导入V6资源的子鼠、寅虎、巳蛇、丑牛共32项，证据在 `Saved/Validation/HeroSkillIconWorkflow20261019/`。后续辰龙新增8项，证据在 `Saved/Validation/DragonV6Import20261019/`；合计五英雄40项。其余英雄不因本规则生成空Profile或假图标。图像导入阶段的早期记录保持历史意义，后续Profile/HUD状态以各轮分阶段结果为准。

新增牛包 `SSLM_OxV6_8Review64_8Runtime48.zip` 的16张PNG与既有源逐字节相同，8张原生UE纹理导入源SHA匹配；按幂等接收复用，不重写相同资源。包内 `DERIVED_FROM_EXISTING_PROJECT_OX_ART` 和 `PENDING_HUMAN_ART_REVIEW` 保持不变。

## 所选角色到实际技能栏（用户2026-10-19追加要求）

`UDivineBeastsAbilityBarPanel::bPreviewSelectedHeroIcons` 是已授权美术阶段的显式开关，由Monolith设置真实技能栏资产。开启时，同一ViewModel绑定本地玩家UI子系统的选择事件，首次读取完整快照，随后根据 `SelectedHeroDefinitionId` 加载本英雄唯一Profile和图标。空选择/注销清空显示，不使用硬编码默认英雄。关闭后仍沿用真实Loadout/GAS授权路径。

Profile新增可空 `ArtPreviewAbilityIds`；非空必须明确八项唯一稳定ID，且均属于既有Entries。固定顺序为普通、攻击一、攻击二、终极、移动、防御、控制、特色，对应Primary/Slot1/Slot2/Slot4/Slot3/Slot5/Slot6/Slot7。它只是客户端美术顺序，不生成AbilitySet。当前六项占位与V6八项并非相同身份，不把V6图冒充火球、治疗等占位技能。

美术模式八槽均禁用，原生激活入口再次拒绝，隐藏快捷键并显示“仅美术预览、不可施放”。Profile/图标共用既有GamePlatformData两阶段租约；换账号、角色、失活和销毁取消旧代次并清理旧显示。新增原生回归 `DivineBeasts.UI.Resources.SelectedHeroIcons.ReadOnlySelectionLifecycle` 验证首次选择、换角色、旧图迟到、注销、重新显示及重复绑定。实际构建、测试、HUD与联网状态分别留证。

预览工具可传 `PREVIEW_OUTPUT` 将后续英雄证据放入自己的工作空间目录。`show` 调用真实技能栏的 `PreviewHeroIcons` 入口，同一VM投影八图；不直接给槽位Image改画刷。64/48源图工具按实际清单统计PNG数量，单英雄为16份。
