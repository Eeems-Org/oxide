import QtQuick 2.9
import QtQuick.Controls 2.15
import "."

Button {
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
    property bool repeatOnHold: true
    property int holdInterval: 100
    property int key: 0
    property string value: ""
    property int size: 1
    property int basesize: this.keyboard ? this.keyboard.basesize : 90
    property int fontsize: 8
    property int borderWidth: 1

    signal click(Item item)
    signal hold(Item item)
    signal release(Item item)

    width: size * basesize + (size - 1) * 4
    height: basesize

    function getText() {
        if (keyboard && keyboard.hasShift && keyboard.hasAlt && keyboard.hasCtrl) {
            return shiftaltctrltext;
        }
        if (keyboard && keyboard.hasShift && keyboard.hasAlt) {
            return shiftalttext;
        }
        if (keyboard && keyboard.hasShift && keyboard.hasCtrl) {
            return shiftctrltext;
        }
        if (keyboard && keyboard.hasAlt && keyboard.hasCtrl) {
            return altctrltext;
        }
        if (keyboard && keyboard.hasShift) {
            return shifttext;
        }
        if (keyboard && keyboard.hasAlt) {
            return alttext;
        }
        if (keyboard && keyboard.hasCtrl) {
            return ctrltext;
        }
        return text;
    }

    function doClick() {
        if (root.click(root) !== false) {
            var modifiers = Qt.NoModifier;
            if (keyboard && keyboard.hasShift) {
                modifiers = modifiers | Qt.ShiftModifier;
            }
            if (keyboard && keyboard.hasAlt) {
                modifiers = modifiers | Qt.AltModifier;
            }
            if (keyboard && keyboard.hasCtrl) {
                modifiers = modifiers | Qt.ControlModifier;
            }
            if (keyboard && keyboard.hasMeta) {
                modifiers = modifiers | Qt.MetaModifier;
            }
            if (root.key > 0) {
                handler.keyPress(root.key, modifiers, root.value);
            } else {
                var t = root.getText();
                if (t.length > 1) {
                    handler.stringPress(t, modifiers);
                } else {
                    handler.charPress(t, modifiers);
                }
            }
            if (!root.checkable && keyboard) {
                keyboard.hasShift = false;
                keyboard.hasAlt = false;
                keyboard.hasCtrl = false;
                keyboard.hasMeta = false;
            }
        }
    }

    text: getText()
    focusPolicy: Qt.NoFocus
    flat: true

    contentItem: Text {
        text: root.text
        font: root.font
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

    onClicked: root.doClick()

    Timer {
        id: timer
        repeat: true
        interval: root.holdInterval
        onTriggered: root.doClick()
    }
}
