#!/bin/bash
echo "Checkout in /tmp/..."
cd /tmp
#OLD VERSION, when it cas on GoogleCode:
# svn checkout http://tamanoir.googlecode.com/svn/trunk/ tamanoir
git clone https://github.com/cseyve/tamanoir.git
cd tamanoir/

echo "Building with fakeroot..."
dpkg-buildpackage -uc -us -rfakeroot

