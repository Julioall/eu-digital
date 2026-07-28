[CmdletBinding()]
param(
    [string]$RepositoryRoot,
    [string]$SpecsPath
)

$ErrorActionPreference = 'Stop'
if (-not $RepositoryRoot) {
    $RepositoryRoot = Join-Path $PSScriptRoot '..'
}
$RepositoryRoot = (Resolve-Path -LiteralPath $RepositoryRoot).Path
if (-not $SpecsPath) {
    $SpecsPath = Join-Path $RepositoryRoot 'specs'
}

$Errors = [System.Collections.Generic.List[string]]::new()

function ConvertFrom-InlineList {
    param(
        [Parameter(Mandatory)]
        [string]$Value,

        [Parameter(Mandatory)]
        [string]$Context
    )

    $Trimmed = $Value.Trim()
    if ($Trimmed -notmatch '^\[(.*)\]$') {
        $script:Errors.Add("$Context deve usar lista inline: [ITEM-001, ITEM-002].")
        return @()
    }

    $Inner = $Matches[1].Trim()
    if (-not $Inner) {
        return @()
    }

    return @(
        $Inner.Split(',') |
            ForEach-Object { $_.Trim().Trim('"').Trim("'") } |
            Where-Object { $_ }
    )
}

function ConvertFrom-SpecFrontmatter {
    param(
        [Parameter(Mandatory)]
        [string]$Content,

        [Parameter(Mandatory)]
        [string]$FileName
    )

    $Match = [regex]::Match(
        $Content,
        '\A(?:\uFEFF)?---[ \t]*\r?\n(?<frontmatter>.*?)\r?\n---[ \t]*(?:\r?\n|$)',
        [System.Text.RegularExpressions.RegexOptions]::Singleline
    )
    if (-not $Match.Success) {
        $script:Errors.Add("${FileName}: frontmatter YAML ausente ou malformado.")
        return $null
    }

    $Metadata = [ordered]@{}
    $LineNumber = 1
    foreach ($Line in ($Match.Groups['frontmatter'].Value -split '\r?\n')) {
        $LineNumber++
        if (-not $Line.Trim() -or $Line.TrimStart().StartsWith('#')) {
            continue
        }
        if ($Line -notmatch '^(?<key>[a-z_]+):[ \t]*(?<value>.*)$') {
            $script:Errors.Add("${FileName}:${LineNumber}: entrada de frontmatter inválida.")
            continue
        }

        $Key = $Matches['key']
        $Value = $Matches['value'].Trim()
        if ($Metadata.Contains($Key)) {
            $script:Errors.Add("${FileName}: campo duplicado no frontmatter: $Key.")
            continue
        }

        if ($Key -in @('dependencies', 'adrs', 'contracts')) {
            $Metadata[$Key] = @(ConvertFrom-InlineList -Value $Value -Context "$FileName/$Key")
        }
        else {
            $Metadata[$Key] = $Value.Trim('"').Trim("'")
        }
    }

    return $Metadata
}

$SchemaPath = Join-Path $RepositoryRoot 'schemas\spec.schema.json'
if (-not (Test-Path -LiteralPath $SchemaPath -PathType Leaf)) {
    $Errors.Add('Schema obrigatório ausente: schemas/spec.schema.json.')
}
else {
    try {
        $Schema = Get-Content -Raw -Encoding UTF8 -LiteralPath $SchemaPath | ConvertFrom-Json
    }
    catch {
        $Errors.Add("schemas/spec.schema.json é inválido: $($_.Exception.Message)")
    }
}

if (-not (Test-Path -LiteralPath $SpecsPath -PathType Container)) {
    $Errors.Add("Diretório de SPECs ausente: $SpecsPath.")
}

$SpecFiles = @()
if (Test-Path -LiteralPath $SpecsPath -PathType Container) {
    $SpecFiles = @(Get-ChildItem -LiteralPath $SpecsPath -Filter 'SPEC-*.md' -File | Sort-Object Name)
}
if ($SpecFiles.Count -eq 0) {
    $Errors.Add('Nenhuma SPEC foi localizada.')
}

