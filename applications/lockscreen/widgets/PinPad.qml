import QtQuick 2.0
import QtQuick.Controls 2.4
import QtQuick.Layouts 1.0
import "."

GridLayout {
    id: root
    focus: true
    property string value: ""
    property string label: "PIN"
    property string message: ""
    property bool showPress: true
    property alias buttonsVisible: buttons.visible
    property color itemTextColor: "black"
    property color itemActiveColor: itemTextColor
    property color itemBackgroundColor: "white"
    property color itemActiveBackgroundColor: itemBackgroundColor
    property color itemBorderColor: itemTextColor
    property int itemBorderWidth: 2
    signal submit(string pin)

    anchors.centerIn: parent
    rowSpacing: buttons.rowSpacing
    columnSpacing: buttons.columnSpacing
    columns: 3
    rows: 6

    function buttonsEnabled(){ return !submitTimer.running && value.length < 4; }
    function add(text){
        if(!buttonsEnabled()){
            return;
        }
        value += text;
        if(value.length < 4){
            return;
        }
        submitTimer.start();
    }
    function keyPress(event){ event.accepted = true; }

    function keyRelease(event){
        switch(event.key){
            case Qt.Key_0:
                button0.clicked();
                break;
            case Qt.Key_1:
                button1.clicked();
                break;
            case Qt.Key_2:
                button2.clicked();
                break;
            case Qt.Key_3:
                button3.clicked();
                break;
            case Qt.Key_4:
                button4.clicked();
                break;
            case Qt.Key_5:
                button5.clicked();
                break;
            case Qt.Key_6:
                button6.clicked();
                break;
            case Qt.Key_7:
                button7.clicked();
                break;
            case Qt.Key_8:
                button8.clicked();
                break;
            case Qt.Key_9:
                button9.clicked();
                break;
            case Qt.Key_Backspace:
                buttonBackspace.clicked();
                break;
        }
        event.accepted = true;
    }

    Timer {
        id: submitTimer
        repeat: false
        interval: 100
        onTriggered: {
            submit(value)
        }
    }

    ColumnLayout {
        Layout.columnSpan: root.columns
        spacing: root.columnSpacing

        RowLayout {
            Item { Layout.fillWidth: true }
            Label {
                text: root.label
                color: root.itemTextColor
            }
            Item { Layout.fillWidth: true }
        }

        RowLayout {
            id: display
            spacing: root.columnSpacing
            property int itemSize: 50
            Layout.fillWidth: true

            Item { Layout.fillWidth: true }
            Rectangle {
                width: parent.itemSize
                height: width
                color: root.value.length > 0 ? root.itemTextColor : root.itemBackgroundColor
                border.color: root.itemBorderColor
                border.width: root.itemBorderWidth
                radius: width / 2
            }
            Rectangle {
                width: parent.itemSize
                height: width
                color: root.value.length > 1 ? root.itemTextColor : root.itemBackgroundColor
                border.color: root.itemBorderColor
                border.width: root.itemBorderWidth
                radius: width / 2
            }
            Rectangle {
                width: parent.itemSize
                height: width
                color: root.value.length > 2 ? root.itemTextColor : root.itemBackgroundColor
                border.color: root.itemBorderColor
                border.width: root.itemBorderWidth
                radius: width / 2
            }
            Rectangle {
                width: parent.itemSize
                height: width
                color: root.value.length > 3 ? root.itemTextColor : root.itemBackgroundColor
                border.color: root.itemBorderColor
                border.width: root.itemBorderWidth
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
            color: root.itemTextColor
        }
        Item { Layout.fillWidth: true }
    }
    GridLayout {
        id: buttons
        rowSpacing: children[3].width / 2
        columnSpacing: children[3].width / 2
        Layout.columnSpan: 3
        Layout.rowSpan: 4
        columns: 3
        rows: 4

        property alias itemTextColor: root.itemTextColor
        property alias itemActiveColor: root.itemTextColor
        property alias itemBackgroundColor: root.itemBackgroundColor
        property alias itemActiveBackgroundColor: root.itemActiveBackgroundColor
        property alias itemBorderColor: root.itemBorderColor
        property alias itemBorderWidth: root.itemBorderWidth

        PinButton { id: button1; text: "1"; onClicked: root.add(text); enabled: root.buttonsEnabled(); showPress: root.showPress }
        PinButton { id: button2; text: "2"; onClicked: root.add(text); enabled: root.buttonsEnabled(); showPress: root.showPress }
        PinButton { id: button3; text: "3"; onClicked: root.add(text); enabled: root.buttonsEnabled(); showPress: root.showPress }

        PinButton { id: button4; text: "4"; onClicked: root.add(text); enabled: root.buttonsEnabled(); showPress: root.showPress }
        PinButton { id: button5; text: "5"; onClicked: root.add(text); enabled: root.buttonsEnabled(); showPress: root.showPress }
        PinButton { id: button6; text: "6"; onClicked: root.add(text); enabled: root.buttonsEnabled(); showPress: root.showPress }

        PinButton { id: button7; text: "7"; onClicked: root.add(text); enabled: root.buttonsEnabled(); showPress: root.showPress }
        PinButton { id: button8; text: "8"; onClicked: root.add(text); enabled: root.buttonsEnabled(); showPress: root.showPress }
        PinButton { id: button9; text: "9"; onClicked: root.add(text); enabled: root.buttonsEnabled(); showPress: root.showPress }

        Item { Layout.fillWidth: true }
        PinButton { id: button0; text: "0"; onClicked: root.add(text); enabled: root.buttonsEnabled(); showPress: root.showPress }
        PinButton {
            id: buttonBackspace
            contentItem: Item {
                Image {
                    anchors.centerIn: parent
                    width: parent.width / 2
                    height: width
                    source: "qrc:/codes.eeems.oxide/img/backspace.png"
                    fillMode: Image.PreserveAspectFit
                }
            }
            hideBorder: true
            onClicked: root.value = root.value.slice(0, -1)
            enabled: root.value.length
            showPress: root.showPress
        }
    }
    Keys.onPressed: keyPress
}
