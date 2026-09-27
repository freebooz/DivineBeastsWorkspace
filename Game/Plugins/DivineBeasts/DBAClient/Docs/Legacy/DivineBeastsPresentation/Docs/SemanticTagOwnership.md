# SemanticTagOwnership（语义标签所有权）

所有权规则：

- Gameplay Fact（玩法事实）Tag归事实Owner。
- Moba.*表现语义归MobaPresentation。
- DBA.Presentation.*项目表现语义归DivineBeastsPresentationRuntime。
- GameplayCue已有Owner时不重复声明。

当前项目表现源码未恢复Element、FiveCamp、Faction、Pantheon、KingSeal或ElementResonance等旧系统语义。

真实Client/Server Tag Dictionary（标签字典）构建一致性需要UE5.8 Build/Automation证据；当前Runner无UE工具链，因此状态为“未执行”。
