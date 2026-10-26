#!/usr/bin/env python3
"""Build with SF3000 GCC + sysroot, or portable Zig 0.13.0 on Windows/Linux."""
import argparse
import concurrent.futures
import json
import os
import re
from pathlib import Path
import shlex
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
BASE = [
 'c4','c4emu','cheats2','cheats','clip','cpu','cpuexec','cpuops','data',
 'dma','dsp1','fxemu','fxinst','gfx','getset','globals','memmap','obc1',
 'ppu','sa1','sa1cpu','sdd1','sdd1emu','seta010','seta011','seta018',
 'seta','spc7110','spc7110dec','srtc','tile'
]

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--target', choices=['sf3000','host'], default='sf3000')
    p.add_argument('--apu', choices=['fast','plus'], default='fast')
    p.add_argument('--reference', action='store_true')
    p.add_argument('--zig', help='Path to Zig 0.13.0 executable')
    p.add_argument('--cc', default=os.environ.get('CC','cc'))
    p.add_argument('--sysroot')
    p.add_argument('--jobs', type=int, default=2)
    p.add_argument('--cflags', action='append', default=[],
                   help='Extra compiler flags, appended after defaults; use --cflags="-O3 -mtune=74kc"')
    p.add_argument('--ldflags', action='append', default=[],
                   help='Extra linker flags (also needed for PGO); use --ldflags="..."')
    p.add_argument('--revision', help='Override source revision for an archive/firmware build')
    p.add_argument('--output-dir', help='Separate output directory for controlled build comparisons')
    args = p.parse_args()
    name = 'hcsnes_' + args.apu + ('_reference' if args.reference else '')
    revision = args.revision
    if revision is None:
        try:
            revision = subprocess.check_output(
                ['git', 'describe', '--always', '--dirty', '--abbrev=8'],
                cwd=ROOT, stderr=subprocess.DEVNULL, text=True).strip()
        except (OSError, subprocess.CalledProcessError):
            stamp = ROOT / 'SOURCE_REVISION'
            revision = stamp.read_text().strip() if stamp.exists() else 'unknown'
    if not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9._-]{0,63}', revision):
        p.error('--revision must be 1-64 letters, digits, dots, underscores or hyphens')
    output_root = Path(args.output_dir).resolve() if args.output_dir else ROOT / 'build' / args.target
    outdir = output_root / name
    outdir.mkdir(parents=True, exist_ok=True)
    env = os.environ.copy()
    temp = ROOT / 'build' / 'tmp'
    temp.mkdir(exist_ok=True)
    env['TMP'] = env['TEMP'] = env['TMPDIR'] = str(temp)
    env.setdefault('ZIG_GLOBAL_CACHE_DIR', str(ROOT / 'build' / 'zig-cache'))
    cc = [args.zig, 'cc'] if args.zig else shlex.split(args.cc)
    flags = ['-std=gnu99','-O2','-g0','-DNDEBUG','-fno-strict-aliasing','-fwrapv',
             '-fno-sanitize=all','-fomit-frame-pointer','-fvisibility=hidden',
             '-DLOAD_FROM_MEMORY','-DLAGFIX',
             '-DHCSNES_GIT_HASH="'+revision+'"',
             '-I'+str(ROOT/'source'),'-I'+str(ROOT/'libretro-common/include'),
             '-I'+str(ROOT)]
    arch = []
    if args.target == 'sf3000':
        if args.zig:
            arch = ['-target','mipsel-linux-gnueabihf.2.17','-mcpu=mips32r2',
                    '-mabi=32','-mfp32']
        else:
            arch = ['-march=mips32r2','-mtune=24kc','-mabi=32','-mfp32',
                    '-mhard-float','-EL','-mlong-calls']
        flags += ['-fPIC']
    elif os.name != 'nt':
        flags += ['-fPIC']
    if args.sysroot:
        arch += ['--sysroot='+str(Path(args.sysroot).resolve())]
    if not args.reference:
        flags += ['-DHCSNES_THREADED_FX']
    sources = [ROOT/'source'/(x+'.c') for x in BASE] + [ROOT/'libretro.c', ROOT/'source/hcsnes_profile.c']
    if args.apu == 'plus':
        flags += ['-DUSE_BLARGG_APU']
        sources += [ROOT/'source/apu_blargg.c']
    else:
        sources += [ROOT/'source'/(x+'.c') for x in ['apu','soundux','spc700']]
    extra_cflags = [flag for group in args.cflags for flag in shlex.split(group)]
    extra_ldflags = [flag for group in args.ldflags for flag in shlex.split(group)]
    flags += extra_cflags
    def compile_one(src):
        obj = outdir / (src.stem+'.o')
        result = subprocess.run(cc+arch+flags+['-c',str(src),'-o',str(obj)],
                                env=env, capture_output=True, text=True)
        if result.returncode:
            raise RuntimeError(src.name+'\n'+result.stdout+result.stderr)
        if result.stderr:
            print(result.stderr, file=sys.stderr)
        return str(obj)
    with concurrent.futures.ThreadPoolExecutor(max_workers=max(1,args.jobs)) as ex:
        objects = list(ex.map(compile_one, sources))
    suffix = '.dll' if args.target == 'host' and os.name == 'nt' else '.so'
    output = output_root/(name+'_libretro'+suffix)
    link = cc+arch+['-shared','-O2','-fno-sanitize=all','-o',str(output)]+objects
    if suffix == '.so':
        link += ['-Wl,--no-undefined','-Wl,--version-script='+str(ROOT/'link.T'),'-lm']
    link += extra_ldflags
    subprocess.run(link,env=env,check=True)
    manifest = {'output':str(output.name),'target':args.target,'apu':args.apu,
                'threaded_fx':not args.reference,'compiler':cc,
                'compile_flags':arch+flags,'link_flags':extra_ldflags,
                'revision':revision,'sources':len(sources)}
    output.with_suffix(output.suffix+'.build.json').write_text(json.dumps(manifest,indent=2)+'\n')
    print(output)
if __name__ == '__main__':
    main()
