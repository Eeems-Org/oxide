import QtQuick 2.10
import QtQuick.Window 2.3
import QtQuick.Controls 2.4
import QtQuick.Layouts 1.0

Rectangle {
    Layout.fillHeight: true
    color: "transparent"
    Rectangle {
        color: "black"
        width: 1
        height: parent.height - 20
        anchors.centerIn: parent
    }
}
