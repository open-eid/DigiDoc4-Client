# SPDX-FileCopyrightText: 2010 Erkko Kebbinau
# SPDX-License-Identifier: LGPL-2.1-or-later
import os
import gettext
import locale
import gi

try:
    # Python 3.
    from urllib.parse import unquote
except ImportError:
    from urllib import unquote

from gi.repository import Nautilus, GObject, Gio
if hasattr(Nautilus, "LocationWidgetProvider"):
     gi.require_version('Nautilus', '3.0')

class OpenDigidocExtension(GObject.GObject, Nautilus.MenuProvider):
    def __init__(self):
        super().__init__()

    def menu_activate_cb(self, menu, paths):
        args = "-sign "
        for path in paths:
            args += "\"%s\" " % path
        cmd = ("qdigidoc4 " + args + "&")
        os.system(cmd)

    def valid_file(self, file):
        return file.get_file_type() == Gio.FileType.REGULAR and file.get_uri_scheme() == 'file'

    def get_file_items(self, *args):
        paths = []
        files = args[-1]
        for file in files:
            if self.valid_file(file):
                path = unquote(file.get_uri()[7:])
                paths.append(path)

        if len(paths) < 1:
            return []

        APP = 'nautilus-qdigidoc'
        locale.setlocale(locale.LC_ALL, '')
        gettext.bindtextdomain(APP)
        gettext.textdomain(APP)

        item = Nautilus.MenuItem(
            name="OpenDigidocExtension::DigidocSigner",
            label=gettext.gettext('Sign digitally'),
            tip=gettext.ngettext('Sign selected file with DigiDoc4 Client',
                                 'Sign selected files with DigiDoc4 Client',
                                 len(paths)),
            icon='qdigidoc4'
        )
        item.connect('activate', self.menu_activate_cb, paths)
        return [item]

    def get_background_items(self, current_folder):
        return []

class OpenCryptoExtension(GObject.GObject, Nautilus.MenuProvider):
    def __init__(self):
        super().__init__()

    def menu_activate_cb(self, menu, paths):
        args = "-crypto "
        for path in paths:
            args += "\"%s\" " % path
        cmd = ("qdigidoc4 " + args + "&")
        os.system(cmd)

    def valid_file(self, file):
        return file.get_file_type() == Gio.FileType.REGULAR and file.get_uri_scheme() == 'file'

    def get_file_items(self, *args):
        paths = []
        files = args[-1]
        for file in files:
            if self.valid_file(file):
                path = unquote(file.get_uri()[7:])
                paths.append(path)

        if len(paths) < 1:
            return []

        APP = 'nautilus-qdigidoc'
        locale.setlocale(locale.LC_ALL, '')
        gettext.bindtextdomain(APP)
        gettext.textdomain(APP)

        item = Nautilus.MenuItem(
            name="OpenCryptoExtension::DigidocEncrypter",
            label=gettext.gettext('Encrypt files'),
            tip=gettext.ngettext('Encrypt selected file with DigiDoc4 Client',
                                 'Encrypt selected files with DigiDoc4 Client',
                                 len(paths)),
            icon='qdigidoc4'
        )
        item.connect('activate', self.menu_activate_cb, paths)
        return [item]

    def get_background_items(self, current_folder):
        return []
