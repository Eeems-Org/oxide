import qbs 1.0
import '../QtModule.qbs' as QtModule

QtModule {
    qtModuleName: "QuickTest"
    Depends { name: "Qt"; submodules: ["core", "testlib", "widgets"]}

    architecture: "arm"
    hasLibrary: true
    staticLibsDebug: []
    staticLibsRelease: []
    dynamicLibsDebug: []
    dynamicLibsRelease: ["/opt/poky/2.1.3/sysroots/cortexa9hf-neon-poky-linux-gnueabi/usr/lib/libQt5Test.so.5.6.2", "/opt/poky/2.1.3/sysroots/cortexa9hf-neon-poky-linux-gnueabi/usr/lib/libQt5Widgets.so.5.6.2", "/opt/poky/2.1.3/sysroots/cortexa9hf-neon-poky-linux-gnueabi/usr/lib/libQt5Gui.so.5.6.2", "/opt/poky/2.1.3/sysroots/cortexa9hf-neon-poky-linux-gnueabi/usr/lib/libQt5Core.so.5.6.2", "pthread"]
    linkerFlagsDebug: []
    linkerFlagsRelease: []
    frameworksDebug: []
    frameworksRelease: []
    frameworkPathsDebug: []
    frameworkPathsRelease: []
    libNameForLinkerDebug: "Qt5QuickTest"
    libNameForLinkerRelease: "Qt5QuickTest"
    libFilePathDebug: ""
    libFilePathRelease: "/opt/poky/2.1.3/sysroots/cortexa9hf-neon-poky-linux-gnueabi/usr/lib/libQt5QuickTest.so.5.6.2"
    cpp.defines: ["QT_QMLTEST_LIB"]
    cpp.includePaths: ["/opt/poky/2.1.3/sysroots/cortexa9hf-neon-poky-linux-gnueabi/usr/include/qt5", "/opt/poky/2.1.3/sysroots/cortexa9hf-neon-poky-linux-gnueabi/usr/include/qt5/QtQuickTest"]
    cpp.libraryPaths: []
    
}
