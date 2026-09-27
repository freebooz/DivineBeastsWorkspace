# ContractsCodegen（协议代码生成）

`Shared/Contracts`是UE与Go跨语言契约唯一真源；项目身份、目录版本和竞技模式上下文位于`Schemas/server-catalog.schema.json`的`x-*`扩展，版本与兼容边界位于`Shared/Docs`。

使用仓库唯一登记的生成器`Backend/internal/tools/contractcodegen`。从`Backend/`执行`go run ./internal/tools/contractcodegen -workspace-root=..`生成离线Go/C++目录；使用`-check`只读检查生成物新鲜度。生成器校验角色—体验、竞技模式—角色/体验映射和兼容矩阵，拒绝缺失、重复或越界身份。

C++目录输出至`Shared/Generated/Cpp/Games/DivineBeasts/DivineBeastsCatalog.generated.hpp`，Go目录输出至`Backend/generated/divinebeasts/catalog_generated.go`。`GeneratedRevision`是上述六个项目Schema、契约版本和兼容矩阵源文件的路径稳定SHA-256；修改任一输入都会改变生成版本。UE私有适配器只消费生成符号，不另建项目Catalog。

正式Proto/OpenAPI绑定由同一工具的`-official`模式调用锁定工具；当前环境未验证这些外部工具链与UE链接。离线目录`-check`通过不等同于Proto/OpenAPI生成、UE编译、联机互通或发布验收。
