"""RETIRED_ENTRY_POINT：旧1定向/2火球/3冰箭键位作者已停用。

当前已批准1火球/2冰箭/3定向，唯一作者入口从退休合同给出；本兼容模块不导入UE、
不创建目录、不含授权集/技能/UI写入逻辑，run/main/apply均明确失败。
"""
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / 'Tools/Unreal/VFX/SpellProjectiles'))
from SpellPresentationContract import guard_authoring


def retired(*args, **kwargs):
    """保留旧调用名，但禁止执行退休键位迁移，错误中指向当前唯一作者。"""
    guard_authoring(__file__)


run = main = apply = retired

if __name__ == '__main__':
    retired()
