# ReplicationModel（复制模型）

`AGamePlatformArenaGameState（竞技游戏状态）` 复制MatchIdPublic、ArenaModeId、MatchPhase、阶段修订、阶段起止服务器时间、TeamStates、ObjectiveStates和公开结果摘要。

比赛时钟只复制PhaseStart/Deadline，客户端使用 `GetServerWorldTimeSeconds（获取同步服务器时间）` 本地计算，不做每秒RPC。K/D/A、比分、英雄和Ready均按事件或阶段低频更新。