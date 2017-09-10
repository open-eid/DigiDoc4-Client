set_app_name( PROGNAME qdigidoc4 )

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
qt5_wrap_ui( SOURCES MainWindow.ui widgets/Accordion.ui widgets/AccordionTitle.ui widgets/AddressItem.ui widgets/CardInfo.ui widgets/ContainerPage.ui widgets/FileItem.ui widgets/InfoStack.ui widgets/ItemList.ui widgets/NoCardInfo.ui widgets/PageIcon.ui widgets/SignatureItem.ui widgets/VerifyCert.ui )

if( APPLE )
	list( APPEND SOURCES Application_mac.mm )
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
	Application.cpp
	DigiDoc.cpp
	FileDialog.cpp
	MainWindow.cpp
	QPKCS11.cpp
	QSigner.cpp
	QSmartCard.cpp
	sslConnect.cpp
	Styles.cpp
	XmlReader.cpp
	common4/TokenData4.cpp
	widgets/Accordion.cpp
	widgets/AccordionTitle.cpp
	widgets/AddressItem.cpp
	widgets/CardInfo.cpp
	widgets/ContainerPage.cpp
	widgets/FadeInNotification.cpp
	widgets/FileItem.cpp
	widgets/ItemList.cpp
	widgets/ItemWidget.cpp
	widgets/InfoStack.cpp
	widgets/LabelButton.cpp
	widgets/NoCardInfo.cpp
	widgets/PageIcon.cpp
	widgets/SignatureItem.cpp
	widgets/VerifyCert.cpp
	)
add_manifest( ${PROGNAME} )
target_link_libraries( ${PROGNAME}
	qdigidoccommon
	Qt5::PrintSupport
	${LIBDIGIDOCPP_LIBRARY}
	${ADDITIONAL_LIBRARIES}
)

if( APPLE )
	set_target_properties( ${PROGNAME} PROPERTIES
		MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/mac/Info.plist.cmake
	)
	add_custom_command( TARGET ${PROGNAME} POST_BUILD
		COMMAND cp -a ${PKCS11_MODULE} $<TARGET_FILE_DIR:${PROGNAME}> )
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
endif()
