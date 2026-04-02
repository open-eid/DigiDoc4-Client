// SPDX-FileCopyrightText: Estonian Information System Authority
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "CryptoDoc.h"

#include <QtCore/QCryptographicHash>
#include <QtCore/QFile>

class QXmlStreamReader;
class QXmlStreamWriter;

using EVP_CIPHER = struct evp_cipher_st;

class CDoc1 final: public CDoc, private QFile
{
public:
	CDoc1() = default;
	CDoc1(const QString &path);
	CKey canDecrypt(const QSslCertificate &cert) const final;
	bool decryptPayload(const QByteArray &key) final;
	bool save(const QString &path) final;
	QByteArray transportKey(const CKey &key) final;
	int version() final;

private:
	void writeDDoc(QIODevice *ddoc);

	static QByteArray fromBase64(QStringView data);
	static std::vector<File> readDDoc(QIODevice *ddoc);
	static void readXML(QIODevice *io, const std::function<void (QXmlStreamReader &)> &f);
	static void writeAttributes(QXmlStreamWriter &x, const QMap<QString,QString> &attrs);
	static void writeBase64Element(QXmlStreamWriter &x, const QString &ns, const QString &name, const QByteArray &data);
	static void writeElement(QXmlStreamWriter &x, const QString &ns, const QString &name, std::function<void ()> &&f = {});
	static void writeElement(QXmlStreamWriter &x, const QString &ns, const QString &name, const QMap<QString,QString> &attrs, std::function<void ()> &&f = {});

	QString method, mime;
	QHash<QString,QString> properties;

	static const QString
		AES128CBC_MTH, AES192CBC_MTH, AES256CBC_MTH,
		AES128GCM_MTH, AES192GCM_MTH, AES256GCM_MTH,
		SHA256_MTH, SHA384_MTH, SHA512_MTH,
		RSA_MTH, CONCATKDF_MTH, AGREEMENT_MTH, KWAES256_MTH;
	static const QString DS, DENC, DSIG11, XENC11;
	static const QString MIME_ZLIB, MIME_DDOC, MIME_DDOC_OLD;
	static const QHash<QString, const EVP_CIPHER*> ENC_MTH;
	static const QHash<QString, QCryptographicHash::Algorithm> SHA_MTH;
};
