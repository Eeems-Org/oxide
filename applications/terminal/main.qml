import QtQuick 2.15
import QtQuick.Window 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.0
import codes.eeems.oxide 3.0
import "qrc:/codes.eeems.oxide"
import Terminal 1.0

OxideWindow {
    id: window
    objectName: "window"
    visible: true
    focus: true
    title: terminal.title
    backgroundColor: "white"

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        TerminalView {
            id: terminal
            objectName: "terminal"
            Layout.fillWidth: true
            Layout.fillHeight: true
            focus: true
            Component.onCompleted:{
                console.log("Forcing focus to terminal")
                forceActiveFocus()
            }
            onActiveFocusChanged: {
                console.log("TerminalView activeFocus changed to", activeFocus)
                if(!activeFocus){
                    forceActiveFocus()
                }
            }
        }

        Keyboard {
            id: keyboard
            Layout.fillWidth: true
        }
    }
}
