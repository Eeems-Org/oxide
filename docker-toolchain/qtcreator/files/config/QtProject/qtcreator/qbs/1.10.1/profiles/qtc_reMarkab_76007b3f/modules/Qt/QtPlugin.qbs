import qbs 1.0
import qbs.FileInfo
import qbs.TextFile

QtModule {
    isPlugin: true

    property string className

    Rule {
        condition: isStaticLibrary
        multiplex: true
        Artifact {
            filePath: product.targetName + "_qt_plugin_import_"
                      + product.moduleProperty(product.moduleName, "qtModuleName") + ".cpp"
            fileTags: "cpp"
        }

        prepare: {
            var cmd = new JavaScriptCommand();
            var pluginName = product.moduleProperty(product.moduleName, "qtModuleName");
            cmd.description = "Creating static import for plugin '" + pluginName + "'.";
            cmd.sourceCode = function() {
                var f = new TextFile(output.filePath, TextFile.WriteOnly);
                var className = product.moduleProperty(product.moduleName, "className");
                f.writeLine("#include <QtPlugin>\n\nQ_IMPORT_PLUGIN(" + className + ")");
                f.close();
            };
            return cmd;
        }
    }
}
