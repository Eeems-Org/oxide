import qbs 1.0
import qbs.FileInfo

Module {
    condition: !qbs.architecture || !architecture || architecture === qbs.architecture

    Depends { name: "cpp" }
    Depends { name: "Qt.core" }

    property string qtModuleName
    property path binPath: Qt.core.binPath
    property path incPath: Qt.core.incPath
    property path libPath: Qt.core.libPath
    property string qtLibInfix: Qt.core.libInfix
    property string libNameForLinkerDebug
    property string libNameForLinkerRelease
    property string libNameForLinker: Qt.core.qtBuildVariant === "debug"
                                      ? libNameForLinkerDebug : libNameForLinkerRelease
    property string libFilePathDebug
    property string libFilePathRelease
    property string libFilePath: Qt.core.qtBuildVariant === "debug"
                                 ? libFilePathDebug : libFilePathRelease
    version: Qt.core.version
    property bool hasLibrary: true
    property bool isStaticLibrary: false
    property bool isPlugin: false

    property string architecture
    property stringList staticLibsDebug
    property stringList staticLibsRelease
    property stringList dynamicLibsDebug
    property stringList dynamicLibsRelease
    property stringList linkerFlagsDebug
    property stringList linkerFlagsRelease
    property stringList staticLibs: Qt.core.qtBuildVariant === "debug"
                                    ? staticLibsDebug : staticLibsRelease
    property stringList dynamicLibs: Qt.core.qtBuildVariant === "debug"
                                    ? dynamicLibsDebug : dynamicLibsRelease
    property stringList frameworksDebug
    property stringList frameworksRelease
    property stringList frameworkPathsDebug
    property stringList frameworkPathsRelease
    property stringList mFrameworks: Qt.core.qtBuildVariant === "debug"
            ? frameworksDebug : frameworksRelease
    property stringList mFrameworkPaths: Qt.core.qtBuildVariant === "debug"
            ? frameworkPathsDebug: frameworkPathsRelease
    cpp.linkerFlags: Qt.core.qtBuildVariant === "debug"
            ? linkerFlagsDebug : linkerFlagsRelease

    Properties {
        condition: qtModuleName != undefined && hasLibrary
        cpp.staticLibraries: (isStaticLibrary ? [libFilePath] : []).concat(staticLibs)
        cpp.dynamicLibraries: (!isStaticLibrary && !Qt.core.frameworkBuild
                               ? [libFilePath] : []).concat(dynamicLibs)
        cpp.frameworks: mFrameworks.concat(!isStaticLibrary && Qt.core.frameworkBuild
                        ? [libNameForLinker] : [])
        cpp.frameworkPaths: mFrameworkPaths
    }
}
