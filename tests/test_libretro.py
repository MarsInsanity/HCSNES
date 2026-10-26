#!/usr/bin/env python3
"""Exercise the real core ABI with an original ROM; no game downloads."""
import ctypes as C
from pathlib import Path
import sys
import zlib
from make_test_rom import make_rom

class Variable(C.Structure): _fields_=[('key',C.c_char_p),('value',C.c_char_p)]
class Game(C.Structure): _fields_=[('path',C.c_char_p),('data',C.c_void_p),('size',C.c_size_t),('meta',C.c_char_p)]
class Timing(C.Structure): _fields_=[('fps',C.c_double),('sample_rate',C.c_double)]
class Geometry(C.Structure): _fields_=[('base_width',C.c_uint),('base_height',C.c_uint),('max_width',C.c_uint),('max_height',C.c_uint),('aspect',C.c_float)]
class AV(C.Structure): _fields_=[('geometry',Geometry),('timing',Timing)]
Env=C.CFUNCTYPE(C.c_bool,C.c_uint,C.c_void_p)
Video=C.CFUNCTYPE(None,C.c_void_p,C.c_uint,C.c_uint,C.c_size_t)
Batch=C.CFUNCTYPE(C.c_size_t,C.c_void_p,C.c_size_t)
Audio=C.CFUNCTYPE(None,C.c_int16,C.c_int16)
Poll=C.CFUNCTYPE(None)
Input=C.CFUNCTYPE(C.c_int16,C.c_uint,C.c_uint,C.c_uint,C.c_uint)
core=C.CDLL(str(Path(sys.argv[1]).resolve()))
mode=b'disabled'; cap=b'1'; changed=False; profiling=b'disabled'
logger = C.CDLL(str(Path(sys.argv[2]).resolve())) if len(sys.argv)>2 else None
if logger:
    logger.captured_log.restype=C.c_char_p
    logger.clear_log()
class SystemInfo(C.Structure):
    _fields_=[('name',C.c_char_p),('version',C.c_char_p),('extensions',C.c_char_p),
              ('need_fullpath',C.c_bool),('block_extract',C.c_bool)]
rendered=duplicates=audio_frames=0
pixels=[]; checksums=[]
@Env
def env(cmd,data):
    global changed
    cmd &= 0xffff
    if cmd==27 and logger:
        C.cast(data,C.POINTER(C.c_void_p))[0]=C.cast(logger.capture_log,C.c_void_p).value;return True
    if cmd==52: C.cast(data,C.POINTER(C.c_uint))[0]=0; return True
    if cmd==15:
        v=C.cast(data,C.POINTER(Variable)).contents
        values={b'snes9x_2005_frameskip':mode,b'snes9x_2005_frameskip_interval':cap,b'hcsnes_profiling':profiling}
        if v.key in values: v.value=values[v.key];return True
        return False
    if cmd==17:
        C.cast(data,C.POINTER(C.c_bool))[0]=changed;changed=False;return True
    if cmd==10:return C.cast(data,C.POINTER(C.c_int))[0]==2
    if cmd==47:C.cast(data,C.POINTER(C.c_int))[0]=3;return True
    if cmd==18:C.cast(data,C.POINTER(C.c_bool))[0]=False;return True
    if cmd in (9,31):C.cast(data,C.POINTER(C.c_char_p))[0]=b'.';return True
    if cmd in (11,16,35,51):return True
    return False
@Video
def video(data,w,h,pitch):
    global rendered,duplicates
    if data:
        rendered+=1
        raw=b''.join(C.string_at(data+y*pitch,w*2) for y in range(h))
        checksums.append(zlib.crc32(raw))
        pixels.append(int.from_bytes(raw[:2],'little'))
    else:duplicates+=1
@Batch
def batch(data,n):
    global audio_frames
    audio_frames+=n;return n
@Audio
def audio(l,r):pass
@Poll
def poll():pass
@Input
def input_state(p,d,i,k):return 0
for name,cb,typ in [('environment',env,Env),('video_refresh',video,Video),
    ('audio_sample_batch',batch,Batch),('audio_sample',audio,Audio),
    ('input_poll',poll,Poll),('input_state',input_state,Input)]:
    f=getattr(core,'retro_set_'+name);f.argtypes=[typ];f(cb)
