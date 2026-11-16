"""检查并暂存用户指定兔图标ZIP；仅解析白名单数据字段，不执行包内HTML或文档。"""
import argparse, hashlib, json, struct, zipfile
from pathlib import Path

ROOT=Path(__file__).resolve().parents[3]
PACK=ROOT/'Game/Plugins/DivineBeasts/ContentPacks/Heroes/DBAHeroPack_Rabbit'
def sha(data): return hashlib.sha256(data).hexdigest()
def stage(archive):
    folder=PACK/'SourceArt/UI/SkillIcons/V6PreReview20261021'
    out=ROOT/'Saved/Validation/RabbitV6IconImport20261021';out.mkdir(parents=True,exist_ok=True)
    approved={e[0]:e[1] for e in json.loads((ROOT/'Docs/Design/ApprovedSkillDisplayNames_V5.json').read_text(encoding='utf-8'))['entries']}
    design=json.loads((ROOT/'Docs/Design/ZodiacHistoryFirstEightSkillsV6_Review.json').read_text(encoding='utf-8'))
    rabbit=next(h for h in design['heroes'] if h['heroId']=='Hero.Zodiac.Rabbit')
    expected={s['abilityId']:s['slotId'] for s in rabbit['skills']}
    with zipfile.ZipFile(archive) as z:
        manifest=json.loads(z.read('RabbitV6_ProvisionalImportManifest.json'))
        assert manifest['heroId']=='Hero.Zodiac.Rabbit' and len(manifest['entries'])==8
        records=[]
        for info in z.infolist():
            if info.is_dir(): continue
            p=Path(info.filename)
            assert not p.is_absolute() and '..' not in p.parts and info.file_size<20_000_000
            data=z.read(info)
            dest=(folder/p).resolve();assert dest.is_relative_to(folder.resolve())
            if dest.exists(): assert sha(dest.read_bytes())==sha(data),'禁止覆盖不同图源'
            else: dest.parent.mkdir(parents=True,exist_ok=True);dest.write_bytes(data)
        for e in manifest['entries']:
            identity=e['abilityId']; assert expected[identity]==e['slotId']
            record={'abilityId':identity,'slotId':e['slotId'],'approvedNameZh':approved[identity],
                    'assetPath':'/DBAHeroPack_Rabbit/UI/Abilities/'+e['assetName'],
                    'selectedCandidate':e['selectedCandidate'],'humanApproved':False}
            for field,size,hashkey in [('runtime48LocalFile',48,'png48Sha256'),('review64LocalFile',64,'png64Sha256'),('candidatePreviewMaster',1024,'masterSha256')]:
                path=folder/e[field]; data=path.read_bytes()
                assert data[:8]==b'\x89PNG\r\n\x1a\n' and struct.unpack('>II',data[16:24])==(size,size)
                assert sha(data)==e[hashkey]
                record[{48:'runtime48',64:'review64',1024:'master1024'}[size]]={'path':path.relative_to(ROOT).as_posix(),'sha256':sha(data),'width':size,'height':size}
            records.append(record)
    report={'archive':str(archive),'archiveSHA256':sha(Path(archive).read_bytes()),'heroId':'Hero.Zodiac.Rabbit',
            'nativeImported':False,'humanApproved':False,'candidateCount':24,'skills':records,
            'sourceBoard':(folder/'RabbitV6_64_48_QABoard.png').relative_to(ROOT).as_posix()}
    (out/'Staging.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    print(json.dumps({'skills':len(records),'source':str(folder),'archiveSHA256':report['archiveSHA256'],'humanApproved':False}))

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('archive');stage(p.parse_args().archive)
