import QtQuick 2.9
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.0
import "."

Item {
    id: root
    clip: true
    property var keyboard: null
    property string shifttext: text
    property string alttext: text
    property string ctrltext: text
    property string metatext: text
    property string shiftalttext: shifttext
    property string shiftctrltext: shifttext
    property string shiftmetatext: shifttext
    property string altctrltext: alttext
    property string altmetatext: alttext
    property string ctrlmetatext: alttext
    property string shiftaltctrltext: shiftalttext
    property string shiftaltmetatext: shiftalttext
    property string shiftctrlmetatext: shiftctrltext
    property string shiftaltctrlmetatext: shiftalttext
    property int key: 0
    property string value: ""
    property real size: 1
    property int basesize: keyboard.basesize || 90
    property int borderWidth: 1
    property alias font: button.font
    property alias text: button.text
    property alias checked: button.checked
    property alias checkable: button.checkable

    signal clicked

    onClicked: doClick()

    Layout.preferredWidth: (size * basesize) + 1
    Layout.preferredHeight: basesize + 1
    Layout.fillHeight: true
    Layout.leftMargin: -1
    Layout.topMargin: -1

    function getText() {
        if (!keyboard){
            return text
        }
        if ((keyboard.hasShift || keyboard.hasCaps) && keyboard.hasAlt && keyboard.hasCtrl) {
            return shiftaltctrltext;
        }
        if ((keyboard.hasShift || keyboard.hasCaps) && keyboard.hasAlt) {
            return shiftalttext;
        }
        if ((keyboard.hasShift || keyboard.hasCaps) && keyboard.hasCtrl) {
            return shiftctrltext;
        }
        if (keyboard.hasAlt && keyboard.hasCtrl) {
            return altctrltext;
        }
        if (keyboard.hasShift || keyboard.hasCaps) {
            return shifttext;
        }
        if (keyboard.hasAlt) {
            return alttext;
        }
        if (keyboard.hasCtrl) {
            return ctrltext;
        }
        return text;
    }

    function doClick() {
        if(!keyboard){
            return
        }
        var modifiers = Qt.NoModifier;
        if (keyboard.hasShift) {
            modifiers = modifiers | Qt.ShiftModifier;
        }
        if (keyboard.hasAlt) {
            modifiers = modifiers | Qt.AltModifier;
        }
        if (keyboard.hasCtrl) {
            modifiers = modifiers | Qt.ControlModifier;
        }
        if (keyboard.hasMeta) {
            modifiers = modifiers | Qt.MetaModifier;
        }
        if (root.key > 0) {
            handler.keyPress(root.key, modifiers, root.value);
        } else {
            var t = getText();
            if (t.length > 1) {
                handler.stringPress(t, modifiers);
            } else {
                handler.charPress(t, modifiers);
            }
        }
        if (!root.checkable) {
            keyboard.hasShift = false;
            keyboard.hasAlt = false;
            keyboard.hasCtrl = false;
            keyboard.hasMeta = false;
        }
    }

    Button {
        id: button
        anchors.fill: parent
        focusPolicy: Qt.NoFocus
        flat: true
        font.pixelSize: 24

        onClicked: root.clicked()
        autoRepeat: !checkable
        autoRepeatDelay: 300
        autoRepeatInterval: 100

        contentItem: Text {
            text: root.getText()
            font: button.font
            opacity: enabled ? 1.0 : 0.3
            color: "black"
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
        background: Rectangle {
            implicitWidth: 100
            implicitHeight: 40
            opacity: enabled ? 1 : 0.3
            color: "white"
            border.color: "black"
            border.width: root.borderWidth
            radius: 2
        }
    }
}
