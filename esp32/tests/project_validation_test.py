"""Compile and exercise the exact firmware project validator with ArduinoJson."""
import json
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
JSON_INCLUDE = ROOT / 'esp32/.pio/libdeps/esp32s3-n16r8/ArduinoJson/src'

class FirmwareValidation(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if not JSON_INCLUDE.exists():
            raise unittest.SkipTest('Run pio run in esp32 first to install ArduinoJson')
        cls.tmp = tempfile.TemporaryDirectory()
        cls.binary = Path(cls.tmp.name) / 'validator'
        main = (ROOT / 'esp32/src/main.cpp').read_text()
        actual = main[main.index('bool validString('):main.index('bool readRecord(')]
        source = '#include <ArduinoJson.h>\n#include <string>\n#include <iostream>\n#include <iterator>\n#include <cmath>\n#include "AuthPolicy.h"\nusing String=std::string;using std::isfinite;constexpr size_t MAX_PROJECT_BYTES=65536;\n' + actual
        source += '\nint main(){std::string data((std::istreambuf_iterator<char>(std::cin)),{});return validProject(data)?0:1;}'
        cpp = Path(cls.tmp.name) / 'validator.cpp'
        cpp.write_text(source)
        subprocess.run(['g++','-std=c++17','-I',str(JSON_INCLUDE),'-I',str(ROOT/'esp32/src'),str(cpp),'-o',str(cls.binary)],check=True,capture_output=True)
        script = r"""
const fs=require('fs'),core=require('./esp32/web/project-core');
const seed=JSON.parse(fs.readFileSync('data/project.json'));
const actor={id:'a',name:'Théo Van de Walle',email:'theo@example.test'};
const cases={seed};
for(const type of ['settings','notes','profile','task','trial','evidence']){
 const p=structuredClone(seed);
 if(type==='settings')p.config.before=80;
 if(type==='notes')p.checkpoints={PC3:{summary:'Pièces assemblées',decisions:'Essais à poursuivre',next:'Contrôler le largage'}};
 if(type==='profile')p.profiles=[{id:'p1',name:'Profil terrain',config:structuredClone(p.config)}];
 if(type==='task')Object.assign(p.tasks[0],{ownerId:'a',owner:'Théo Van de Walle'});
 if(type==='trial'||type==='evidence')p.trials=[{id:'t1',title:'Test de validation',date:'2026-11-11',type:'Terrain',result:'À analyser',hypothesis:'À valider',observation:'Donnée de test',version:'test',config:structuredClone(p.config),profileId:'p1',profileName:'Profil terrain',checkpoint:'PC3'}];
 if(type==='evidence'){p.trials[0].photos=['a'.repeat(32)];p.trials[0].issueIds=[p.issues[0].id];p.issues[0].correction='Réglage documenté';}p.history=core.journal(seed,p,actor,type);cases[type]=p;
}
console.log(JSON.stringify(cases));
"""
        cls.cases=json.loads(subprocess.check_output(['node','-e',script],cwd=ROOT,text=True))

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def valid(self, project):
        return subprocess.run([str(self.binary)],input=json.dumps(project,ensure_ascii=False),text=True).returncode==0

    def test_existing_project_and_new_edits_are_accepted(self):
        for name, project in self.cases.items():
            with self.subTest(name=name): self.assertTrue(self.valid(project))

    def test_saved_parameters_survive_serialization(self):
        project=json.loads(json.dumps(self.cases['settings']))
        self.assertTrue(self.valid(project))
        self.assertEqual(project['config']['before'],80)
        self.assertEqual(project['history'][0]['changes'][0]['after'],80)

    def test_invalid_history_and_parameters_are_rejected(self):
        for mutation in ['history','config','notes','photos','links']:
            project=json.loads(json.dumps(self.cases['settings']))
            if mutation=='history': project['history'][0]['changes']={}
            if mutation=='config': project['config']['before']=101
            if mutation=='notes': project['checkpoints']={'PC3': 'invalid'}
            if mutation in ('photos','links'):
                project=json.loads(json.dumps(self.cases['evidence']))
                if mutation=='photos': project['trials'][0]['photos']=['../forbidden']
                if mutation=='links': project['trials'][0]['issueIds']=['x']*25
            with self.subTest(mutation=mutation): self.assertFalse(self.valid(project))

if __name__=='__main__': unittest.main()
