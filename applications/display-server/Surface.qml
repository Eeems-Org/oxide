import QtQuick 2.15
import QtQuick.Window 2.15

Window {
    x: 0
    y: 0
    width: 100
    height: 100
    visible: true
    Rectangle{
        anchors.fill: parent
        color: "black"
    }
    Component.onCompleted: console.log("HELLO WORLD")
}
