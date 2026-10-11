#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""CliffRock01宏观三向投射数学/三通道共用坐标/资产边界门禁（无需UE Editor）。

对Material变体的解析元参数进行受控静态检查，并验证连续世界坐标
W' = W + A*sin(2π(F·W))在20米悬崖审核范围内不产生世界空间折叠。
使用矩阵行列式引理 det(I+A⊗∇sin)=1+2π(A·F)cos(...) 检查保持正值，
不同法线方向用同一个W'，不得从BaseColor单独施加扭曲造成PBR错位。
该数学/源码校验不能冒充D3D12真实渲染或人工签收。
"""

from __future__ import annotations

import ast
import math
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]
AUTHOR=ROOT/"Tools/Unreal/Terrain/AuthorCliffRock01MacroVariationUE.py"
EXPECTED_ASSET="M_DBA_CliffRock01_WorldAlignedMacro"


def main()->None:
    text=AUTHOR.read_text(encoding="utf-8")
    tree=ast.parse(text)
    values={}
    for node in tree.body:
        if isinstance(node,ast.Assign):
            for target in node.targets:
                if isinstance(target,ast.Name) and target.id in ("FREQ","AMP"):
                    values[target.id]=ast.literal_eval(node.value)
    if set(values)!={"FREQ","AMP"}:
        raise RuntimeError("宏观材质扭曲频率和世界位移参数未找到")
    freq=values["FREQ"]
    amp=values["AMP"]
    dot=sum(f*a for f,a in zip(freq,amp))
    jac_min=1-abs(2*math.pi*dot)
    if jac_min<.45:
        raise RuntimeError("地形材质世界位置扰动幅度过大，可能产生投影折叠")
    displacement=math.sqrt(sum(v*v for v in amp))
    if displacement>160 or displacement<20:
        raise RuntimeError("宏观去重复位移不符合扫描尺度预算")
    # 20m岩壁平面：X为崖壁厚度轴，Y宽19.5米，Z高13米。
    samples=[]
    for y in (-975,-500,0,500,975):
        for z in (0,650,1300):
            phase=freq[0]*0+freq[1]*y+freq[2]*z
            wave=math.sin(2*math.pi*phase)
            moved=(amp[0]*wave,y+amp[1]*wave,z+amp[2]*wave)
            samples.append(moved)
    if max(abs(v[0]) for v in samples)<30:
        raise RuntimeError("20米崖壁宏观去重复在真实垂直面上不起作用")
    if sum(abs(a[1]-b[1]) for a,b in zip(samples[:-1],samples[1:]))<100:
        raise RuntimeError("宏观坐标扰动发生异常恒定")
    tokens=(
        'M_DBA_CliffRock01_WorldAlignedMacro',
        'CliffMacroWarpFrequency',
        'CliffMacroWarpAmplitudeCm',
        'WorldPosition',
        '"CliffWorld"+name',
        '"BaseColor"',
        '"NormalDX"',
        '"ORM"',
        'for function in calls:',
        'link(absolute,function,"WorldPosition")',
        'originalMaterialPreserved=True',
    )
    for token in tokens:
        if token not in text:
            raise RuntimeError("世界三向坐标与PBR渠道合同未在源码体现："+token)
    # 不允许平台Surface与DedicatedServer硬引用项目CliffRock01内容。
    for relative in (
        "Game/Plugins/GamePlatform/Presentation/GamePlatformSurface/Source",
        "Game/Plugins/DivineBeasts/DBAServer/Source"
    ):
        location=ROOT/relative
        if not location.is_dir():
            continue
        for file in location.rglob("*.cpp"):
            if "M_DBA_CliffRock01_WorldAlignedMacro" in file.read_text(encoding="utf-8-sig",errors="replace"):
                raise RuntimeError("客户端岩壁美术资产非法进入底层或服务器源码："+str(file))
    print("CLIFF_MACRO_WARP_CONTRACT_PASS",
          "amplitude_cm",round(displacement,2),
          "min_world_jacobian",round(jac_min,4),
          "world_scale_cm",2000,
          "samples",len(samples),
          "source_pbr_channels_share_one_warp=True")
    print("CLIFF_MACRO_HUMAN_GPU_PREVIEW_NOT_CLAIMED")


if __name__=="__main__":
    main()
