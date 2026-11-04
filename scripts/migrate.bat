@echo off
:: Run PowerShell silently to securely parse the .env file and run migrations
powershell -NoProfile -ExecutionPolicy Bypass -Command ^ "$env:DB_URL = (ConvertFrom-StringData (Get-Content .env -Raw)).DATABASE_URL; if (-not $env:DB_URL) { Write-Error 'DB_URL not found in .env'; exit 1 }; migrate -path migrations -database $env:DB_URL up"
