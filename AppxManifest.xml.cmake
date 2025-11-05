<?xml version="1.0" encoding="utf-8"?>
<Package xmlns="http://schemas.microsoft.com/appx/manifest/foundation/windows10"
    xmlns:desktop="http://schemas.microsoft.com/appx/manifest/desktop/windows10"
    xmlns:rescap="http://schemas.microsoft.com/appx/manifest/foundation/windows10/restrictedcapabilities"
    xmlns:uap="http://schemas.microsoft.com/appx/manifest/uap/windows10"
    xmlns:uap3="http://schemas.microsoft.com/appx/manifest/uap/windows10/3"
    IgnorableNamespaces="desktop uap uap3 rescap">
  <Identity Name="RiigiInfossteemiAmet.DigiDoc4client" ProcessorArchitecture="${PLATFORM}" Version="${PROJECT_VERSION}.0"
    Publisher="CN=8BBBE4D8-620A-4884-A12A-72F1A2030D8B" />
  <Properties>
    <DisplayName>DigiDoc4 Client</DisplayName>
    <PublisherDisplayName>Riigi Infosüsteemi Amet</PublisherDisplayName>
    <Logo>Assets\DigiDoc.50x50.png</Logo>
  </Properties>
  <Resources>
    <Resource Language="en-us" />
  </Resources>
  <Dependencies>
    <TargetDeviceFamily Name="Windows.Desktop" MinVersion="10.0.17763.0" MaxVersionTested="10.0.19041.0" />
    <PackageDependency Name="Microsoft.VCLibs.140.00.UWPDesktop" MinVersion="14.0.30035.0"
      Publisher="CN=Microsoft Corporation, O=Microsoft Corporation, L=Redmond, S=Washington, C=US" />
  </Dependencies>
  <Capabilities>
    <rescap:Capability Name="runFullTrust" />
  </Capabilities>
  <Applications>
    <Application Id="DigiDoc4" Executable="qdigidoc4.exe" EntryPoint="Windows.FullTrustApplication">
      <uap:VisualElements DisplayName="DigiDoc4" Description="DigiDoc4" BackgroundColor="#00355f"
        Square150x150Logo="Assets\DigiDoc.150x150.png" Square44x44Logo="Assets\DigiDoc.44x44.png" />
      <Extensions>
        <uap3:Extension Category="windows.fileTypeAssociation">
          <uap3:FileTypeAssociation Name="asice" Parameters="&quot;%1&quot;">
            <uap:DisplayName>DigiDoc signed document</uap:DisplayName>
            <uap:Logo>Assets\qdigidoc_client_document_256x256.png</uap:Logo>
            <uap:SupportedFileTypes>
              <uap:FileType>.adoc</uap:FileType>
              <uap:FileType>.asice</uap:FileType>
              <uap:FileType>.asics</uap:FileType>
              <uap:FileType>.bdoc</uap:FileType>
              <uap:FileType>.ddoc</uap:FileType>
              <uap:FileType>.edoc</uap:FileType>
              <uap:FileType>.sce</uap:FileType>
              <uap:FileType>.scs</uap:FileType>
            </uap:SupportedFileTypes>
          </uap3:FileTypeAssociation>
        </uap3:Extension>
        <uap3:Extension Category="windows.fileTypeAssociation">
          <uap3:FileTypeAssociation Name="cdoc" Parameters="-crypto &quot;%1&quot;">
            <uap:DisplayName>DigiDoc encrypted container</uap:DisplayName>
            <uap:Logo>Assets\qdigidoc_crypto_document_256x256.png</uap:Logo>
            <uap:SupportedFileTypes>
              <uap:FileType>.cdoc</uap:FileType>
              <uap:FileType>.cdoc2</uap:FileType>
            </uap:SupportedFileTypes>
          </uap3:FileTypeAssociation>
        </uap3:Extension>
      </Extensions>
    </Application>
    <Application Id="DigidocTool" Executable="digidoc-tool.exe" EntryPoint="Windows.FullTrustApplication">
      <uap:VisualElements DisplayName="digidoc-tool" Description="digidoc-tool" BackgroundColor="transparent"
        Square150x150Logo="Assets\DigiDoc.150x150.png" Square44x44Logo="Assets\DigiDoc.44x44.png" AppListEntry="none" />
      <Extensions>
        <uap3:Extension Category="windows.appExecutionAlias">
          <uap3:AppExecutionAlias>
            <desktop:ExecutionAlias Alias="digidoc-tool.exe" />
          </uap3:AppExecutionAlias>
        </uap3:Extension>
      </Extensions>
    </Application>
  </Applications>
</Package>
