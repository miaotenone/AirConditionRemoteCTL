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
exit $LASTEXITCODE
