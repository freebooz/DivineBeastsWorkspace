# SSLM 卯兔 V6 图标候选交付说明

本包仅含**24张独立原创程序化绘制概念候选**（1024×1024）、8张系统预选1024母版、8张64×64审核PNG与8张48×48运行候选PNG，及机器可读清单。程序化概念稿不等于经过画师精修的电影级正式美术；系统预选不等于人工优选签字。色彩和轮廓符合项目初步方向，但尚需人审后由美术精修或高质量图生图替换。

生产原则：兔的稳定技能ID与SlotId沿用仓库V6审查稿；不能以改名为由变更AbilityId；第4位对应Slot4大招，第5位对应Slot3移动。旧兔英雄包五张V1资源一律不得覆盖。本包不包含任何`.uasset`，未调用Monolith，未进入项目真实编辑器，未声称新手村或战斗HUD显示完成。

验收建议：先打开 RabbitV6_24Candidates_ReviewBoard.jpg 逐项对比A/B/C，在回复中使用 `1A 2B 3C ...` 指定8项。生成正式8图之前优先打磨第4大招与第8月影。正式应独立高品质2048母版，重新导出64和48，进入引擎时通过编辑器/Monolith创建并验证真实Texture2D。小图实际像素文件已在 Review_64 与 Runtime_48 子目录，切勿用审查板放大截图作为资产源。

放置规则：将各PNG复制至`Game/Plugins/DivineBeasts/ContentPacks/Heroes/DBAHeroPack_Rabbit/SourceArt/UI/SkillIcons/V6/`，导入真实`.uasset`置于`Content/UI/Abilities/`，并以清单中的softObjectPath绑定兔唯一UIProfile。保存编辑器资产、冷重载、真UDivineBeastsAbilityBarViewModel只读投影、64/48审图、新手村截图、多人联机、Cook与服务器排除另行留证。
