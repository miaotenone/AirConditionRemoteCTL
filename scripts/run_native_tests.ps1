# Compatibility entrypoint. Python owns the suite list and fail-fast behavior.
$ErrorActionPreference = "Stop"
& python (Join-Path $PSScriptRoot "run_native_tests.py") @args
exit $LASTEXITCODE
