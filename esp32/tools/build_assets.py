#!/usr/bin/env python3
"""Build an offline ESP32 UI from the current root GitHub Pages sources."""
from pathlib import Path
import gzip
import re

ESP = Path(__file__).resolve().parents[1]
ROOT = ESP.parent

def build():
    app = (ROOT / 'app.js').read_text(encoding='utf-8')
    start = app.index('function connectionModal(){')
    end = app.index('function disconnectGitHub', start)
    modal = 'function connectionModal(){accountLoginModal();}\n'
    app = app[:start] + modal + app[end:]
    app = app.replace('new RalfGithubStore(window.RALF_REMOTE)', 'new RalfEsp32Store(window.RALF_REMOTE)')
    # Header names and HTML IDs stay internal; visible labels use local storage terminology.
    app = app.replace('GitHub', 'ESP32')
    app = app.replace('issues,reports})[v]()','issues,reports,documents,members})[v]()')
    app = app.replace('function ensureEditor(){','function ensureEditor(){if(remote.user?.mustChangePassword){passwordModal(true);return false;}')
    app = app.replace('Sauvegardé sur ESP32 · visible pour l’équipe', 'Sauvegardé dans l’ESP32 · visible pour l’équipe')
    app = app.replace('Chaque sauvegarde ajoute un commit ESP32.', 'Les données persistent après un redémarrage. La dernière version valide est conservée en secours.')
    app = app.replace("const draftKey='ralf-pending-v2'", "const draftKey='ralf-esp32-pending-v1'")
    app = app.replace('crypto.randomUUID()', 'projectId()')
    app = '''function projectId(){const bytes=new Uint8Array(16);crypto.getRandomValues(bytes);return Array.from(bytes,b=>b.toString(16).padStart(2,'0')).join('');}\n''' + app
    app = app.replace("toast(error.message==='CONFLICT'?", "toast(error.message==='PROJECT_TOO_LARGE'?'Le suivi dépasse 64 Kio. Exporte et archive des essais.':error.message==='CONFLICT'?")
    html = (ROOT/'index.html').read_text(encoding='utf-8')
    html = re.sub(r'(src|href)="(config.js|sync.js|app.js|style.css)(?:\?[^\"]*)?"', lambda m:f'{m[1]}="{m[2]}"', html)
    html = html.replace('<script src="app.js">','<script src="members-ui.js"></script><script src="app.js">')
    html = html.replace('src="sync.js"','src="esp32-sync.js"').replace('Connexion GitHub','Connexion ESP32').replace('Chargement GitHub','Chargement ESP32').replace('Consultation publique','Hébergement local ESP32')
    css = (ROOT/'style.css').read_text(encoding='utf-8')
    css = re.sub(r"@import\s+url\([^)]*\)\s*;\s*", '', css)
    css += '\n'+(ESP/'web/members.css').read_text(encoding='utf-8')
    config = "window.RALF_REMOTE={owner:'RALF',repo:'ESP32',branch:'mémoire interne',path:'projet local'};\n"
    assets = {'index.html':html,'app.js':app,'style.css':css,'config.js':config,'esp32-sync.js':(ESP/'web/esp32-sync.js').read_text(encoding='utf-8'),'members-ui.js':(ESP/'web/members-ui.js').read_text(encoding='utf-8')}
    output=ESP/'data';output.mkdir(exist_ok=True)
    for name,content in assets.items():
        (output/(name+'.gz')).write_bytes(gzip.compress(content.encode('utf-8'),mtime=0))
    (output/'logo-ralf.jpg').write_bytes((ROOT/'logo-ralf.jpg').read_bytes())
    (output/'seed.json').write_bytes((ROOT/'data/project.json').read_bytes())
    return assets

if __name__=='__main__':
    assets=build()
    print('ESP32 assets: '+', '.join(assets))
    print('Compressed web assets: '+str(sum(p.stat().st_size for p in (ESP/'data').glob('*.gz')))+' bytes')
