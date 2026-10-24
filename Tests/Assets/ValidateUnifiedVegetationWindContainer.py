"""只读核对真实IoStore ExportBundleData清单；元数据不算入包，不代替网络/GPU/人工证据。"""
import argparse
import csv
import hashlib
import json
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'Tools/Unreal/Weather'))
import UnifiedVegetationWindContract as contract


def inventory(path):
    """拒绝空文件/损坏头/非正尺寸，保留实际包数据，不把Shader或PackageStore元数据冒充资产。"""
    path = pathlib.Path(path)
    with path.open(encoding='utf-8-sig', newline='') as source:
        reader = csv.DictReader(source)
        if not {'Filename', 'ChunkType', 'Size'}.issubset(reader.fieldnames or []):
            raise ValueError('IoStore清单缺少必要字段')
        rows = list(reader)
    if not rows:
        raise ValueError('IoStore清单为空')
    names = set()
    for row in rows:
        if row['ChunkType'].strip() != 'ExportBundleData':
            continue
        if not row['Size'].strip().isdigit() or int(row['Size']) <= 0:
            raise ValueError('实际包数据尺寸非法')
        names.add(row['Filename'].strip().replace('\\', '/'))
    if not names:
        raise ValueError('没有非空ExportBundleData，不能证明资产入包')
    return names


def suffix(package):
    mount, rest = package.strip('/').split('/', 1)
    return mount + '/Content/' + rest + ('.umap' if '/Maps/' in rest else '.uasset')


def inspect(client_csv, server_csv, cold_report, bindings_report):
    if pathlib.Path(client_csv).resolve() == pathlib.Path(server_csv).resolve():
        raise ValueError('Client/Server不能共用一份清单')
    client, server = inventory(client_csv), inventory(server_csv)
    cold = json.loads(pathlib.Path(cold_report).read_text(encoding='utf-8-sig'))
    bindings = json.loads(pathlib.Path(bindings_report).read_text(encoding='utf-8-sig'))
    if cold.get('stage') != 'ColdReload' or cold.get('status') != 'PASS':
        raise ValueError('缺少通过的独立原生冷重载报告')
    if bindings.get('weather_activation_pending') is not False:
        raise ValueError('天气激活配置尚未完成')
    required = {contract.GLOBAL_FUNCTION, contract.GENERIC_FUNCTION, contract.ST9_FUNCTION,
                contract.ST9_MASTER, '/GamePlatformSurface/ParameterCollections/MPC_GP_SurfaceGlobal'}
    required.update(p.split('.')[0] for p in bindings['materials'] + bindings['prototype_masters'])
    missing = sorted(p for p in required if not any(name.endswith('/' + suffix(p)) for name in client))
    review_leaks = sorted(name for name in client if '/DBAWorldPack_Village/Content/Development/Wind/' in name)
    server_leaks = sorted(name for name in server if '/GamePlatformSurface/Content/' in name or
                          '/DBAFoliagePack_Core/Content/' in name or '/DBAWorldPack_Village/Content/Development/Wind/' in name)
    weather_package = '/DBAWorldPack_Village/Weather/Definitions/DA_DBA_Weather_Clear'
    server_missing = [] if any(name.endswith('/' + suffix(weather_package)) for name in server) else [weather_package]
    evidence = {str(pathlib.Path(p).resolve()): hashlib.sha256(pathlib.Path(p).read_bytes()).hexdigest()
                for p in (client_csv, server_csv, cold_report, bindings_report)}
    return {'status': 'FAIL' if missing or review_leaks or server_leaks or server_missing else 'PASS',
            'client_missing': missing, 'client_review_leaks': review_leaks, 'server_visual_leaks': server_leaks,
            'server_missing': server_missing,
            'required_package_count': len(required), 'input_sha256': evidence,
            'network_verified': False, 'gpu_verified': False, 'manual_visual_accepted': False}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('client-csv', 'server-csv', 'cold-report', 'bindings-report', 'output'):
        parser.add_argument('--' + name, required=True)
    args = parser.parse_args()
    result = inspect(args.client_csv, args.server_csv, args.cold_report, args.bindings_report)
    pathlib.Path(args.output).write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding='utf-8')
    print(json.dumps(result, ensure_ascii=False)); raise SystemExit(0 if result['status'] == 'PASS' else 1)
