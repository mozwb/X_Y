$ErrorActionPreference = "Stop"

$root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$exe = Join-Path $root "build\out\bin\X_Y.exe"
if (-not (Test-Path $exe))
{
    throw "X_Y not found: $exe (build first)"
}

$out = Join-Path $root "build\test-fixtures"
New-Item -ItemType Directory -Force $out | Out-Null

$include = Join-Path $root "Engine\include"
$fixtures = Join-Path $PSScriptRoot "fixtures"

& gcc -std=c11 -shared -o (Join-Path $out "echo_service.dll") `
    (Join-Path $fixtures "echo_service.c") "-I$include" "-I$fixtures"
if ($LASTEXITCODE -ne 0) { throw "build echo_service failed" }

& gcc -std=c11 -shared -o (Join-Path $out "echo_client.dll") `
    (Join-Path $fixtures "echo_client.c") "-I$include" "-I$fixtures"
if ($LASTEXITCODE -ne 0) { throw "build echo_client failed" }

$service = (Join-Path $out "echo_service.dll").Replace('\', '/')
$client = (Join-Path $out "echo_client.dll").Replace('\', '/')

$lines = @(
    "engine create a",
    "engine load a `"$service`"",
    "engine load a `"$client`"",
    "engine services a",
    "engine run a echo hello",
    "engine share a echo",
    "engine shared",
    "engine unload a echo_service",
    "engine unshare echo",
    "engine shared",
    "engine unload a echo_service",
    "engine destroy a",
    "exit"
)

$previousErrorAction = $ErrorActionPreference
$ErrorActionPreference = "Continue"
$output = ($lines -join "`n") + "`n" | & $exe 2>&1
$ErrorActionPreference = $previousErrorAction
$text = (($output | ForEach-Object { $_.ToString() }) -join "`n")

if ($LASTEXITCODE -ne 0)
{
    throw "X_Y exit $LASTEXITCODE`n$text"
}

function Assert-Contains([string]$needle)
{
    if (-not $text.Contains($needle))
    {
        throw "output missing '$needle'`n--- output ---`n$text"
    }
}

Assert-Contains "created engine a"
Assert-Contains "hello"
Assert-Contains "SERVICE"
Assert-Contains "shared service echo from engine a"
Assert-Contains "PROVIDER"
Assert-Contains "unshare"
Assert-Contains "unloaded echo_service"
Assert-Contains "destroyed engine a"

Write-Host "container smoke: OK"
