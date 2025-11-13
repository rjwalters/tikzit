# deploy the Mac app bundle. Note the bin/ directory
# of Qt should be in your PATH

# copy in libraries and set (most) library paths
macdeployqt tikzit.app

# macdeployqt doesn't fix the path to some libraries for some reason, so we do it by hand


cd ../../..

# create DMG
hdiutil create -volname TikZiT -srcfolder tikzit.app -ov -format UDZO tikzit.dmg


