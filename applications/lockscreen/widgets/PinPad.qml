import QtQuick 2.0
import QtQuick.Controls 2.4
import QtQuick.Layouts 1.0
import "."

GridLayout {
    id: root
    property string value: ""
    property string label: "PIN"
    property string message: ""
    signal submit(string pin)

    anchors.centerIn: parent
    rowSpacing: children[3].width / 2
    columnSpacing: children[3].width / 2
    columns: 3
    rows: 6

    function buttonsEnabled(){ return value.length < 4; }
    function add(text){
        value += text;
        console.log(value);
        if(value.length < 4){
            return;
        }
        submit(value);
    }

    ColumnLayout {
        Layout.columnSpan: root.columns
        spacing: root.columnSpacing

        RowLayout {
            Item { Layout.fillWidth: true }
            Label {
                text: root.label
                color: "white"
            }
            Item { Layout.fillWidth: true }
        }

        RowLayout {
            spacing: root.columnSpacing
            property int itemSize: 50
            Layout.fillWidth: true

            Item { Layout.fillWidth: true }
            Rectangle {
                width: parent.itemSize
                height: width
                color: root.value.length > 0 ? "white" : "black"
                border.color: "white"
                border.width: 1
                radius: width / 2
            }
            Rectangle {
                width: parent.itemSize
                height: width
                color: root.value.length > 1 ? "white" : "black"
                border.color: "white"
                border.width: 1
                radius: width / 2
            }
            Rectangle {
                width: parent.itemSize
                height: width
                color: root.value.length > 2 ? "white" : "black"
                border.color: "white"
                border.width: 1
                radius: width / 2
            }
            Rectangle {
                width: parent.itemSize
                height: width
                color: root.value.length > 3 ? "white" : "black"
                border.color: "white"
                border.width: 1
                radius: width / 2
            }
            Item { Layout.fillWidth: true }
        }
    }
    RowLayout {
        Layout.columnSpan: root.columns
        Layout.fillHeight: true
        Item { Layout.fillWidth: true }
        Label {
            text: root.message
            color: "white"
        }
        Item { Layout.fillWidth: true }
    }

    PinButton { text: "1"; onClicked: root.add(text); enabled: root.buttonsEnabled() }
    PinButton { text: "2"; onClicked: root.add(text); enabled: root.buttonsEnabled() }
    PinButton { text: "3"; onClicked: root.add(text); enabled: root.buttonsEnabled() }

    PinButton { text: "4"; onClicked: root.add(text); enabled: root.buttonsEnabled() }
    PinButton { text: "5"; onClicked: root.add(text); enabled: root.buttonsEnabled() }
    PinButton { text: "6"; onClicked: root.add(text); enabled: root.buttonsEnabled() }

    PinButton { text: "7"; onClicked: root.add(text); enabled: root.buttonsEnabled() }
    PinButton { text: "8"; onClicked: root.add(text); enabled: root.buttonsEnabled() }
    PinButton { text: "9"; onClicked: root.add(text); enabled: root.buttonsEnabled() }

    Item { Layout.fillWidth: true }
    PinButton { text: "0"; onClicked: root.add(text); enabled: root.buttonsEnabled() }
    PinButton {
        contentItem: Item {
            Image {
                anchors.centerIn: parent
                width: parent.width / 2
                height: width
                source: "qrc:/img/backspace.png"
                fillMode: Image.PreserveAspectFit
            }
        }
        hideBorder: true
        onClicked: root.value = root.value.slice(0, -1)
        enabled: root.value.length
    }
}
