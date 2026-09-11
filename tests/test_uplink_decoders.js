'use strict';

const assert = require('assert');
const fs = require('fs');
const path = require('path');
const vm = require('vm');

const repositoryRoot = path.resolve(__dirname, '..');
const decoderPaths = [
  'Network_Decoders/chirpstack_uplink_decoder.js',
  'Network_Decoders/the_things_stack_uplink_decoder.js'
];

const telemetryV2 = [
  0x21, 0x34, 0x12, 0x0B, 0x8F, 87, 0x74, 0x0E,
  0x55, 0x0F, 0x0A, 0x0F, 0x87, 0x0F, 0x83, 0xFF,
  2, 5, 1, 3, 48, 7, 5, 10, 30, 12, 40, 8, 20, 0, 0, 0x05
];

const telemetryV2_1 = telemetryV2.slice();
telemetryV2_1[0] = 0x31;
telemetryV2_1.push(0x78, 0x56, 0x34, 0x12, 0xCF, 0x00, 0x3A, 0xFF, 0xBC, 0x03);

/* Exact FPort-2 Base64 payload from the supplied ChirpStack uplink log. */
const suppliedTelemetryV2_1 = Array.from(Buffer.from(
  'MRAAC48A+AyNCYwJlAn6/wEDCAMCAAM7Xy5NAD8AAAAAAAAAzwA6/7wD',
  'base64'
));

const telemetryV2_2 = [
  0x41, 0x10, 0x00, 0x0B, 0x8F, 0xF8, 0x0C, 0x8D,
  0x09, 0x8C, 0x09, 0x94, 0x09, 0xFA, 0xFF, 0x01,
  0x03, 0x08, 0x03, 0x02, 0x00, 0x03, 0x3B, 0x5F,
  0x2E, 0x4D, 0x00, 0x3F, 0x00, 0x00, 0x00, 0x00,
  0xCF, 0x00, 0x3A, 0xFF, 0xBC, 0x03
];

for (const relativePath of decoderPaths) {
  const context = vm.createContext({});
  const source = fs.readFileSync(path.join(repositoryRoot, relativePath), 'utf8');
  vm.runInContext(source, context, { filename: relativePath });

  const legacy = context.decodeUplink({ fPort: 2, bytes: telemetryV2 });
  assert.strictEqual(legacy.data.protocol.version, 2);
  assert.strictEqual(legacy.data.battery.percent, 87);
  assert.strictEqual(legacy.data.temperature.current_centi_c, 3925);

  const appended = context.decodeUplink({ fPort: 2, bytes: telemetryV2_1 });
  assert.strictEqual(appended.data.version, 'V2.1');
  assert.strictEqual(appended.data.protocol.payload_size_bytes, 42);
  assert.strictEqual(appended.data.bma.steps, 0x12345678);
  assert.strictEqual(appended.data.bma.accel_y_mg, -198);

  const supplied = context.decodeUplink({ fPort: 2, bytes: suppliedTelemetryV2_1 });
  assert.strictEqual(supplied.data.version, 'V2.1');
  assert.strictEqual(supplied.data.protocol.sequence, 16);
  assert.strictEqual(supplied.data.battery.voltage_mv, 3320);
  assert.strictEqual(supplied.data.temperature.current_c, 24.45);
  assert.strictEqual(supplied.data.bma.accel_z_mg, 956);

  const compact = context.decodeUplink({ fPort: 2, bytes: telemetryV2_2 });
  assert.strictEqual(compact.data.version, 'V2.2');
  assert.strictEqual(compact.data.protocol.payload_size_bytes, 38);
  assert.strictEqual(compact.data.battery.voltage_mv, 3320);
  assert.deepStrictEqual(
    JSON.parse(JSON.stringify(compact.data.temperature)),
    {
      current_c: 24.45,
      min_c: 24.44,
      max_c: 24.52,
      max_negative_excursion_c: -0.06
    }
  );
  assert.strictEqual(compact.data.bma.accel_x_mg, 207);
  assert.strictEqual(compact.data.bma.accel_y_mg, -198);
  assert.strictEqual(compact.data.bma.accel_z_mg, 956);
  assert.ok(!Object.prototype.hasOwnProperty.call(compact.data.battery, 'percent'));
  assert.ok(!Object.prototype.hasOwnProperty.call(compact.data.battery, 'voltage_v'));
  assert.ok(!Object.prototype.hasOwnProperty.call(compact.data, 'candidates'));
  assert.ok(!Object.prototype.hasOwnProperty.call(compact.data, 'event_reference_flags'));
  assert.ok(!Object.prototype.hasOwnProperty.call(compact.data.status, 'health_fault'));

  const wrongLength = context.decodeUplink({ fPort: 2, bytes: telemetryV2_2.slice(0, 37) });
  assert.match(wrongLength.errors[0], /exactly 38 bytes/);
}

const configuratorHtml = fs.readFileSync(
  path.join(repositoryRoot, 'Bolus_Downlink_Configurator/index.html'),
  'utf8'
);
const inlineScript = configuratorHtml.match(/<script>([\s\S]*?)<\/script>/);
assert.ok(inlineScript, 'Configurator inline JavaScript was not found.');

const elements = new Map();
const documentMock = {
  querySelectorAll: () => [],
  getElementById: (id) => {
    if (!elements.has(id)) {
      elements.set(id, {
        value: '',
        checked: false,
        disabled: false,
        className: '',
        textContent: '',
        addEventListener: () => {},
        select: () => {}
      });
    }
    return elements.get(id);
  },
  execCommand: () => true
};
const configuratorContext = vm.createContext({
  document: documentMock,
  localStorage: { getItem: () => null, setItem: () => {} },
  navigator: { clipboard: { writeText: async () => {} } },
  atob: (value) => Buffer.from(value, 'base64').toString('binary'),
  btoa: (value) => Buffer.from(value, 'binary').toString('base64')
});
vm.runInContext(inlineScript[1], configuratorContext, {
  filename: 'Bolus_Downlink_Configurator/index.html'
});

const configuratorLegacy = configuratorContext.decodeTelemetry(telemetryV2);
assert.strictEqual(configuratorLegacy.protocol.version, 2);
const configuratorAppended = configuratorContext.decodeTelemetry(telemetryV2_1);
assert.strictEqual(configuratorAppended.protocol.version_name, 'V2.1');
const configuratorSupplied = configuratorContext.decodeTelemetry(suppliedTelemetryV2_1);
assert.strictEqual(configuratorSupplied.temperature.current_c, 24.45);
const configuratorCompact = configuratorContext.decodeTelemetry(telemetryV2_2);
assert.strictEqual(configuratorCompact.protocol.version_name, 'V2.2');
assert.strictEqual(configuratorCompact.battery.voltage_mv, 3320);
assert.ok(!Object.prototype.hasOwnProperty.call(configuratorCompact.battery, 'percent'));
assert.ok(!Object.prototype.hasOwnProperty.call(configuratorCompact.temperature, 'current_centi_c'));

console.log('Telemetry V2/V2.1/V2.2 encoder/decoder vectors passed.');
