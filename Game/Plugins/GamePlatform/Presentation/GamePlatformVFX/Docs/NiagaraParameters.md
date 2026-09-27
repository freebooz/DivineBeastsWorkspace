# NiagaraParameters（Niagara参数）

公共参数协议由 FGamePlatformVFXParameterSchema + FGamePlatformVFXParameters 组成，不再把无约束TMap作为唯一协议。支持Float、Vector、Position、Color、Integer、Boolean。

Schema限定名称、类型、最大覆盖数量、Float/Integer范围与Required参数。未知名称、类型不匹配、NaN、越界和缺少Required参数均Fail Closed。

Position与Vector分离；LWC端点/世界位置通过SetVariablePosition写入。Object/Data Interface当前不作为公共请求类型开放，避免不受信任对象注入。