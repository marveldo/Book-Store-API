# Builds models\model.json from models\model.template.json, then runs drogon_ctl.
#   Values come from: real environment variable  >  .env file.
#   Nothing is written to $env: or the system, so there is nothing to clean up.
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot          # project root (this script lives in scripts\)
Push-Location $root
try {
    $envFile  = '.env'
    $template = 'models\model.template.json'
    $output   = 'models\model.json'
    if (-not (Test-Path $template)) { throw "$template not found" }

    # 1. read .env into a local hashtable (not into the environment)
    $fileVars = @{}
    if (Test-Path $envFile) {
        foreach ($line in Get-Content $envFile) {
            $line = $line.Trim()
            if ($line -eq '' -or $line.StartsWith('#')) { continue }
            if ($line.StartsWith('export ')) { $line = $line.Substring(7).Trim() }
            $eq = $line.IndexOf('=')
            if ($eq -lt 1) { continue }
            $key   = $line.Substring(0, $eq).Trim()
            $value = $line.Substring($eq + 1).Trim()
            if ($value -match '^"(.*)"$' -or $value -match "^'(.*)'$") { $value = $Matches[1] }
            elseif ($value -match '^(.*?)\s+#') { $value = $Matches[1].Trim() }
            $fileVars[$key] = $value
        }
    } else {
        Write-Warning "no $envFile, using environment variables only"
    }

    # 2. fill every ${NAME} placeholder found in the template
    $json = Get-Content $template -Raw
    $names = [regex]::Matches($json, '\$\{([A-Za-z_][A-Za-z0-9_]*)\}') | ForEach-Object { $_.Groups[1].Value } | Sort-Object -Unique
    # placeholders that must become JSON numbers, not strings ("5432" -> 5432)
    $numeric = @('DB_PORT')
    $missing = @()
    foreach ($name in $names) {
        $value = [Environment]::GetEnvironmentVariable($name)          # real environment variable wins
        if ($null -eq $value) { $value = $fileVars[$name] }
        if ($null -eq $value) { $missing += $name; continue }
        $value = $value.Trim()
        if ($numeric -contains $name) {
            if ($value -notmatch '^\d+$') { throw "$name must be a whole number, got '$value'" }
            $json = $json.Replace('"${' + $name + '}"', $value)        # drop the quotes around it
            $json = $json.Replace('${' + $name + '}', $value)
            continue
        }
        $escaped = $value.Replace('\', '\\').Replace('"', '\"')        # JSON-safe
        $json = $json.Replace('${' + $name + '}', $escaped)
    }
    if ($missing.Count) { throw "no value for: $($missing -join ', ') (set them in $envFile or the environment)" }

    [IO.File]::WriteAllText((Join-Path $root $output), $json, (New-Object Text.UTF8Encoding $false))   # UTF-8 without BOM
    Write-Host "wrote $output"

    # 3. generate the model classes
    drogon_ctl create model models
}
finally {
    Pop-Location                                   # always return to the folder you started in
}
