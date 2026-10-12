"""Refresh API suggestions and reference docs from the repository."""
from pathlib import Path
import json,re,shutil
root=Path(__file__).resolve().parent
source=root.parent/'doc'
(root/'docs').mkdir(exist_ok=True)
api={}
for name in ['APP_API.md','FILE_API.md','OBJECT_API.md','NAVIGATION_API.md','SCRIPT_API.md']:
 for line in (source/name).read_text(encoding='utf8').splitlines():
  if not line.startswith('|'):continue
  for signature in re.findall(r'`((?:app\.|fs\.|this\.|event\.)?[A-Za-z_$][\w.$]*(?:\([^`]*\))?)`',line):
   key=signature.split('(')[0]
   if key not in ['true','false','undefined','null']:
    api.setdefault(key,{'name':key,'signature':signature,'description':line.strip('| ').split('|')[-1].strip(),'file':name})
(root/'api.json').write_text(json.dumps(list(api.values()),ensure_ascii=False,indent=2)+'\n',encoding='utf8')
for file in source.glob('*.md'):shutil.copyfile(file,root/'docs'/file.name)
print(f'{len(api)} API entries')
