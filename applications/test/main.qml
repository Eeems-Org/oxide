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
        RowLayout{
            id: buttons
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            Button{
                text: "Black Pen"
                onClicked: canvas.brush = Oxide.brushFromColor("black")
            }
            Button{
                text: "White Pen"
                onClicked: canvas.brush = Oxide.brushFromColor("white")
            }
            Item{
                Layout.fillWidth: true
            }
            Button{
                text: "3px"
                onClicked: canvas.penWidth = 3
            }
            Button{
                text: "6px"
                onClicked: canvas.penWidth = 6
            }
            Button{
                text: "12px"
                onClicked: canvas.penWidth = 12
            }
            Button{
                text: "24px"
                onClicked: canvas.penWidth = 24
            }
        }
        Canvas{
            id: canvas
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: buttons.bottom
            anchors.bottom: parent.bottom
        }
        Label{
            text: "Ctrl-Q, Ctrl-W, Backspace, or Escape to quit"
            color: "black"
            anchors.centerIn: parent
        }
    }
}
