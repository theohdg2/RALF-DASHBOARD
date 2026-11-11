Import('env')
from pathlib import Path
import runpy
runpy.run_path(str(Path(env['PROJECT_DIR'])/'tools'/'build_assets.py'))['build']()
