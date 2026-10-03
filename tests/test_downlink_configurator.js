'use strict';
// Node regression coverage for the dependency-free offline downlink builder.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const root = path.resolve(__dirname, '..');
const html = fs.readFileSync(path.join(root, 'Bolus_Downlink_Configurator/index.html'), 'utf8');
const start = html.indexOf('<script>');
const end = html.indexOf('</script>', start);
assert.ok(start >= 0 && end > start, 'Inline JavaScript not found');
const script = html.slice(start + '<script>'.length, end);
const controls = html.split('class="enable" data-target="').slice(1).map(part => part.split('"')[0]);
const elements = new Map(), handlers = new Map();
const toggles = controls.map(target => ({ dataset: { target }, checked: false, addEventListener() {} }));
const documentMock = {
  getElementById(id) {
    if (!elements.has(id)) elements.set(id, {
      value: '', checked: false, disabled: false, className: '', textContent: '',
      addEventListener(event, fn) { handlers.set(id + ':' + event, fn); },
      select() {}
    });
    return elements.get(id);
  },
  querySelectorAll(selector) {
    if (selector === '.enable') return toggles;
    if (selector === '.enable:checked') return toggles.filter(toggle => toggle.checked);
    if (selector === '.copy') return [];
    throw new Error('Unexpected selector: ' + selector);
  },
  execCommand() { return true; }
};
documentMock.getElementById('txid').value = '1';
const context = vm.createContext({
  document: documentMock,
  localStorage: { getItem: () => null, setItem() {} },
  navigator: { clipboard: { async writeText() {} } },
  btoa: value => Buffer.from(value, 'binary').toString('base64'),
  atob: value => Buffer.from(value, 'base64').toString('binary')
});
vm.runInContext(script, context);
const commands = vm.runInContext('CMD', context);
const keys = Object.keys(commands);
const defaults = {
  bmaEvent: 4, bmaStep: 0, guard: 2000, quiet: 120, tmpPeriod: 600,
  mpuBurst: 250, uplinkPeriod: 900, eventEnable: 1, mpuTrigger: 1,
  txPower: 10, sf: 7, bw: 0, cr: 1, txTimeout: 3000,
  retryDelay: 2000, maxAttempts: 3, acqLevel: 3, tmpAlert: 1,
  tmpHigh: 4100, tmpLow: 3500, tmpCycle: 6
};
function select(chosen, overrides = {}) {
  for (const key of keys) {
    const value = Object.prototype.hasOwnProperty.call(overrides, key) ? overrides[key] : defaults[key];
    documentMock.getElementById(key).value = String(value);
  }
  toggles.forEach(toggle => { toggle.checked = chosen.includes(toggle.dataset.target); });
}
const run = source => vm.runInContext(source, context);
const hex = bytes => Array.from(bytes, byte => byte.toString(16).padStart(2, '0')).join('').toUpperCase();

// All 21 command IDs must match the firmware protocol header.
const protocol = fs.readFileSync(path.join(root, 'App/Services/downlink_management_service.h'), 'utf8');
const firmwareIds = protocol.split('\n')
  .filter(line => line.includes('DOWNLINK_CMD_') && line.includes('0x'))
  .map(line => parseInt(line.split('0x')[1], 16));
assert.equal(keys.length, 21);
assert.deepEqual(keys.map(key => commands[key].id), firmwareIds);

select(['uplinkPeriod'], { uplinkPeriod: 60 });
assert.equal(hex(run('buildDownlink(1, selectedKeys())')), 'D101010107043C000000');
assert.equal(Buffer.from(run('buildDownlink(1, selectedKeys())')).toString('base64'), '0QEBAQcEPAAAAA==');
handlers.get('generate:click')();
assert.equal(documentMock.getElementById('hexCompact').value, 'D101010107043C000000');

// Apply acquisition preset first, then explicit motion sensitivity.
select(['bmaEvent', 'acqLevel'], { bmaEvent: 5, acqLevel: 4 });
assert.equal(hex(run('buildDownlink(2, selectedKeys())')), 'D1010202110104010105');
select(keys);
assert.match(run('validateBuilder(3, selectedKeys())').join(' '), /split into separate transactions/);

select(['bw'], { bw: 1 });
assert.match(run('validateBuilder(3, selectedKeys())').join(' '), /also select SF7/);
select(['bw', 'sf'], { bw: 1, sf: 7 });
assert.equal(run('validateBuilder(3, selectedKeys())').length, 0);
select(['bmaStep'], { bmaStep: 4 });
assert.match(run('validateBuilder(3, selectedKeys())').join(' '), /Default/);

select(['uplinkPeriod'], { uplinkPeriod: '' });
assert.match(run('validateBuilder(3, selectedKeys())').join(' '), /integer value/);
handlers.get('generate:click')();
assert.equal(documentMock.getElementById('hexCompact').value, '', 'Invalid input clears stale payload');
select(['uplinkPeriod'], { uplinkPeriod: 60 });
documentMock.getElementById('txid').value = '';
assert.match(run('validateBuilder(0, selectedKeys())').join(' '), /Transaction ID/);
const ack = run('decodeControlUplink([0xD2,1,1,0,0x10,0,12,0])');
assert.equal(ack.apply_mask.raw, '0x0010');
assert.match(ack.apply_mask.note, /does not prove/);
console.log('PASS: offline builder commands, vectors, ordering, validation, and ACK semantics');
