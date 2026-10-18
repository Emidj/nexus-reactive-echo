from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
out=ROOT/'build/lut-fixtures'
out.mkdir(parents=True,exist_ok=True)
def cube(size,fn=lambda r,g,b:(r,g,b),header=''):
    rows=['TITLE "NEXUS test"',f'LUT_3D_SIZE {size}']
    if header: rows.append(header)
    for b in range(size):
        for g in range(size):
            for r in range(size):
                rows.append(' '.join(f'{x:.9g}' for x in fn(r/(size-1),g/(size-1),b/(size-1))))
    return '\n'.join(rows)+'\n'
identity=cube(2)
files={
 'identity.cube':identity, 'identity17.cube':cube(17), 'identity65.cube':cube(65),
 'identidad áé.cube':'\ufeff'+identity.replace('\n','\r\n'),
 'inverse.cube':cube(2,lambda r,g,b:(1-r,1-g,1-b)),
 'swap.cube':cube(2,lambda r,g,b:(b,r,g)),
 'square.cube':cube(3,lambda r,g,b:(r*r,g*g,b*b)),
 'domain.cube':cube(2,header='DOMAIN_MIN 0.1 0.2 0.3\nDOMAIN_MAX 0.9 0.8 0.7'),
 'comments.cube':'# comment\n'+identity.replace('0 0 0','0e0 +0.0 -0.0 # origin'),
 'reload.cube':identity,
 'size1.cube':identity.replace('SIZE 2','SIZE 1'), 'size66.cube':identity.replace('SIZE 2','SIZE 66'),
 'fractional.cube':identity.replace('SIZE 2','SIZE 2.5'),
 'near-integer.cube':identity.replace('SIZE 2','SIZE 2.00000001'),
 'duplicate.cube':'LUT_3D_SIZE 2\n'+identity,
 'incomplete.cube':'\n'.join(identity.splitlines()[:-1]),
 'extra.cube':identity+'0 0 0\n', 'nan.cube':identity.replace('0 0 0','nan 0 0'),
 'infinity.cube':identity.replace('0 0 0','1e999 0 0'),
 'hex.cube':identity.replace('0 0 0','0x1 0 0'),
 'huge-number.cube':identity.replace('0 0 0','70000 0 0'),
 'extra-number.cube':identity.replace('0 0 0','0 0 0 0'),
 'bad-domain.cube':cube(2,header='DOMAIN_MIN 1 0 0\nDOMAIN_MAX 0 1 1'),
 'same-domain.cube':cube(2,header='DOMAIN_MAX 0 1 1'),
 'late-domain.cube':identity+'DOMAIN_MIN 0 0 0\n',
 'duplicate-domain.cube':cube(2,header='DOMAIN_MIN 0 0 0\nDOMAIN_MIN 0 0 0'),
 'one-dimensional.cube':'LUT_1D_SIZE 2\n0 0 0\n1 1 1\n',
 'unknown.cube':identity+'UNKNOWN 1\n', 'empty.cube':'# blank\n',
 'long-line.cube':'#'+('a'*2000)+'\n'+identity,
 'bad-title.cube':identity.replace('"NEXUS test"','NEXUS test'),
 'comma.cube':identity.replace('0 0 0','0,0 0 0'),
 'shaper.cube':'LUT_1D_INPUT_RANGE 0 1\n'+identity,
}
for name,data in files.items(): (out/name).write_bytes(data.encode('utf8'))
with (out/'oversize.cube').open('wb') as f: f.seek(32*1024*1024); f.write(b'\n')
print('LUT fixtures prepared')
