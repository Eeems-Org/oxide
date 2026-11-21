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

    width: size * basesize + (size - 1) * 4
    height: basesize
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

    focusPolicy: Qt.NoFocus
    flat: true

    contentItem: Text {
        text: getText()
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

    onClicked: doClick()
    onHold: {
        hold(this);
        repeatOnHold && timer.start();
    }
    onRelease: {
        release(this);
        repeatOnHold && timer.stop();
    }
    Timer {
        id: timer
        repeat: true
        interval: root.holdInterval
        onTriggered: root.doClick()
    }
}
