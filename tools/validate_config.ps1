[CmdletBinding()]
param(
    [string]$RepositoryRoot
)

$ErrorActionPreference = 'Stop'
if (-not $RepositoryRoot) {
    $RepositoryRoot = Join-Path $PSScriptRoot '..'
}
$RepositoryRoot = (Resolve-Path -LiteralPath $RepositoryRoot).Path
$Errors = [System.Collections.Generic.List[string]]::new()

function ConvertFrom-SimpleYamlScalar {
    param([string]$Value)

    $Trimmed = $Value.Trim()
    if ($Trimmed -match '^"(.*)"$' -or $Trimmed -match "^'(.*)'$") {
        return $Matches[1]
    }
    if ($Trimmed -eq 'true') {
        return $true
    }
    if ($Trimmed -eq 'false') {
        return $false
    }
    if ($Trimmed -match '^-?[0-9]+$') {
        return [int64]$Trimmed
    }
    if ($Trimmed -eq '[]') {
        return ,@()
    }
    return $Trimmed
}

function ConvertFrom-SimpleYaml {
    param(
        [Parameter(Mandatory)]
        [string]$Content,

        [Parameter(Mandatory)]
        [string]$FileName
    )

    $Root = [ordered]@{}
    $Containers = @($Root)
    $LineNumber = 0
    foreach ($Line in ($Content -split '\r?\n')) {
        $LineNumber++
        if (-not $Line.Trim() -or $Line.TrimStart().StartsWith('#')) {
            continue
        }
        if ($Line.Contains("`t")) {
            $script:Errors.Add("${FileName}:${LineNumber}: tabs não são permitidos.")
            continue
        }
        if ($Line -notmatch '^(?<indent> *)(?<key>[a-z_][a-z0-9_]*):(?:[ ](?<value>.*))?$') {
            $script:Errors.Add("${FileName}:${LineNumber}: sintaxe YAML não suportada.")
            continue
        }

        $Indent = $Matches['indent'].Length
        if ($Indent % 2 -ne 0) {
            $script:Errors.Add("${FileName}:${LineNumber}: indentação deve usar múltiplos de dois espaços.")
            continue
        }
        $Level = [int]($Indent / 2)
        if ($Level -ge $Containers.Count -or $null -eq $Containers[$Level]) {
            $script:Errors.Add("${FileName}:${LineNumber}: nível de indentação inválido.")
            continue
        }

        $Key = $Matches['key']
        $Value = $Matches['value']
        $Parent = $Containers[$Level]
        if ($Parent.Contains($Key)) {
            $script:Errors.Add("${FileName}:${LineNumber}: chave duplicada: $Key.")
            continue
        }

        if ($null -eq $Value) {
            $Child = [ordered]@{}
            $Parent[$Key] = $Child
            if ($Containers.Count -le ($Level + 1)) {
                $Containers += $Child
            }
            else {
                $Containers[$Level + 1] = $Child
                for ($Index = $Level + 2; $Index -lt $Containers.Count; $Index++) {
                    $Containers[$Index] = $null
                }
            }
        }
        else {
            $Parent[$Key] = ConvertFrom-SimpleYamlScalar -Value $Value
        }
    }

    return $Root
}