$SeenIds = @{}
foreach ($SpecFile in $SpecFiles) {
    $Content = Get-Content -Raw -Encoding UTF8 -LiteralPath $SpecFile.FullName
    $Metadata = ConvertFrom-SpecFrontmatter -Content $Content -FileName $SpecFile.Name
    if ($null -eq $Metadata) {
        continue
    }

    foreach ($RequiredField in @($Schema.required)) {
        if (-not $Metadata.Contains($RequiredField) -or $null -eq $Metadata[$RequiredField] -or $Metadata[$RequiredField] -eq '') {
            $Errors.Add("$($SpecFile.Name): campo obrigatório ausente: $RequiredField.")
        }
    }
    $AllowedFields = @($Schema.properties.PSObject.Properties.Name)
    foreach ($Field in @($Metadata.Keys)) {
        if ($Field -notin $AllowedFields) {
            $Errors.Add("$($SpecFile.Name): campo de frontmatter não permitido: $Field.")
        }
    }

    $Id = [string]$Metadata['id']
    $Title = [string]$Metadata['title']
    $Status = [string]$Metadata['status']
    $Phase = [string]$Metadata['phase']

    if ($Id -and $Id -notmatch $Schema.properties.id.pattern) {
        $Errors.Add("$($SpecFile.Name): id inválido: $Id.")
    }
    if ($Id -and $SpecFile.BaseName -notmatch "^$([regex]::Escape($Id))(?:-|$)") {
        $Errors.Add("$($SpecFile.Name): id $Id não corresponde ao nome do arquivo.")
    }
    if ($Id) {
        if ($SeenIds.ContainsKey($Id)) {
            $Errors.Add("$($SpecFile.Name): id duplicado: $Id.")
        }
        else {
            $SeenIds[$Id] = $SpecFile.Name
        }
    }
    if ($Title.Length -lt [int]$Schema.properties.title.minLength) {
        $Errors.Add("$($SpecFile.Name): título muito curto.")
    }
    if ($Status -and $Status -notin @($Schema.properties.status.enum)) {
        $Errors.Add("$($SpecFile.Name): status inválido: $Status.")
    }
    if (-not $Phase) {
        $Errors.Add("$($SpecFile.Name): phase não pode ser vazio.")
    }
    if ($Id -and $Content -notmatch "(?m)^# $([regex]::Escape($Id))(?:\s|$)") {
        $Errors.Add("$($SpecFile.Name): heading principal não corresponde a $Id.")
    }

    $RequiredSections = @(
        @{ Name = 'Objetivo'; Pattern = 'Objetivo' },
        @{ Name = 'Escopo negativo'; Pattern = 'Escopo negativo' },
        @{ Name = 'Criterios de aceite'; Pattern = 'Crit[^\r\n]*rios de aceite' }
    )
    foreach ($Section in $RequiredSections) {
        if ($Content -notmatch "(?m)^## $($Section.Pattern)[ \t]*(?:\r?$)") {
            $Errors.Add("$($SpecFile.Name): secao obrigatoria ausente: $($Section.Name).")
        }
    }
    if ($Content -match '(?m)^## Crit[^\r\n]*rios de aceite[ \t]*(?:\r?$)') {
        $CriteriaBlock = [regex]::Match(
            $Content,
            '(?ms)^## Crit[^\r\n]*rios de aceite[ \t]*(?:\r?$)\r?\n(?<body>.*?)(?=^## |\z)'
        )
        if (-not $CriteriaBlock.Success -or $CriteriaBlock.Groups['body'].Value -notmatch '(?m)^[ \t]*-[ \t]+\[[ xX]\][ \t]+\S') {
            $Errors.Add("$($SpecFile.Name): critérios de aceite devem conter pelo menos um checkbox.")
        }
    }

    foreach ($Dependency in @($Metadata['dependencies'])) {
        if ($Dependency -notmatch [string]$Schema.properties.dependencies.items.pattern) {
            $Errors.Add("$($SpecFile.Name): dependência inválida: $Dependency.")
            continue
        }
        $DependencyMatches = @(Get-ChildItem -LiteralPath $SpecsPath -Filter "$Dependency-*.md" -File)
        if ($DependencyMatches.Count -eq 0) {
            $Errors.Add("$($SpecFile.Name): dependência não localizada: $Dependency.")
        }
    }

    $AdrDirectory = Join-Path $RepositoryRoot 'docs\04-adrs'
    foreach ($Adr in @($Metadata['adrs'])) {
        if ($Adr -notmatch [string]$Schema.properties.adrs.items.pattern) {
            $Errors.Add("$($SpecFile.Name): ADR inválida: $Adr.")
            continue
        }
        $AdrMatches = @()
        if (Test-Path -LiteralPath $AdrDirectory -PathType Container) {
            $AdrMatches = @(Get-ChildItem -LiteralPath $AdrDirectory -Filter "$Adr-*.md" -File)
        }
        if ($AdrMatches.Count -ne 1) {
            $Errors.Add("$($SpecFile.Name): ADR deve ser localizável de forma única: $Adr.")
        }
    }

    $ContractDirectory = Join-Path $RepositoryRoot 'docs\03-contracts'
    foreach ($Contract in @($Metadata['contracts'])) {
        if ($Contract -notmatch [string]$Schema.properties.contracts.items.pattern) {
            $Errors.Add("$($SpecFile.Name): nome de contrato inválido: $Contract.")
            continue
        }
        $ContractPath = Join-Path $ContractDirectory $Contract
        if (-not (Test-Path -LiteralPath $ContractPath -PathType Leaf)) {
            $Errors.Add("$($SpecFile.Name): contrato não localizado: $Contract.")
        }
    }
}

if ($Errors.Count -gt 0) {
    $Errors | Sort-Object -Unique | ForEach-Object { Write-Error $_ }
    exit 1
}

Write-Output "SPECs válidas: $($SpecFiles.Count)"
