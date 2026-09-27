# NetworkingAndPerformance（网络与性能）

记分板、K/D/A、TeamScore、Hero和Ready均为事件驱动或阶段低频复制；倒计时复制服务器起止时间，不使用Tick RPC（逐帧远程调用）。

性能验证应分别用2/4/6/8/10客户端执行1v1～5v5，记录Server Frame（服务器帧）、CPU、内存、复制带宽、GameState/PlayerState更新与结果提交延迟。当前未获得真实基线，不推断生产容量。