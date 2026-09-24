#!/usr/bin/env python3
"""Tiny µIR v0 builder example; emits the canonical Dense 4x2 test."""
import struct, pathlib
OP_END=0; OP_INPUT=1; OP_CONST=2; OP_DENSE=0x10; OP_RELU=0x11; OP_ASSERT=0x20
def tensor(op,i,v): return bytes([op,i])+struct.pack('<H',len(v))+struct.pack('<%df'%len(v),*v)
b=bytearray(); b+=tensor(OP_INPUT,0,[1,.5,-.2,.8]); b+=tensor(OP_CONST,1,[0,.1,.2,.3,.4,0,.1,.2]); b+=tensor(OP_CONST,2,[.01,.01]); b+=bytes([OP_DENSE,3,0,1,2])+struct.pack('<HH',4,2); b+=bytes([OP_RELU,4,3]); b+=bytes([OP_ASSERT,4])+struct.pack('<Hf',2,1e-5)+struct.pack('<ff',.11,.42); b+=bytes([OP_END]); out=struct.pack('<IHHI',0x5249554D,0,0,len(b))+b; pathlib.Path('dense_4x2.muir').write_bytes(out); print('wrote dense_4x2.muir',len(out),'bytes')
