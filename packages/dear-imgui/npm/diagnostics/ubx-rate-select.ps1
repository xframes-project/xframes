param(
    [Parameter(Mandatory=$true)][int]$ProcessId,
    [Parameter(Mandatory=$true)][ValidateSet(10,20,60)][int]$Rate
)
$ErrorActionPreference = 'Stop'
# One compilation of the existing PID-scoped input helper for this specific
# connected-App Combo gesture. The harness verifies the ordinary callback.
. "$PSScriptRoot/native-window.ps1" -ProcessId $ProcessId -Action move -X 220 -Y 200
Start-Sleep -Milliseconds 200
[XFramesFixtureWindow]::Run($ProcessId, 'click', 220, 200, '')
Start-Sleep -Milliseconds 250
$rateIndex = @(10,20,60).IndexOf($Rate)
$optionY = 224 + $rateIndex * 20
[XFramesFixtureWindow]::Run($ProcessId, 'move', 180, $optionY, '')
Start-Sleep -Milliseconds 200
[XFramesFixtureWindow]::Run($ProcessId, 'click', 180, $optionY, '')
