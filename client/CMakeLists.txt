set_app_name( PROGNAME qdigidoc4 )

add_executable( TSLDownload TSLDownload.cpp )
target_link_libraries( TSLDownload Qt5::Network )
add_custom_command(
	OUTPUT TSL.qrc
	DEPENDS TSLDownload
	COMMAND $<TARGET_FILE:TSLDownload> "${CMAKE_CURRENT_BINARY_DIR}" ${TSL_URL} ${TSL_INCLUDE}
	WORKING_DIRECTORY ${_qt5Core_install_prefix}/bin
)

include_directories( ${CMAKE_SOURCE_DIR} )

configure_file( translations/tr.qrc tr.qrc COPYONLY )
qt5_add_translation( SOURCES translations/en.ts translations/et.ts translations/ru.ts )
qt5_add_resources( SOURCES images/images.qrc fonts/fonts.qrc ${CMAKE_CURRENT_BINARY_DIR}/tr.qrc ${CMAKE_CURRENT_BINARY_DIR}/TSL.qrc )
qt5_wrap_ui( SOURCES
	MainWindow.ui
	dialogs/AddRecipients.ui
	dialogs/CertificateDetails.ui
	dialogs/CertificateHistory.ui
	dialogs/FirstRun.ui
	dialogs/KeyDialog.ui
	dialogs/MobileDialog.ui
	dialogs/MobileProgress.ui
	dialogs/PinPopup.ui
	dialogs/PinUnblock.ui
	dialogs/RoleAddressDialog.ui
	dialogs/SettingsDialog.ui
	dialogs/SignatureDialog.ui
	dialogs/SmartIDDialog.ui
	dialogs/WaitDialog.ui
	dialogs/WarningDialog.ui
	widgets/Accordion.ui
	widgets/AccordionTitle.ui
	widgets/AddressItem.ui
	widgets/CardWidget.ui
	widgets/ContainerPage.ui
	widgets/FileItem.ui
	widgets/InfoStack.ui
	widgets/ItemList.ui
	widgets/MainAction.ui
	widgets/NoCardInfo.ui
	widgets/OtherData.ui
	widgets/PageIcon.ui
	widgets/SignatureItem.ui
	widgets/VerifyCert.ui
	widgets/WarningItem.ui
	widgets/WarningRibbon.ui
)

add_executable( ${PROGNAME} WIN32 MACOSX_BUNDLE
	${PROGNAME}.rc
	${SOURCES}
	${RESOURCE_FILES}
	main.cpp
	AccessCert.cpp
	Application.cpp
	CheckConnection.cpp
	CliApplication.cpp
	DigiDoc.cpp
	Diagnostics.cpp
	DiagnosticsTask.cpp
	DocumentModel.cpp
	MainWindow.cpp
	MainWindow_MyEID.cpp
	QCardLock.cpp
	QPKCS11.cpp
	QSigner.cpp
	QSmartCard.cpp
	sslConnect.cpp
	Styles.cpp
	XmlReader.cpp
	PrintSheet.cpp
	SslCertificate.cpp
	crypto/CryptoDoc.cpp
	crypto/LdapSearch.cpp
	dialogs/AddRecipients.cpp
	dialogs/CertificateDetails.cpp
	dialogs/CertificateHistory.cpp
	dialogs/FileDialog.cpp
	dialogs/FirstRun.cpp
	dialogs/KeyDialog.cpp
	dialogs/MobileDialog.cpp
	dialogs/MobileProgress.cpp
	dialogs/PinPopup.cpp
	dialogs/PinUnblock.cpp
	dialogs/RoleAddressDialog.cpp
	dialogs/SettingsDialog.cpp
	dialogs/SmartIDDialog.cpp
	dialogs/SmartIDProgress.cpp
	dialogs/SignatureDialog.cpp
	dialogs/WaitDialog.cpp
	dialogs/WarningDialog.cpp
	effects/ButtonHoverFilter.cpp
	effects/FadeInNotification.cpp
	widgets/Accordion.cpp
	widgets/AccordionTitle.cpp
	widgets/AddressItem.cpp
	widgets/CardWidget.cpp
	widgets/CardPopup.cpp
	widgets/ContainerPage.cpp
	widgets/DropdownButton.cpp
	widgets/FileItem.cpp
	widgets/FileList.cpp
	widgets/Item.cpp
	widgets/ItemList.cpp
	widgets/InfoStack.cpp
	widgets/LabelButton.cpp
	widgets/MainAction.cpp
	widgets/NoCardInfo.cpp
	widgets/OtherData.cpp
	widgets/PageIcon.cpp
	widgets/SignatureItem.cpp
	widgets/StyledWidget.cpp
	widgets/VerifyCert.cpp
	widgets/WarningItem.cpp
	widgets/WarningList.cpp
	widgets/WarningRibbon.cpp
)
add_manifest( ${PROGNAME} )

