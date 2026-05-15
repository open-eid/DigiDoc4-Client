<?xml version="1.0" encoding="utf-8"?>
<Package xmlns="http://schemas.microsoft.com/appx/manifest/foundation/windows10"
    xmlns:com="http://schemas.microsoft.com/appx/manifest/com/windows10"
    xmlns:desktop4="http://schemas.microsoft.com/appx/manifest/desktop/windows10/4"
    xmlns:desktop5="http://schemas.microsoft.com/appx/manifest/desktop/windows10/5"
    xmlns:rescap="http://schemas.microsoft.com/appx/manifest/foundation/windows10/restrictedcapabilities"
    xmlns:uap="http://schemas.microsoft.com/appx/manifest/uap/windows10"
    xmlns:uap10="http://schemas.microsoft.com/appx/manifest/uap/windows10/10"
    IgnorableNamespaces="com desktop4 desktop5 rescap uap uap10">
  <Identity Name="RiigiInfossteemiAmet.DigiDoc4ShellExt" ProcessorArchitecture="${PLATFORM}" Version="${PROJECT_VERSION}.0"
    Publisher="${SIGNCERT_SUBJECT}" />
  <Properties>
    <DisplayName>DigiDoc4 Shell Extension</DisplayName>
    <PublisherDisplayName>Riigi Infosüsteemi Amet</PublisherDisplayName>
    <Logo>Assets\DigiDoc.50x50.png</Logo>
    <uap10:AllowExternalContent>true</uap10:AllowExternalContent>
  </Properties>
  <Resources>
    <Resource Language="en-us" />
  </Resources>
  <Dependencies>
    <TargetDeviceFamily Name="Windows.Desktop" MinVersion="10.0.19041.0" MaxVersionTested="10.0.19041.0" />
  </Dependencies>
  <Capabilities>
    <rescap:Capability Name="runFullTrust" />
  </Capabilities>
  <Applications>
    <Application Id="DigiDoc4" Executable="qdigidoc4.exe" EntryPoint="Windows.FullTrustApplication">
      <uap:VisualElements DisplayName="DigiDoc4 Shell Extension" Description="DigiDoc4 Shell Extension"
        BackgroundColor="transparent" Square150x150Logo="Assets\DigiDoc.150x150.png"
        Square44x44Logo="Assets\DigiDoc.44x44.png" AppListEntry="none" />
      <Extensions>
        <desktop4:Extension Category="windows.fileExplorerContextMenus">
          <desktop4:FileExplorerContextMenus>
            <desktop5:ItemType Type="*">
              <desktop5:Verb Id="DigiDocSign" Clsid="4ef3a5aa-125c-45f5-b5fd-d4c478050afa"/>
              <desktop5:Verb Id="DigiDocEnc" Clsid="bb67aa19-089b-4ec9-a059-13d985987cdc"/>
            </desktop5:ItemType>
          </desktop4:FileExplorerContextMenus>
        </desktop4:Extension>
        <com:Extension Category="windows.comServer">
          <com:ComServer>
            <com:SurrogateServer DisplayName="DigiDoc4 Shell Extension">
              <com:Class Id="4ef3a5aa-125c-45f5-b5fd-d4c478050afa" Path="$<TARGET_FILE_NAME:EsteidShellExtensionV2>" ThreadingModel="STA"/>
              <com:Class Id="bb67aa19-089b-4ec9-a059-13d985987cdc" Path="$<TARGET_FILE_NAME:EsteidShellExtensionV2>" ThreadingModel="STA"/>
            </com:SurrogateServer>
          </com:ComServer>
        </com:Extension>
      </Extensions>
    </Application>
  </Applications>
</Package>
