# µIR v0
Header: `<IHHI`: magic `MUIR`, version 0, flags, byte length of instruction body. All multibyte values are little-endian.

Opcodes: `00 END`; `01 INPUT dst:u8 len:u16 f32[len]`; `02 CONST` same; `10 DENSE dst,x,w,b:u8 in:u16 out:u16`; `11 RELU dst,src:u8`; `12 ADD dst,a,b:u8`; `20 ASSERT_CLOSE src:u8 len:u16 epsilon:f32 expected:f32[len]`.

Limits: 32 tensors, 256 floats/tensor, 4096 instructions. The VM rejects malformed/truncated input and shape mismatches.
