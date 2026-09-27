# PartyAndMatchmakingBoundary（组队与匹配边界）

Party（组队）在进入Arena前由Go MatchService（匹配服务）处理。队伍分配算法保证Party原子性，不在Arena服务器中拆分、重组或重新匹配。

客户端匹配请求仅允许ArenaModeId、PartyId、PreferredRegion和ClientRequestId；MMR、HiddenRating、TrustScore、Penalty等隐藏权威值不属于客户端请求模型。