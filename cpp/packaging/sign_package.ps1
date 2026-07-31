param (
    [Parameter(Mandatory=$true)]
    [string]$PackagePath,

    [Parameter(Mandatory=$false)]
    [string]$Thumbprint
)

# This script signs the MSIX package. 
# For development, if no thumbprint is provided, it can use a self-signed certificate.

if (-not (Test-Path $PackagePath)) {
    Write-Error "Package not found: $PackagePath"
    exit 1
}

$SignTool = "C:\Program Files (x86)\Windows Kits\10\bin\10.0.22621.0\x64\signtool.exe"
if (-not (Test-Path $SignTool)) {
    # Try finding it in the registry or via vswhere if not in default path
    $SignTool = Get-Command signtool.exe -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Source
    if (-not $SignTool) {
        Write-Error "signtool.exe not found. Please install the Windows SDK."
        exit 1
    }
}

if ([string]::IsNullOrEmpty($Thumbprint)) {
    Write-Host "No thumbprint provided. In a real CI environment, an EV/OV certificate must be used."
    Write-Host "For local testing, please provide the thumbprint of a trusted self-signed cert."
    exit 0
}

Write-Host "Signing $PackagePath with certificate thumbprint $Thumbprint..."

& $SignTool sign /fd SHA256 /a /sha1 $Thumbprint /t "http://timestamp.digicert.com" $PackagePath

if ($LASTEXITCODE -ne 0) {
    Write-Error "Failed to sign package."
    exit $LASTEXITCODE
}

Write-Host "Package signed successfully."
exit 0
