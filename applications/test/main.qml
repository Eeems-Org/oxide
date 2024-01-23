import QtQuick 2.10
import QtQuick.Window 2.3
import QtQuick.Controls 2.4
import QtQuick.Layouts 1.0

Window{
    width: Screen.width
    height: Screen.height
    x: 0
    y: 0
    visible: true
    Rectangle{
        color: "white"
        anchors.fill: parent
    }
    Label{
        text: "Hello World!"
        color: "black"
        anchors.centerIn: parent
    }
}
