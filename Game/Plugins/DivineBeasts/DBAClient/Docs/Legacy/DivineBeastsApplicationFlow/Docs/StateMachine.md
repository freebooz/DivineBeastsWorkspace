# StateMachine（状态机）

权威状态机属于 GamePlatformApplicationFlow。

主路径：
Boot→Initialize→Authentication→LoadProfile→LoadRoster→CharacterEntry→ValidateSelection→RequestWorld→TransferWorld→WorldReady→InWorld。

CharacterEntry 可进入 CreateCharacter；Create 成功后立即进入持久角色 Selection Validation。RequestWorld/TransferWorld 失败可进入 Recovering；恢复失败超过上限回 CharacterEntry。

InWorld 可重新 RequestWorld，用于新手完成后切入 OpenWorld、PostMatch 返回世界等场景。
