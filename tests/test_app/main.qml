import QtQuick 2.15
import QtQuick.Window 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.0
import codes.eeems.oxide 3.0
import "qrc:/codes.eeems.oxide"

OxideWindow{
    id: window
    objectName: "window"
    visible: true
    focus: true
    property string currentColor: "black"
    property int currentWidth: 6
    Component.onCompleted: updatePen()
    function updatePen() {
        canvas.setPen(Oxide.createPen(
            Oxide.brushFromColor(currentColor, Qt.SolidPattern),
            currentWidth,
            Qt.SolidLine,
            Qt.RoundCap,
            Qt.BevelJoin
        ));
    }
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
            text: "Ctrl-Q, Ctrl-W, Backspace, or Escape to quit"
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
        component MyButton: Button{
            id: control
            contentItem: Text {
                text: control.text
                font: control.font
                color: control.enabled ? "black" : "white"
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
            }
            background: Rectangle{
                color: control.enabled ? "white" : "black"
                border.color: "black"
                border.width: 1
                radius: 2
            }
        }
        RowLayout{
            id: buttons
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right

            MyButton{
                text: "Black Pen"
                enabled: window.currentColor !== "black"
                onClicked: {
                    window.currentColor = "black"
                    window.updatePen()
                }
            }
            MyButton{
                text: "White Pen"
                enabled: window.currentColor !== "white"
                onClicked: {
                    window.currentColor = "white"
                    window.updatePen()
                }
            }
            MyButton{
                text: "Red Pen"
                enabled: window.currentColor !== "red"
                onClicked: {
                    window.currentColor = "red"
                    window.updatePen()
                }
            }
            Item{
                Layout.fillWidth: true
            }
            MyButton{
                text: "3px"
                enabled: window.currentWidth !== 3
                onClicked: {
                    window.currentWidth = 3
                    window.updatePen()
                }
            }
            MyButton{
                text: "6px"
                enabled: window.currentWidth !== 6
                onClicked: {
                    window.currentWidth = 6
                    window.updatePen()
                }
            }
            MyButton{
                text: "12px"
                enabled: window.currentWidth !== 12
                onClicked: {
                    window.currentWidth = 12
                    window.updatePen()
                }
            }
            MyButton{
                text: "24px"
                enabled: window.currentWidth !== 24
                onClicked: {
                    window.currentWidth = 24
                    window.updatePen()
                }
            }
        }
        OxideCanvas{
            id: canvas
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: buttons.bottom
            anchors.bottom: parent.bottom
        }
    }
}
