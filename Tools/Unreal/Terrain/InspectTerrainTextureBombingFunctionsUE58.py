#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""SSLM纹理轰炸只读函数签名探测：UE5.8引擎内置、平台既有函数。"""
from __future__ import annotations
import json

FUNCS={
  "UE.TextureBombing": "/Engine/Functions/Engine_MaterialFunctions01/Texturing/Texture_Bombing",
  "UE.TextureBombSingle": "/Engine/Functions/Engine_MaterialFunctions01/Texturing/TextureBomb_SingleSample",
  "Platform.AntiTiling": "/GamePlatformSurface/MaterialFunctions/Core/MF_GP_TextureAntiTiling",
}
def main():
 import unreal # type: ignore
 lib=unreal.MaterialEditingLibrary
 a=unreal.EditorAssetLibrary
 for label,path in FUNCS.items():
  obj=a.load_asset(path)
  if not obj:
   print("SSLM_BOMB_FUNCTION_MISSING",label,path,flush=True);continue
  expressions=lib.get_material_function_expressions(obj)
  pins=[]
  for e in expressions:
   name=e.get_class().get_name()
   if name not in ("MaterialExpressionFunctionInput","MaterialExpressionFunctionOutput"):continue
   row={"class":name}
   for attr in ("input_name","output_name","input_type","description","preview_value","preview_value_as_default","sort_priority"):
    try:row[attr]=str(e.get_editor_property(attr))[:250]
    except Exception:pass
   pins.append(row)
  print("SSLM_BOMB_FUNCTION_SIGNATURE",json.dumps({"label":label,"path":path,"expr":len(expressions),"pins":pins},ensure_ascii=False),flush=True)

if __name__=="__main__":main()
