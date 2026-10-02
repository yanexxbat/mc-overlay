"""Independent virtual-name regression cases, without game downloads."""
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools/compatibility'))
from generate_profiles import namespace_method


class Hierarchy:
    def __init__(self, flags=1, parent_flags=1):
        self.mapping = {'Camera': {'obf': 'ger'}, 'CameraInterface': {'obf': 'fvr$b'}}
        self.classes = {
            'Camera': {'parent': 'java/lang/Object', 'interfaces': ['CameraInterface'],
                       'methods': [{'raw_name': 'b', 'raw_descriptor': '()Lftm;', 'flags': flags}]},
            'CameraInterface': {'parent': 'java/lang/Object', 'interfaces': [],
                                'methods': [{'raw_name': 'b', 'raw_descriptor': '()Lftm;', 'flags': parent_flags}]}}
    def load(self, name): return self.classes.get(name)


class VirtualNames(unittest.TestCase):
    def test_camera_override_inherits_interface_name(self):
        # Actual 1.21.11 raw/interface/intermediary signature, independent of
        # the generated C++ table and Java fixture names that hid this bug.
        mapping = {('fvr$b', 'b', '()Lftm;'): 'method_71156'}
        self.assertEqual(namespace_method(Hierarchy(), 'Camera', 'b', '()Lftm;', mapping), 'method_71156')

    def test_static_and_private_declarations_are_not_virtual(self):
        mapping = {('fvr$b', 'b', '()Lftm;'): 'method_71156'}
        for flags in (2, 9):
            self.assertEqual(namespace_method(Hierarchy(flags), 'Camera', 'b', '()Lftm;', mapping), 'b')
            self.assertEqual(namespace_method(Hierarchy(parent_flags=flags), 'Camera', 'b', '()Lftm;', mapping), 'b')

    def test_local_mapping_takes_precedence(self):
        mapping = {('ger', 'b', '()Lftm;'): 'local_name', ('fvr$b', 'b', '()Lftm;'): 'interface_name'}
        self.assertEqual(namespace_method(Hierarchy(), 'Camera', 'b', '()Lftm;', mapping), 'local_name')

    def test_unmapped_signature_keeps_raw_name(self):
        self.assertEqual(namespace_method(Hierarchy(), 'Camera', 'b', '()Lftm;', {}), 'b')


if __name__ == '__main__': unittest.main()
