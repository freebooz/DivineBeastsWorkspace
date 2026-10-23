# -*- coding: utf-8 -*-
"""读取实际雪Niagara栈，防止旧初始化器覆盖已校准雪花大小与生命周期。

仅用于本机原生资产验收。检查实际启用模块、GPU编译、原生EffectType与固定边界；
不能以该测试代替可见雪花、地形遮挡或GPU毫秒测量。
"""
import importlib.util
import json
from pathlib import Path

spec = importlib.util.spec_from_file_location("snow_native_query", Path(__file__).with_name("VerifySnowNativeGraphsMonolith.py"))
native = importlib.util.module_from_spec(spec)
spec.loader.exec_module(native)
ASSET = "/GamePlatformVFX/Weather/Niagara/NS_GP_Weather_Snow_Detailed"


def main():
    for emitter in ("NE_GP_Snow_Near", "NE_GP_Snow_Far", "NE_GP_Snow_GroundDrift"):
        result = native.mcp_call(9316, "niagara", "get_ordered_modules", {
            "asset_path": ASSET, "emitter": emitter, "usage": "particle_spawn"})
        modules = json.loads(result["result"])
        initializers = [m for m in modules if m["enabled"] and "/Initialization/" in m["script_path"]]
        assert len(initializers) == 1, f"{emitter}: 多个初始化器会覆盖雪花尺寸/生命期: {len(initializers)}"
        assert "/V2/" in initializers[0]["script_path"], "只保留当前校准的V2初始化器"
    diag = native.mcp_call(9316, "niagara", "get_system_diagnostics", {"asset_path": ASSET})
    assert diag["error_count"] == 0, "雪Niagara编译存在错误"
    info = native.mcp_call(9316, "niagara", "get_system_summary", {"asset_path": ASSET, "detail_level": "compact"})
    assert info["system_properties"]["fixed_bounds"]
    assert "NET_GP_WeatherAmbient" in info["system_properties"]["effect_type"]
    assert info["emitter_count"] == 3
    print("VILLAGE_SNOW_PARTICLE_STACK_NATIVE_PASS emitters=3 initializers=3 errors=0")


if __name__ == "__main__":
    main()
