import QtQuick 2.10
import QtQuick.Window 2.3
import QtQuick.Controls 2.4
import QtQuick.Layouts 1.0

Item {
    id: root
    clip: true
    width: height
    state: "released"
    signal clicked;
    signal longPress;
    property string source: "qrc:/img/icon.png"
    property string text: ""
    property bool bold: false
    property int itemPadding: 10
    Item {
        id: spacer
        height: root.height * 0.05
    }
    Image {
        id: icon
        fillMode: Image.PreserveAspectFit
        anchors.top: spacer.bottom
        height: root.height * 0.70
        width: root.width
        source: root.source
    }
    Text {
        text: root.text
        width: root.width
        height: root.height * 0.20
        font.pointSize: 100
        font.bold: root.bold
        font.family: "Noto Serif"
        font.italic: true
        minimumPointSize: 1
        anchors.top: icon.bottom
        fontSizeMode: Text.Fit
        verticalAlignment: Text.AlignVCenter
        horizontalAlignment: Text.AlignHCenter
    }
    MouseArea {
        anchors.fill: root
        enabled: root.enabled
        onClicked: {
            root.state = "released"
            root.clicked()
        }
        onPressAndHold: root.longPress()
        onPressed: root.state = "pressed"
        onReleased: root.state = "released"
        onCanceled: root.state = "released"
    }
    states: [
        State { name: "released" },
        State { name: "pressed" }
    ]
    transitions: [
        Transition {
            from: "pressed"; to: "released"
            ParallelAnimation {
                PropertyAction { target: image; property: "width"; value: root.width - root.itemPadding * 2 }
                PropertyAction { target: image; property: "height"; value: root.width - root.itemPadding * 2 }
            }
        },
        Transition {
            from: "released"; to: "pressed"
            ParallelAnimation {
                PropertyAction { target: image; property: "width"; value: image.width - 10}
                PropertyAction { target: image; property: "height"; value: image.height - 10 }
            }
        }
    ]
}
