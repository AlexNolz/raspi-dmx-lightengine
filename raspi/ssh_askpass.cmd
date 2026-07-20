@echo off
powershell -NoProfile -ExecutionPolicy Bypass -Command "Write-Output $env:LIGHT_ENGINE_SSH_PASSWORD"
