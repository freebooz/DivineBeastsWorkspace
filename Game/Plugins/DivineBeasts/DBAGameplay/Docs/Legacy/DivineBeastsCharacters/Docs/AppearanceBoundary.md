# AppearanceBoundary（外观边界）

Appearance Selection只描述建模/显示选择，不参与阵营、五行、竞技资格或技能能力。

第一版Schema仅预留 BodyVariant、HeadPreset、SkinMarkingPreset 三类逻辑字段；具体允许值必须由真实Definition资产填写。未声明字段一律本地拒绝。

Element、FiveCamp、Faction、Pantheon、KingSeal不属于外观字段。男性/女性等外观差异原则上继续使用同一个HeroDefinitionId + AppearanceVariant，而不是扩成24个Hero ID。
