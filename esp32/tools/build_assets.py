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
    modal = '''function connectionModal(){
 openModal(`<h2>Édition locale sur l’ESP32</h2><p class="muted">Les données sont partagées entre les appareils connectés à cet ESP32 et conservées dans sa mémoire.</p>
 ${dirty?'<div class="notice">Un brouillon attend sa sauvegarde. En cas de conflit, exporte-le puis recharge la version partagée.</div>':''}
 <p class="muted">Utilise la clé d’édition locale affichée dans le moniteur série au démarrage. Elle reste en mémoire dans cette page jusqu’au rechargement.</p>
 <form id="connectForm"><label>Clé d’édition ESP32<input id="editorKey" name="key" type="password" autocomplete="off" spellcheck="false" required></label><div class="form-actions"><button class="primary">Activer l’édition</button><button type="button" onclick="disconnectGitHub()">Déconnecter</button></div></form>
 <div class="form-actions"><button onclick="retrySync()">Réessayer la sauvegarde</button><button onclick="exportProject()">Exporter le brouillon JSON</button><button onclick="reloadShared()">Recharger la version partagée</button></div>
 ${recoveredDraft?'<div class="notice">Brouillon récupérable : <button onclick="downloadRecovered()">Exporter ce brouillon</button></div>':''}
 <p class="muted section-gap">Capacité du suivi : 64 Kio. Les archives CSV et JSON se téléchargent sur ton appareil.</p>`);
 $('#connectForm').onsubmit=async e=>{e.preventDefault();const key=$('#editorKey').value;$('#editorKey').value='';remote.setToken(key);try{await remote.authenticate();const loaded=await remote.read();if(!validateProject(loaded.data))throw Error('INVALID_DATA');if(!dirty){state=remote.accept(loaded);remoteReady=true;remoteError='';lastSaved=state.updatedAt;render();}$('#modal').close();updateSyncUI();toast('Édition locale activée');}catch(error){remote.setToken('');updateSyncUI();toast(error.status===401?'Clé d’édition refusée (HTTP 401). Consulte le moniteur série.':error.status===403?'Origine de la page refusée (HTTP 403). Ouvre le site directement depuis l’adresse de l’ESP32.':error.status===404?'Route API absente (HTTP 404). Recharge le firmware et les fichiers du site de la même version.':error.message==='STORAGE_UNAVAILABLE'?'Mémoire du projet indisponible (HTTP 503). Consulte le moniteur série.':error.message==='INVALID_DATA'?'Le projet reçu est invalide. Ouvre /api/project pour vérifier la réponse.':error.message==='INVALID_RESPONSE'?'Réponse de session inattendue. Recharge le firmware et les fichiers web.':error.name==='SyntaxError'?'La réponse de l’ESP32 n’est pas un JSON valide. Ouvre /api/project.':error.name==='AbortError'?'Délai de connexion dépassé. Réessaie après quelques secondes.':'Requête réseau impossible. Vérifie la connexion au Wi-Fi de l’ESP32.');}};
}
'''
    app = app[:start] + modal + app[end:]
    app = app.replace('new RalfGithubStore(window.RALF_REMOTE)', 'new RalfEsp32Store(window.RALF_REMOTE)')
    # Header names and HTML IDs stay internal; visible labels use local storage terminology.
    app = app.replace('GitHub', 'ESP32')
    app = app.replace('Sauvegardé sur ESP32 · visible pour l’équipe', 'Sauvegardé dans l’ESP32 · visible pour l’équipe')
    app = app.replace('Chaque sauvegarde ajoute un commit ESP32.', 'Les données persistent après un redémarrage. La dernière version valide est conservée en secours.')
    app = app.replace("const draftKey='ralf-pending-v2'", "const draftKey='ralf-esp32-pending-v1'")
    app = app.replace('crypto.randomUUID()', 'projectId()')
    app = '''function projectId(){const bytes=new Uint8Array(16);crypto.getRandomValues(bytes);return Array.from(bytes,b=>b.toString(16).padStart(2,'0')).join('');}\n''' + app
    app = app.replace("toast(error.message==='CONFLICT'?", "toast(error.message==='PROJECT_TOO_LARGE'?'Le suivi dépasse 64 Kio. Exporte et archive des essais.':error.message==='CONFLICT'?")
    html = (ROOT/'index.html').read_text(encoding='utf-8')
    html = re.sub(r'(src|href)="(config.js|sync.js|app.js|style.css)(?:\?[^\"]*)?"', lambda m:f'{m[1]}="{m[2]}"', html)
    html = html.replace('src="sync.js"','src="esp32-sync.js"').replace('Connexion GitHub','Connexion ESP32').replace('Chargement GitHub','Chargement ESP32').replace('Consultation publique','Hébergement local ESP32')
    css = (ROOT/'style.css').read_text(encoding='utf-8')
    css = re.sub(r"@import\s+url\([^)]*\)\s*;\s*", '', css)
    config = "window.RALF_REMOTE={owner:'RALF',repo:'ESP32',branch:'mémoire interne',path:'projet local'};\n"
    assets = {'index.html':html,'app.js':app,'style.css':css,'config.js':config,'esp32-sync.js':(ESP/'web/esp32-sync.js').read_text(encoding='utf-8')}
    output=ESP/'data';output.mkdir(exist_ok=True)
    for name,content in assets.items():
        (output/(name+'.gz')).write_bytes(gzip.compress(content.encode('utf-8'),mtime=0))
    (output/'seed.json').write_bytes((ROOT/'data/project.json').read_bytes())
    return assets

if __name__=='__main__':
    assets=build()
    print('ESP32 assets: '+', '.join(assets))
    print('Compressed web assets: '+str(sum(p.stat().st_size for p in (ESP/'data').glob('*.gz')))+' bytes')
