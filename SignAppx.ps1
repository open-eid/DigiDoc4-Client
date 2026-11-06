# SPDX-FileCopyrightText: Estonian Information System Authority
# SPDX-License-Identifier: LGPL-2.1-or-later

param(
    [Parameter(Mandatory = $true)]
    [string] $PackagePath,

    [Parameter(Mandatory = $true)]
    [string] $ManifestPath,

    [Parameter(Mandatory = $true)]
    [string] $CertificatePath
)

$ErrorActionPreference = 'Stop'
$certificate = $null

try {
    [xml] $manifest = Get-Content -LiteralPath $ManifestPath -Raw
    $publisher = $manifest.Package.Identity.Publisher
    if ([string]::IsNullOrWhiteSpace($publisher)) {
        throw "The Appx manifest does not define a publisher."
    }

    $certificate = New-SelfSignedCertificate `
        -Type Custom `
        -Subject $publisher `
        -FriendlyName 'DigiDoc4 ephemeral Appx test certificate' `
        -CertStoreLocation 'Cert:\CurrentUser\My' `
        -Provider 'Microsoft Software Key Storage Provider' `
        -KeyAlgorithm RSA `
        -KeyLength 2048 `
        -HashAlgorithm SHA256 `
        -KeyExportPolicy NonExportable `
        -KeyUsage DigitalSignature `
        -TextExtension @('2.5.29.37={text}1.3.6.1.5.5.7.3.3,1.3.6.1.4.1.311.10.3.13', '2.5.29.19={text}') `
        -NotAfter (Get-Date).AddDays(30)

    Export-Certificate -Cert $certificate -FilePath $CertificatePath -Type CERT -Force | Out-Null

    & signtool.exe sign /v /s My /sha1 $certificate.Thumbprint /fd SHA256 $PackagePath
    if ($LASTEXITCODE -ne 0) {
        throw "SignTool failed with exit code $LASTEXITCODE."
    }
}
finally {
    if ($null -ne $certificate) {
        Remove-Item -LiteralPath "Cert:\CurrentUser\My\$($certificate.Thumbprint)" -DeleteKey
    }
}
