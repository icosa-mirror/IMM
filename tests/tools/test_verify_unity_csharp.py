"""Guard current source discovery without changing Unity-generated projects."""
from pathlib import Path
import tempfile
import unittest
import xml.etree.ElementTree as ET
from unittest.mock import patch

import verify_unity_csharp as verifier


class RuntimeSourceOwnershipTests(unittest.TestCase):
    def test_new_sources_included_and_nested_assemblies_excluded(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            for relative in ('Main.cs', 'Authoring/NewImage.cs', 'URP/Separate.cs',
                             'External/Other.cs', 'URP/ImmUnity.URP.asmdef', 'External/Other.asmref'):
                path = root / relative
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text('', encoding='utf-8')
            self.assertEqual([path.relative_to(root).as_posix() for path in verifier.assembly_sources(root)],
                             ['Authoring/NewImage.cs', 'Main.cs'])

    def test_overlay_is_scoped_to_its_project_and_preserves_generated_file(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / 'Example.Runtime.asmdef').write_text('{}', encoding='utf-8')
            source = root / 'New.cs'
            source.write_text('class New {}', encoding='utf-8')
            generated = root / 'Example.Runtime.csproj'
            generated.write_text('<Project />', encoding='utf-8')
            original = generated.read_bytes()
            with patch.dict(verifier.RUNTIME_SOURCE_ROOTS, {'Example.Runtime': root}):
                overlay = verifier.write_runtime_sources('Example.Runtime', root)
            tree = ET.parse(overlay)
            target = next(node for node in tree.getroot() if node.tag.endswith('Target'))
            self.assertEqual(target.attrib['Condition'], "'$(MSBuildProjectName)' == 'Example.Runtime'")
            includes = [node.attrib['Include'] for node in target.iter() if 'Include' in node.attrib]
            self.assertEqual(includes, [str(source.resolve())])
            self.assertEqual(generated.read_bytes(), original)


if __name__ == '__main__':
    unittest.main()
