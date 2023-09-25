#!/bin/bash
# Requires xcode and cmake, also OpenSSL has to be installed via homebrew.

set -e

######### Versions of libraries/frameworks to be compiled
QT_VER="6.5.2"
OPENSSL_VER="3.0.11"
OPENLDAP_VER="2.6.6"
REBUILD=false
BUILD_PATH=~/cmake_builds
: ${MACOSX_DEPLOYMENT_TARGET:="11.0"}
export MACOSX_DEPLOYMENT_TARGET

while [[ $# -gt 0 ]]
do
    key="$1"
    case $key in
    -o|--openssl)
        OPENSSL_PATH="$2"
        shift
        ;;
    -p|--path)
        BUILD_PATH="$2"
        : ${OPENSSL_PATH:="${BUILD_PATH}/OpenSSL"}
        shift
        ;;
    -q|--qt)
        QT_VER="$2"
        shift
        ;;
    -l|--openldap)
        OPENLDAP_VER="$2"
        shift
        ;;
    -r|--rebuild)
        REBUILD=true
        ;;
    -h|--help)
        echo "Build Qt for Digidoc4 client"
        echo ""
        echo "Usage: $0 [-r|--rebuild] [-p build-path] [-h|--help] [-q|--qt qt-version] [-l|--openldap openldap-version]"
        echo ""
        echo "Options:"
        echo "  -o or --openssl openssl-path:"
        echo "     OpenSSL path; default ${OPENSSL_VER} will be built ${OPENSSL_PATH}"
        echo "  -p or --path build-path"
        echo "     folder where the dependencies should be built; default ${BUILD_PATH}"
        echo "  -q or --qt qt-version:"
        echo "     Specific version of Qt to build; default ${QT_VER} "
        echo "  -l or --openldap openldap-version:"
        echo "     Specific version of OpenLDAP to build; default ${OPENLDAP_VER} "
        echo "  -r or --rebuild:"
        echo "     Rebuild even if dependency is already built"
        echo " "
        echo "  -h or --help:"
        echo "     Print usage of the script "
        exit 0
        ;;
    esac
    shift # past argument or value
done

QT_PATH=${BUILD_PATH}/Qt-${QT_VER}-OpenSSL
OPENLDAP_PATH=${BUILD_PATH}/OpenLDAP
GREY='\033[0;37m'
ORANGE='\033[0;33m'
RED='\033[0;31m'
RESET='\033[0m'

if [[ ! -d ${OPENSSL_PATH} ]] ; then
    echo -e "\n${ORANGE}##### Building OpenSSL ${OPENSSL_VER} ${OPENSSL_PATH} #####${RESET}\n"
    mkdir -p ${BUILD_PATH} && cd ${BUILD_PATH}
    if [ ! -f openssl-${OPENSSL_VER}.tar.gz ]; then
        curl -O -L https://www.openssl.org/source/openssl-${OPENSSL_VER}.tar.gz
    fi
    rm -rf openssl-${OPENSSL_VER}
    tar xf openssl-${OPENSSL_VER}.tar.gz
    cd openssl-${OPENSSL_VER}
    for ARCH in x86_64 arm64; do
        ./Configure darwin64-${ARCH} --prefix=${OPENSSL_PATH} shared no-autoload-config no-module no-tests enable-ec_nistp_64_gcc_128
        make -s > /dev/null
        make install_sw
        mv ${OPENSSL_PATH} ${OPENSSL_PATH}.${ARCH}
        make distclean
    done
    cp -a ${OPENSSL_PATH}.x86_64 ${OPENSSL_PATH}
    cd ${OPENSSL_PATH}.arm64
    for i in lib/lib*3.dylib; do
        lipo -create ${OPENSSL_PATH}.x86_64/${i} ${i} -output ${OPENSSL_PATH}/${i}
    done
    cd -
else
    echo -e "\n${GREY}  OpenSSL not built${RESET}"
fi

