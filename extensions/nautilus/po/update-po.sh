#!/bin/sh
# SPDX-FileCopyrightText: 2010 Erkko Kebbinau
# SPDX-License-Identifier: LGPL-2.1-or-later
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc, 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

# Generate .pot file:
pushd ..
xgettext -k_ -kN_ nautilus-qdigidoc.py --output=po/nautilus-qdigidoc.pot
popd

# Fix up charset
sed -i -e '/Content-Type/ s/CHARSET/UTF-8/' nautilus-qdigidoc.pot

# Update po files:
for f in *.po ; do
	msgmerge -U "$f" nautilus-qdigidoc.pot
done
