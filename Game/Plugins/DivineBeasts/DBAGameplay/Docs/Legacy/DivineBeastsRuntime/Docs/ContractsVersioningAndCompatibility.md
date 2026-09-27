# ContractsVersioningAndCompatibility（契约版本与兼容）

`Shared/Docs/contract-version.json`保存当前跨端ContractVersion；`Shared/Contracts/Games/DivineBeasts/Schemas/server-catalog.schema.json`的`x-catalog-version`保存项目目录版本；`Shared/Docs/compatibility-matrix.json`声明当前支持范围。当前版本为1.4.0、CatalogVersion为1，兼容范围为闭区间1.2.0至1.4.x。

代码生成把含通配符的闭区间上界确定性转换为运行时半开区间，因此当前客户端—服务器和服务器—后端范围均为[1.2.0, 1.5.0)。超出上界、低于下界或无效SemVer均拒绝兼容；若兼容矩阵未来声明多个不连续区间，当前单区间运行时接口会在生成阶段显式失败，不能扩大为一个包络范围。

FDivineBeastsContractVersion（契约版本查询）把生成值提供给UE Runtime，并实现语义版本区间判断。它不执行网络连接、握手、HTTP或后端协议协商。

真实网络兼容协商以后由Online/Server私有Adapter完成；Runtime只提供稳定查询数据。
