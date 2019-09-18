import qbs 1.0
import '../QtModule.qbs' as QtModule

QtModule {
    qtModuleName: "PlatformSupport"
    Depends { name: "Qt"; submodules: ["core-private", "gui-private"]}

    architecture: "arm"
    hasLibrary: true
    staticLibsDebug: []
    staticLibsRelease: ["/opt/poky/2.1.3/sysroots/cortexa9hf-neon-poky-linux-gnueabi/usr/lib/libQt5Gui.so.5.6.2", "/opt/poky/2.1.3/sysroots/cortexa9hf-neon-poky-linux-gnueabi/usr/lib/libQt5DBus.so.5.6.2", "/opt/poky/2.1.3/sysroots/cortexa9hf-neon-poky-linux-gnueabi/usr/lib/libQt5Core.so.5.6.2", "pthread", "udev", "fontconfig", "freetype", "EGL", "GLESv2"]
    dynamicLibsDebug: []
    dynamicLibsRelease: []
    linkerFlagsDebug: []
    linkerFlagsRelease: []
    frameworksDebug: []
    frameworksRelease: []
    frameworkPathsDebug: []
    frameworkPathsRelease: []
    libNameForLinkerDebug: "Qt5PlatformSupport"
    libNameForLinkerRelease: "Qt5PlatformSupport"
    libFilePathDebug: ""
    libFilePathRelease: "/opt/poky/2.1.3/sysroots/cortexa9hf-neon-poky-linux-gnueabi/usr/lib/libQt5PlatformSupport.a"
    cpp.defines: ["QT_PLATFORMSUPPORT_LIB"]
    cpp.includePaths: ["/opt/poky/2.1.3/sysroots/cortexa9hf-neon-poky-linux-gnueabi/usr/include/qt5", "/opt/poky/2.1.3/sysroots/cortexa9hf-neon-poky-linux-gnueabi/usr/include/qt5/QtPlatformSupport", "/opt/poky/2.1.3/sysroots/cortexa9hf-neon-poky-linux-gnueabi/usr/include/qt5/QtPlatformSupport/5.6.2", "/opt/poky/2.1.3/sysroots/cortexa9hf-neon-poky-linux-gnueabi/usr/include/qt5/QtPlatformSupport/5.6.2/QtPlatformSupport"]
    cpp.libraryPaths: []
    isStaticLibrary: true
}
