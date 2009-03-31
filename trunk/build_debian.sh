#!/bin/bash
echo "Checkout in /tmp/..."
cd /tmp
svn sv+ssh:...

echo "Building with fakeroot..."
dpkg-buildpackage -uc -us -rfakeroot

