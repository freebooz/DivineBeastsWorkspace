# StableIds（稳定标识）

当前GamePlatformCore（平台核心）没有现成FGamePlatformGameId/FGamePlatformServerRoleId等稳定ID包装类型，因此本轮不重复发明多套string wrapper（字符串包装器），Runtime采用FName（名称）作为轻量UE表示，并把允许值放在Shared生成Catalog中。

三类ServerRole、六个Experience和五个ArenaMode均由Shared真源生成到C++和Go。Runtime的IsServerRoleId、IsExperienceId、IsArenaModeId与映射查询均消费生成数组。

协议身份不使用C++ enum序号，避免跨语言枚举序号漂移。Stable ID uniqueness（稳定ID唯一性）由Build/Contracts验证脚本检查。
