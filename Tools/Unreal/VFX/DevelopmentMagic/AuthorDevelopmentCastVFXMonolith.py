"""已退休兼容入口；禁止历史作者回写当前稳定Definition。"""

def retired(*args, **kwargs):
    raise RuntimeError("RETIRED_ENTRY_POINT: use Tools/Unreal/VFX/SpellProjectiles/AuthorSpellPresentationUE58.py through Monolith; audit first, explicit apply only")

author = apply = refine_existing = audit = definitions_source = retired

if __name__ == "__main__":
    retired()
