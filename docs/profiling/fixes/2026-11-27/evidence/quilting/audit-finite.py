import hashlib,json,struct
from pathlib import Path
from array import array
folder=Path(__file__).resolve().parent
results={}
for name in ['baseline.bin','new.bin','baseline-1.bin','new-1.bin','baseline-2.bin','new-2.bin']:
 arrays=cells=nonfinite=empty=0
 with (folder/name).open('rb') as f:
  while True:
   header=f.read(8)
   if not header:break
   assert len(header)==8
   x,y=struct.unpack('=ii',header)
   assert (x==0 and y==0) or (0<x<=10000 and 0<y<=10000),(x,y)
   empty+=int(x==0)
   data=f.read(x*y*4)
   assert len(data)==x*y*4
   v=array("I");v.frombytes(data)
   assert v.itemsize==4
   nonfinite+=sum((bits & 0x7f800000)==0x7f800000 for bits in v)
   arrays+=1;cells+=len(v)
 results[name]={'arrays':arrays,'float_cells':cells,'nonfinite_cells':nonfinite,'empty_arrays':empty,'bytes':(folder/name).stat().st_size}
 assert nonfinite==0,name
(folder/'strict-finite-audit.json').write_text(json.dumps({'method':'Python integer IEEE-754 binary32 exponent classification on dimensions-validated raw float32 payloads; compiled production fast-math flags do not affect this audit','files':results},indent=2)+'\n')
print(json.dumps(results))
