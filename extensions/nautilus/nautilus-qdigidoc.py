#
# QDigiDoc Nautilus Extension
#
# Copyright (C) 2010  Erkko Kebbinau
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc, 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
#
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

APP = 'nautilus-qdigidoc'

locale.setlocale(locale.LC_ALL, '')
gettext.bindtextdomain(APP)
gettext.textdomain(APP)

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
