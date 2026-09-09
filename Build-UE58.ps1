[CmdletBinding()]
param(
    [ValidateSet('Generate', 'Editor', 'Package')]
    [string]$Action = 'Editor',
    [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8'
)
$ErrorActionPreference = 'Stop'
$projectRoot = $PSScriptRoot
$projectFile = Join-Path $projectRoot 'WaveMaker.uproject'
$logDir = Join-Path $projectRoot 'Saved\UpgradeLogs'
$configDir = Join-Path $projectRoot 'Saved\UnrealBuildTool'
New-Item -ItemType Directory -Path $logDir, $configDir -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $projectRoot 'Config\UnrealBuildTool\BuildConfiguration.xml') -Destination (Join-Path $configDir 'BuildConfiguration.xml')
$dotnet = Join-Path $EngineRoot 'Engine\Binaries\ThirdParty\DotNet\10.0\win-x64\dotnet.exe'
$ubt = Join-Path $EngineRoot 'Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.dll'
if (!(Test-Path -LiteralPath $dotnet)) { throw "UE 5.8 .NET runtime not found: $dotnet" }
Push-Location $projectRoot
try {
    switch ($Action) {
        'Generate' {
            & $dotnet $ubt -projectfiles "-project=$projectFile" -game -rocket -progress "-log=$logDir\GenerateProjectFiles.log"
        }
        'Editor' {
            & $dotnet $ubt WaveMakerEditor Win64 Development "-project=$projectFile" -WaitMutex -NoHotReloadFromIDE "-log=$logDir\EditorBuild.log"
        }
        'Package' {
            $uat = Join-Path $EngineRoot 'Engine\Build\BatchFiles\RunUAT.bat'
            & $uat BuildCookRun "-project=$projectFile" -noP4 -installed -unattended -utf8output -platform=Win64 -clientconfig=Shipping -build -cook -stage -pak -prereqs -archive "-archivedirectory=$projectRoot\Builds\UE5.8.2"
        }
    }
    if ($LASTEXITCODE -ne 0) { throw "Unreal $Action failed with exit code $LASTEXITCODE. See $logDir and Saved\Logs." }
}
finally { Pop-Location }
