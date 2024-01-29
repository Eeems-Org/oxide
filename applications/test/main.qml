import QtQuick 2.15
import QtQuick.Window 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.0
import codes.eeems.oxide 2.8
import "qrc:/codes.eeems.oxide"

OxideWindow{
    id: window
    objectName: "window"
    visible: true
    focus: true
    Shortcut{
        sequences: [StandardKey.Quit, StandardKey.Cancel, "Backspace", "Ctrl+Q", "Ctrl+W"]
        context: Qt.ApplicationShortcut
        autoRepeat: false
        onActivated: Qt.quit()
    }
    leftMenu: [
        Button{
            text: "Click to Quit"
            onClicked: Qt.quit()
        }
    ]
    centerMenu: [
        Label{
            color: "white"
            text: "Hello World!"
        }

    ]
    rightMenu: [
        Label{
            color: "white"
            text: "Tap to Quit"
            Layout.leftMargin: 10
            Layout.rightMargin: 10
            TapHandler {
                onTapped: Qt.quit()
            }
        }
    ]
    initialItem: Item{
        anchors.fill: parent
        focus: true
        Rectangle{
            color: "white"
            anchors.fill: parent
        }
        Canvas{
            anchors.fill: parent
            penWidth: 3
        }
        Label{
            text: "Ctrl-Q, Ctrl-W, Backspace, or Escape to quit"
            color: "black"
            anchors.centerIn: parent
        }
    }
}
