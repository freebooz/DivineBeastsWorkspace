# RPCAndAuthorityValidation（RPC与权威验证）

UGamePlatformRPCAndAuthorityValidator 静态扫描 Server、Client、NetMulticast、Reliable 声明，并检查 GamePlatformDebug 中 Server RPC 是否存在 Shipping 条件门禁。

报告必须标记 Static validation only（仅静态验证）。

静态扫描不能证明调用者认证、Ownership、Rate Limit、重放防护和高价值操作授权正确，这些仍需 Integration/Security tests。