# Rebuild & run the native (host) unit tests for the pure logic modules.
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root
New-Item -ItemType Directory -Force -Path build | Out-Null

Write-Host "== ac_encoder =="
g++ -std=c++17 -I src tests/native/ac_encoder_test.cpp src/ac_encoder.cpp -o build/ac_encoder_test.exe
& build/ac_encoder_test.exe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "== ac_state =="
g++ -std=c++17 -I src tests/native/ac_state_test.cpp src/ac_state.cpp src/ac_encoder.cpp -o build/ac_state_test.exe
& build/ac_state_test.exe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "== wifi_policy =="
g++ -std=c++17 -I src tests/native/wifi_policy_test.cpp src/wifi_policy.cpp -o build/wifi_policy_test.exe
& build/wifi_policy_test.exe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "== link_guard =="
g++ -std=c++17 -I src tests/native/link_guard_test.cpp src/link_guard.cpp -o build/link_guard_test.exe
& build/link_guard_test.exe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "== reset_info =="
g++ -std=c++17 -I src tests/native/reset_info_test.cpp src/reset_info.cpp -o build/reset_info_test.exe
& build/reset_info_test.exe
exit $LASTEXITCODE