function Test-JsonSchemaSubset {
    param(
        $Value,
        [Parameter(Mandatory)]
        $Schema,
        [Parameter(Mandatory)]
        [string]$Path
    )

    $Type = [string]$Schema.type
    switch ($Type) {
        'object' {
            if ($Value -isnot [System.Collections.IDictionary]) {
                $script:Errors.Add("$Path deve ser objeto.")
                return
            }

            foreach ($Required in @($Schema.required)) {
                if (-not $Value.Contains([string]$Required)) {
                    $script:Errors.Add("$Path.$Required é obrigatório.")
                }
            }

            $PropertyNames = @($Schema.properties.PSObject.Properties.Name)
            if ($Schema.additionalProperties -eq $false) {
                foreach ($Key in @($Value.Keys)) {
                    if ($Key -notin $PropertyNames) {
                        $script:Errors.Add("$Path.$Key não é permitido pelo schema.")
                    }
                }
            }

            foreach ($Property in $Schema.properties.PSObject.Properties) {
                if ($Value.Contains($Property.Name)) {
                    Test-JsonSchemaSubset -Value $Value[$Property.Name] -Schema $Property.Value -Path "$Path.$($Property.Name)"
                }
            }
        }
        'string' {
            if ($Value -isnot [string]) {
                $script:Errors.Add("$Path deve ser string.")
                return
            }
            if ($Schema.pattern -and $Value -notmatch [string]$Schema.pattern) {
                $script:Errors.Add("$Path não corresponde ao padrão exigido.")
            }
            if ($Schema.PSObject.Properties['enum'] -and $Value -notin @($Schema.enum)) {
                $script:Errors.Add("$Path possui valor não permitido: $Value.")
            }
            if ($Schema.PSObject.Properties['const'] -and $Value -ne [string]$Schema.const) {
                $script:Errors.Add("$Path deve ser '$($Schema.const)'.")
            }
        }
        'integer' {
            if ($Value -isnot [int] -and $Value -isnot [long]) {
                $script:Errors.Add("$Path deve ser inteiro.")
                return
            }
            if ($null -ne $Schema.minimum -and $Value -lt [long]$Schema.minimum) {
                $script:Errors.Add("$Path deve ser maior ou igual a $($Schema.minimum).")
            }
        }
        'boolean' {
            if ($Value -isnot [bool]) {
                $script:Errors.Add("$Path deve ser booleano.")
            }
            elseif ($Schema.PSObject.Properties['const'] -and $Value -ne [bool]$Schema.const) {
                $script:Errors.Add("$Path deve ser $($Schema.const).")
            }
        }
        'array' {
            if ($Value -is [string] -or $Value -isnot [System.Collections.IEnumerable]) {
                $script:Errors.Add("$Path deve ser array.")
            }
        }
        default {
            $script:Errors.Add("$Path usa tipo de schema não suportado: $Type.")
        }
    }
}

$ConfigDirectory = Join-Path $RepositoryRoot 'config'
$SchemaDirectory = Join-Path $RepositoryRoot 'schemas'
$ConfigFiles = @()
if (Test-Path -LiteralPath $ConfigDirectory -PathType Container) {
    $ConfigFiles = @(Get-ChildItem -LiteralPath $ConfigDirectory -Filter '*.yaml' -File | Sort-Object Name)
}
if ($ConfigFiles.Count -eq 0) {
    $Errors.Add('Nenhuma configuração normativa foi localizada em config/.')
}

foreach ($ConfigFile in $ConfigFiles) {
    $SchemaPath = Join-Path $SchemaDirectory "$($ConfigFile.BaseName).schema.json"
    if (-not (Test-Path -LiteralPath $SchemaPath -PathType Leaf)) {
        $Errors.Add("$($ConfigFile.Name): schema ausente em schemas/$($ConfigFile.BaseName).schema.json.")
        continue
    }

    try {
        $Schema = Get-Content -Raw -Encoding UTF8 -LiteralPath $SchemaPath | ConvertFrom-Json
    }
    catch {
        $Errors.Add("$($ConfigFile.Name): schema JSON inválido: $($_.Exception.Message)")
        continue
    }

    $Config = ConvertFrom-SimpleYaml -Content (Get-Content -Raw -Encoding UTF8 -LiteralPath $ConfigFile.FullName) -FileName $ConfigFile.Name
    Test-JsonSchemaSubset -Value $Config -Schema $Schema -Path $ConfigFile.BaseName
}

if ($Errors.Count -gt 0) {
    $Errors | Sort-Object -Unique | ForEach-Object { Write-Error $_ }
    exit 1
}

Write-Output "Configurações válidas: $($ConfigFiles.Count)"
