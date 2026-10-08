# Sube el repo a GitHub (solo la primera vez o para publicar cambios).
param([string]$Url = "https://github.com/dominmd-ucam/Clawdmeter.git",
      [string]$Message = "Clawdmeter: servicios, tiempo, iconos y scripts de instalacion")
$ErrorActionPreference = "Continue"
. "$PSScriptRoot\common.ps1"
Set-Location $script:RepoRoot

if (Test-Path "daemon_copy.log") { Remove-Item "daemon_copy.log" -Force }
if (-not (git config user.name))  { git config user.name "Domingo" }
if (-not (git config user.email)) { git config user.email "dominmd99@gmail.com" }

$remotes = git remote
if ($remotes -contains "origin") {
    $cur = git remote get-url origin
    if ($cur -ne $Url) {
        if ($remotes -notcontains "upstream") { git remote rename origin upstream } else { git remote remove origin }
        git remote add origin $Url
    }
} else {
    git remote add origin $Url
}

git add -A
git commit -m $Message
git push -u origin main
if ($LASTEXITCODE -ne 0) {
    Write-Bad "El push fallo. Si el repo remoto tiene un README inicial, ejecuta: git push -u origin main --force"
} else {
    Write-Ok "Subido a $Url"
}
