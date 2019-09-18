import qbs 1.0
import '../QtModule.qbs' as QtModule

QtModule {
    qtModuleName: "OpenGLExtensions"
    Depends { name: "Qt"; submodules: ["core", "gui"]}

    architecture: "arm"
    hasLibrary: true
    staticLibsDebug: []
    staticLibsRelease: ["/opt/poky/2.1.3/sysroots/cortexa9hf-neon-poky-linux-gnueabi/usr/lib/libQt5Gui.so.5.6.2", "/opt/poky/2.1.3/sysroots/cortexa9hf-neon-poky-linux-gnueabi/usr/lib/libQt5Core.so.5.6.2", "pthread", "GLESv2"]
    dynamicLibsDebug: []
    dynamicLibsRelease: []
    linkerFlagsDebug: []
    linkerFlagsRelease: []
    frameworksDebug: []
    frameworksRelease: []
    frameworkPathsDebug: []
    frameworkPathsRelease: []
    libNameForLinkerDebug: "Qt5OpenGLExtensions"
    libNameForLinkerRelease: "Qt5OpenGLExtensions"
    libFilePathDebug: ""
    libFilePathRelease: "/opt/poky/2.1.3/sysroots/cortexa9hf-neon-poky-linux-gnueabi/usr/lib/libQt5OpenGLExtensions.a"
    cpp.defines: ["QT_OPENGLEXTENSIONS_LIB"]
    cpp.includePaths: ["/opt/poky/2.1.3/sysroots/cortexa9hf-neon-poky-linux-gnueabi/usr/include/qt5", "/opt/poky/2.1.3/sysroots/cortexa9hf-neon-poky-linux-gnueabi/usr/include/qt5/QtOpenGLExtensions"]
    cpp.libraryPaths: []
    isStaticLibrary: true
}
