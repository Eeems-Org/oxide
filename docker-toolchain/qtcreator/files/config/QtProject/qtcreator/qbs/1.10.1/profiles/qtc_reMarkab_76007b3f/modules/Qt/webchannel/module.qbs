import qbs 1.0
import '../QtModule.qbs' as QtModule

QtModule {
    qtModuleName: "WebChannel"
    Depends { name: "Qt"; submodules: ["core", "qml"]}

    architecture: "arm"
    hasLibrary: true
    staticLibsDebug: []
    staticLibsRelease: []
    dynamicLibsDebug: []
    dynamicLibsRelease: ["/opt/poky/2.1.3/sysroots/cortexa9hf-neon-oe-linux-gnueabi/usr/lib/libQt5Qml.so.5.12.0", "/opt/poky/2.1.3/sysroots/cortexa9hf-neon-oe-linux-gnueabi/usr/lib/libQt5Network.so.5.12.0", "/opt/poky/2.1.3/sysroots/cortexa9hf-neon-oe-linux-gnueabi/usr/lib/libQt5Core.so.5.12.0", "pthread"]
    linkerFlagsDebug: []
    linkerFlagsRelease: []
    frameworksDebug: []
    frameworksRelease: []
    frameworkPathsDebug: []
    frameworkPathsRelease: []
    libNameForLinkerDebug: "Qt5WebChannel"
    libNameForLinkerRelease: "Qt5WebChannel"
    libFilePathDebug: ""
    libFilePathRelease: "/opt/poky/2.1.3/sysroots/cortexa9hf-neon-oe-linux-gnueabi/usr/lib/libQt5WebChannel.so.5.12.0"
    cpp.defines: ["QT_WEBCHANNEL_LIB"]
    cpp.includePaths: ["/opt/poky/2.1.3/sysroots/cortexa9hf-neon-oe-linux-gnueabi/usr/include/qt5", "/opt/poky/2.1.3/sysroots/cortexa9hf-neon-oe-linux-gnueabi/usr/include/qt5/QtWebChannel"]
    cpp.libraryPaths: []
    
}