target_link_libraries( ${PROGNAME}
	qdigidoccommon
	Qt5::PrintSupport
	Qt5::Svg
	${LIBDIGIDOCPP_LIBRARY}
	${LDAP_LIBRARIES}
)

target_compile_definitions(${PROGNAME} PRIVATE MOBILEID_URL="${MOBILEID_URL}" SMARTID_URL="${SMARTID_URL}" )

if( APPLE )
	target_sources( ${PROGNAME} PRIVATE Application_mac.mm dialogs/CertificateDetails_mac.mm Diagnostics_unix.cpp)
	set_source_files_properties( Application_mac.mm dialogs/CertificateDetails_mac.mm PROPERTIES COMPILE_FLAGS "-fobjc-arc" )
	set_source_files_properties( crypto/LdapSearch.cpp PROPERTIES COMPILE_FLAGS "-Wno-deprecated-declarations" )
	target_link_libraries( ${PROGNAME} "-framework Security" "-framework Quartz" )
	set_target_properties( ${PROGNAME} PROPERTIES
		MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/mac/Info.plist.cmake
	)
	add_custom_command( TARGET ${PROGNAME} POST_BUILD
		COMMAND cp -a ${PKCS11_MODULE} $<TARGET_FILE_DIR:${PROGNAME}>
		COMMAND mkdir -p $<TARGET_FILE_DIR:${PROGNAME}>/../Library/QuickLook
		COMMAND cp -a $<TARGET_FILE_DIR:DigiDocQL>/../../../DigiDocQL.qlgenerator
			$<TARGET_FILE_DIR:${PROGNAME}>/../Library/QuickLook )
	get_filename_component( PKCS11_NAME ${PKCS11_MODULE} NAME )
	add_custom_target( macdeployqt DEPENDS ${PROGNAME}
		COMMAND mkdir -p ${CMAKE_CURRENT_BINARY_DIR}/${PROGNAME}.app/Contents/Frameworks/
		COMMAND cp -a /Library/Frameworks/libdigidoc.framework $<TARGET_FILE_DIR:${PROGNAME}>/../Frameworks
		COMMAND ${_qt5Core_install_prefix}/bin/macdeployqt ${CMAKE_CURRENT_BINARY_DIR}/${PROGNAME}.app
			-executable=${CMAKE_CURRENT_BINARY_DIR}/${PROGNAME}.app/Contents/Frameworks/libdigidoc.framework/Versions/2/libdigidoc
			-executable=${CMAKE_CURRENT_BINARY_DIR}/${PROGNAME}.app/Contents/MacOS/${PKCS11_NAME} 
		COMMAND find $<TARGET_FILE_DIR:${PROGNAME}>/../PlugIns/imageformats -type f -a ! -name libqgif.dylib -a ! -name libqjpeg.dylib -a ! -name libqsvg.dylib -delete
		COMMAND rm -rf $<TARGET_FILE_DIR:${PROGNAME}>/../PlugIns/bearer
		COMMAND cp -a /Library/Frameworks/digidocpp.framework $<TARGET_FILE_DIR:${PROGNAME}>/../Frameworks
		COMMAND rm $<TARGET_FILE_DIR:${PROGNAME}>/../Frameworks/digidocpp.framework/Resources/digidoc-tool
		COMMAND rm $<TARGET_FILE_DIR:${PROGNAME}>/../Frameworks/libdigidoc.framework/Resources/cdigidoc
		COMMAND ln -sf ../Frameworks/digidocpp.framework/Libraries/libcrypto.dylib
		    $<TARGET_FILE_DIR:${PROGNAME}>/../Frameworks/libcrypto.1.1.dylib
		COMMAND ln -sf ../Frameworks/digidocpp.framework/Libraries/libcrypto.dylib
			$<TARGET_FILE_DIR:${PROGNAME}>/../Frameworks/libcrypto.dylib
		COMMAND ln -sf ../Frameworks/digidocpp.framework/Libraries/libssl.dylib
		    $<TARGET_FILE_DIR:${PROGNAME}>/../Frameworks/libssl.1.1.dylib
		COMMAND ln -sf ../Frameworks/digidocpp.framework/Libraries/libssl.dylib
			$<TARGET_FILE_DIR:${PROGNAME}>/../Frameworks/libssl.dylib
		COMMAND find $<TARGET_FILE_DIR:${PROGNAME}>/../Frameworks -name Headers | xargs rm -rf
		COMMAND find $<TARGET_FILE_DIR:${PROGNAME}>/../Frameworks -name 'TEST*.crt' -delete
		COMMAND install_name_tool -delete_rpath /Library/Frameworks $<TARGET_FILE:${PROGNAME}>
		COMMAND install_name_tool -change /Library/libdigidocpp/lib/libssl.1.1.dylib @loader_path/../../../libssl.1.1.dylib
			${CMAKE_CURRENT_BINARY_DIR}/${PROGNAME}.app/Contents/Frameworks/libdigidoc.framework/Versions/2/libdigidoc
		COMMAND install_name_tool -change /Library/libdigidocpp/lib/libcrypto.1.1.dylib @loader_path/../../../libcrypto.1.1.dylib
			${CMAKE_CURRENT_BINARY_DIR}/${PROGNAME}.app/Contents/Frameworks/libdigidoc.framework/Versions/2/libdigidoc
	)

	add_custom_target( codesign DEPENDS ${PROGNAME}
		COMMAND codesign -f -s \"$$SIGNCERT\"
			$<TARGET_FILE_DIR:${PROGNAME}>/*.*
			$<TARGET_FILE_DIR:${PROGNAME}>/../Frameworks/digidocpp.framework/Libraries/*
			$<TARGET_FILE_DIR:${PROGNAME}>/../Frameworks/*.framework
			$<TARGET_FILE_DIR:${PROGNAME}>/../PlugIns/*/*
			$<TARGET_FILE_DIR:${PROGNAME}>/../Library/QuickLook/DigiDocQL.qlgenerator
		COMMAND if echo \"$$SIGNCERT\" | grep -q "Developer ID" \; then
				codesign -f -s \"$$SIGNCERT\" $<TARGET_FILE_DIR:${PROGNAME}>/../.. --entitlements ${CMAKE_SOURCE_DIR}/${PROGNAME}.eToken.entitlements\;
			else
				codesign -f -s \"$$SIGNCERT\" $<TARGET_FILE_DIR:${PROGNAME}>/../.. --entitlements ${CMAKE_SOURCE_DIR}/${PROGNAME}.entitlements\;
			fi
	)
	add_custom_target( productbuild DEPENDS ${PROGNAME}
		COMMAND productbuild --component ${CMAKE_CURRENT_BINARY_DIR}/${PROGNAME}.app /Applications
			--sign \"$$INSTCERT\" ${CMAKE_BINARY_DIR}/qdigidoc4-store_${VERSION}$ENV{VER_SUFFIX}.pkg
	)
	add_custom_target( zip DEPENDS ${PROGNAME}
		COMMAND zip --symlinks -r -D ${CMAKE_BINARY_DIR}/qdigidoc4_${VERSION}$ENV{VER_SUFFIX}.zip ${PROGNAME}.app
	)
	add_custom_target( zipdebug DEPENDS ${PROGNAME}
		COMMAND dsymutil -o ${PROGNAME}.dSYM $<TARGET_FILE:${PROGNAME}>
		COMMAND zip -r ${CMAKE_BINARY_DIR}/qdigidoc4-dbg_${VERSION}$ENV{VER_SUFFIX}.zip ${PROGNAME}.dSYM
	)
