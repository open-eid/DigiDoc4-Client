#!/bin/sh
# SPDX-FileCopyrightText: 2010 Erkko Kebbinau
# SPDX-License-Identifier: LGPL-2.1-or-later

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
