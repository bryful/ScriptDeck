'use strict';
const path = require('path');
function scriptPath(root, relative) {
  if (typeof relative !== 'string' || !relative || path.isAbsolute(relative)) throw Error('不正なscriptFileです。');
  const target = path.resolve(root, relative);
  const rel = path.relative(root, target);
  if (rel === '..' || rel.startsWith('..' + path.sep) || path.isAbsolute(rel)) throw Error('scriptFileがDeck外を参照しています。');
  return target;
}
function readDeck(text) {
  const deck = JSON.parse(text);
  if (deck.format !== 'ScriptDeck' || deck.storage !== 'folder' || !Array.isArray(deck.cards)) throw Error('Folder形式のmain.deckを選択してください。');
  return deck;
}
function launchArgs(mode, root) { return mode === 'magic' ? ['--magicFolder', '--vscode', root] : [root]; }
module.exports = {scriptPath, readDeck, launchArgs};
