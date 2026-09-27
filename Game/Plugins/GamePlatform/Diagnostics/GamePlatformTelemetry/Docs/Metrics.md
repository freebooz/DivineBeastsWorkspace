# Metrics（指标）

第一版定义 server.frame_ms、server.active_players、server.active_ai、server.memory_bytes、server.telemetry.buffer_depth、server.telemetry.dropped_total、client.frame_ms、client.loading.duration_ms、client.telemetry.buffer_depth、client.telemetry.dropped_total、backend.request.duration_ms、backend.request.error_total。

允许低基数 Label（标签）：build_version、environment、platform、server_role、region、arena_mode、result、error_code、service、operation。其中 service/operation 只允许稳定服务名和稳定路由模板，不允许 RequestId、PlayerId 或动态 URL。

高基数身份字段禁止作为 Metric Label；如确有调查需要，应作为经过批准的 Event 属性，并受 Privacy Class（隐私等级）约束。