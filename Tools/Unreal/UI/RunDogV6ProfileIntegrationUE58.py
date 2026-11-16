# -*- coding: utf-8 -*-
"""SSLM戌狗V6：在真实UE5.8中复用现有唯一UIProfile接线工具的受控入口。

执行必须由UnrealEditor PythonScriptCommandlet或Monolith editor.run_python启动。
通过环境变量SSLM_DOG_PROFILE_MODE选择inspect/apply/verify/reload，
不授予GAS、不新建主资产，不修改正式V5显示名称。
"""
import os, json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[3]
SCRIPT=ROOT/"Tools/Unreal/UI/CompleteImportedHeroIconProfilesMonolith.py"
PLAN=ROOT/"Saved/Validation/DogV6Staging20261019/IntegrationPlan.json"
MODE=os.getenv("SSLM_DOG_PROFILE_MODE","inspect").strip().lower()
if MODE not in ("inspect","apply","verify","reload"):
    raise RuntimeError("不支持的戌狗UIProfile操作模式")
if not PLAN.is_file():
    raise RuntimeError("缺少戌狗集成配置清单")
data=json.loads(PLAN.read_text(encoding="utf-8"))
if len(data.get("heroes",[]))!=1 or data["heroes"][0]["heroId"]!="Hero.Zodiac.Dog":
    raise RuntimeError("安全门禁：本次只允许处理戌狗唯一UIProfile")
scope={"__name__":"__main__","__file__":str(SCRIPT),
       "PROFILE_PLAN":str(PLAN),"PIPELINE_MODE":MODE}
exec(compile(SCRIPT.read_text(encoding="utf-8"),str(SCRIPT),"exec"),scope)
