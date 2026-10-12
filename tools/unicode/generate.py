#!/usr/bin/env python3
"""Regenerate pinned Unicode 17 tables. No network or third-party dependencies.
Download the files listed in sources.json from https://www.unicode.org/Public/17.0.0/ucd/,
then run: python3 tools/unicode/generate.py --data-dir /path/to/ucd-files
Files may be flat or retain their UCD subdirectories. Checksums are mandatory.
"""
import argparse, hashlib, json, pathlib
ROOT = pathlib.Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--data-dir', type=pathlib.Path, required=True)
args = parser.parse_args()
manifest = json.loads((ROOT/'tools/unicode/sources.json').read_text())
files = {}
for name, digest in manifest['files'].items():
    path = args.data_dir/name
    if not path.exists(): path = args.data_dir/pathlib.Path(name).name
    raw = path.read_bytes()
    if hashlib.sha256(raw).hexdigest() != digest: raise SystemExit('Checksum mismatch: '+name)
    files[pathlib.Path(name).name] = raw.decode('utf-8')
# One compact property word per scalar, coalesced into non-default intervals.
props = [0] * 0x110000
GCB = dict(zip(['Other','CR','LF','Control','Extend','ZWJ','Regional_Indicator',
               'Prepend','SpacingMark','L','V','T','LV','LVT'],range(14)))
def records(name):
    for line in files[name].splitlines():
        fields = line.split('#',1)[0].strip().split(';')
        if len(fields)<2: continue
        limits = fields[0].strip().split('..')
        lo,hi = int(limits[0],16),int(limits[-1],16)
        yield lo,hi,[x.strip() for x in fields[1:]]
def apply(lo,hi,bits):
    for cp in range(lo,hi+1): props[cp] |= bits
for lo,hi,fields in records('GraphemeBreakProperty.txt'):
    # Hangul syllable LV/LVT is calculated arithmetically at runtime.
    if fields[0] not in ('LV','LVT'): apply(lo,hi,GCB[fields[0]])
for lo,hi,fields in records('DerivedCoreProperties.txt'):
    if fields[0]=='InCB': apply(lo,hi,{'Consonant':1,'Extend':2,'Linker':3}[fields[1]]<<4)
    if fields[0]=='Default_Ignorable_Code_Point': apply(lo,hi,1<<14)
for lo,hi,fields in records('emoji-data.txt'):
    if fields[0] in ('Extended_Pictographic','Emoji','Emoji_Presentation'):
        apply(lo,hi,1<<{'Extended_Pictographic':6,'Emoji':7,'Emoji_Presentation':8}[fields[0]])
for lo,hi,fields in records('EastAsianWidth.txt'):
    if fields[0] in ('W','F','A'): apply(lo,hi,(2 if fields[0]=='A' else 1)<<9)
first = None
for line in files['UnicodeData.txt'].splitlines():
    fields=line.split(';');cp=int(fields[0],16);category=fields[2]
    if fields[1].endswith(', First>'): first=cp;continue
    lo=first if fields[1].endswith(', Last>') else cp; first=None
    bits=1<<12
    if category in ('Mn','Me','Mc','Cf'): bits |= 1<<11
    if category in ('Cc','Zl','Zp'): bits |= 1<<13
    apply(lo,cp,bits)
intervals=[]
start=0
for cp in range(1,len(props)+1):
    if cp==len(props) or props[cp]!=props[start]:
        if props[start]: intervals.append((start,cp-1,props[start]))
        start=cp
header='// Generated from Unicode 17.0.0; do not edit. See tools/unicode and LICENSE.unicode.\n'
header+='#pragma once\n#include <cstdint>\nnamespace esc::detail::layout_data {\n'
header+='struct Range { char32_t first, last; std::uint16_t properties; };\ninline constexpr Range ranges[] = {\n'
header+=''.join(f'    {{0x{lo:X}, 0x{hi:X}, 0x{bits:X}}},\n' for lo,hi,bits in intervals)
header+='};\n} // namespace esc::detail::layout_data\n'
(ROOT/'src/detail/layout_data.hpp').write_text(header)
# Preserve every official boundary test, including start/end boundaries.
cases=[];scalars=[];boundaries=[]
for line in files['GraphemeBreakTest.txt'].splitlines():
    tokens=line.split('#',1)[0].split()
    if not tokens: continue
    points=[];ends=[]
    for token in tokens:
        if token=='÷': ends.append(len(points))
        elif token!='×': points.append(int(token,16))
    cases.append((len(scalars),len(points),len(boundaries),len(ends)))
    scalars+=points;boundaries+=ends
out='// Generated unchanged boundary expectations from Unicode 17 GraphemeBreakTest.\n// See tools/unicode and LICENSE.unicode.\n#pragma once\n#include <cstddef>\nnamespace unicode_cases {\n'
def array(name,typename,values,render):
    return f'inline constexpr {typename} {name}[] = {{\n'+''.join('    '+', '.join(map(render,values[i:i+12]))+',\n' for i in range(0,len(values),12))+'};\n'
out+=array('scalars','char32_t',scalars,lambda x:f'0x{x:X}')
out+=array('boundaries','std::size_t',boundaries,str)
out+='struct Case { std::size_t scalar_offset, scalar_count, boundary_offset, boundary_count; };\n'
out+=array('cases','Case',cases,lambda x:'{'+', '.join(map(str,x))+'}')
out+='}\n'
(ROOT/'tests/contract/data/grapheme_break_cases.hpp').write_text(out)
print(len(intervals),'property intervals;',len(cases),'official conformance cases')
