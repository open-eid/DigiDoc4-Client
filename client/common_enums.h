/*
 * QDigiDoc4
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#pragma once

namespace ria {
namespace qdigidoc4 {

enum ContainerState {
    Uninitialized           = (1 << 0),

    UnsignedContainer       = (1 << 1),
    UnsignedSavedContainer  = (1 << 2),
    SignedContainer         = (1 << 3),

    UnencryptedContainer    = (1 << 4),
    EncryptedContainer      = (1 << 5),

    SignatureContainers     = UnsignedContainer | UnsignedSavedContainer | SignedContainer,
    CryptoContainers        = UnencryptedContainer | EncryptedContainer,
};

enum Actions {
    AddressAdd,

    CardPhoto,

    ContainerCancel,
    ContainerConvert,
    ContainerEncrypt,
    ContainerEmail,
    ContainerSummary,
    ContainerLocation,
    ContainerNavigate,
    ContainerSave,
    ContainerSaveAs,

    EncryptContainer,
    DecryptContainer,
    DecryptToken,

    FileAdd,
    FileRemove,

    HeadSettings,
    HeadHelp,

    SignatureAdd,
    SignatureMobile,
    SignatureSmartID,
    SignatureToken,
    SignatureRemove,
    SignatureWarning,
    ClearSignatureWarning
};

enum ExtensionType {
    ExtSignature,
    ExtCrypto,
    ExtPDF,
    ExtOther
};

enum FileType {
    SignatureDocument,
    CryptoDocument,
    Other
};

enum ItemType {
    ItemFile,
    ItemSignature,
    ItemAddress,
    ToAddAdresses,
    AddedAdresses,
};

enum Pages {
    SignIntro,
    SignDetails,
    CryptoIntro,
    CryptoDetails,
    MyEid
};

enum WarningType {
    NoWarning = 0,

    CertExpiredWarning,
    CertExpiryWarning,
    CertRevokedWarning,
    UnblockPin1Warning,
    UnblockPin2Warning,

    CheckConnectionWarning,
    InvalidSignatureWarning,
    InvalidTimestampWarning,
    UnknownSignatureWarning,
    UnknownTimestampWarning,
};

}
}
