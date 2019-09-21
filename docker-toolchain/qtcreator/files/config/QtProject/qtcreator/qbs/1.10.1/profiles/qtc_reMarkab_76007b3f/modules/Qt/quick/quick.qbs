/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing
**
** This file is part of Qbs.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms and
** conditions see http://www.qt.io/terms-conditions. For further information
** use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file.  Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, The Qt Company gives you certain additional
** rights.  These rights are described in The Qt Company LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
****************************************************************************/

import qbs
import qbs.Process
import qbs.FileInfo
import qbs.TextFile
import '../QtModule.qbs' as QtModule
import 'quick.js' as QC

QtModule {
    qtModuleName: "Quick"
    Depends { name: "Qt"; submodules: ["core", "gui", "qml"].concat("qml-private") }

    hasLibrary: true
    architecture: "arm"
    staticLibsDebug: []
    staticLibsRelease: []
    dynamicLibsDebug: []
    dynamicLibsRelease: ["/opt/poky/2.1.3/sysroots/cortexa9hf-neon-oe-linux-gnueabi/usr/lib/libQt5Qml.so.5.12.0", "/opt/poky/2.1.3/sysroots/cortexa9hf-neon-oe-linux-gnueabi/usr/lib/libQt5Gui.so.5.12.0", "/opt/poky/2.1.3/sysroots/cortexa9hf-neon-oe-linux-gnueabi/usr/lib/libQt5Network.so.5.12.0", "/opt/poky/2.1.3/sysroots/cortexa9hf-neon-oe-linux-gnueabi/usr/lib/libQt5Core.so.5.12.0", "pthread"]
    linkerFlagsDebug: []
    linkerFlagsRelease: []
    frameworksDebug: []
    frameworksRelease: []
    frameworkPathsDebug: []
    frameworkPathsRelease: []
    libNameForLinkerDebug: "Qt5Quick"
    libNameForLinkerRelease: "Qt5Quick"
    libFilePathDebug: ""
    libFilePathRelease: "/opt/poky/2.1.3/sysroots/cortexa9hf-neon-oe-linux-gnueabi/usr/lib/libQt5Quick.so.5.12.0"
    cpp.defines: {
        var result = ["QT_QUICK_LIB"];
        if (qmlDebugging)
            result.push("QT_QML_DEBUG");
        return result;
    }
    cpp.includePaths: ["/opt/poky/2.1.3/sysroots/cortexa9hf-neon-oe-linux-gnueabi/usr/include/qt5", "/opt/poky/2.1.3/sysroots/cortexa9hf-neon-oe-linux-gnueabi/usr/include/qt5/QtQuick"]
    cpp.libraryPaths: []
    property bool qmlDebugging: false
    property string qmlPath: "/opt/poky/2.1.3/sysroots/cortexa9hf-neon-oe-linux-gnueabi/usr/lib/qt5/qml"
    property string qmlImportsPath: "/opt/poky/2.1.3/sysroots/cortexa9hf-neon-oe-linux-gnueabi/usr/imports"
    property bool compilerAvailable: true
    Scanner {
        condition: compilerAvailable
        inputs: 'qt.quick.qrc'
        searchPaths: [FileInfo.path(input.filePath)]
        scan: QC.scanQrc(input.filePath)
    }

    FileTagger {
        condition: compilerAvailable
        patterns: "*.qrc"
        fileTags: ["qt.quick.qrc"]
        priority: 100
    }

    Rule {
        condition: compilerAvailable
        inputs: ["qt.quick.qrc"]
        Artifact {
            filePath: input.fileName + ".json"
            fileTags: ["qt.quick.qrcinfo"]
        }
        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.silent = true;
            cmd.sourceCode = function() {
                var content = QC.contentFromQrc(input.filePath);
                content.qrcFilePath = input.filePath;

                var tf = new TextFile(output.filePath, TextFile.WriteOnly);
                tf.write(JSON.stringify(content));
                tf.close();
            }
            return cmd;
        }
    }

    Rule {
        condition: compilerAvailable
        inputs: ["qt.quick.qrcinfo"]
        outputFileTags: ["cpp", "qrc"]
        multiplex: true
        outputArtifacts: {
            var infos = [];
            inputs["qt.quick.qrcinfo"].forEach(function (input) {
                var tf = new TextFile(input.filePath, TextFile.ReadOnly);
                infos.push(JSON.parse(tf.readAll()));
                tf.close();
            });

            var result = [];
            infos.forEach(function (info) {
                if (info.newQrcFileName) {
                    result.push({
                        filePath: info.newQrcFileName,
                        fileTags: ["qrc"]
                    });
                }
                info.qmlJsFiles.forEach(function (qmlJsFile) {
                    result.push({
                        filePath: qmlJsFile.output,
                        fileTags: ["cpp"]
                    });
                });
            });
            result.push({
                filePath: "qtquickcompiler_loader.cpp",
                fileTags: ["cpp"]
            });
            return result;
        }
        prepare: {
            var infos = [];
            inputs["qt.quick.qrcinfo"].forEach(function (input) {
                var tf = new TextFile(input.filePath, TextFile.ReadOnly);
                infos.push(JSON.parse(tf.readAll()));
                tf.close();
            });

            var cmds = [];
            var qmlCompiler = FileInfo.joinPaths(product.Qt.core.binPath,
                                                 "qtquickcompiler" + product.cpp.executableSuffix);
            var cmd;
            var loaderFlags = [];

            function findOutput(fileName) {
                for (var k in outputs) {
                    for (var i in outputs[k]) {
                        if (outputs[k][i].fileName === fileName)
                            return outputs[k][i];
                    }
                }
                throw new Error("Qt Quick compiler rule: Cannot find output artifact "
                                + fileName + ".");
            }

            infos.forEach(function (info) {
                if (info.newQrcFileName) {
                    loaderFlags.push("--resource-file-mapping="
                                     + FileInfo.fileName(info.qrcFilePath)
                                     + ":" + info.newQrcFileName);
                    cmd = new Command(qmlCompiler, ["--filter-resource-file",
                            info.qrcFilePath,
                            findOutput(info.newQrcFileName).filePath]);
                    cmd.description = "generating " + info.newQrcFileName;
                    cmds.push(cmd);
                } else {
                    loaderFlags.push("--resource-file-mapping=" + info.qrcFilePath);
                }
                info.qmlJsFiles.forEach(function (qmlJsFile) {
                    cmd = new Command(qmlCompiler, [
                            "--resource=" + info.qrcFilePath,
                            qmlJsFile.input,
                            findOutput(qmlJsFile.output).filePath]);
                    cmd.description = "generating " + qmlJsFile.output;
                    cmd.workingDirectory = FileInfo.path(info.qrcFilePath);
                    cmds.push(cmd);
                });
            });

            cmd = new Command(qmlCompiler, loaderFlags.concat(
                    infos.map(function (info) { return info.qrcFilePath; }),
                    findOutput("qtquickcompiler_loader.cpp").filePath
                    ));
            cmd.description = "generating loader source";
            cmds.push(cmd);
            return cmds;
        }
    }
}
