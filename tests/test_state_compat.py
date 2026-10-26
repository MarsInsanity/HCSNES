#!/usr/bin/env python3
"""Load a real state from a previous same-platform core and compare WRAM replay.
Usage: python tests/test_state_compat.py old_core new_core
"""
import ctypes as C
from pathlib import Path
import sys
from make_test_rom import make_rom

class Game(C.Structure):
    _fields_=[('path',C.c_char_p),('data',C.c_void_p),('size',C.c_size_t),('meta',C.c_char_p)]
class Var(C.Structure): _fields_=[('key',C.c_char_p),('value',C.c_char_p)]
Env=C.CFUNCTYPE(C.c_bool,C.c_uint,C.c_void_p)
Video=C.CFUNCTYPE(None,C.c_void_p,C.c_uint,C.c_uint,C.c_size_t)
Audio=C.CFUNCTYPE(None,C.c_int16,C.c_int16)
Batch=C.CFUNCTYPE(C.c_size_t,C.c_void_p,C.c_size_t)
Poll=C.CFUNCTYPE(None)
Input=C.CFUNCTYPE(C.c_int16,C.c_uint,C.c_uint,C.c_uint,C.c_uint)

def exercise(filename, saved=None):
    core=C.CDLL(str(Path(filename).resolve()))
    @Env
    def env(cmd,data):
        cmd &= 0xffff
        if cmd==15:
            v=C.cast(data,C.POINTER(Var)).contents
            if v.key in (b'snes9x_2005_frameskip',b'hcsnes_profiling'):
                v.value=b'disabled';return True
        if cmd==17: C.cast(data,C.POINTER(C.c_bool))[0]=False;return True
        if cmd==52: C.cast(data,C.POINTER(C.c_uint))[0]=0;return True
        if cmd==10: return C.cast(data,C.POINTER(C.c_int))[0]==2
        if cmd in (9,31): C.cast(data,C.POINTER(C.c_char_p))[0]=b'.';return True
        if cmd==18: C.cast(data,C.POINTER(C.c_bool))[0]=False;return True
        return cmd in (11,16,35)
    callbacks=[('environment',env),('video_refresh',Video(lambda *a:None)),
        ('audio_sample',Audio(lambda *a:None)),('audio_sample_batch',Batch(lambda p,n:n)),
        ('input_poll',Poll(lambda:None)),('input_state',Input(lambda *a:0))]
    for name,cb in callbacks:
        f=getattr(core,'retro_set_'+name);f.argtypes=[type(cb)];f(cb)
    core.retro_load_game.argtypes=[C.POINTER(Game)];core.retro_load_game.restype=C.c_bool
    core.retro_serialize_size.restype=C.c_size_t
    core.retro_serialize.argtypes=[C.c_void_p,C.c_size_t];core.retro_serialize.restype=C.c_bool
    core.retro_unserialize.argtypes=[C.c_void_p,C.c_size_t];core.retro_unserialize.restype=C.c_bool
    core.retro_get_memory_data.argtypes=[C.c_uint];core.retro_get_memory_data.restype=C.c_void_p
    core.retro_init()
    rom=C.create_string_buffer(make_rom())
    assert core.retro_load_game(C.byref(Game(b'test.sfc',C.cast(rom,C.c_void_p),32768,None)))
    size=core.retro_serialize_size()
    if saved is None:
        for _ in range(120):core.retro_run()
        buf=C.create_string_buffer(size)
        assert core.retro_serialize(buf,size)
        saved=buf.raw
    else:
        assert len(saved)==size,('state size changed',len(saved),size)
        buf=C.create_string_buffer(saved)
        assert core.retro_unserialize(buf,size),'old state rejected'
    for _ in range(10):core.retro_run()
    ram=C.string_at(core.retro_get_memory_data(2),131072)
    core.retro_unload_game();core.retro_deinit()
    return saved,ram

state,expected=exercise(sys.argv[1])
_,actual=exercise(sys.argv[2],state)
assert actual==expected,'old-state replay diverged'
print('PASS: previous-version state size and WRAM replay ('+str(len(state))+' bytes)')
