#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
build_dir=$(mktemp -d)
trap 'rm -rf "$build_dir"' EXIT
cc=${CC:-gcc}
flags=(-std=c11 -g -O1 -Wall -Wextra -Wno-misleading-indentation -fsanitize=undefined -fno-sanitize-recover=all -IApp/Services -IApp/Config -IApp/Application)
"$cc" "${flags[@]}" -Itests/host tests/test_functional.c App/Services/downlink_management_service.c App/Config/bolus_runtime_config.c App/Services/event_episode_service.c -o "$build_dir/functional"
"$build_dir/functional"
"$cc" "${flags[@]}" tests/test_telemetry_codec_v2_2.c App/Services/telemetry_codec.c -o "$build_dir/codec"
"$build_dir/codec"
echo 'PASS: telemetry C encoder'
node tests/test_uplink_decoders.js
node tests/test_downlink_configurator.js
