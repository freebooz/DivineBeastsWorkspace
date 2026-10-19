# 卯兔V6八技能图标候选、评审、导入及技能栏接线（2026-10-19）

对象：Hero.Zodiac.Rabbit（卯兔·踏月玉灵）。用户清单中的“虎英雄内容包”依上下文按兔英雄内容包更正，不修改虎包。
状态：冻结V6候选八技能的身份和槽位；V6尚未正式GAS批准，以下不代表服务器技能已经授予。

## 冻结八技能与稳定AbilityId
1 月辉弹（Primary；dba.ability.rabbit_eight_jade_moon_shot@1；V5：月辉弹；系统预选：A）
2 玉刃（Slot1；dba.ability.rabbit_eight_crescent_arc@1；V5：双月斩；系统预选：C）
3 落英月雨（Slot2；dba.ability.rabbit_eight_falling_petals@1；V5：落英雨；系统预选：B）
4 千月回环（Slot4；dba.ability.rabbit_eight_moon_bloom@1；V5：返影斩；系统预选：A）
5 踏月（Slot3；dba.ability.rabbit_eight_moon_leap@1；V5：踏月；系统预选：B）
6 回月（Slot5；dba.ability.rabbit_eight_jade_moon_shield@1；V5：回月；系统预选：A）
7 月痕（Slot6；dba.ability.rabbit_eight_lunar_bind@1；V5：绊月刃；系统预选：C）
8 月影（Slot7；dba.ability.rabbit_eight_jade_heal@1；V5：回春露；系统预选：A）

独立被动月闪不占八技能栏；第4终极Slot4、第5位移动Slot3不可互换。第4千月回环及第8月影不得沿用历史治疗机制。

## 九阶段实际进度
1 已冻结八技能候选设计及稳定ID。V6正式机制、伤害和技能授权仍待策划审核。
2 ChatGPT工作容器已制作24张1024程序化候选（三版/技能），另有8张64与8张48预审PNG。仅为简化概念图，不是电影级正式图标。该二进制包未进入此Windows工作树。
3 人工优选尚未完成；临时预选1A 2C 3B 4A 5B 6A 7C 8A仅供比较，不代表用户批准。
4 2048电影级正式母版与8套经审核纹理未完成。须在人工优选后重新精修。
5 UE5.8导入尚未完成。原兔包有5张V1 Texture2D与既有占位Profile，必须保留不覆盖。正式新纹理应以对应T_DBA_Rabbit_Sxx_*_V6导入DBAHeroPack_Rabbit/Content/UI/Abilities；不伪造uasset。
6 不另建重复主资产；正式签审后更新唯一DivineBeastsAbilityUIProfile:Hero.Zodiac.Rabbit，以稳定AbilityId和非空中文说明建立软纹理引用。
7 复用既有UDivineBeastsAbilityBarViewModel：服务器OwnerOnly授权快照 => GamePlatformData异步加载UIProfile/图标 => HUD真实已授权槽位；禁止凭图标直接授予技能。
8 外部16张64/48文件已完成初步文件尺寸和hash验收，UE中实际DPI、颜色Gamma、冷却灰显、暗亮背景均未验收。
9 尚未在Village和MainArena真实HUD运行，不宣称成功；待引擎和玩法批准后做UE原生截图、两客户端与专服Cook。

## 阻塞与下一步
2026-10-19 Windows工程诊断：UnrealEditor未运行，HWebCodex的Monolith MCP提供者列表为空；其代理进程存在不代表可以执行编辑器操作。
会话文件到工程的原生导入因host-provided download URL resolves to a non-public address失败；未发生部分导入，也未在项目生成任何新图标。需使用支持的资源交付通道或用户将候选ZIP解压到工程。
先让用户人工选定8图，再委托电影级艺术精修；只有高保真母版经64/48人工签收后，才能导入真实UE引擎。
预期贴图属性为sRGB、UI LOD、UI压缩、NoMipmaps、NeverStream、Bilinear、Clamp，属性和冷重载必须由真实UE5.8证据支持。
保持已签审的技能栏整体高度≤65像素、图标48、3px施法条与3px气势条不受图标影响。禁止复制平台层资源管理器和UI ViewModel。
当前工作树还有其他英雄及共用UI并发修改，本轮仅新增兔的独立设计文档与机器可读清单，不应带入其他更改。

验收闭环：24独立候选+人工选择结果+8正式母版+8张64+8张48+每图SHA+8真实Texture2D+唯一UIProfile冷重载+授权事件链+Village与竞技HUD截图+客户端/服务端Cook。
