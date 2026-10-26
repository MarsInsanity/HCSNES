#!/usr/bin/env python3
"""Package binaries and complete source, excluding compiler/cache intermediates."""
from pathlib import Path
import hashlib, json, re, subprocess, sys, zipfile
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tests'))
from make_test_rom import make_rom
output=Path(sys.argv[1]).resolve()
output.parent.mkdir(parents=True,exist_ok=True)
version=re.search(r'#define HCSNES_VERSION "([^"]+)"',(ROOT/'source/hcsnes_version.h').read_text()).group(1)
prefix='HCSNES-R36S-v'+version+'/'
entries={}
def add(name,data):
    if isinstance(data,Path):data=data.read_bytes()
    entries[prefix+name]=data
for name in ['README.md','PROFILING.md','CHANGELOG.md','VALIDATION.md','LICENSE-HCSNES','copyright']:
    add(name,ROOT/name)
for name in ['hcsnes_fast_libretro.so','hcsnes_plus_libretro.so','hcsnes_fast_reference_libretro.so']:
    add('sdcard/cubegm/cores/'+name,ROOT/'build/sf3000'/name)
    meta=json.loads((ROOT/'build/sf3000'/(name+'.build.json')).read_text())
    meta['compiler']=['Zig 0.13.0','cc']
    meta['compile_flags']=[f.replace(str(ROOT),'<SOURCE_ROOT>').replace('\\','/') for f in meta['compile_flags']]
    add('build-info/'+name+'.json',(json.dumps(meta,indent=2)+'\n').encode())
for name in ['hcsnes_bench','hcsnes_fx_test']:
    add('sdcard/hcsnes/'+name,ROOT/'build/sf3000'/name)
for item in (ROOT/'integration').glob('*.sh'):
    add('sdcard/hcsnes/'+item.name,item)
add('sdcard/hcsnes/hcsnes-test.sfc',make_rom())
for item in ROOT.rglob('*'):
    if not item.is_file():continue
    rel=item.relative_to(ROOT)
    if any(x in ('build','.git','__pycache__') for x in rel.parts):continue
    if item.suffix in ('.pyc','.sfc','.smc','.state'):continue
    if item.name in ('write-test.txt','hello.c'):continue
    add('source/'+rel.as_posix(),item)
revision=json.loads((ROOT/'build/sf3000/hcsnes_fast_libretro.so.build.json').read_text())['revision']
add('source/SOURCE_REVISION',(revision+'\n').encode())
checksums=''.join(hashlib.sha256(data).hexdigest()+'  '+name[len(prefix):]+'\n'
                  for name,data in sorted(entries.items()))
add('SHA256SUMS.txt',checksums.encode())
with zipfile.ZipFile(output,'w',zipfile.ZIP_DEFLATED,compresslevel=9) as z:
    for name,data in entries.items():
        info=zipfile.ZipInfo(name,date_time=(2026,10,25,0,0,0))
        info.compress_type=zipfile.ZIP_DEFLATED
        info.create_system=3
        executable=name.endswith('.sh') or name.rsplit('/',1)[-1] in ('hcsnes_bench','hcsnes_fx_test')
        info.external_attr=(0o100755 if executable else 0o100644)<<16
        z.writestr(info,data)
with zipfile.ZipFile(output) as z:
    assert z.testzip() is None
    for line in checksums.splitlines():
        digest,name=line.split('  ',1)
        assert hashlib.sha256(z.read(prefix+name)).hexdigest()==digest
print(json.dumps({'path':str(output),'bytes':output.stat().st_size,'files':len(entries),
                  'sha256':hashlib.sha256(output.read_bytes()).hexdigest()},indent=2))
