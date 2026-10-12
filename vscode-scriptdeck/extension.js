'use strict';
const vscode = require('vscode');
const fs = require('fs');
const path = require('path');
const cp = require('child_process');
const {scriptPath, readDeck, launchArgs} = require('./core');
const api = require('./api.json');
function activate(context) {
  const output = vscode.window.createOutputChannel('ScriptDeck');
  const changed = new vscode.EventEmitter();
  const running = new Set();
  context.subscriptions.push(output, changed);
  async function decks() {
    const uris = await vscode.workspace.findFiles('**/main.deck', '**/{node_modules,.git}/**');
    const result = [];
    for (const uri of uris) {
      try { result.push({uri, root:path.dirname(uri.fsPath), deck:readDeck(fs.readFileSync(uri.fsPath,'utf8'))}); }
      catch (e) { output.appendLine(`${uri.fsPath}: ${e.message}`); }
    }
    return result;
  }
  async function selectDeck() {
    const all = await decks();
    const active = vscode.window.activeTextEditor?.document.uri.fsPath;
    const matches = all.filter(d => active && !path.relative(d.root,active).startsWith('..') && !path.isAbsolute(path.relative(d.root,active)));
    if (matches.length === 1) return matches[0];
    if (all.length === 1) return all[0];
    if (!all.length) throw Error('Folder DeckのフォルダーをVSCodeで開いてください。');
    const pick = await vscode.window.showQuickPick(all.map(d=>({label:d.deck.name || path.basename(d.root),description:d.root,value:d})),{placeHolder:'Deckを選択'});
    return pick?.value;
  }
  async function registerExecutable() {
    const config = vscode.workspace.getConfiguration('scriptdeck');
    const current = config.get('executablePath');
    const selection = await vscode.window.showOpenDialog({
      title: 'ScriptDeck.exeを選択', openLabel: '登録',
      canSelectFiles: true, canSelectFolders: false, canSelectMany: false,
      filters: { '実行ファイル': ['exe'] },
      ...(current && path.isAbsolute(current) ? {defaultUri: vscode.Uri.file(current)} : {})
    });
    if (!selection?.length) return;
    const file = selection[0].fsPath;
    if (path.basename(file).toLowerCase() !== 'scriptdeck.exe' || !fs.statSync(file).isFile()) {
      throw Error('ScriptDeck.exeを選択してください。');
    }
    // Preserve an existing workspace override; otherwise register for the user.
    const scope = config.inspect('executablePath');
    const target = scope?.workspaceValue !== undefined ? vscode.ConfigurationTarget.Workspace : vscode.ConfigurationTarget.Global;
    await config.update('executablePath', file, target);
    vscode.window.showInformationMessage(`ScriptDeck.exeを登録しました: ${file}`);
    return file;
  }
  async function executable() {
    const file = vscode.workspace.getConfiguration('scriptdeck').get('executablePath');
    return typeof file === 'string' && file.trim() ? file : await registerExecutable();
  }
  async function save() { if (!await vscode.workspace.saveAll(false)) throw Error('ファイルの保存が完了しませんでした。'); }
  function idle() { if(running.size) throw Error('起動中のScriptDeckを終了してから実行してください。'); }
  async function launch(mode,root) {
    idle(); const exe = await executable(); if (!exe) return false; await save();
    await new Promise((resolve,reject)=>{
      const child=cp.spawn(exe,launchArgs(mode,root),{cwd:fs.existsSync(root)?root:path.dirname(root),shell:false,stdio:['ignore','pipe','pipe']});
      running.add(child);
      child.stdout.on('data',d=>output.append(d.toString())); child.stderr.on('data',d=>output.append(d.toString()));
      child.once('error',e=>{running.delete(child);reject(Error(`ScriptDeckを起動できません: ${e.message}。設定のexecutablePathを確認してください。`));});
      child.once('spawn',resolve);
      child.once('close',code=>{running.delete(child);changed.fire();if(code)output.appendLine(`ScriptDeck終了コード: ${code}`);});
    });
    return true;
  }
  async function exportDeck(kind) {
    idle(); const d=await selectDeck();if(!d)return;const exe=await executable();if(!exe)return;await save();
    const suffix=kind==='exportDeckx'?'deckx':'deck';
    const uri=await vscode.window.showSaveDialog({defaultUri:vscode.Uri.file(path.join(path.dirname(d.root),path.basename(d.root)+'.'+suffix)),filters:{ScriptDeck:[suffix]}});if(!uri)return;
    await new Promise((resolve,reject)=>{
      const child=cp.spawn(exe,['-run',d.uri.fsPath,kind,uri.fsPath],{cwd:d.root,shell:false});
      let errors='';child.stdout.on('data',b=>output.append(b.toString()));child.stderr.on('data',b=>{errors+=b;output.append(b.toString());});
      child.once('error',reject);child.once('close',code=>code===0?resolve():reject(Error(errors||`エクスポート失敗 (${code})`)));
    });
    vscode.window.showInformationMessage(`出力しました: ${uri.fsPath}`);
  }
  const handlers={
    registerExecutable,
    async newFolder(){idle();const parent=await vscode.window.showOpenDialog({canSelectFolders:true,canSelectFiles:false,canSelectMany:false,openLabel:'作成先を選択'});if(!parent)return;
      const name=await vscode.window.showInputBox({prompt:'新しいDeckのフォルダー名',value:'Untitled',validateInput:v=>!v.trim()||/[<>:"/\\|?*\x00-\x1f]/.test(v)||/[. ]$/.test(v)||v==='..'||/^(con|prn|aux|nul|com[1-9]|lpt[1-9])(?:\.|$)/i.test(v)?'有効なフォルダー名を入力してください。':undefined});if(!name)return;
      const root=path.join(parent[0].fsPath,name);if(fs.existsSync(root))throw Error('同名のフォルダーが存在します。');
      if (!await launch('magic',root)) return;
      vscode.window.showInformationMessage('Magicで作成・保存後、フォルダーをVSCodeで開いてください。');
    },
    async play(){const d=await selectDeck();if(d)await launch('play',d.root);},
    async magic(){const d=await selectDeck();if(d)await launch('magic',d.root);},
    async stop(){
      if(!running.size)return vscode.window.showInformationMessage('この拡張から起動したScriptDeckはありません。');
      for(const child of running){
        if(process.platform!=='win32')throw Error('終了コマンドはWindows専用です。');
        await new Promise((resolve,reject)=>cp.execFile('powershell.exe',['-NoProfile','-NonInteractive','-Command',`$p = Get-Process -Id ${Number(child.pid)} -ErrorAction Stop; if (-not $p.CloseMainWindow()) { throw '終了要求を送信できません。ScriptDeckのウィンドウを閉じてください。' }`],{windowsHide:true},e=>e?reject(e):resolve()));
      }
    },
    exportDeck:()=>exportDeck('exportDeck'),exportDeckx:()=>exportDeck('exportDeckx'),
    async reference(){const pick=await vscode.window.showQuickPick(['APP_API.md','FILE_API.md','OBJECT_API.md','NAVIGATION_API.md','SCRIPT_API.md']);if(pick)await vscode.commands.executeCommand('markdown.showPreview',vscode.Uri.joinPath(context.extensionUri,'docs',pick));}
  };
  for(const [name,fn]of Object.entries(handlers))context.subscriptions.push(vscode.commands.registerCommand('scriptdeck.'+name,async()=>{try{return await fn();}catch(e){output.appendLine(e.stack||e.message);vscode.window.showErrorMessage(e.message);}}));
  function item(label,file,children,root){const n=new vscode.TreeItem(label,children?vscode.TreeItemCollapsibleState.Collapsed:vscode.TreeItemCollapsibleState.None);Object.assign(n,{file,children,root});
    if(file){n.resourceUri=vscode.Uri.file(scriptPath(root,file));n.command={command:'vscode.open',title:'JSを開く',arguments:[n.resourceUri]};}return n;}
  const tree={onDidChangeTreeData:changed.event,getTreeItem:n=>n,async getChildren(node){
    if(node)return node.children||[];
    return(await decks()).map(d=>item(d.deck.name||path.basename(d.root),null,[item('Deckスクリプト',d.deck.scriptFile,null,d.root),...d.deck.cards.map(c=>item(c.name||c.id,null,[item('カードスクリプト',c.scriptFile,null,d.root),...(c.objects||[]).map(o=>item(`${o.name||o.id} (${o.type})`,o.scriptFile,null,d.root))],d.root))],d.root));
  }};
  context.subscriptions.push(vscode.window.registerTreeDataProvider('scriptdeck.decks',tree));
  const watcher=vscode.workspace.createFileSystemWatcher('**/main.deck');context.subscriptions.push(watcher,watcher.onDidChange(()=>changed.fire()),watcher.onDidCreate(()=>changed.fire()),watcher.onDidDelete(()=>changed.fire()),vscode.workspace.onDidChangeWorkspaceFolders(()=>changed.fire()));
  const selector={scheme:'file',language:'javascript'};
  function containingDeck(doc) {
    // Find the owning Deck from the file itself, including outside a workspace
    // and paths excluded from VSCode's workspace search.
    let root = path.dirname(doc.uri.fsPath);
    while (true) {
      const main = path.join(root, 'main.deck');
      if (fs.existsSync(main)) {
        try { return {root, deck:readDeck(fs.readFileSync(main,'utf8').replace(/^\uFEFF/,''))}; }
        catch (e) { output.appendLine(`${main}: ${e.message}`); return; }
      }
      const parent = path.dirname(root);
      if (parent === root) return;
      root = parent;
    }
  }
  async function belongs(doc) { return !!containingDeck(doc); }
  context.subscriptions.push(vscode.languages.registerCompletionItemProvider(selector,{
    async provideCompletionItems(doc,pos){if(!await belongs(doc))return[];
      const prefix=doc.lineAt(pos).text.slice(0,pos.character);const match=prefix.match(/([\w$.]+)\.\w*$/);const owner=match?.[1];
      const items=api.filter(a=>owner?a.name.startsWith(owner+'.'):!a.name.includes('.')).map(a=>{const label=owner?a.name.slice(owner.length+1):a.name;const i=new vscode.CompletionItem(label,a.signature.includes('(')?vscode.CompletionItemKind.Method:vscode.CompletionItemKind.Property);i.detail=a.signature;i.documentation=new vscode.MarkdownString(a.description);return i;});
      if(!owner)for(const name of ['app','fs'])items.push(new vscode.CompletionItem(name,vscode.CompletionItemKind.Module));
      if(/(?:objectByName|goCard|cardByName)\(\s*["'][^"']*$/.test(prefix)){
        const d=containingDeck(doc);if(d){const names=prefix.includes('objectByName(')?d.deck.cards.flatMap(c=>(c.objects||[]).map(o=>o.name)):d.deck.cards.map(c=>c.name);for(const name of new Set(names.filter(Boolean)))items.push(new vscode.CompletionItem(name,vscode.CompletionItemKind.Value));}}
      return items;
    }
  },'.','"',"'"));
  context.subscriptions.push(vscode.languages.registerHoverProvider(selector,{async provideHover(doc,pos){if(!await belongs(doc))return;const range=doc.getWordRangeAtPosition(pos,/[\w$.]+/);if(!range)return;const a=api.find(a=>a.name===doc.getText(range));if(a)return new vscode.Hover(new vscode.MarkdownString('```javascript\n'+a.signature+'\n```\n'+a.description),range);}}));
  let warned = false;
  context.subscriptions.push(vscode.workspace.onDidChangeTextDocument(e=>{
    if (!running.size) { warned=false; return; }
    if (!warned && e.contentChanges.length && e.document.uri.fsPath.endsWith('.js')) { warned=true; vscode.window.showWarningMessage('コード編集後の内容は再起動時に読み込みます。ScriptDeckを終了してから編集してください。'); }
  }));
}
module.exports={activate};
