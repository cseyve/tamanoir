#!/bin/bash
echo "Checkout in /tmp/..."
cd /tmp
svn checkout http://tamanoir.googlecode.com/svn/trunk/ tamanoir
cd tamanoir/

echo "Building with fakeroot..."
dpkg-buildpackage -uc -us -rfakeroot

