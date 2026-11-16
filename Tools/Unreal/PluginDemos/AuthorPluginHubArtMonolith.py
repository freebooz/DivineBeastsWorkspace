"""兼容旧制作入口，委托当前唯一的每插件独立封面导入器。"""
from pathlib import Path
import runpy
runpy.run_path(str(Path(__file__).with_name('ImportPluginHubPicturesMonolith.py')), run_name='__main__')
