# -*- coding: utf-8 -*-
"""本轮自有UE编辑器中的确定性数据纹理烘焙；不拥有正式材质或Niagara。

Transient Material -> 线性RGBA RenderTarget -> 工程Saved PNG。校准通过后才
由唯一纹理作者导入SourceArt；不创建另一套运行时资产或编辑器地图。
"""
from pathlib import Path
import json
import uuid
import unreal as u

ROOT = Path(__file__).resolve().parents[4]
STATE_KEY = "_sslm_fireball_data_bake"

FIELD = r"""
struct Fields {
 float hash(float2 p){return frac(sin(dot(p,float2(127.1,311.7)))*43758.5453);}
 float noise(float2 uv,float period){
  float2 p=uv*period,i=floor(p),f=frac(p);f=f*f*(3.0-2.0*f);
  float2 a=i-floor(i/period)*period;
  float2 b=i+float2(1,0);b-=floor(b/period)*period;
  float2 c=i+float2(0,1);c-=floor(c/period)*period;
  float2 d=i+1;d-=floor(d/period)*period;
  return lerp(lerp(hash(a),hash(b),f.x),lerp(hash(c),hash(d),f.x),f.y);
 }
};
Fields f;
#if DATA_MODE == 0
float v=UV.x<.333333 ? 0 : (UV.x<.666667 ? .5 : 1);
return float4(v,v,v,UV.x>=.333333 && UV.x<.666667 ? .5 : 1);
#elif DATA_MODE == 1
float g=f.noise(UV,4),n=f.noise(UV+float2(.137,.281),8);
float r=1-smoothstep(.025,.17,abs(n+.15*(g-.5)-.52));
float b=1-smoothstep(.008,.055,abs(f.noise(UV+float2(.33,.17),16)-.54));
return float4(r,g,b,1);
#elif DATA_MODE == 2
float a=1-smoothstep(.15,.5,length(UV-.5));
return float4(.5+.22*(f.noise(UV,4)-.5),.5+.22*(f.noise(UV+float2(.31,.17),4)-.5),0,a);
#else
return float4(f.noise(UV,4),f.noise(UV+float2(.13,.37),8),f.noise(UV+float2(.43,.19),16),1);
#endif
"""


def prepare_data_bake(cfg=None):
    """创建且强持有本进程Transient烘焙材质；不保存到Content。"""
    assert "-SSLMFireballMaterialsOwned" in u.SystemLibrary.get_command_line()
    assert Path(u.Paths.get_project_file_path()).resolve() == ROOT / "Game/DivineBeastsArena.uproject"
    run_id=str(uuid.uuid4())
    out=ROOT/"Saved/Validation/FireballFlight"/run_id/"DataBake"
    out.mkdir(parents=True,exist_ok=False)
    mats={}
    ml=u.MaterialEditingLibrary
    for name,mode in [("Calibration",0),("CoreFlow",1),("HeatRG",2),("NoisePack",3)]:
        mat=u.new_object(u.Material,name="SSLM_FB_Data_"+name+run_id[:8])
        mat.set_editor_property("blend_mode",u.BlendMode.BLEND_ALPHA_COMPOSITE)
        mat.set_editor_property("shading_model",u.MaterialShadingModel.MSM_UNLIT)
        uv=ml.create_material_expression(mat,u.MaterialExpressionTextureCoordinate,-500,0)
        custom=ml.create_material_expression(mat,u.MaterialExpressionCustom,0,0)
        pin=u.CustomInput();pin.set_editor_property("input_name","UV")
        custom.set_editor_property("inputs",[pin])
        custom.set_editor_property("code","#define DATA_MODE "+str(mode)+"\n"+FIELD)
        custom.set_editor_property("output_type",u.CustomMaterialOutputType.CMOT_FLOAT4)
        assert ml.connect_material_expressions(uv,"",custom,"UV")
        rgb=ml.create_material_expression(mat,u.MaterialExpressionComponentMask,300,0)
        for k,v in [("r",True),("g",True),("b",True),("a",False)]:rgb.set_editor_property(k,v)
        alpha=ml.create_material_expression(mat,u.MaterialExpressionComponentMask,300,200)
        for k,v in [("r",False),("g",False),("b",False),("a",True)]:alpha.set_editor_property(k,v)
        assert ml.connect_material_expressions(custom,"",rgb,"")
        assert ml.connect_material_expressions(custom,"",alpha,"")
        assert ml.connect_material_property(rgb,"",u.MaterialProperty.MP_EMISSIVE_COLOR)
        # Canvas的目标Alpha是透射率。此专用数据写入材质输入1-A，导出才是源数据A；
        # AlphaComposite的RGB为独立直通值，不作Gamma或亮度补偿。
        inverse=ml.create_material_expression(mat,u.MaterialExpressionOneMinus,450,200)
        assert ml.connect_material_expressions(alpha,"",inverse,"")
        assert ml.connect_material_property(inverse,"",u.MaterialProperty.MP_OPACITY)
        ml.recompile_material(mat);mats[name]=mat
    state={"run_id":run_id,"out":out,"materials":mats,"targets":[]}
    setattr(u,STATE_KEY,state)
    return {"run_id":run_id,"out":str(out),"status":"PreparedPendingShaderCompletion"}


def bake_data_textures(cfg=None):
    """等待原生Shader完成后输出数据PNG，校准由外部逐字节独立检查。"""
    assert "-SSLMFireballMaterialsOwned" in u.SystemLibrary.get_command_line()
    state=getattr(u,STATE_KEY)
    u.AutomationLibrary.finish_loading_before_screenshot()
    world=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
    rows=[]
    for name,size in [("Calibration",256),("CoreFlow",1024),("HeatRG",512),("NoisePack",512)]:
        target=u.RenderingLibrary.create_render_target2d(world,size,size,u.TextureRenderTargetFormat.RTF_RGBA8)
        # RTF_RGBA8在锁定引擎枚举中即线性；bForceLinearGamma不是反射属性。
        target.set_editor_property("target_gamma",1.0)
        u.RenderingLibrary.clear_render_target2d(world,target,u.LinearColor(0,0,0,0))
        u.RenderingLibrary.draw_material_to_render_target(world,target,state["materials"][name])
        file_name="T_DBA_Fireball_"+name+".png"
        u.RenderingLibrary.export_render_target(world,target,str(state["out"]),file_name)
        path=state["out"]/file_name
        assert path.is_file() and path.stat().st_size>100
        state["targets"].append(target)
        rows.append({"name":name,"file":str(path),"size":[size,size],"srgb":False,"alpha":"straight"})
    result={"run_id":state["run_id"],"status":"ExportedPendingCalibration","rows":rows,"native":"UE5.8 DrawMaterialToRenderTarget RGBA8 linear gamma 1"}
    (state["out"]/"Result.json").write_text(json.dumps(result,indent=2),encoding="utf-8")
    return result


def release_data_bake():
    """释放仅本轮持有的Transient对象，不接触地图或其它任务包。"""
    if hasattr(u,STATE_KEY):delattr(u,STATE_KEY)
