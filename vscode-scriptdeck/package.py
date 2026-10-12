"""Build an offline VSIX without npm dependencies."""
import json
from pathlib import Path
from xml.sax.saxutils import escape
from zipfile import ZipFile, ZIP_DEFLATED
root=Path(__file__).resolve().parent
pkg=json.loads((root/'package.json').read_text(encoding='utf-8-sig'))
name=pkg['name'];version=pkg['version'];publisher=pkg['publisher']
manifest=f'''<?xml version="1.0" encoding="utf-8"?>
<PackageManifest Version="2.0.0" xmlns="http://schemas.microsoft.com/developer/vsx-schema/2011">
<Metadata><Identity Language="en-US" Id="{name}" Version="{version}" Publisher="{publisher}"/><DisplayName>{escape(pkg['displayName'])}</DisplayName><Description xml:space="preserve">{escape(pkg['description'])}</Description><Tags>ScriptDeck,JavaScript</Tags><Categories>Programming Languages,Other</Categories><Properties><Property Id="Microsoft.VisualStudio.Code.Engine" Value="{escape(pkg['engines']['vscode'])}"/><Property Id="Microsoft.VisualStudio.Code.ExtensionDependencies" Value=""/><Property Id="Microsoft.VisualStudio.Code.ExtensionPack" Value=""/><Property Id="Microsoft.VisualStudio.Code.ExtensionKind" Value="workspace"/></Properties><License>extension/LICENSE</License></Metadata>
<Installation><InstallationTarget Id="Microsoft.VisualStudio.Code"/></Installation><Dependencies/><Assets><Asset Type="Microsoft.VisualStudio.Code.Manifest" Path="extension/package.json" Addressable="true"/><Asset Type="Microsoft.VisualStudio.Services.Content.Details" Path="extension/README.md" Addressable="true"/><Asset Type="Microsoft.VisualStudio.Services.Content.License" Path="extension/LICENSE" Addressable="true"/></Assets></PackageManifest>'''
content_types='''<?xml version="1.0" encoding="utf-8"?><Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types"><Default Extension="json" ContentType="application/json"/><Default Extension="js" ContentType="application/javascript"/><Default Extension="md" ContentType="text/markdown"/><Default Extension="vsixmanifest" ContentType="text/xml"/><Override PartName="/extension/LICENSE" ContentType="text/plain"/></Types>'''
target=root/f'{name}-{version}.vsix'
with ZipFile(target,'w',ZIP_DEFLATED) as z:
 z.writestr('extension.vsixmanifest',manifest)
 z.writestr('[Content_Types].xml',content_types)
 for name in ['package.json','extension.js','core.js','api.json','README.md','LICENSE']:
  z.write(root/name,'extension/'+name)
 for file in sorted((root/'docs').glob('*.md')):z.write(file,'extension/docs/'+file.name)
with ZipFile(target) as z:
 assert z.testzip() is None
 assert json.loads(z.read('extension/package.json').decode('utf-8-sig'))['version']==version
print(target)
