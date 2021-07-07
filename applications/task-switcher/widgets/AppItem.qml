import QtQuick 2.10
import QtQuick.Window 2.3
import QtQuick.Controls 2.4
import QtQuick.Layouts 1.0

Item {
    id: root
    clip: true
    width: height
    enabled: visible
    state: "released"
    signal clicked;
    signal longPress;
    property string source: "qrc:/img/icon.png"
    property string text: ""
    property bool animating: false
    property bool clickOnDone: false

    Item {
        id: spacer
        height: root.height * 0.05
        width: 0
    }
    Image {
        id: icon
        fillMode: Image.PreserveAspectFit
        anchors.top: spacer.bottom
        anchors.left: spacer.right
        height: root.height * (root.text ? 0.70 : 0.90)
        width: root.width
        source: root.source
        sourceSize.width: width
        sourceSize.height: height
    }
    Text {
        text: root.text
        width: root.width
        height: root.text ? root.height * 0.20 : 0
        font.pointSize: 100
        minimumPointSize: 1
        anchors.top: icon.bottom
        anchors.left: icon.left
        fontSizeMode: Text.Fit
        verticalAlignment: Text.AlignVCenter
        horizontalAlignment: Text.AlignHCenter
    }
    MouseArea {
        anchors.fill: root
        enabled: root.enabled
        onClicked: root.clicked()
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
                PropertyAction { target: icon; property: "width"; value: root.width * 0.90 }
                PropertyAction { target: icon; property: "height"; value: root.height * 0.70 }
            }
        },
        Transition {
            from: "released"; to: "pressed"
            ParallelAnimation {
                PropertyAction { target: icon; property: "width"; value: icon.width - 10}
                PropertyAction { target: icon; property: "height"; value: icon.height - 10 }
            }
        }
    ]
}
