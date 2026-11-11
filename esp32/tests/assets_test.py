from pathlib import Path
import gzip
import importlib.util
import subprocess
import unittest

ESP=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('builder',ESP/'tools/build_assets.py');builder=importlib.util.module_from_spec(spec);spec.loader.exec_module(builder)
class AssetsTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls): cls.assets=builder.build()
    def test_no_external_dependencies(self):
        self.assertNotIn('@import',self.assets['style.css'])
        self.assertNotIn('api.github.com',self.assets['app.js'])
        self.assertNotIn('githubToken',self.assets['app.js'])
        self.assertNotIn('crypto.randomUUID',self.assets['app.js'])
        self.assertNotIn('commit ESP32',self.assets['app.js'])
    def test_generated_javascript(self):
        import tempfile
        for name in ['app.js','config.js','esp32-sync.js']:
            with tempfile.TemporaryDirectory() as folder:
                path=Path(folder)/name;path.write_text(self.assets[name],encoding='utf-8')
                subprocess.run(['node','--check',str(path)],check=True,capture_output=True)
    def test_gzip_roundtrip(self):
        for name,source in self.assets.items():self.assertEqual(gzip.decompress((ESP/'data'/(name+'.gz')).read_bytes()).decode(),source)
    def test_partitions_separate_web_and_state(self):
        rows=[]
        for line in (ESP/'partitions.csv').read_text().splitlines():
            if not line or line.startswith('#'):continue
            name,kind,sub,offset,size,*_=line.split(',');rows.append((name.strip(),int(offset,16),int(size,16)))
        self.assertTrue(any(x[0]=='state' for x in rows));self.assertTrue(any(x[0]=='spiffs' for x in rows))
        for a,b in zip(rows,rows[1:]):self.assertLessEqual(a[1]+a[2],b[1])
        self.assertLessEqual(rows[-1][1]+rows[-1][2],16*1024*1024)
        # PlatformIO selects the last filesystem partition for uploadfs.
        self.assertEqual([r[0] for r in rows if r[0] in ('state','spiffs')][-1],'spiffs')
if __name__=='__main__':unittest.main()
