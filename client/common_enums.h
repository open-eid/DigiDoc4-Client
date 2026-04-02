// SPDX-FileCopyrightText: Estonian Information System Authority
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

namespace ria::qdigidoc4 {

enum ContainerState : unsigned char {
    Uninitialized           = (1 << 0),

    UnsignedContainer       = (1 << 1),
    UnsignedSavedContainer  = (1 << 2),
    SignedContainer         = (1 << 3),

    UnencryptedContainer    = (1 << 4),
    EncryptedContainer      = (1 << 5),

    SignatureContainers     = UnsignedContainer | UnsignedSavedContainer | SignedContainer,
    CryptoContainers        = UnencryptedContainer | EncryptedContainer,
};

enum Actions : unsigned char {
    ContainerClose,
    ContainerCancel,
    ContainerConvert,
    ContainerEncrypt,

    EncryptContainer,
    DecryptContainer,
    DecryptToken,

    SignatureAdd,
    SignatureMobile,
    SignatureSmartID,
    SignatureToken,
    ClearSignatureWarning,
    ClearCryptoWarning,
};

}
