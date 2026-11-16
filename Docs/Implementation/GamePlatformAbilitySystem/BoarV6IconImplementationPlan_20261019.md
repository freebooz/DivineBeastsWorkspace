# SSLM 亥猪 V6 八技能图标制作与 UE5.8 集成实施计划（2026-10-19）

状态：设计身份核对通过；8枚64/48像素概念预览PNG已在ChatGPT会话中生成，但Windows工程尚未收到文件，UE原生纹理0/8。不能将概念看板116像素切片冒充独立1024像素高质量母版或正式批准图标。

## 一、设计与审批边界

唯一角色：Hero.Zodiac.Boar（亥猪·岩甲玄鬃）；定位续航坦克／低血反扑。冻结输入显示顺序Primary、Slot1、Slot2、Slot4、Slot3、Slot5、Slot6、Slot7。被动“战意”不占8主动槽。

| 显示顺序 | 输入槽 | V5当前名 | V6设计名 | 稳定AbilityId | 新纹理文件名（不含扩展名） |
|---|---|---|---|---|---|
| 1 | Primary | 獠牙撞 | 獠牙撞 | dba.ability.boar_eight_tusk_ram@1 | T_DBA_Boar_S01_TuskRam_V6 |
| 2 | Slot1 | 怒锤 | 怒锤 | dba.ability.boar_eight_break_armor@1 | T_DBA_Boar_S02_RageHammer_V6 |
| 3 | Slot2 | 重压波 | 重压波 | dba.ability.boar_eight_pressure_wave@1 | T_DBA_Boar_S03_PressureWave_V6 |
| 4 | Slot4 | 连踏 | 玄鬃裂地 | dba.ability.boar_eight_earthsplit_mane@1 | T_DBA_Boar_S04_ManeEarthsplit_V6 |
| 5 | Slot3 | 獠冲 | 獠撞 | dba.ability.boar_eight_boar_charge@1 | T_DBA_Boar_S05_TuskCharge_V6 |
| 6 | Slot5 | 岩甲 | 岩甲 | dba.ability.boar_eight_iron_mane@1 | T_DBA_Boar_S06_IronMane_V6 |
| 7 | Slot6 | 震鬃 | 震鬃 | dba.ability.boar_eight_roar_stun@1 | T_DBA_Boar_S07_ManeShock_V6 |
| 8 | Slot7 | 鬃吼 | 鬃吼 | dba.ability.boar_eight_rage_focus@1 | T_DBA_Boar_S08_ManeRoar_V6 |

V6的“玄鬃裂地”和“獠撞”尚待人工批准更名；正式DisplayName仍采用V5“连踏”和“獠冲”。其他6项相同。独立被动“战意”不占八槽。旧ID中break_armor、roar_stun等历史单词不可被理解为当前V6机制已确认。

## 二、先保证图标质量：独立原图+优选

1. 以玄墨黑、青黑岩石、深铜金、琥珀暖光为主，兼顾青玉饰面与温润广角光泽。禁粗糙颗粒、过度锐化、火焰盖过主形体及其他游戏图标的照搬。
2. 八个核心轮廓必须互不相同：正面獠牙撞、重甲前蹄重锤、圆环压力波、三段地裂、侧身高速冲锋、肩背护盾、放射刚鬃、仰头怒吼。不得重复八张猪头；尤其第1/5及第3/4及第7/8项务必形成动作差异。
3. 每技能独立生成3张A/B/C候选，先512—1024像素独立母版（建议1024），各候选构图不同，人工审查选择1张，八项共8张正式母版；不得将设计看板裁切误当成高清正式母版。
4. 每张母版制作真正64×64审核PNG与48×48正式PNG，缩放一次优质采样，再人工检查小尺寸主体形状、Alpha边缘与亮部是否过曝；不烘入技能框、文字、快捷键和冷却蒙版。检验PNG文件头、实际尺寸、8个SHA256与当前色彩规范。
5. 本次用户会话已制作的8张ConceptReview 48/64 PNG来自概念看板116px裁切，仅供UI早期布局和辨识预审；正式成品须换成上述独立原图。附当前受阻的会话下载包名称SSLM_Boar_V6_8_Concept48_64_ReviewPack.zip。

## 三、原生UE导入与配置绑定

