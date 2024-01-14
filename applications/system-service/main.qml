import QtQuick 2.15
import QtQuick.Window 2.15

Window{
    width: Screen.width
    height: Screen.width
    x: 0
    y: 0
    opacity: 0
    visible: true
    Shortcut{
        sequences: ["Alt+Tab"]
        onActivated: controller.taskSwitcher()
    }
    Shortcut{
        sequences: ["Shift+Meta+S", "Shift+End+S", StandardKey.Print]
        onActivated: controller.screenshot()
    }
    Shortcut{
        sequences: ["Ctrl+Shift+Esc", "Ctrl+Shift+1", Qt.Key_Launch0]
        onActivated: controller.processManager()
    }
    Shortcut{
        sequences: ["Meta+Backspace", "End+Backspace"]
        onActivated: controller.back()
    }
    Shortcut{
        sequences: [Qt.Key_Suspend, "Ctrl+Shift+Alt+P+S"]
        onActivated: controller.suspend()
    }
    Shortcut{
        sequences: [Qt.Key_PowerDown, "Ctrl+Shift+Alt+P+O"]
        onActivated: controller.powerOff()
    }
    Shortcut{
        sequences: [Qt.Key_LogOff, Qt.Key_ScreenSaver, "End+L", "Meta+L"]
        onActivated: controller.lock()
    }
    Shortcut{
        sequences: [Qt.Key_Terminal, "Ctrl+Alt+T"]
        onActivated: controller.terminal()
    }
}
