const {test}=require('node:test');const assert=require('node:assert/strict');const Module=require('node:module');const fs=require('node:fs');const os=require('node:os');const path=require('node:path');const {EventEmitter}=require('node:events');
test('Extension activates, builds tree, completes APIs, and saves before launch',async()=>{
 const root=fs.mkdtempSync(path.join(os.tmpdir(),'ScriptDeck 拡張 '));fs.mkdirSync(path.join(root,'script'));
 const main=path.join(root,'main.deck');fs.writeFileSync(main,JSON.stringify({format:'ScriptDeck',storage:'folder',name:'確認',scriptFile:'script/deck.js',cards:[{id:'c',name:'カード',scriptFile:'script/カード.js',objects:[{id:'b',name:'ボタン',type:'button',scriptFile:'script/カード_ボタン.js'},{id:'t',name:'表示',type:'text'}]}]}));
 const handlers={};let tree,completion,hover,spawned,saved=false;const messages=[];let child;
 const disposable={dispose(){}};const uri=f=>({fsPath:f,scheme:'file'});
 const vscode={EventEmitter:class{constructor(){this.event=()=>disposable;}fire(){}dispose(){}},TreeItem:class{constructor(label,state){this.label=label;this.collapsibleState=state;}},TreeItemCollapsibleState:{Collapsed:1,None:0},CompletionItem:class{constructor(label,kind){this.label=label;this.kind=kind;}},CompletionItemKind:{Method:1,Property:2,Module:3,Value:4},MarkdownString:class{constructor(value){this.value=value;}},Hover:class{constructor(contents){this.contents=contents;}},Uri:{file:uri,joinPath:(u,...parts)=>uri(path.join(u.fsPath,...parts))},window:{activeTextEditor:undefined,createOutputChannel:()=>({append(){},appendLine(){},dispose(){}}),registerTreeDataProvider:(_id,t)=>{tree=t;return disposable;},showErrorMessage:m=>messages.push(m),showInformationMessage:m=>messages.push(m),showWarningMessage:m=>messages.push(m)},workspace:{findFiles:async()=>[uri(main)],saveAll:async()=>{saved=true;return true;},getConfiguration:()=>({get:()=> 'C:\\Program Files\\ScriptDeck.exe'}),createFileSystemWatcher:()=>({onDidChange:()=>disposable,onDidCreate:()=>disposable,onDidDelete:()=>disposable,dispose(){}}),onDidChangeWorkspaceFolders:()=>disposable,onDidChangeTextDocument:()=>disposable},commands:{registerCommand:(id,fn)=>{handlers[id]=fn;return disposable;}},languages:{registerCompletionItemProvider:(_s,p)=>{completion=p;return disposable;},registerHoverProvider:(_s,p)=>{hover=p;return disposable;}}};
 const cp={spawn(exe,args,options){assert.ok(saved);spawned={exe,args,options};child=new EventEmitter();child.stdout=new EventEmitter();child.stderr=new EventEmitter();process.nextTick(()=>child.emit('spawn'));return child;}};
 const original=Module._load;Module._load=function(name,...args){if(name==='vscode')return vscode;if(name==='child_process')return cp;return original.call(this,name,...args);};
 try{
  delete require.cache[require.resolve('../extension')];require('../extension').activate({subscriptions:[],extensionUri:uri(path.resolve(__dirname,'..'))});assert.equal(Object.keys(handlers).length,8);
  const roots=await tree.getChildren();assert.equal(roots[0].label,'確認');const nodes=await tree.getChildren(roots[0]);assert.equal(nodes[0].command.arguments[0].fsPath,path.join(root,'script/deck.js'));assert.equal(nodes[1].children[1].label,'ボタン (button)');assert.equal(nodes[1].children[2].label,'表示 (text)');assert.equal(nodes[1].children[2].command,undefined);
  const doc={uri:uri(path.join(root,'script/deck.js')),lineAt:()=>({text:'app.'})};assert.ok((await completion.provideCompletionItems(doc,{character:4})).some(x=>x.label==='writeLine'));
  // Files must complete even when workspace search returns no Decks.
  const findFiles=vscode.workspace.findFiles;vscode.workspace.findFiles=async()=>[];
  doc.lineAt=()=>({text:'fs.'});
  let suggestions=await completion.provideCompletionItems(doc,{character:3});
  assert.ok(suggestions.some(x=>x.label==='readText'));assert.ok(suggestions.some(x=>x.label==='writeText'));
  doc.lineAt=()=>({text:'fs.rea'});suggestions=await completion.provideCompletionItems(doc,{character:6});assert.ok(suggestions.some(x=>x.label==='readText'));
  doc.lineAt=()=>({text:'f'});assert.ok((await completion.provideCompletionItems(doc,{character:1})).some(x=>x.label==='fs'));
  vscode.workspace.findFiles=findFiles;
  doc.getWordRangeAtPosition=()=>({});doc.getText=()=> 'app.writeLine';assert.ok(await hover.provideHover(doc,{}));
  await handlers['scriptdeck.play']();assert.deepEqual(spawned.args,[root]);assert.equal(spawned.options.shell,false);
  await handlers['scriptdeck.magic']();assert.ok(messages.some(m=>m.includes('起動中')));child.emit('close',0);
  saved=false;vscode.workspace.saveAll=async()=>false;spawned=null;await handlers['scriptdeck.play']();assert.equal(spawned,null);assert.ok(messages.some(m=>m.includes('保存')));

  let configured = '', selection, updates = [], prompts = 0, workspaceValue;
  vscode.ConfigurationTarget = {Global:1, Workspace:2};
  vscode.workspace.getConfiguration = () => ({
    get: () => configured, inspect: () => ({workspaceValue}),
    update: async (key,value,target) => {updates.push({key,value,target});configured=value;}
  });
  vscode.window.showOpenDialog = async options => {prompts++;assert.equal(options.canSelectFiles,true);assert.equal(options.canSelectFolders,false);return selection;};
  saved=false;spawned=null;
  await handlers['scriptdeck.play']();
  assert.equal(prompts,1);assert.equal(spawned,null);assert.equal(saved,false);assert.equal(updates.length,0);
  await handlers['scriptdeck.exportDeck']();
  assert.equal(prompts,2);assert.equal(spawned,null);
  const executable=path.join(root,'ScriptDeck.exe');fs.writeFileSync(executable,'test fixture');selection=[uri(executable)];
  vscode.workspace.saveAll=async()=>{saved=true;return true;};
  await handlers['scriptdeck.play']();
  assert.equal(spawned.exe,executable);assert.deepEqual(updates,[{key:'executablePath',value:executable,target:1}]);child.emit('close',0);
  const before=prompts;await handlers['scriptdeck.magic']();assert.equal(prompts,before);child.emit('close',0);
  workspaceValue=executable;await handlers['scriptdeck.registerExecutable']();assert.equal(updates.at(-1).target,2);
  const count=updates.length;selection=[uri(main)];await handlers['scriptdeck.registerExecutable']();assert.equal(updates.length,count);assert.ok(messages.some(m=>m==='ScriptDeck.exeを選択してください。'));
  selection=undefined;await handlers['scriptdeck.registerExecutable']();assert.equal(configured,executable);assert.equal(updates.length,count);
 }finally{Module._load=original;fs.rmSync(root,{recursive:true,force:true});}
});
