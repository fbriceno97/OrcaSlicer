#!/usr/bin/env python3
"""Check the JSON profile contract still used by the native setup wizard.

Run against the installed/portable resources, not just the checkout: .opc caches
alone support PresetBundle but cannot populate GuideFrame's JSON-based picker.
"""
import argparse
import hashlib
import json
from pathlib import Path


def check_profiles(profiles, reference=None):
    profiles = Path(profiles)
    models = []
    vendors = 0
    for manifest in sorted(profiles.glob('*.json')):
        vendor = json.loads(manifest.read_text(encoding='utf-8-sig'))
        if not isinstance(vendor, dict) or 'machine_model_list' not in vendor:
            continue
        vendors += 1
        for kind in ('machine_model_list', 'machine_list', 'filament_list', 'process_list'):
            for entry in vendor.get(kind, []):
                path = profiles / manifest.stem / entry['sub_path']
                if not path.is_file():
                    raise ValueError(f'{manifest.name}: missing {kind} file: {path}')
                if kind == 'machine_model_list':
                    model = json.loads(path.read_text(encoding='utf-8-sig'))
                    nozzles = model['nozzle_diameter'].replace(' ', '')
                    if not nozzles:
                        raise ValueError(f'No nozzle variants: {path}')
                    models.append(dict(model=entry['name'], name=model['name'],
                                       vendor=manifest.stem, nozzle_diameter=nozzles,
                                       materials=model['default_materials'],
                                       cover='', nozzle_selected=''))
    names = {(model['vendor'], model['model']) for model in models}
    for required in ('Bambu Lab A1', 'Bambu Lab P2S'):
        if ('BBL', required) not in names:
            raise ValueError(f'Setup cannot offer required printer: {required}')
    if reference:
        for source in Path(reference).rglob('*.json'):
            relative = source.relative_to(reference)
            target = profiles / relative
            if not target.is_file() or hashlib.sha256(source.read_bytes()).digest() != hashlib.sha256(target.read_bytes()).digest():
                raise ValueError(f'Packaged profile missing or changed: {relative}')
    return vendors, models


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--profiles', type=Path, required=True)
    parser.add_argument('--reference', type=Path)
    args = parser.parse_args()
    vendors, models = check_profiles(args.profiles, args.reference)
    print(f'PASS: {vendors} JSON vendor manifests, {len(models)} selectable models; A1/P2S present')


if __name__ == '__main__':
    main()
