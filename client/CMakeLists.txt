set_app_name( PROGNAME qdigidocclient )

add_executable( TSLDownload TSLDownload.cpp )
target_link_libraries( TSLDownload Qt5::Network )
add_custom_command(
	OUTPUT TSL.qrc tl-mp.xml EE.xml
	DEPENDS TSLDownload
	COMMAND $<TARGET_FILE:TSLDownload> "${CMAKE_CURRENT_BINARY_DIR}" EE
	WORKING_DIRECTORY ${_qt5Core_install_prefix}/bin
)

add_definitions( -DPKCS11_MODULE="${PKCS11_MODULE}" )
include_directories( ${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/common ${OPENSSL_INCLUDE_DIR} )

configure_file( translations/tr.qrc tr.qrc COPYONLY )
qt5_add_translation( SOURCES translations/en.ts translations/et.ts translations/ru.ts )
qt5_add_resources( SOURCES images/images.qrc fonts/fonts.qrc ${CMAKE_CURRENT_BINARY_DIR}/tr.qrc ${CMAKE_CURRENT_BINARY_DIR}/TSL.qrc )
qt5_wrap_ui( SOURCES MainWindow.ui MainWindow2.ui MainWindow3.ui MobileDialog.ui SettingsDialog.ui SignatureDialog.ui )

if( APPLE )
	list( APPEND SOURCES Application_mac.mm TreeWidgetMac.mm )
	set_source_files_properties( TreeWidgetMac.mm PROPERTIES COMPILE_FLAGS "-fobjc-arc" )
	set_source_files_properties( Application_mac.mm PROPERTIES COMPILE_FLAGS "-fobjc-arc" )
	list( APPEND ADDITIONAL_LIBRARIES "-framework Security" "-framework Quartz" )
elseif( WIN32 )
	find_package( Qt5 COMPONENTS WinExtras )
	list( APPEND SOURCES QCSP.cpp QCNG.cpp )
	list( APPEND ADDITIONAL_LIBRARIES Qt5::WinExtras NCrypt Crypt32 )
endif()

add_executable( ${PROGNAME} WIN32 MACOSX_BUNDLE
	${PROGNAME}.rc
	${SOURCES}
	${RESOURCE_FILES}
	main.cpp
	AccessCert.cpp
	Application.cpp
	CheckConnection.cpp
	DigiDoc.cpp
	FileDialog.cpp
	MainWindow.cpp
	MainWindow2.cpp
	MainWindow3.cpp
	MobileDialog.cpp
	PrintSheet.cpp
	QPKCS11.cpp
	QSigner.cpp
	SettingsDialog.cpp
	SignatureDialog.cpp
	TreeWidget.cpp
)
add_manifest( ${PROGNAME} )
target_link_libraries( ${PROGNAME}
	qdigidoccommon
	qdigidoccrypto
	Qt5::PrintSupport
	${LIBDIGIDOCPP_LIBRARY}
	${ADDITIONAL_LIBRARIES}
)

if( APPLE )
	set_target_properties( ${PROGNAME} PROPERTIES
		MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/mac/Info.plist.cmake
	)
	add_custom_command( TARGET ${PROGNAME} POST_BUILD
		COMMAND cp -a ${PKCS11_MODULE} $<TARGET_FILE_DIR:${PROGNAME}>
        )
	get_filename_component( PKCS11_NAME ${PKCS11_MODULE} NAME )
	get_filename_component( LIBPATH ${OPENSSL_CRYPTO_LIBRARY} PATH )
	add_custom_target( macdeployqt DEPENDS ${PROGNAME}
		COMMAND mkdir -p ${CMAKE_CURRENT_BINARY_DIR}/${PROGNAME}.app/Contents/Frameworks/
		COMMAND cp -a /Library/Frameworks/libdigidoc.framework $<TARGET_FILE_DIR:${PROGNAME}>/../Frameworks
		COMMAND ${_qt5Core_install_prefix}/bin/macdeployqt ${CMAKE_CURRENT_BINARY_DIR}/${PROGNAME}.app
			-executable=${CMAKE_CURRENT_BINARY_DIR}/${PROGNAME}.app/Contents/Frameworks/libdigidoc.framework/Versions/2/libdigidoc
		COMMAND rm -rf $<TARGET_FILE_DIR:${PROGNAME}>/../PlugIns/imageformats
		COMMAND rm -rf $<TARGET_FILE_DIR:${PROGNAME}>/../PlugIns/bearer
		COMMAND cp -a /Library/Frameworks/digidocpp.framework $<TARGET_FILE_DIR:${PROGNAME}>/../Frameworks
		COMMAND rm $<TARGET_FILE_DIR:${PROGNAME}>/../Frameworks/digidocpp.framework/Resources/digidoc-tool
		COMMAND rm $<TARGET_FILE_DIR:${PROGNAME}>/../Frameworks/libdigidoc.framework/Resources/cdigidoc
		COMMAND ln -sf ../Frameworks/digidocpp.framework/Libraries/libcrypto.dylib
			$<TARGET_FILE_DIR:${PROGNAME}>/../Frameworks/libcrypto.1.0.0.dylib
		COMMAND ln -sf ../Frameworks/digidocpp.framework/Libraries/libcrypto.dylib
			$<TARGET_FILE_DIR:${PROGNAME}>/../Frameworks/libcrypto.dylib
		COMMAND ln -sf ../Frameworks/digidocpp.framework/Libraries/libssl.dylib
			$<TARGET_FILE_DIR:${PROGNAME}>/../Frameworks/libssl.1.0.0.dylib
		COMMAND ln -sf ../Frameworks/digidocpp.framework/Libraries/libssl.dylib
			$<TARGET_FILE_DIR:${PROGNAME}>/../Frameworks/libssl.dylib
		COMMAND find $<TARGET_FILE_DIR:${PROGNAME}>/../Frameworks -name Headers | xargs rm -rf
		COMMAND find $<TARGET_FILE_DIR:${PROGNAME}>/../Frameworks -name 'TEST*.crt' -delete
		COMMAND install_name_tool -delete_rpath /Library/Frameworks $<TARGET_FILE:${PROGNAME}>
	)
	add_custom_target( codesign DEPENDS ${PROGNAME}
		COMMAND codesign -f -s \"$$SIGNCERT\"
			$<TARGET_FILE_DIR:${PROGNAME}>/*.*
			$<TARGET_FILE_DIR:${PROGNAME}>/../Frameworks/digidocpp.framework/Libraries/*
			$<TARGET_FILE_DIR:${PROGNAME}>/../Frameworks/*.framework
			$<TARGET_FILE_DIR:${PROGNAME}>/../PlugIns/*/*
			$<TARGET_FILE_DIR:${PROGNAME}>/../Library/QuickLook/DigiDocQL.qlgenerator
		COMMAND codesign -f -s \"$$SIGNCERT\" $<TARGET_FILE_DIR:${PROGNAME}>/../..
			--entitlements ${CMAKE_SOURCE_DIR}/${PROGNAME}.entitlements
	)
	add_custom_target( productbuild DEPENDS ${PROGNAME}
		COMMAND productbuild --component ${CMAKE_CURRENT_BINARY_DIR}/${PROGNAME}.app /Applications
			--sign \"$$INSTCERT\" ${CMAKE_BINARY_DIR}/qdigidoc-store_${VERSION}$ENV{VER_SUFFIX}.pkg
	)
	add_custom_target( zip DEPENDS ${PROGNAME}
		COMMAND zip --symlinks -r -D ${CMAKE_BINARY_DIR}/qdigidoc_${VERSION}$ENV{VER_SUFFIX}.zip ${PROGNAME}.app
	)
	add_custom_target( zipdebug DEPENDS ${PROGNAME}
		COMMAND dsymutil -o ${PROGNAME}.dSYM $<TARGET_FILE:${PROGNAME}>
		COMMAND zip -r ${CMAKE_BINARY_DIR}/qdigidoc-dbg_${VERSION}$ENV{VER_SUFFIX}.zip ${PROGNAME}.dSYM
	)
elseif(WIN32)
	get_filename_component(LIBS_PATH ${LIBDIGIDOCPP_LIBRARIES} DIRECTORY)
	if(CMAKE_SIZEOF_VOID_P EQUAL 8)
		set(PLATFORM x64)
		set(SSL_PATH "C:/OpenSSL-Win64/bin")
	else()
		set(PLATFORM x86)
		set(SSL_PATH "C:/OpenSSL-Win32/bin")
	endif()
	set(MSI_FILE "Digidoc3_Client-${VERSION}$ENV{VER_SUFFIX}.${PLATFORM}")
	list(APPEND CANDLE_CMD "$ENV{WIX}bin\\candle.exe" -nologo -arch ${PLATFORM} -dMSI_VERSION=${VERSION} -dPlatform=${PLATFORM}
		-dssl_path="${SSL_PATH}" -dlibs_path="${LIBS_PATH}" -dclient_path=$<TARGET_FILE:${PROGNAME}> -dqtconf=${CMAKE_SOURCE_DIR}/qt.conf
		-dschemasLocation=${LIBS_PATH}/schema SchemasFragment.wxs ${CMAKE_SOURCE_DIR}/qdigidoc.wxs
		${CMAKE_SOURCE_DIR}/cmake/modules/WelcomeDlg2.wxs ${CMAKE_SOURCE_DIR}/cmake/modules/WixUI_Minimal2.wxs)
	list(APPEND LIGHT_CMD "$ENV{WIX}bin\\light.exe" -nologo -ext WixUIExtension
		qdigidoc.wixobj SchemasFragment.wixobj WelcomeDlg2.wixobj WixUI_Minimal2.wixobj
		-dWixUIDialogBmp=${CMAKE_SOURCE_DIR}/cmake/modules/dlgbmp.bmp
		-dWixUIBannerBmp=${CMAKE_SOURCE_DIR}/cmake/modules/banner.bmp)
	if(EXISTS ${LIBS_PATH}/../certs)
		list(APPEND CANDLE_CMD -dcertsLocation=CertsFragment CertsFragment.wxs)
		list(APPEND LIGHT_CMD CertsFragment.wixobj)
		add_custom_target(CertsFragment.wxs
			COMMAND ${CMAKE_COMMAND} -E copy_directory ${LIBS_PATH}/../certs CertsFragment
			COMMAND del "CertsFragment\\TEST*.crt"
			COMMAND "$ENV{WIX}bin\\heat.exe" dir CertsFragment -nologo -cg Certs -gg -scom -sreg -sfrag -srd -dr CertsFolder -var var.certsLocation -out CertsFragment.wxs
			WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
		)
	else()
		add_custom_target(CertsFragment.wxs)
	endif()
	if(EXISTS ${LIBS_PATH}/Xalan-C_1_11.dll)
		list(APPEND CANDLE_CMD -dxalan=true)
	endif()
	add_custom_target(msi DEPENDS ${PROGNAME} CertsFragment.wxs
		COMMAND "$ENV{WIX}bin\\heat.exe" dir ${LIBS_PATH}/schema -nologo -cg Schemas -gg -scom -sreg -sfrag -srd -dr SchemaFolder -var var.schemasLocation -out SchemasFragment.wxs
		COMMAND ${CANDLE_CMD}
		COMMAND ${LIGHT_CMD} -loc ${CMAKE_SOURCE_DIR}/qdigidoc.en-US.wxl -cultures:en-US -o "${MSI_FILE}.en-US.msi"
		COMMAND ${LIGHT_CMD} -loc ${CMAKE_SOURCE_DIR}/qdigidoc.et-EE.wxl -cultures:et-EE -o "${MSI_FILE}.et-EE.msi"
		#Build MSI with QT
		COMMAND ${CANDLE_CMD} -dqt_path=${_qt5Core_install_prefix}
		COMMAND ${LIGHT_CMD} -loc ${CMAKE_SOURCE_DIR}/qdigidoc.en-US.wxl -cultures:en-US -o "${MSI_FILE}.en-US.qt.msi"
		COMMAND ${LIGHT_CMD} -loc ${CMAKE_SOURCE_DIR}/qdigidoc.et-EE.wxl -cultures:et-EE -o "${MSI_FILE}.et-EE.qt.msi"
		WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
	)
	if(SIGNCERT)
		list(APPEND SIGNCMD signtool.exe sign /a /v /s MY /n "${SIGNCERT}" /fd SHA256
			/du http://installer.id.ee /tr http://sha256timestamp.ws.symantec.com/sha256/timestamp /td SHA256)
		add_custom_command(TARGET ${PROGNAME} POST_BUILD
			COMMAND ${SIGNCMD} $<TARGET_FILE:${PROGNAME}>
		)
		add_custom_command(TARGET msi POST_BUILD
			COMMAND ${SIGNCMD} "${MSI_FILE}.en-US.msi" "${MSI_FILE}.et-EE.msi" "${MSI_FILE}.en-US.qt.msi" "${MSI_FILE}.et-EE.qt.msi"
			WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
		)
	endif()
else()
	install( TARGETS ${PROGNAME} DESTINATION ${CMAKE_INSTALL_BINDIR} )
	set_target_properties( ${PROGNAME} PROPERTIES COMPILE_DEFINITIONS "DATADIR=\"${CMAKE_INSTALL_FULL_DATADIR}\"" )
	configure_file( qdigidocclient.1.cmake qdigidocclient.1 )
	install( FILES ${CMAKE_CURRENT_BINARY_DIR}/qdigidocclient.1 DESTINATION ${CMAKE_INSTALL_MANDIR}/man1 )
	install( FILES qdigidoc-client.appdata.xml DESTINATION ${CMAKE_INSTALL_DATADIR}/appdata )
	install( FILES qdigidoc-client.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications )
	install( FILES qdigidoc-client.xml DESTINATION ${CMAKE_INSTALL_DATADIR}/mime/packages )
	foreach(RES 16 22 32 48 128)
		foreach(TGT application-vnd.etsi.asic-e+zip.png application-vnd.etsi.asic-s+zip.png application-x-ddoc.png application-x-p12d.png)
			install(
				FILES images/qdigidoc_client_document_${RES}x${RES}.png
				DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/${RES}x${RES}/mimetypes/
				RENAME ${TGT}
			)
		endforeach()
		install(
			FILES images/digidoc_icon_${RES}x${RES}.png
			DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/${RES}x${RES}/apps/
			RENAME qdigidoc-client.png
		)
	endforeach()
endif()
