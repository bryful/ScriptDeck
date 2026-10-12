const {test}=require('node:test');
const assert=require('node:assert/strict');
const path=require('node:path');
const {scriptPath,readDeck,launchArgs}=require('../core');
test('Folder main.deck validation',()=>{assert.equal(readDeck('{"format":"ScriptDeck","storage":"folder","cards":[]}').storage,'folder');for(const text of ['{}','{"format":"ScriptDeck","cards":[]}','not json'])assert.throws(()=>readDeck(text));});
test('External scripts remain inside Deck including Japanese and spaces',()=>{const root=path.resolve('Deck 日本語');assert.equal(scriptPath(root,'script/カード ボタン.js'),path.join(root,'script/カード ボタン.js'));for(const file of ['../outside.js',path.resolve('outside.js'),'',null])assert.throws(()=>scriptPath(root,file));});
test('Launch arguments preserve paths without shell quoting',()=>{assert.deepEqual(launchArgs('magic','C:\\Deck 日本語'),['--magicFolder','C:\\Deck 日本語']);assert.deepEqual(launchArgs('play','a b'),['a b']);});
test('Manifest commands match implementation and exclude script-opening commands',()=>{const pkg=require('../package.json');const source=require('fs').readFileSync(require('path').join(__dirname,'../extension.js'),'utf8');assert.equal(pkg.contributes.commands.length,8);for(const cmd of pkg.contributes.commands){assert.match(source,new RegExp(cmd.command.split('.')[1]));assert.ok(!cmd.command.includes('openScript'));}assert.equal(pkg.capabilities.untrustedWorkspaces.supported,false);});
test('API catalog has real app/fs APIs and event handlers',()=>{const api=require('../api.json');for(const name of ['app.writeLine','fs.readText','mouseUp','openCard'])assert.ok(api.some(a=>a.name===name),name);});