if [[ "$REBUILD" = true || ! -d ${QT_PATH} ]] ; then
    qt_ver_parts=( ${QT_VER//./ } )
    QT_MINOR="${qt_ver_parts[0]}.${qt_ver_parts[1]}"
    echo -e "\n${ORANGE}##### Building Qt ${QT_VER} ${QT_PATH} #####${RESET}\n"
    mkdir -p ${BUILD_PATH}
    pushd ${BUILD_PATH}
    for PACKAGE in qtbase-everywhere-src-${QT_VER} qtsvg-everywhere-src-${QT_VER} qttools-everywhere-src-${QT_VER} qt5compat-everywhere-src-${QT_VER}; do
        if [ ! -f ${PACKAGE}.tar.xz ]; then
            curl -O -L http://download.qt.io/official_releases/qt/${QT_MINOR}/${QT_VER}/submodules/${PACKAGE}.tar.xz
        fi
        rm -rf ${PACKAGE}
        tar xf ${PACKAGE}.tar.xz
        pushd ${PACKAGE}
        if [[ "${PACKAGE}" == *"qtbase"* ]] ; then
            # CVE-2023-34410-qtbase-6.5.diff
            patch -Np1 <<'EOF'
diff --git a/src/corelib/serialization/qxmlstream.cpp b/src/corelib/serialization/qxmlstream.cpp
index 6e34d4d..cf46d69 100644
--- a/src/corelib/serialization/qxmlstream.cpp
+++ b/src/corelib/serialization/qxmlstream.cpp
@@ -185,7 +185,7 @@
     addData() or by waiting for it to arrive on the device().
 
     \value UnexpectedElementError The parser encountered an element
-    that was different to those it expected.
+    or token that was different to those it expected.
 
 */
 
@@ -322,13 +322,34 @@
 
   QXmlStreamReader is a well-formed XML 1.0 parser that does \e not
   include external parsed entities. As long as no error occurs, the
-  application code can thus be assured that the data provided by the
-  stream reader satisfies the W3C's criteria for well-formed XML. For
-  example, you can be certain that all tags are indeed nested and
-  closed properly, that references to internal entities have been
-  replaced with the correct replacement text, and that attributes have
-  been normalized or added according to the internal subset of the
-  DTD.
+  application code can thus be assured, that
+  \list
+  \li the data provided by the stream reader satisfies the W3C's
+      criteria for well-formed XML,
+  \li tokens are provided in a valid order.
+  \endlist
+
+  Unless QXmlStreamReader raises an error, it guarantees the following:
+  \list
+  \li All tags are nested and closed properly.
+  \li References to internal entities have been replaced with the
+      correct replacement text.
+  \li Attributes have been normalized or added according to the
+      internal subset of the \l DTD.
+  \li Tokens of type \l StartDocument happen before all others,
+      aside from comments and processing instructions.
+  \li At most one DOCTYPE element (a token of type \l DTD) is present.
+  \li If present, the DOCTYPE appears before all other elements,
+      aside from StartDocument, comments and processing instructions.
+  \endlist
+
+  In particular, once any token of type \l StartElement, \l EndElement,
+  \l Characters, \l EntityReference or \l EndDocument is seen, no
+  tokens of type StartDocument or DTD will be seen. If one is present in
+  the input stream, out of order, an error is raised.
+
+  \note The token types \l Comment and \l ProcessingInstruction may appear
+  anywhere in the stream.
 
   If an error occurs while parsing, atEnd() and hasError() return
   true, and error() returns the error that occurred. The functions
@@ -659,6 +680,7 @@
         d->token = -1;
         return readNext();
     }
+    d->checkToken();
     return d->type;
 }
 
@@ -743,6 +765,11 @@
     "ProcessingInstruction"
 );
 
+static constexpr auto QXmlStreamReader_XmlContextString = qOffsetStringArray(
+    "Prolog",
+    "Body"
+);
+
 /*!
     \property  QXmlStreamReader::namespaceProcessing
     \brief the namespace-processing flag of the stream reader.
@@ -777,6 +804,15 @@
     return QLatin1StringView(QXmlStreamReader_tokenTypeString.at(d->type));
 }
 
+/*!
+   \internal
+   \return \param loc (Prolog/Body) as a string.
+ */
+static constexpr QLatin1StringView contextString(QXmlStreamReaderPrivate::XmlContext ctxt)
+{
+    return QLatin1StringView(QXmlStreamReader_XmlContextString.at(static_cast<int>(ctxt)));
+}
+
 #endif // QT_NO_XMLSTREAMREADER
 
 QXmlStreamPrivateTagStack::QXmlStreamPrivateTagStack()
@@ -864,6 +900,8 @@
 
     type = QXmlStreamReader::NoToken;
     error = QXmlStreamReader::NoError;
