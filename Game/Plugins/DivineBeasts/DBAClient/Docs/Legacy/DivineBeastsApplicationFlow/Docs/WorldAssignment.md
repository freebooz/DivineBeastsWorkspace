# WorldAssignment（世界分配）

客户端世界入口请求只包含 RequestId、CharacterId、DesiredExperienceId、ExpectedCharacterRevision 和可选 PreferredRegion。SessionId 在 Gateway 由认证会话覆盖，客户端不能自报。

GameServerControlService/worldcontrol 负责验证角色、Experience→ServerRole映射、容量/区域和Allocator结果，再返回 AssignmentId、GameServerId、ServerRole、Experience、Map、Region、Endpoint、TicketId、TransferTicket、CharacterId、SessionId。

第一版非竞技世界入口仅支持 OpenWorld.Hub/Main 与 Village.Main/Tutorial/Training。
