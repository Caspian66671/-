$ErrorActionPreference = "Stop"
$Root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$ConfigPath = Join-Path $Root "deepseek_config.ps1"

$key = Read-Host "Paste DeepSeek API Key"
if ([string]::IsNullOrWhiteSpace($key)) {
    Write-Host "No key entered."
    exit 1
}

$model = Read-Host "Model [deepseek-v4-flash]"
if ([string]::IsNullOrWhiteSpace($model)) {
    $model = "deepseek-v4-flash"
}

Set-Content -Encoding UTF8 -Path $ConfigPath -Value @(
    "# Local DeepSeek config. This file is ignored by Git.",
    "`$env:DEEPSEEK_API_KEY = `"$key`"",
    "`$env:DEEPSEEK_MODEL = `"$model`""
)

Write-Host "Saved local DeepSeek config:"
Write-Host $ConfigPath
Write-Host "Now run start_demo.bat again."
