# CharacterSelection（持久角色选择）

客户端只发送 CharacterId、SelectionRequestId、ExpectedCharacterRevision。后端从认证上下文确定 PlayerId，并检查角色存在、归属、状态、Revision、Hero可用性和资格。

成功后返回 FDivineBeastsValidatedSelection（已校验选择），Flow 才更新 SelectedCharacter。SelectPersistentCharacter 与 Arena 的 SelectArenaHero 必须保持不同命名和生命周期。
