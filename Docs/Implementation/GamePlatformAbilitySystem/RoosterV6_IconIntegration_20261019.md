# SSLM 酉鸡 V6 八技能图标接线与验收记录（2026-10-19）

> 后续状态见 [2026-10-21源包找回与交付记录](RoosterV6_SourceDelivery_20261021.md)：正式原包已完成字节校验并纳入英雄包的仓库交付路径，真实编辑器导入、Profile绑定和游戏画面仍待执行。下文保留2026-10-19历史状态。

## 视觉冻结及实物清单

角色：酉鸡·金鸣，V6历史称号酉鸡·金翎晨鸣。八技能依次为金羽击、破晓、晨鸣波、破晓金鸣、鸣步、金羽、鸣震、晨鸣。八槽UI顺序为 `Primary / Slot1 / Slot2 / Slot4 / Slot3 / Slot5 / Slot6 / Slot7`。具体稳定AbilityId及48源图SHA见同目录`RoosterV6SkillIconImportManifest_20261019.json`。独立图标为1254×1254母稿，64×64审核图及48×48运行图；这些是 ChatGPT 会话已生成的真实PNG，而非Python凭空创建的新概念。

## 现有资产边界

复用 `Game/Plugins/DivineBeasts/ContentPacks/Heroes/DBAHeroPack_Rooster`（鸡英雄表现包）旧版五份Texture2D与现有 `DA_DBA_Rooster_PlaceholderUIProfile`（技能界面配置），不覆盖任何V1图标，不新建相同HeroDefinitionId的PrimaryDataAsset。复用 `UDivineBeastsAbilityBarViewModel`（技能栏视图模型）从权威Loadout和GamePlatformData资源租约异步获得UIProfile及图标。V6所有玩法尚处于PENDING_USER_REVIEW且canGrantToServer=false；本轮只读美术展示，不能向GAS新增授权或修改服务器Cook。

## 分阶段执行和真实门禁

1. 解压用户下载的`SSLM_RoosterV6_8SkillIcons_ReadyForUE58.zip`并原样放入`Game/Saved/ArtReview/RoosterV6/`。脚本会验证ZIP SHA、八个48 PNG SHA及图像分辨率；源图不符停止。
2. 在真实UE5.8 Editor（虚幻编辑器）内由Monolith（编辑器MCP）执行 `Tools/Unreal/UI/ImportRoosterV6SkillIconsUE58.py`；默认`inspect`（只读）。显式`ROOSTER_V6_MODE='import'`（导入）后创建八份真实48×48 Texture2D（纹理），路径为`/DBAHeroPack_Rooster/UI/Abilities/V6/Textures/`，sRGB、UI压缩、无Mipmap，保存验证。
3. 执行`ROOSTER_V6_MODE='bind'`（配置绑定），先读取唯一Profile、做快照及二进制备份，再仅增补缺失的八条软图标引用及只读ArtPreview顺序；原有冲突条目禁止覆盖。
4. 执行`ROOSTER_V6_MODE='verify'`（真实性校验），验证8项真实48纹理、Profile映射、无授权ViewModel保持0槽位，生成`Game/Saved/Validation/RoosterV6IconImport/Verify.json`。该步骤不是完整游戏HUD验收。
5. 在`Village`（新手村）及`MainArena`（竞技场）实测：64×64、48×48分别显示，8槽顺序正确，技能名称与图标一致；加载、换英雄、卸载租约、冷却遮罩等状态与其他UI正常，无无权限技能。用游戏进程截图与日志记录结果，不可用静态脚本替代。
6. 按照项目规范仅提交本轮酉鸡新增文件/经UE保存的资产；其它生肖及正在进行的并行修改全部排除。

## 当前可核验状态

- ChatGPT对话端8份源图、8份64审核图、8份48运行图及完整ZIP已生成并可下载；尺寸及SHA检查通过。
- 向HWebCodex调用`import_conversation_files_to_project`接收图片/ZIP时，返回 `host-provided download URL resolves to a non-public address`，因此远程主机并没有收到原图。
- 本轮已提前制作部署清单与安全导入工具，但没有真实UE纹理、Profile绑定或Village/MainArena截图的验收证据。图片上传成功后才能继续导入，禁止把仅准备脚本误报为已导入。
