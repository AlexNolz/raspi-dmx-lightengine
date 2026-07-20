$ErrorActionPreference = "Stop"

$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
$Key = Join-Path $env:USERPROFILE ".ssh\raspi_dmx_light_engine"
$Remote = "root@raspi-dmx"
$RemoteDir = "/tmp/light-engine-deploy"

if (!(Test-Path $Key)) {
  throw "Deploy key not found: $Key"
}

ssh -i $Key -o BatchMode=yes -o StrictHostKeyChecking=accept-new $Remote "mkdir -p $RemoteDir/web"
scp -i $Key -o BatchMode=yes `
  (Join-Path $Root "light_engine.py") `
  (Join-Path $Root "light_engine_config.json") `
  (Join-Path $Root "README_light_engine.md") `
  "${Remote}:${RemoteDir}/"
scp -i $Key -o BatchMode=yes `
  (Join-Path $Root "web/index.html") `
  (Join-Path $Root "web/app.css") `
  (Join-Path $Root "web/app.js") `
  "${Remote}:${RemoteDir}/web/"
scp -i $Key -o BatchMode=yes `
  (Join-Path $Root "raspi/install_raspi.sh") `
  (Join-Path $Root "raspi/artnet-dmx-current.initd") `
  "${Remote}:${RemoteDir}/"
ssh -i $Key -o BatchMode=yes $Remote "sh $RemoteDir/install_raspi.sh"
ssh -i $Key -o BatchMode=yes $Remote "light-engine-status"
