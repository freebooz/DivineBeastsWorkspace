import os
import unreal

marker = r"E:\\work\\2026\\DivineBeastsWorkspace\\Game\\Saved\\PythonExecuteProbe.txt"
with open(marker, "w", encoding="utf-8") as f:
    f.write("ok")
unreal.log("[DBA Python Probe] ExecutePythonScript works")
