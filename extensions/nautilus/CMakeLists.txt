find_package(Gettext)

file(GLOB PO_FILES po/*.po)
gettext_create_translations(po/nautilus-qdigidoc.pot ALL ${PO_FILES})

install(FILES nautilus-qdigidoc.py DESTINATION ${CMAKE_INSTALL_DATADIR}/nautilus-python/extensions/)