core.retro_load_game.argtypes=[C.POINTER(Game)];core.retro_load_game.restype=C.c_bool
core.retro_get_system_av_info.argtypes=[C.POINTER(AV)]
core.retro_serialize_size.restype=C.c_size_t
core.retro_serialize.argtypes=[C.c_void_p,C.c_size_t];core.retro_serialize.restype=C.c_bool
core.retro_unserialize.argtypes=[C.c_void_p,C.c_size_t];core.retro_unserialize.restype=C.c_bool
core.retro_get_memory_data.argtypes=[C.c_uint];core.retro_get_memory_data.restype=C.c_void_p
core.retro_get_memory_size.argtypes=[C.c_uint];core.retro_get_memory_size.restype=C.c_size_t
info=SystemInfo()
core.retro_get_system_info(C.byref(info))
assert info.version.startswith(b'0.2.0+'),info.version
core.retro_init()
rom=C.create_string_buffer(make_rom())
game=Game(b'hcsnes-test.sfc',C.cast(rom,C.c_void_p),32768,None)
assert core.retro_load_game(C.byref(game)), 'load failed'
av=AV();core.retro_get_system_av_info(C.byref(av))
assert 59<av.timing.fps<61 and av.timing.sample_rate>30000
for i in range(120):core.retro_run()
assert rendered==120 and audio_frames>60000,(rendered,audio_frames)
assert any(pixels), 'test ROM never produced a colored backdrop'
size=core.retro_serialize_size();assert size>0
state=C.create_string_buffer(size);assert core.retro_serialize(state,size)
ram_ptr=core.retro_get_memory_data(2);ram_size=core.retro_get_memory_size(2)
assert ram_ptr and ram_size==131072
for i in range(10):core.retro_run()
expected=C.string_at(ram_ptr,ram_size)
assert core.retro_unserialize(state,size)
for i in range(10):core.retro_run()
assert C.string_at(ram_ptr,ram_size)==expected,'state replay RAM mismatch'
mode=b'fixed';cap=b'1';changed=True;rendered=duplicates=audio_frames=0
for i in range(60):core.retro_run()
assert rendered==30 and duplicates==30,(rendered,duplicates)
assert audio_frames>30000,'audio stopped on skipped frames'
mode=b'auto';changed=True;rendered=duplicates=0
for i in range(60):core.retro_run()
assert rendered==60 and duplicates==0,'auto skipped with no audio feedback'
mode=b'manual';changed=True;rendered=duplicates=0
for i in range(60):core.retro_run()
assert rendered==60 and duplicates==0
# Profiling cannot alter serialized size, execution or rendered output.
assert not core.retro_serialize(state,size-1)
assert not core.retro_unserialize(state,size-1)
mode=b'disabled';profiling=b'disabled';changed=True
assert core.retro_unserialize(state,size)
checksums.clear()
for i in range(600):core.retro_run()
expected_ram=C.string_at(ram_ptr,ram_size);expected_video=checksums[:]
assert core.retro_unserialize(state,size)
profiling=b'enabled';changed=True;checksums.clear()
if logger: logger.clear_log()
for i in range(600):core.retro_run()
assert core.retro_serialize_size()==size
assert C.string_at(ram_ptr,ram_size)==expected_ram
assert checksums==expected_video,'profiling changed video output'
if logger:
    lines=[x for x in logger.captured_log().decode().splitlines() if x.startswith('HCSNES profile ')]
    assert len(lines)==2,lines
    for index,line in enumerate(lines):
        assert 'frame='+str((index+1)*300)+' ' in line,line
        assert 'n=300 reason=window' in line and 'rendered=300 skipped=0' in line,line
        assert 'gsu_inst=0 plot=0 rpix=0' in line,line
        assert 'cpu_other=' in line and 'video_cb=' in line and 'audio_cb=' in line,line
    # A mode change remains live while profiling; partial windows flush on Off.
    mode=b'fixed';changed=True
    for i in range(60):core.retro_run()
    profiling=b'disabled';changed=True
    core.retro_run()
    assert 'n=60 reason=off' in logger.captured_log().decode()
    assert 'rendered=30 skipped=30' in logger.captured_log().decode()
    logger.clear_log()
    for i in range(301):core.retro_run()
    assert not logger.captured_log(),'profiling logged while disabled'
core.retro_unload_game();core.retro_deinit()
print('PASS: ROM boot, RGB565 video, audio, state replay, option transitions, fixed/auto/threshold frameskip, profiling cadence and execution equivalence')
