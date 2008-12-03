import os, subprocess
# Reference : http://www.siteduzero.com/forum-83-234070-p1-un-bundle-mac-pour-vos-applications-qt4.html

APPNAME = "Tamanoir-simple"
VERSION = "0.1"

def makedmg(d, volname,
            destdir='dist',
            internet_enable=True,
            format='UDBZ'):
    ''' Copy a directory d into a dmg named volname '''
    dmg = os.path.join(destdir, volname+'.dmg')
    if os.path.exists(dmg):
        os.unlink(dmg)
    subprocess.check_call(['hdiutil', 'create', '-srcfolder', os.path.abspath(d),
                            '-volname', volname, '-format', format, dmg])
    if internet_enable:
        subprocess.check_call(['hdiutil', 'internet-enable', '-yes', dmg])
    return dmg

makedmg(os.path.join("dist", APPNAME+'.app'), APPNAME+'-'+VERSION)
