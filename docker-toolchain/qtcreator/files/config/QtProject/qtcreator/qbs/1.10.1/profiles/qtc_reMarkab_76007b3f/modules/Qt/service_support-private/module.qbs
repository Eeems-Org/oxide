import qbs 1.0
import '../QtModule.qbs' as QtModule

QtModule {
    qtModuleName: "ServiceSupport"
    Depends { name: "Qt"; submodules: ["core-private", "gui-private", "dbus"]}

    architecture: "arm"
    hasLibrary: true
    staticLibsDebug: []
    staticLibsRelease: ["/opt/poky/2.1.3/sysroots/cortexa9hf-neon-oe-linux-gnueabi/usr/lib/libQt5Gui.so.5.12.0", "/opt/poky/2.1.3/sysroots/cortexa9hf-neon-oe-linux-gnueabi/usr/lib/libQt5DBus.so.5.12.0", "/opt/poky/2.1.3/sysroots/cortexa9hf-neon-oe-linux-gnueabi/usr/lib/libQt5Core.so.5.12.0", "pthread"]
    dynamicLibsDebug: []
    dynamicLibsRelease: []
    linkerFlagsDebug: []
    linkerFlagsRelease: []
    frameworksDebug: []
    frameworksRelease: []
    frameworkPathsDebug: []
    frameworkPathsRelease: []
    libNameForLinkerDebug: "Qt5ServiceSupport"
    libNameForLinkerRelease: "Qt5ServiceSupport"
    libFilePathDebug: ""
    libFilePathRelease: "/opt/poky/2.1.3/sysroots/cortexa9hf-neon-oe-linux-gnueabi/usr/lib/libQt5ServiceSupport.a"
    cpp.defines: ["QT_SERVICE_SUPPORT_LIB"]
    cpp.includePaths: ["/opt/poky/2.1.3/sysroots/cortexa9hf-neon-oe-linux-gnueabi/usr/include/qt5", "/opt/poky/2.1.3/sysroots/cortexa9hf-neon-oe-linux-gnueabi/usr/include/qt5/QtServiceSupport", "/opt/poky/2.1.3/sysroots/cortexa9hf-neon-oe-linux-gnueabi/usr/include/qt5/QtServiceSupport/5.12.0", "/opt/poky/2.1.3/sysroots/cortexa9hf-neon-oe-linux-gnueabi/usr/include/qt5/QtServiceSupport/5.12.0/QtServiceSupport"]
    cpp.libraryPaths: []
    isStaticLibrary: true
}
