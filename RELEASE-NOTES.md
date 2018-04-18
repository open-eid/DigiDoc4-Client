DigiDoc4 version 0.6.0 release notes
--------------------------------------

- Various notification translations and links changed
- macOS UI changes: dialogs as sheets, restore window when icon in dock is clicked
- Support for eTokens with only auth or signing certs, Linux support for eTokens
- Ubuntu packaging fixes
- Small UI changes (SF logo, fonts in various windows homogenized)
- Additional checks added for PIN change (increasing/decreasing sequence, repeated digits etc)
- Signature envelope printing functionality ported from DigiDoc3
- Rules for detecting eSeal signature type updated (additional SK policy, qcStatements extension checked)
- DigiDoc4 packaged as Windows store app
- Various small fixes and documentation updates

[Full Changelog](https://github.com/open-eid/DigiDoc4-Client/compare/v0.5.0-BETA...v0.6.0-BETA)

DigiDoc4 version 0.5.0 release notes
--------------------------------------

- Always load proxy settings on startup in case if settings changed externally
- User's picture caching removed
- Multiple UI/UX improvements: myEid sections closed on new click,
  Estonian country code prefilled in Mobile ID dialog, repeated warnings etc.
- Malicious filenames sanitized when saving bdoc/cdoc content
- Crash on incorrect PIN2 fixed on Linux
- MacOS Services option "Encrypt with DigiDoc4" implemented
- MacOS App Sandbox fix: ask permissions when downloading bdoc/cdoc content

[Full Changelog](https://github.com/open-eid/DigiDoc4-Client/compare/v0.4.0-BETA...v0.5.0-BETA)

DigiDoc4 version 0.4.0 release notes
--------------------------------------

- eToken (e-Seal) support
- Navigation logic changes; signature container wrapped in crypto-container on crypto page and vice versa
- ASiC-E signing fix
- Network proxy support in DigiDoc4 client
- Multiple text and UI/UX improvements
- Card polling backend changes

[Full Changelog](https://github.com/open-eid/DigiDoc4-Client/compare/v0.3.0-BETA...v0.4.0-BETA)

DigiDoc4 version 0.3.0 release notes
--------------------------------------

- On macOS always ask permission to create signature- or cryptocontainer
- Show validation result summary on warning ribbon
- Always open encapsulated documents in new window
- Enable signing of signed PDF-s by wrapping in new container
- Additional UI and translation improvements 

[Full Changelog](https://github.com/open-eid/DigiDoc4-Client/compare/v0.2.0-BETA...v0.3.0-BETA)

DigiDoc4 version 0.2.0 release notes
--------------------------------------

DigiDoc4 Client is an application for digitally signing and encrypting documents; the software includes functionality to manage (change pin codes, update certificates etc.) the Estonian ID-card. 

DigiDoc4 Client is a reinterpretation of two existing applications:
- DigiDoc3 Client which is used for digital signing and encryption;
- ID Card utility which is used for managing Estonian eIDs.
The functionality of the application mirrors existing applications; major changes provided by the new client are refreshed UI and the improvements in the workflow/UX of the application.