+    currentContext = XmlContext::Prolog;
+    foundDTD = false;
 }
 
 /*
@@ -3838,6 +3876,97 @@
     }
 }
 
+static constexpr bool isTokenAllowedInContext(QXmlStreamReader::TokenType type,
+                                               QXmlStreamReaderPrivate::XmlContext loc)
+{
+    switch (type) {
+    case QXmlStreamReader::StartDocument:
+    case QXmlStreamReader::DTD:
+        return loc == QXmlStreamReaderPrivate::XmlContext::Prolog;
+
+    case QXmlStreamReader::StartElement:
+    case QXmlStreamReader::EndElement:
+    case QXmlStreamReader::Characters:
+    case QXmlStreamReader::EntityReference:
+    case QXmlStreamReader::EndDocument:
+        return loc == QXmlStreamReaderPrivate::XmlContext::Body;
+
+    case QXmlStreamReader::Comment:
+    case QXmlStreamReader::ProcessingInstruction:
+        return true;
+
+    case QXmlStreamReader::NoToken:
+    case QXmlStreamReader::Invalid:
+        return false;
+    }
+
+    // GCC 8.x does not treat __builtin_unreachable() as constexpr
+#if !defined(Q_CC_GNU_ONLY) || (Q_CC_GNU >= 900)
+    Q_UNREACHABLE_RETURN(false);
+#else
+    return false;
+#endif
+}
+
+/*!
+   \internal
+   \brief QXmlStreamReader::isValidToken
+   \return \c true if \param type is a valid token type.
+   \return \c false if \param type is an unexpected token,
+   which indicates a non-well-formed or invalid XML stream.
+ */
+bool QXmlStreamReaderPrivate::isValidToken(QXmlStreamReader::TokenType type)
+{
+    // Don't change currentContext, if Invalid or NoToken occur in the prolog
+    if (type == QXmlStreamReader::Invalid || type == QXmlStreamReader::NoToken)
+        return false;
+
+    // If a token type gets rejected in the body, there is no recovery
+    const bool result = isTokenAllowedInContext(type, currentContext);
+    if (result || currentContext == XmlContext::Body)
+        return result;
+
+    // First non-Prolog token observed => switch context to body and check again.
+    currentContext = XmlContext::Body;
+    return isTokenAllowedInContext(type, currentContext);
+}
+
+/*!
+   \internal
+   Checks token type and raises an error, if it is invalid
+   in the current context (prolog/body).
+ */
+void QXmlStreamReaderPrivate::checkToken()
+{
+    Q_Q(QXmlStreamReader);
+
+    // The token type must be consumed, to keep track if the body has been reached.
+    const XmlContext context = currentContext;
+    const bool ok = isValidToken(type);
+
+    // Do nothing if an error has been raised already (going along with an unexpected token)
+    if (error != QXmlStreamReader::Error::NoError)
+        return;
+
+    if (!ok) {
+        raiseError(QXmlStreamReader::UnexpectedElementError,
+                   QObject::tr("Unexpected token type %1 in %2.")
+                   .arg(q->tokenString(), contextString(context)));
+        return;
+    }
+
+    if (type != QXmlStreamReader::DTD)
+        return;
+
+    // Raise error on multiple DTD tokens
+    if (foundDTD) {
+        raiseError(QXmlStreamReader::UnexpectedElementError,
+                   QObject::tr("Found second DTD token in %1.").arg(contextString(context)));
+    } else {
+        foundDTD = true;
+    }
+}
+
 /*!
  \fn bool QXmlStreamAttributes::hasAttribute(QAnyStringView qualifiedName) const
 
diff --git a/src/corelib/serialization/qxmlstream_p.h b/src/corelib/serialization/qxmlstream_p.h
index 070424a..f09adaa 100644
--- a/src/corelib/serialization/qxmlstream_p.h
+++ b/src/corelib/serialization/qxmlstream_p.h
@@ -297,6 +297,17 @@
     QStringDecoder decoder;
     bool atEnd;
 
+    enum class XmlContext
+    {
+        Prolog,
+        Body,
+    };
+
+    XmlContext currentContext = XmlContext::Prolog;
+    bool foundDTD = false;
+    bool isValidToken(QXmlStreamReader::TokenType type);
+    void checkToken();
+
     /*!
       \sa setType()
      */
EOF
            ./configure -prefix ${QT_PATH} -opensource -nomake tests -nomake examples -no-securetransport -openssl-linked -confirm-license -appstore-compliant -- -DOPENSSL_ROOT_DIR=${OPENSSL_PATH} -DCMAKE_OSX_ARCHITECTURES="x86_64;arm64"
        else
            ${QT_PATH}/bin/qt-configure-module .
        fi
        cmake --build . --parallel
        cmake --build . --target install
        popd
        rm -rf ${PACKAGE}
    done
    popd
else
    echo -e "\n${GREY}  Qt not built${RESET}"
fi

if [[ "$REBUILD" = true || ! -d ${OPENLDAP_PATH} ]] ; then
    echo -e "\n${ORANGE}##### Building OpenLDAP ${OPENLDAP_VER} ${OPENLDAP_PATH} #####${RESET}\n"
    mkdir -p ${BUILD_PATH}
    pushd ${BUILD_PATH}
    if [ ! -f openldap-${OPENLDAP_VER}.tgz ]; then
        curl -O -L http://mirror.eu.oneandone.net/software/openldap/openldap-release/openldap-${OPENLDAP_VER}.tgz
    fi
    tar xf openldap-${OPENLDAP_VER}.tgz
    pushd openldap-${OPENLDAP_VER}
    sed -ie 's! doc!!' Makefile.in
    ARCH="-arch x86_64 -arch arm64"
    CFLAGS="${ARCH}" CXXFLAGS="${ARCH}" LDFLAGS="${ARCH} -L${OPENSSL_PATH}/lib" CPPFLAGS="-I${OPENSSL_PATH}/include" ./configure \
        --prefix ${OPENLDAP_PATH} --enable-static --disable-shared --disable-syslog --disable-local --disable-slapd \
        --without-threads --without-cyrus-sasl --with-tls=openssl
    make
    make install
    popd
    popd
else
    echo -e "\n${GREY}  OpenLDAP not built${RESET}"
fi
