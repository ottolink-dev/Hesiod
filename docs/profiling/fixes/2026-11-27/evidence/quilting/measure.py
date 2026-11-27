from pathlib import Path
import subprocess
folder=Path(__file__).resolve().parent
heap='/nix/store/q7fmb6pfjkd9mj1jr3qz4mrsa2dndj1s-heaptrack-1.5.0-unstable-2025-07-21/bin/'
for method in [1,2]:
 for version in ['baseline','new']:
  name=f'{version}-{method}'
  if not (folder/(name+'.zst')).exists():
   with (folder/(name+'.log')).open('w') as log:
    subprocess.run([heap+'heaptrack','--record-only','-o',str(folder/name),str(folder/version),str(folder/(name+'.bin')),str(method)],stdout=log,stderr=subprocess.STDOUT,check=True)
  with (folder/(name+'-summary.txt')).open('w') as summary:
   subprocess.run([heap+'heaptrack_print','-f',str(folder/(name+'.zst')),'-n','5'],stdout=summary,stderr=subprocess.STDOUT,check=True)
 subprocess.run(['cmp',str(folder/f'baseline-{method}.bin'),str(folder/f'new-{method}.bin')],check=True)
print('large outputs match')