elseif(WIN32)
	target_sources( ${PROGNAME} PRIVATE QCSP.cpp QCNG.cpp QWin.cpp CertStore.cpp Diagnostics_win.cpp)
	find_package( Qt5 COMPONENTS WinExtras )
	target_link_libraries( ${PROGNAME} Qt5::WinExtras NCrypt Crypt32 Cryptui )
	get_filename_component(LIBS_PATH ${LIBDIGIDOCPP_LIBRARIES} DIRECTORY)
	if(CMAKE_SIZEOF_VOID_P EQUAL 8)
		set(PLATFORM x64)
		set(SSL_PATH "C:/OpenSSL-Win64/bin")
		set(OPENSSL_SUFFIX "-x64")
	else()
		set(PLATFORM x86)
		set(SSL_PATH "C:/OpenSSL-Win32/bin")
		set(OPENSSL_SUFFIX "")
	endif()
	if(OPENSSL_ROOT_DIR)
		set(SSL_PATH "${OPENSSL_ROOT_DIR}/bin")
	endif()
	set(MSI_FILE "Digidoc4_Client-${VERSION}$ENV{VER_SUFFIX}.${PLATFORM}")
	list(APPEND CANDLE_CMD "$ENV{WIX}bin\\candle.exe" -nologo -arch ${PLATFORM} -dMSI_VERSION=${VERSION} -dPlatform=${PLATFORM}
		-dssl_path="${SSL_PATH}" -dlibs_path="${LIBS_PATH}" -dclient_path=$<TARGET_FILE:${PROGNAME}> -dqtconf=${CMAKE_SOURCE_DIR}/qt.conf
		-dschemasLocation=${LIBS_PATH}/schema SchemasFragment.wxs ${CMAKE_SOURCE_DIR}/qdigidoc4.wxs
		${CMAKE_SOURCE_DIR}/cmake/modules/WelcomeDlg2.wxs ${CMAKE_SOURCE_DIR}/cmake/modules/WixUI_Minimal2.wxs)
	list(APPEND LIGHT_CMD "$ENV{WIX}bin\\light.exe" -nologo -ext WixUIExtension
		qdigidoc4.wixobj SchemasFragment.wixobj WelcomeDlg2.wixobj WixUI_Minimal2.wixobj
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
		COMMAND ${LIGHT_CMD} -loc ${CMAKE_SOURCE_DIR}/qdigidoc4.en-US.wxl -cultures:en-US -o "${MSI_FILE}.en-US.msi"
		COMMAND ${LIGHT_CMD} -loc ${CMAKE_SOURCE_DIR}/qdigidoc4.et-EE.wxl -cultures:et-EE -o "${MSI_FILE}.et-EE.msi"
		#Build MSI with QT
		COMMAND ${CANDLE_CMD} -dqt_path=${_qt5Core_install_prefix}
		COMMAND ${LIGHT_CMD} -loc ${CMAKE_SOURCE_DIR}/qdigidoc4.en-US.wxl -cultures:en-US -o "${MSI_FILE}.en-US.qt.msi"
		COMMAND ${LIGHT_CMD} -loc ${CMAKE_SOURCE_DIR}/qdigidoc4.et-EE.wxl -cultures:et-EE -o "${MSI_FILE}.et-EE.qt.msi"
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
	configure_file(${CMAKE_SOURCE_DIR}/AppxManifest.xml.cmake ${CMAKE_BINARY_DIR}/AppxManifest.xml)
	add_custom_target(appx DEPENDS ${PROGNAME}
		COMMAND ${CMAKE_COMMAND} -E remove ${MSI_FILE}.appx
		COMMAND ${CMAKE_COMMAND} -E remove_directory appx
		COMMAND ${CMAKE_COMMAND} -E make_directory appx
		COMMAND ${CMAKE_COMMAND} -E copy AppxManifest.xml appx
		COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${PROGNAME}> appx
		COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/qt.conf appx
		COMMAND ${CMAKE_COMMAND} -E copy ${LIBS_PATH}/798.p12 appx
		COMMAND ${CMAKE_COMMAND} -E copy ${LIBS_PATH}/libcrypto-1_1${OPENSSL_SUFFIX}.dll appx
		COMMAND ${CMAKE_COMMAND} -E copy ${LIBS_PATH}/libssl-1_1${OPENSSL_SUFFIX}.dll appx
		COMMAND ${CMAKE_COMMAND} -E copy ${LIBS_PATH}/zlib1.dll appx
		COMMAND ${CMAKE_COMMAND} -E copy ${LIBS_PATH}/xerces-c_3_2.dll appx
		COMMAND ${CMAKE_COMMAND} -E copy ${LIBS_PATH}/XalanMessages_1_11.dll appx
		COMMAND ${CMAKE_COMMAND} -E copy ${LIBS_PATH}/Xalan-C_1_11.dll appx
		COMMAND ${CMAKE_COMMAND} -E copy ${LIBS_PATH}/xsec_2_0.dll appx
		COMMAND ${CMAKE_COMMAND} -E copy ${LIBS_PATH}/digidocpp.dll appx
		COMMAND ${CMAKE_COMMAND} -E copy ${LIBS_PATH}/digidocpp.conf appx
		COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/Assets appx/Assets
		COMMAND ${CMAKE_COMMAND} -E copy_directory ${LIBS_PATH}/schema appx/schema
		COMMAND ${_qt5Core_install_prefix}/bin/windeployqt --no-translations --no-compiler-runtime --no-patchqt appx
		COMMAND ${CMAKE_COMMAND} -E remove_directory appx/bearer
		COMMAND ${CMAKE_COMMAND} -E remove_directory appx/iconengines
		COMMAND ${CMAKE_COMMAND} -E remove_directory appx/imageformats
		COMMAND ${CMAKE_COMMAND} -E remove_directory appx/styles
		WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
	)
	if(EXISTS ${LIBS_PATH}/../certs)
		add_custom_command(TARGET appx POST_BUILD
			COMMAND ${CMAKE_COMMAND} -E copy_directory ${LIBS_PATH}/../certs appx/certs
			COMMAND del "appx\\certs\\TEST*.crt"
			COMMAND ${CMAKE_COMMAND} -E copy ${LIBS_PATH}/libxml2.dll appx
			COMMAND ${CMAKE_COMMAND} -E copy ${LIBS_PATH}/digidoc.dll appx
			COMMAND ${CMAKE_COMMAND} -E copy ${LIBS_PATH}/digidoc.ini appx
			WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
		)
	endif()
	add_custom_command(TARGET appx POST_BUILD
		COMMAND makeappx.exe pack -d appx -p ${MSI_FILE}.appx
		# https://msdn.microsoft.com/en-us/library/windows/desktop/jj835832(v=vs.85).aspx
		#Popups GUI
		#COMMAND MakeCert.exe /n "CN=8BBBE4D8-620A-4884-A12A-72F1A2030D8B" /r /h 0 /eku "1.3.6.1.5.5.7.3.3,1.3.6.1.4.1.311.10.3.13" /m 120 /a sha256 /sv qdigidoc4.pvk qdigidoc4.cer
		#COMMAND Pvk2Pfx.exe /pvk qdigidoc4.pvk /spc qdigidoc4.cer /pfx qdigidoc4.pfx /f
		COMMAND signtool.exe sign -f ${CMAKE_SOURCE_DIR}/qdigidoc4.pfx -fd SHA256 -v ${MSI_FILE}.appx
		WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
	)
else()
	find_package( Threads REQUIRED )
	target_sources( ${PROGNAME} PRIVATE Diagnostics_unix.cpp)
	target_link_libraries( ${PROGNAME} Threads::Threads )
	configure_file( qdigidoc4.1.cmake qdigidoc4.1 )
	install( TARGETS ${PROGNAME} DESTINATION ${CMAKE_INSTALL_BINDIR} )
	install( FILES ${CMAKE_CURRENT_BINARY_DIR}/qdigidoc4.1 DESTINATION ${CMAKE_INSTALL_MANDIR}/man1 )
	install( FILES qdigidoc4.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications )
	install( FILES qdigidoc4.xml DESTINATION ${CMAKE_INSTALL_DATADIR}/mime/packages )
	foreach(RES 16 22 32 48 128)
		foreach(TGT application-vnd.etsi.asic-e+zip.png application-vnd.etsi.asic-s+zip.png application-x-ddoc.png application-vnd.lt.archyvai.adoc-2008.png)
			install(
				FILES images/qdigidoc_client_document_${RES}x${RES}.png
				DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/${RES}x${RES}/mimetypes/
				RENAME ${TGT}
			)
		endforeach()
		install(
			FILES images/qdigidoc_crypto_document_${RES}x${RES}.png
			DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/${RES}x${RES}/mimetypes/
			RENAME application-x-cdoc.png
		)
		install(
			FILES images/digidoc_icon_${RES}x${RES}.png
			DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/${RES}x${RES}/apps/
			RENAME qdigidoc4.png
		)
	endforeach()
endif()
