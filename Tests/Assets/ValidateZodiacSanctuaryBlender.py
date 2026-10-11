# -*- coding: utf-8 -*-
"""从空白 Blender 会话独立读取保存的神兽圣境，验证真实几何和交付文件。

只读检查项目层前端美术，不产生 UE 资产或改变运行态。缺少 BLEND、
FBX、纹理、六尊雕塑或角色落脚空间时明确失败；渲染图另由人工视觉复核。
"""
import argparse
import json
import math
import sys
from pathlib import Path

import bpy
from mathutils import Vector


def main():
    """打开真正保存的制作件；--python-exit-code 1 使异常不能伪装成功。"""
    parser=argparse.ArgumentParser()
    parser.add_argument('--source',required=True)
    parser.add_argument('--require-render',action='store_true')
    args=parser.parse_args(sys.argv[sys.argv.index('--')+1:])
    root=Path(args.source).resolve()
    blend=root/'DBA_ZodiacSanctuary.blend'
    assert blend.is_file(), '尚未生成真实 Blender 场景：'+str(blend)
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.wm.open_mainfile(filepath=str(blend))
    manifest=json.loads((root/'SceneManifest.json').read_text(encoding='utf-8'))
    expected={'Tiger','Dragon','Deer','DragonTurtle','Phoenix','Qilin'}
    sculptures={o.name.removeprefix('Sculpt_DBA_') for o in bpy.data.objects if o.name.startswith('Sculpt_DBA_')}
    assert sculptures==expected, '六尊神兽雕塑不完整：'+repr(sculptures)
    assert all((root/o['fbx']).is_file() and (root/o['fbx']).stat().st_size>10000 for o in manifest['objects']), 'FBX 缺失或不是完整几何交付'
    for record in manifest['objects']:
        obj=bpy.data.objects.get(record['name'])
        assert obj and obj.type=='MESH', '保存件缺少真实网格：'+record['name']
        assert len(obj.data.vertices)==record['vertices'], '保存件与导出清单顶点数量不符'
        assert len(obj.data.polygons)>0 and obj.data.uv_layers, '网格缺少面或 UV'
        assert all(math.isfinite(value) for vertex in obj.data.vertices for value in vertex.co), '顶点含无效数值'
    for record in manifest['materials']:
        mat=bpy.data.materials.get(record['name'])
        assert mat and mat.use_nodes, '保存件缺少 PBR 材质'
        assert any(n.type=='BSDF_PRINCIPLED' for n in mat.node_tree.nodes), '材质没有 Principled 着色器'
        if record['texture']:
            assert (root/record['texture']).is_file(), '材质源纹理未交付'
    # 中央两米高角色占用区只应命中脚下平台，不应被石兽、枝干等遮住。
    deps=bpy.context.evaluated_depsgraph_get()
    hit,point,normal,face,obj,matrix=bpy.context.scene.ray_cast(deps,Vector((0,0,2.2)),Vector((0,0,-1)),distance=4)
    assert hit and -.08<=point.z<=.14, '中央角色落脚台缺失或被场景对象占据'
    assert bpy.context.scene.camera and bpy.context.scene.unit_settings.scale_length==1, '制作镜头或米制合同缺失'
    if args.require_render:
        preview=root/'Previews'/'ZodiacSanctuary.png'
        assert preview.is_file(), '制作镜头尚未渲染'
        image=bpy.data.images.load(str(preview),check_existing=False)
        assert tuple(image.size)==(1664,936), '制作预览图分辨率不符'
    print('DBA_SANCTUARY_SAVED_BLEND_VALIDATION_OK '+json.dumps({'meshes':len(manifest['objects']),'sculptures':sorted(sculptures),'materials':len(manifest['materials']),'center_ground_z_meters':point.z}))


if __name__=='__main__':
    main()
