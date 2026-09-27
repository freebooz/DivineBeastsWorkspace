# ProjectIdentity（项目身份）

正式GameId：divinebeasts。

正式ProjectId：DivineBeastsArena。

来源：Shared/Contracts/Games/DivineBeasts/Schemas/server-catalog.schema.json中的`x-project-identity`（项目身份）扩展；该文件同时拥有角色—体验和竞技模式上下文映射。

项目身份是跨语言稳定值，Go与C++均由`Backend/internal/tools/contractcodegen`从同一个Shared目录生成；不得在运行时代码、另一份项目目录或构建脚本中复制身份常量。Shared的项目角色、体验、竞技模式和兼容版本共同组成可审计生成输入。

Runtime不允许另一份手写GameId表。
