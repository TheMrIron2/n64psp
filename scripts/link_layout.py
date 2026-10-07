"""Generate and verify reserved PSP text slots from an application layout plan."""
import argparse
import json
from pathlib import Path
import subprocess


def validate(plan):
    end = 0
    names = set()
    for chunk in plan['chunks']:
        address, slot = chunk['address'], chunk['slot']
        if address < end or address % 64 or slot <= 0 or slot % 64:
            raise ValueError('overlapping or unaligned text slot')
        end = address + slot
        for function in chunk['functions']:
            if function['name'] in names:
                raise ValueError('duplicate pinned function')
            names.add(function['name'])
            if function['offset'] < 0 or function['offset'] + function['size'] > slot:
                raise ValueError('function exceeds text slot')
    if end != plan['pinned_end']:
        raise ValueError('incorrect pinned extent')


def generate(plan, template, build_dir, library_dir):
    validate(plan)
    body = ['    _ftext = .;']
    for chunk in plan['chunks']:
        owner = chunk['owner']
        if chunk['kind'] == 'application':
            selector = '"' + str(Path(build_dir) / owner) + '"'
        elif chunk['kind'] == 'archive':
            selector = '"' + str(Path(library_dir) / owner) + ':' + chunk['member'] + '"'
        elif chunk['kind'] == 'startup':
            selector = '*' + owner
        else:
            raise ValueError('unknown input owner')
        address, end = chunk['address'], chunk['address'] + chunk['slot']
        body.extend([f'    . = 0x{address:x};',
                     f'    {selector}({chunk["section"]})',
                     f'    ASSERT(. > 0x{address:x}, "missing pinned input section");',
                     f'    ASSERT(. <= 0x{end:x}, "pinned text slot overflow");',
                     f'    . = 0x{end:x};'])
    body.extend(['    *(.text .stub .text.* .gnu.linkonce.t.*)',
                 '    KEEP (*(.text.*personality*))', '    *(.gnu.warning)',
                 '    *(.mips16.fn.*) *(.mips16.call.*)'])
    start = template.index('  .text           :')
    end = template.index('  .init           :', start)
    return template[:start] + '  .text :\n  {\n' + '\n'.join(body) + '\n  } =0\n' + template[end:]


def verify(plan, elf, nm):
    validate(plan)
    symbols = {}
    output = subprocess.check_output([nm, '-S', '--defined-only', str(elf)], text=True)
    for line in output.splitlines():
        fields = line.split()
        if len(fields) == 4:
            symbols.setdefault(fields[3], set()).add((int(fields[0], 16), int(fields[1], 16)))
    failures = []
    count = 0
    for chunk in plan['chunks']:
        for function in chunk['functions']:
            expected = (chunk['address'] + function['offset'], function['size'])
            if symbols.get(function['name'], set()) != {expected}:
                failures.append(function['name'])
            count += 1
    if failures:
        raise ValueError('pinned function address/size changed: ' + ', '.join(failures))
    print(f'Verified {count} pinned functions in {elf}')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--plan', required=True, type=Path)
    parser.add_argument('--template', type=Path)
    parser.add_argument('--output', type=Path)
    parser.add_argument('--build-dir')
    parser.add_argument('--library-dir')
    parser.add_argument('--verify-elf', type=Path)
    parser.add_argument('--nm', default='psp-nm')
    args = parser.parse_args()
    plan = json.loads(args.plan.read_text())
    if args.verify_elf:
        verify(plan, args.verify_elf, args.nm)
    else:
        if not all((args.template, args.output, args.build_dir, args.library_dir)):
            parser.error('generation requires template, output, build-dir and library-dir')
        script = generate(plan, args.template.read_text(), args.build_dir, args.library_dir)
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(script)


if __name__ == '__main__':
    main()
