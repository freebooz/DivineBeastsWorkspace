# -*- coding: utf-8 -*-
"""兼容旧CI入口：SSLM UI清单验证改为读取版本化C++ Catalog与真实WBP。

原32份声明式设计仍可通过--require-legacy-specs显式检查，但不再作为UI可用性门禁。
本入口只运行源和资产静态检查，不生成/篡改任何Widget Blueprint。
"""
from ValidateUICatalogNativeAssets import main

if __name__ == "__main__":
    main()
