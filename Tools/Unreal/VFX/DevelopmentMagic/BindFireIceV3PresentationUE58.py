"""RETIRED_ENTRY_POINT：历史入口已停用；退休状态统一读取当前表现合同。"""
from pathlib import Path
import sys
ROOT = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(ROOT / "Tools/Unreal/VFX/SpellProjectiles"))
from SpellPresentationContract import guard_authoring

def retired(*args, **kwargs):
    guard_authoring(__file__)

author = apply = refine_existing = audit = definitions_source = main = retired

if __name__ == "__main__":
    retired()
