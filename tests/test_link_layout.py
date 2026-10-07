import importlib.util
from pathlib import Path
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location('link_layout', Path(__file__).parents[1] / 'scripts/link_layout.py')
layout = importlib.util.module_from_spec(spec)
spec.loader.exec_module(layout)


class LayoutTest(unittest.TestCase):
    def plan(self):
        return {'pinned_end': 128, 'chunks': [
            {'address': 0, 'slot': 64, 'kind': 'startup', 'owner': 'crt0_prx.o',
             'section': '.text', 'functions': []},
            {'address': 64, 'slot': 64, 'kind': 'archive', 'owner': 'math.a',
             'member': 'tnl.o', 'section': '.text',
             'functions': [{'name': 'transform', 'offset': 4, 'size': 32}]}]}

    def test_script_reserves_whole_archive_member_and_cold_tail(self):
        template = 'HEAD\n  .text           :\nOLD\n  .init           :\nTAIL'
        script = layout.generate(self.plan(), template, 'out', 'library')
        self.assertIn('"library/math.a:tnl.o"(.text)', script)
        self.assertIn('ASSERT(. <= 0x80,', script)
        self.assertLess(script.index('. = 0x80;'), script.index('*(.text .stub'))
        self.assertTrue(script.endswith('  .init           :\nTAIL'))

    def test_overflow_and_overlap_rejected(self):
        plan = self.plan()
        plan['chunks'][1]['functions'][0]['size'] = 64
        with self.assertRaises(ValueError):
            layout.validate(plan)
        plan = self.plan()
        plan['chunks'][1]['address'] = 0
        with self.assertRaises(ValueError):
            layout.validate(plan)

    def test_verify_rejects_displacement_size_change_and_missing_function(self):
        for output in ['00000044 00000020 T transform\n',
                       '00000048 00000020 T transform\n',
                       '00000044 00000024 T transform\n', '']:
            with patch.object(layout.subprocess, 'check_output', return_value=output):
                if output.startswith('00000044 00000020'):
                    layout.verify(self.plan(), 'test.elf', 'nm')
                else:
                    with self.assertRaises(ValueError):
                        layout.verify(self.plan(), 'test.elf', 'nm')


if __name__ == '__main__':
    unittest.main()