6. 源素材归属于 Game/Plugins/DivineBeasts/ContentPacks/Heroes/DBAHeroPack_Boar/SourceArt/UI/SkillIcons/V6/；独立母版放Masters、正式48像素放48、正式64像素放64；新原生Texture2D放在/DBAHeroPack_Boar/UI/Abilities/<资产名>；不碰旧V1五图。
7. 先用官方受信任文件来源导入会话ZIP，确保远程磁盘有16个真实PNG。当前原生附件传输返回 host-provided download URL resolves to a non-public address，因此仍未成功进入Windows。不得使用私有网络URL、凭据或伪造UAsset绕过传输限制。
8. 重新确认UE5.8 Editor和Monolith 0.23.0正在运行于Game/DivineBeastsArena.uproject。使用Monolith ui.import_texture_from_bytes为八张真正48 PNG创建UTexture2D，验证sRGB、UI压缩、NoMipmaps、NeverStream、Bilinear和Clamp，保存后独立冷重载逐个核验SourceSize=48×48及路径；项目AGENTS规定必须使用Monolith编辑器能力生成用户界面原生资产。
9. 使用现有Tools/Unreal/UI/AuditImportedHeroSkillIconSmallSizes.py --plan <IntegrationPlan.json>校验64/48源图SHA、尺寸与近似并留人工审查标记；不采用Monolith capture_scene_preview(widget)已知错误Gamma预览，使用客户端原生截图或SourceToSlate RHI回读。
10. UI只使用现有UDivineBeastsAbilityUIProfile。已有DA_DBA_Boar_PlaceholderUIProfile必须先真实读回与备份，保留全部占位Entry，再按稳定AbilityId增加8个仅视觉候选Entry，并按Hero.Zodiac.Boar建立唯一主资产；不要创建第二份同主资产身份。独立美术预览使用ArtPreviewAbilityIds指定8项顺序。
11. 接入既有UDivineBeastsAbilityBarViewModel与UDivineBeastsAbilityBarPanel，复用GamePlatformData两阶段世界租约及事件回调；只读预览必须显示“仅美术预览、不可施放”并在RequestActivateSlot中拒绝，不创建本地假GAS授权。正式GAS技能待独立审批。
12. 按当前Docs/Implementation/GamePlatformAbilitySystem/HeroSkillIconImportWorkflow.md复用CompleteImportedHeroIconProfilesMonolith.py的inspect/apply/reload/verify顺序，应用前核对ProtectedFiles.json与OriginalEntries_Boar.json；所有相同主资产/同AbilityId错误都失败关闭。

## 四、HUD与回归门禁

13. 使用保存的原生Widget完成48与64像素实际显示检查：技能栏8槽总高≤65像素，3像素施法条及气势条无遮挡，源图不加锐化、不因截图Gamma问题改变RGB，字体/底座保持原设计。
14. 使用PreviewImportedHeroIconsInVillageMonolith.py的open/show/capture/cleanup在Standalone新手村展示只读美术层，获取真实战斗HUD和关卡帧截图，清理本次UI与PIE；正式Village/MainArena联机需要服务器版本/授予/Client Cook/Server排除额外验收。
15. 覆盖首次选择Boar、换角色、旧异步迟到、退出、重生、断图、双本地玩家、PIE终止等场景。只有真正通过图标质量人工签收、Profile/VM运行及Client/Server Cook，才能报告完整交付。

## 五、当前准确状态

- 角色V6八稳定AbilityId与名称：已从真实项目设计读取；V6设计尚处PENDING_HUMAN_APPROVAL。
- 8张独立1024高清正式图：未完成；目前只有设计看板的8个116px概念切片与衍生的64/48预审PNG。
- PNG会话下载包：已生成；向远程Windows传输：失败，尚未落地。
- 新8张原生Texture2D：未创建；旧V1五张与旧Profile仍保留。
- UIProfile的Boar V6八项映射、ViewModel真实加载、Village HUD、MainArena、Cook：均未按本批正式资源验证。
- 可继续执行条件：官方来源文件成功进入工作空间，并完成独立高分辨率正式原图/人工优选。应用脚本不得绕过这些门禁。

## 六、输出质量

正式交付应有8×3候选对照、8张独立1024母版、8+8张48/64成品、8个真正UE纹理、完整路径/SHA及Monolith冷读回、唯一UIProfile映射、预览/联机分开记录、原生HUD截图与人工签收。不得以本计划文档或本批概念缩图声明整个任务完成。
