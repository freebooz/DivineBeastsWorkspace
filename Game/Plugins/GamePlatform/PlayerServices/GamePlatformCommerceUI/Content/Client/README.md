# Commerce Client Content（商城客户端内容模板）

本目录预留 Unreal Editor（虚幻编辑器）真实 Content/Client（二进制客户端内容）资产。

第一版 C++ 可复用模板基类已经建立：
- `UGamePlatformCommerceCatalogScreen（商城目录页面基类）` → 计划资产 `WBP_CommerceCatalog（商城目录蓝图）`。
- `UGamePlatformCommerceProductDetailScreen（商品详情页面基类）` → `WBP_CommerceProductDetail（商品详情蓝图）`。
- `UGamePlatformCommercePurchaseConfirmScreen（购买确认页面基类）` → `WBP_CommercePurchaseConfirm（购买确认蓝图）`。
- `UGamePlatformCommerceOrderStatusScreen（订单状态页面基类）` → `WBP_CommerceOrderStatus（订单状态蓝图）`。

禁止用文本伪造 .uasset（UE二进制资产）。上述 WBP（Widget Blueprint，界面蓝图）必须由 Unreal Editor 创建后，再执行真实 Client Build/Cook（客户端构建/烘焙）与人工 UI 验证。

Widget（界面组件）只绑定 `UGamePlatformCommerceViewModel（商城视图模型）`，不得自行解析 HTTP JSON、判断支付成功或调用 Inventory/Entitlement Grant（背包/权益发放）。
