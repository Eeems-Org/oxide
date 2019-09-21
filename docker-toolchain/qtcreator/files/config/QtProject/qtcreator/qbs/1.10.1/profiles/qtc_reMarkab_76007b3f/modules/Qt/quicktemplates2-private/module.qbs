import qbs 1.0
import '../QtModule.qbs' as QtModule

QtModule {
    qtModuleName: "QuickTemplates2"
    Depends { name: "Qt"; submodules: ["core", "gui", "quick"]}

    architecture: "arm"
    hasLibrary: true
    staticLibsDebug: []
    staticLibsRelease: []
    dynamicLibsDebug: []
    dynamicLibsRelease: ["/opt/poky/2.1.3/sysroots/cortexa9hf-neon-oe-linux-gnueabi/usr/lib/libQt5Quick.so.5.12.0", "/opt/poky/2.1.3/sysroots/cortexa9hf-neon-oe-linux-gnueabi/usr/lib/libQt5Gui.so.5.12.0", "/opt/poky/2.1.3/sysroots/cortexa9hf-neon-oe-linux-gnueabi/usr/lib/libQt5Qml.so.5.12.0", "/opt/poky/2.1.3/sysroots/cortexa9hf-neon-oe-linux-gnueabi/usr/lib/libQt5Network.so.5.12.0", "/opt/poky/2.1.3/sysroots/cortexa9hf-neon-oe-linux-gnueabi/usr/lib/libQt5Core.so.5.12.0", "pthread"]
    linkerFlagsDebug: []
    linkerFlagsRelease: []
    frameworksDebug: []
    frameworksRelease: []
    frameworkPathsDebug: []
    frameworkPathsRelease: []
    libNameForLinkerDebug: "Qt5QuickTemplates2"
    libNameForLinkerRelease: "Qt5QuickTemplates2"
    libFilePathDebug: ""
    libFilePathRelease: "/opt/poky/2.1.3/sysroots/cortexa9hf-neon-oe-linux-gnueabi/usr/lib/libQt5QuickTemplates2.so.5.12.0"
    cpp.defines: ["QT_QUICKTEMPLATES2_LIB"]
    cpp.includePaths: ["/opt/poky/2.1.3/sysroots/cortexa9hf-neon-oe-linux-gnueabi/usr/include/qt5", "/opt/poky/2.1.3/sysroots/cortexa9hf-neon-oe-linux-gnueabi/usr/include/qt5/QtQuickTemplates2", "/opt/poky/2.1.3/sysroots/cortexa9hf-neon-oe-linux-gnueabi/usr/include/qt5/QtQuickTemplates2/5.12.0", "/opt/poky/2.1.3/sysroots/cortexa9hf-neon-oe-linux-gnueabi/usr/include/qt5/QtQuickTemplates2/5.12.0/QtQuickTemplates2"]
    cpp.libraryPaths: []
    
}
