#!/usr/bin/env python3
"""Native differential/unit/integration tests. Requires GCC/Clang or --zig."""
import argparse, os, shlex, subprocess, sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser()
p.add_argument('--zig')
p.add_argument('--cc',default=os.environ.get('CC','cc'))
p.add_argument('--cflags',default='')
a=p.parse_args()
cc=[a.zig,'cc'] if a.zig else shlex.split(a.cc)
out=ROOT/'build/tests';out.mkdir(parents=True,exist_ok=True)
temp=ROOT/'build/tmp';temp.mkdir(exist_ok=True)
env=os.environ.copy()
env.update(TMP=str(temp),TEMP=str(temp),TMPDIR=str(temp))
env.setdefault('ZIG_GLOBAL_CACHE_DIR',str(ROOT/'build/zig-cache'))
def run(cmd):subprocess.run(cmd,cwd=ROOT,env=env,check=True)
run([sys.executable,'tools/gen_fx_dispatch.py','--check'])
for test in ['test_frameskip','test_fx','test_rtc','test_profile']:
    exe=out/(test+('.exe' if os.name=='nt' else ''))
    extra = ['source/hcsnes_profile.c'] if test in ('test_fx','test_profile') else []
    if test == 'test_profile': extra += ['-DHCSNES_PROFILE_TEST']
    run(cc+['-O2','-g0','-fwrapv','-fno-strict-aliasing',
            '-I'+str(ROOT/'libretro-common/include'),
            str(ROOT/'tests'/(test+'.c')),'-o',str(exe)]+shlex.split(a.cflags)+extra)
    run([str(exe)])
suffix='.dll' if os.name=='nt' else '.so'
log_capture=out/('log_capture'+suffix)
run(cc+['-shared','-fPIC','tests/log_capture.c','-o',str(log_capture)])
for apu in ['fast','plus']:
    args=[sys.executable,'tools/build.py','--target','host','--apu',apu]
    args+=['--zig',a.zig] if a.zig else ['--cc',a.cc]
    if a.cflags: args += ['--cflags='+a.cflags]
    run(args)
    suffix='.dll' if os.name=='nt' else '.so'
    run([sys.executable,'tests/test_libretro.py',
         str(ROOT/'build/host'/('hcsnes_'+apu+'_libretro'+suffix)),str(log_capture)])
print('All HCSNES native tests passed.')
