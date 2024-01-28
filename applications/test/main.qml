import QtQuick 2.15
import QtQuick.Window 2.3
import QtQuick.Controls 2.4
import QtQuick.Layouts 1.0

Window{
    width: Screen.width
    height: Screen.height
    x: 0
    y: 0
    visible: true
    Component.onCompleted:{
        mouseArea.forceActiveFocus();
        console.log(activeFocusItem);
    }
    Page{
        anchors.fill: parent
        focus: true
        Keys.onPressed: (event) => console.log(event.key)
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
