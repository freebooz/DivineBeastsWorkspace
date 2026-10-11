# -*- coding: utf-8 -*-
"""UE5.8 D3D12真实Shader编译诊断：只处理开发Probe的Material/MI。

特别要求：以UnrealEditor-Cmd.exe -d3d12启动，不得附加-NullRHI。
只编辑本任务自建的/Game/Development/FallingFoliage/Probes测试材质，
不重建或更改GamePlatformVFX及DBAPresentationPack正式.uasset。
编译器无报错并不等于游戏端ShaderMap可用，仍须真实GPU截图验证。
"""
import json
import time
from pathlib import Path
import unreal

BASE = "/Game/Development/FallingFoliage/Probes"
ROOT = Path(unreal.Paths.project_dir()).resolve().parent
OUT = ROOT / "Saved/Validation/FallingFoliage/PBRAtlas20261011/GPUSceneReview/ShaderD3D12WarmupAudit.json"
LIB = unreal.MaterialEditingLibrary
ED = unreal.EditorAssetLibrary
MATERIALS = [
    BASE + "/M_DBA_FoliageShaderIsolation_Solid",
    BASE + "/M_DBA_FoliageShaderIsolation_UV0",
    BASE + "/M_DBA_FoliageShaderIsolation_Opacity",
    BASE + "/M_DBA_PBRMaskSamplerProbe",
]
INSTANCE = BASE + "/MI_DBA_PearPetal_MaskSamplerProbe"
report = {"engine": str(unreal.SystemLibrary.get_engine_version()),
          "materialCompiler": "UE5.8 Editor D3D12, not NullRHI",
          "shaderPlatform": "PCD3D_SM6_expected",
          "materials": [], "instance": INSTANCE, "errors": [],
          "success": False, "gpuVisualPassed": False}

def require(condition, description):
    """高风险资源与身份不匹配时拒绝错误写入。"""
    if not condition:
        raise RuntimeError(description)

try:
    project = str(unreal.Paths.get_project_file_path()).replace("\\", "/")
    require(project.endswith("/DivineBeastsArena.uproject"), "必须在神兽联盟UE5.8工作空间")
    require(report["engine"].startswith("5.8."), "必须使用UE5.8")
    cmdline = str(unreal.SystemLibrary.get_command_line()) if hasattr(
        unreal.SystemLibrary, "get_command_line") else "unknown"
    report["commandLine"] = cmdline
    require("-NullRHI" not in cmdline and "-nullrhi" not in cmdline.lower(),
            "本诊断不允许空图形接口NullRHI")
    for path in MATERIALS:
        mat = ED.load_asset(path)
        require(isinstance(mat, unreal.Material), "独立Probe母材质不存在："+path)
        started = time.monotonic()
        result = LIB.recompile_material(mat)
        failures = [str(x) for x in (result or []) if "error" in str(x).lower()]
        require(not failures, "Probe Shader编译出现报错：" + path + ": " + str(failures))
        require(ED.save_loaded_asset(mat, only_if_is_dirty=False),
                "真实Shader诊断材质保存失败：" + path)
        report["materials"].append({
            "path": path, "recompileResponse": str(result)[:1100],
            "seconds": round(time.monotonic() - started, 2),
            "saved": True,
        })
        print("SHADER_D3D12_WARMUP_MATERIAL",path,flush=True)
    mi = ED.load_asset(INSTANCE)
    require(isinstance(mi, unreal.MaterialInstanceConstant), "PBR隔离MI缺失")
    parent = mi.get_editor_property("parent")
    require(parent is not None and
            parent.get_path_name().startswith(BASE + "/M_DBA_PBRMaskSamplerProbe."),
            "PBR试验MI父级异常，不得修改")
    if hasattr(LIB, "update_material_instance"):
        LIB.update_material_instance(mi)
    require(ED.save_loaded_asset(mi, only_if_is_dirty=False), "隔离MI不能保存")
    report["success"] = True
    print("SHADER_D3D12_WARMUP_DONE",len(report["materials"]),flush=True)
except Exception as error:
    report["errors"].append(str(error))
    print("SHADER_D3D12_WARMUP_FAILED",str(error),flush=True)
    raise
finally:
    OUT.parent.mkdir(parents=True,exist_ok=True)
    OUT.write_text(json.dumps(report,ensure_ascii=False,indent=2)+"\n",encoding="utf8")
