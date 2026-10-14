# -*- coding: utf-8 -*-
"""草地容器门禁的独立失败夹具；CSV为临时测试数据，不是发布证据。
验证正确导出、缺失、BulkData、错误平台、零长度及重复条目，任何退化不能误放行。
"""
import csv
from pathlib import Path
import tempfile
import unittest
from ValidateVillageGrassContainer import REQUIRED, validate


class GrassContainerTest(unittest.TestCase):
    """独立临时目录，退出自动清理；没有工程资源写入或真实引擎进程。"""
    def check(self, change=None):
        rows = [['../../../Plugins/' + p, 'ExportBundleData', 'WindowsClient', 512] for p in REQUIRED]
        if change:
            change(rows)
        # 限定临时夹具在工程Saved，避免受限Windows沙箱重定向的系统Temp不可写；不删除用户数据。
        scratch = Path(__file__).resolve().parents[2] / 'Saved/Validation/VillageAmbient20261013'
        scratch.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='GrassContainerFixture-', dir=scratch) as directory:
            path = Path(directory) / 'Fixture.csv'
            with path.open('w', encoding='utf-8', newline='') as stream:
                writer = csv.writer(stream)
                writer.writerow(['Filename', 'ChunkType', 'Platform', 'Size'])
                writer.writerows(rows)
            return validate(path)['Passed']

    def test_exact_exports(self):
        self.assertTrue(self.check())

    def test_missing(self):
        self.assertFalse(self.check(lambda r: r.pop()))

    def test_bulk_is_not_export(self):
        self.assertFalse(self.check(lambda r: r[0].__setitem__(1, 'BulkData')))

    def test_server_is_not_client(self):
        self.assertFalse(self.check(lambda r: r[0].__setitem__(2, 'WindowsServer')))

    def test_zero_length(self):
        self.assertFalse(self.check(lambda r: r[0].__setitem__(3, 0)))

    def test_duplicate_export(self):
        self.assertFalse(self.check(lambda r: r.append(r[0].copy())))


if __name__ == '__main__':
    unittest.main()
