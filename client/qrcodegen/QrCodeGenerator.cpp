/*
 * Copyright (c) 2023 Alex Spataru <https://github.com/alex-spataru>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <sstream>
#include <string>
#include <QPainter>
#include <QTextStream>
#include <QSvgRenderer>

#include "QrCodeGenerator.h"

/**
 * @brief Default constructor for the QrCodeGenerator class.
 * @param parent QObject parent
 */
QrCodeGenerator::QrCodeGenerator(QObject *parent)
  : QObject(parent)
{
  // No implementation needed for default constructor.
}

/**
 * @brief Generates a QR code image from a given text string.
 * @param data The data to encode in the QR code.
 * @param size The image size for the generated QR code.
 * @param borderSize The size of the border around the QR code.
 * @param errorCorrection The level of error correction to apply.
 * @return QImage representing the generated QR code.
 */
QImage QrCodeGenerator::generateQr(const QString &data, quint16 size,
                                   quint16 borderSize,
                                   qrcodegen::QrCode::Ecc errorCorrection)
{
  auto b = data.toUtf8();
  const auto qrCode
      = qrcodegen::QrCode::encodeText(b.constData(), errorCorrection);
  return qrCodeToImage(qrCode, borderSize, size);
}

/**
 * @brief Generates a QR code image from a given text string.
 * @param data The data to encode in the QR code.
 * @param size The image size for the generated QR code.
 * @param borderSize The size of the border around the QR code.
 * @param errorCorrection The level of error correction to apply.
 * @return QImage representing the generated QR code.
 */
QImage QrCodeGenerator::generateQr(const QByteArray &data, quint16 size,
                                   quint16 borderSize,
                                   qrcodegen::QrCode::Ecc errorCorrection)
{
    std::vector<uint8_t> c(data.constData(), data.constData() + data.size());
    const auto qrCode
        = qrcodegen::QrCode::encodeBinary(c, errorCorrection);
    return qrCodeToImage(qrCode, borderSize, size);
}

/**
 * @brief Generates an SVG string representing a QR code.
 * @param data The data to encode in the QR code.
 * @param borderSize The size of the border around the QR code.
 * @param errorCorrection The level of error correction to apply.
 * @return QString containing the SVG representation of the QR code.
 */
QString QrCodeGenerator::generateSvgQr(const QString &data, quint16 borderSize,
                                       qrcodegen::QrCode::Ecc errorCorrection)
{
  auto b = data.toUtf8();
  const auto qrCode
      = qrcodegen::QrCode::encodeText(b.constData(), errorCorrection);
  return toSvgString(qrCode, borderSize);
}

/**
 * @brief Converts a QR code to its SVG representation as a string.
 * @param qr The QR code to convert.
 * @param border The border size to use.
 * @return QString containing the SVG representation of the QR code.
 */
QString QrCodeGenerator::toSvgString(const qrcodegen::QrCode &qr,
                                     quint16 border) const
{
  QString str;
  QTextStream sb(&str);

  sb << R"(<?xml version="1.0" encoding="UTF-8"?>)"
     << R"(<!DOCTYPE svg PUBLIC "-//W3C//DTD SVG 1.1//EN" "http://www.w3.org/Graphics/SVG/1.1/DTD/svg11.dtd">)"
     << R"(<svg xmlns="http://www.w3.org/2000/svg" version="1.1" viewBox="0 0 )"
     << (qr.getSize() + border * 2) << " " << (qr.getSize() + border * 2)
     << R"(" stroke="none"><rect width="100%" height="100%" fill="#FFFFFF"/><path d=")";

  for (int y = 0; y < qr.getSize(); y++)
  {
    for (int x = 0; x < qr.getSize(); x++)
    {
      if (qr.getModule(x, y))
      {
        sb << (x == 0 && y == 0 ? "" : " ") << "M" << (x + border) << ","
           << (y + border) << "h1v1h-1z";
      }
    }
  }

  sb << R"(" fill="#000000"/></svg>)";
  return str;
}

/**
 * @brief Converts a QR code to a QImage.
 * @param qrCode The QR code to convert.
 * @param border The border size to use.
 * @param size The image size to generate.
 * @return QImage representing the QR code.
 */
// QImage QrCodeGenerator::qrCodeToImage(const qrcodegen::QrCode &qrCode,
//                                       quint16 border, quint16 size) const
// {
//   QString svg = toSvgString(qrCode, border);
//   QSvgRenderer render(svg.toUtf8());
//   QImage image(size, size, QImage::Format_Mono);
//   image.fill(Qt::white);
//   QPainter painter(&image);
//   painter.setRenderHint(QPainter::Antialiasing);
//   render.render(&painter);
//   return image;
// }

// QR code image is being rendered from SVG output, not directly from bitmaps or
// matrices QSvgRenderer has internal limits on path complexity or buffer size —
// hitting those leads to the truncation warning.
//  If the generated SVG string is too large or malformed, the QSvgRenderer will
//  throw warnings.
// like:  W : qt.svg: Invalid path data; path truncated.
QImage QrCodeGenerator::qrCodeToImage(const qrcodegen::QrCode &qrCode,
                                      quint16 border, quint16 size) const
{
  const int qrSize = qrCode.getSize();
  const int totalSize = qrSize + 2 * border;
  // Calculate scaling factor to fit requested size
  const int pixelSize = size / totalSize;
  const int imageSize = pixelSize * totalSize;
  // Create the output image
  QImage image(imageSize, imageSize, QImage::Format_RGB32);
  image.fill(Qt::white);

  QPainter painter(&image);
  painter.setBrush(Qt::black);
  painter.setPen(Qt::NoPen);

  // Draw each QR module (black square)
  for (int y = 0; y < qrSize; ++y)
  {
    for (int x = 0; x < qrSize; ++x)
    {
      if (qrCode.getModule(x, y))
      {
        int xPos = (x + border) * pixelSize;
        int yPos = (y + border) * pixelSize;
        painter.drawRect(xPos, yPos, pixelSize, pixelSize);
      }
    }
  }

  return image;
}
