import QtQuick 2.15
import QtQuick.Window 2.15
import "qrc:/codes.eeems.oxide"

OxideWindow{
    width: Screen.width
    height: Screen.width
    x: 0
    y: 0
    visible: true
    backgroundColor: "transparent"
    Shortcut{
        sequences: ["Alt+Tab"]
        context: Qt.ApplicationShortcut
        onActivated: controller.taskSwitcher()
    }
    Shortcut{
        sequences: ["Shift+Meta+S", "Shift+End+S", StandardKey.Print]
        context: Qt.ApplicationShortcut
        onActivated: controller.screenshot()
    }
    Shortcut{
        sequences: ["Ctrl+Shift+Esc", "Ctrl+Shift+1", "Launch0"]
        context: Qt.ApplicationShortcut
        onActivated: controller.processManager()
    }
    Shortcut{
        sequences: ["Meta+Backspace", "End+Backspace"]
        context: Qt.ApplicationShortcut
        onActivated: controller.back()
    }
    Shortcut{
        sequences: ["LogOff", "ScreenSaver", "End+L", "Meta+L"]
        context: Qt.ApplicationShortcut
        onActivated: controller.lock()
    }
    Shortcut{
        sequences: ["Terminal", "Ctrl+Alt+T"]
        context: Qt.ApplicationShortcut
        onActivated: controller.terminal()
    }
    Shortcut{
        sequences: ["End+Alt+4", "Alt+F4"]
        context: Qt.ApplicationShortcut
        onActivated: controller.close()
    }
}
