import sys
src = open(sys.argv[1], encoding='utf-8').read()
out = []
for line in src.split('\n'):
    e = line.replace('\\', '\\\\').replace('"', '\\"')
    out.append('"%s\\n"' % e)
open(sys.argv[2], 'w').write('\n'.join(out) + '\n')
