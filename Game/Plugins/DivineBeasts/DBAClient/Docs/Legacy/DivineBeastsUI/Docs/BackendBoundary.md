# BackendBoundary（后端边界）

新增Go业务后端接口：无。  
新增Go微服务：无。

DivineBeastsUIClient不依赖Shared后端DTO，不调用Gateway、PlayerDataService、MatchService或GameServerControlService，也不包含HTTP客户端。

UI缺数据时，应由ApplicationFlow、Arena、Quest、World等业务Owner增加公开的客户端View State / Query Source，而不是创建“UI API服务”。

DBAClient组合层调用的都是UE客户端业务Owner公开接口；角色创建、世界分配、赛后返回等后端交互仍由既有Owner内部承担。

Password/Token/Ticket不得进入UI状态、Widget属性或Telemetry。
