import QtQuick 2.15
import QtQuick.Window 2.3
import QtQuick.Controls 2.4
import QtQuick.Layouts 1.0
import "qrc:/codes.eeems.oxide"

OxideWindow{
    id: window
    objectName: "window"
    visible: true
    focus: true
    Component.onCompleted:mouseArea.forceActiveFocus()
    initialItem: Item{
        anchors.fill: parent
        focus: true
        Shortcut{
            sequences: [StandardKey.Quit, StandardKey.Cancel, Qt.Key_Backspace, "Ctrl+Q", "Ctrl+W"]
            context: Qt.ApplicationShortcut
            autoRepeat: false
            onActivated: Qt.quit()
        }
        Rectangle{
            color: "white"
            anchors.fill: parent
        }
        Label{
            text: "Hello World!"
            color: "black"
            anchors.centerIn: parent
        }
        MouseArea{
            id: mouseArea
            anchors.fill: parent
            onClicked: Qt.quit()
            TapHandler {
                onTapped: Qt.quit()
            }
        }
    }
}
