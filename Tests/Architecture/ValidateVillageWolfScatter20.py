"""历史入口保留；正式怪物目标已由用户改为200只，统一转发新合同门禁。"""
import runpy
from pathlib import Path
runpy.run_path(str(Path(__file__).with_name('ValidateVillageMonsters200.py')),run_name='__main__')