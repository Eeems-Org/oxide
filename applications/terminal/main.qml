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
    title: qsTr("Terminal")

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        TerminalView {
            id: terminal
            Layout.fillWidth: true
            Layout.fillHeight: true
            focus: true
        }

        Keyboard {
            Layout.fillWidth: true
        }
    }
}
