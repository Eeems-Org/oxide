import qbs 1.0
import '../QtModule.qbs' as QtModule

QtModule {
    qtModuleName: "WebSockets"
    Depends { name: "Qt"; submodules: ["core", "network"]}

    architecture: "arm"
    hasLibrary: true
    staticLibsDebug: []
    staticLibsRelease: []
    dynamicLibsDebug: []
    dynamicLibsRelease: ["/opt/poky/2.1.3/sysroots/cortexa9hf-neon-poky-linux-gnueabi/usr/lib/libQt5Network.so.5.6.2", "/opt/poky/2.1.3/sysroots/cortexa9hf-neon-poky-linux-gnueabi/usr/lib/libQt5Core.so.5.6.2", "pthread"]
    linkerFlagsDebug: []
    linkerFlagsRelease: []
    frameworksDebug: []
    frameworksRelease: []
    frameworkPathsDebug: []
    frameworkPathsRelease: []
    libNameForLinkerDebug: "Qt5WebSockets"
    libNameForLinkerRelease: "Qt5WebSockets"
    libFilePathDebug: ""
    libFilePathRelease: "/opt/poky/2.1.3/sysroots/cortexa9hf-neon-poky-linux-gnueabi/usr/lib/libQt5WebSockets.so.5.6.2"
    cpp.defines: ["QT_WEBSOCKETS_LIB"]
    cpp.includePaths: ["/opt/poky/2.1.3/sysroots/cortexa9hf-neon-poky-linux-gnueabi/usr/include/qt5", "/opt/poky/2.1.3/sysroots/cortexa9hf-neon-poky-linux-gnueabi/usr/include/qt5/QtWebSockets"]
    cpp.libraryPaths: []
    
}
