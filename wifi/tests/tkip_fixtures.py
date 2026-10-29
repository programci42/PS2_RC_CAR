"""Offline independent Scapy 2.6.1 reference; never transmits packets."""
import sys,json,struct,zlib
sys.path.insert(0,r'C:\ps2dev\testdeps\scapy')
from scapy.modules.krack.crypto import build_TKIP_payload,build_MIC_ICV,michael
d=json.load(sys.stdin)
ap=bytes.fromhex(d['ap']);sa=':'.join(f'{x:02x}' for x in ap)
gtk=bytes.fromhex(d['gtk']);vectors={}
def packet(eth,pn,qos=False,badmic=False):
    da=eth[:6];src=eth[6:12];plain=bytes.fromhex('aaaa03000000')+eth[12:]
    hdr=bytearray(26 if qos else 24);hdr[0]=0x88 if qos else 8;hdr[1]=0x42
    hdr[4:10]=da;hdr[10:16]=ap;hdr[16:22]=src
    if qos:hdr[24]=7
    # Scapy calculates both Michael and TKIP key mixing independently of C.
    mic=michael(gtk[16:24],da+src+bytes([7 if qos else 0,0,0,0])+plain)
    if badmic:mic=bytes([mic[0]^1])+mic[1:]
    data=plain+mic;data+=struct.pack('<I',zlib.crc32(data)&0xffffffff)
    encrypted=bytearray(build_TKIP_payload(data,pn,sa,gtk[:16]))
    encrypted[3]=0x60 # GTK key index 1, extended IV
    return bytes(hdr)+encrypted,plain
for name in ['offer','ack']:
    eth=bytes.fromhex(d[name]);eth=b'\xff'*6+eth[6:]
    vectors['fx_tkip_'+name]=packet(eth,1 if name=='offer' else 2)[0]
eth=bytes.fromhex(d['offer']);eth=b'\xff'*6+eth[6:]
vectors['fx_tkip_badmic1']=packet(eth,3,badmic=True)[0]
vectors['fx_tkip_badmic2']=packet(eth,4,badmic=True)[0]
vectors['fx_tkip_qos'],vectors['fx_tkip_qos_plain']=packet(eth,0x100010007,qos=True)
for i,pn in enumerate([1,255,256,65535,65536,0x123456789abc]):
    e=b'\xff'*6+ap+b'\x08\x00'+bytes((x*17+i)&255 for x in range(1+i*7))
    vectors[f'fx_tkip_vector_{i}'],vectors[f'fx_tkip_plain_{i}']=packet(e,pn)
with open('tests/tkip_fixtures.h','w',encoding='ascii') as f:
    f.write('/* Offline Scapy 2.6.1 independent public vectors. No network I/O. */\n')
    for name,b in vectors.items():f.write(f'static const unsigned char {name}[{len(b)}]={{'+','.join(map(str,b))+'};\n')
