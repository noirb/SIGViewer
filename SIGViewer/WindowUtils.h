#ifndef _SIGVIEWER_WINDOW_UTILS
#define _SIGVIEWER_WINDOW_UTILS

#include <Windows.h>

// a place for dirty hacks
// In a perfect world none of these would be necessary

namespace WindowHacks
{
    // searches for a window containing 'title_part' in in its window title
    // Returns a handle if the window is found, NULL otherwise
    HWND FindWindowByName(char* title_part)
    {
        // Get root window
        HWND rootWindow = FindWindowEx(NULL, NULL, NULL, NULL);
        HWND window = rootWindow;
        TCHAR windowtext[MAX_PATH];
        TCHAR consoletitle[MAX_PATH];
        GetConsoleTitle(consoletitle, MAX_PATH);

        while (1)
        {
            // Check window title for a match
            GetWindowText(window, windowtext, MAX_PATH);
            if (strstr(windowtext, title_part) != NULL &&
                strcmp(windowtext, consoletitle) != 0) // don't match ourself
            {
                break;
            }

            // Get next window...
            window = FindWindowEx(NULL, window, NULL, NULL);
            if (window == NULL || window == rootWindow)
            {
                return NULL; // desired window could not be found
            }
        }
        return window;
    }

    bool SendKeyStroke(HWND target_window, char key)
    {
        HWND myWindow = GetActiveWindow();
        INPUT *keystroke;
        UINT keystrokes_to_send, keystrokes_sent;

        if (target_window == NULL)
        {
            return false;
        }

        if (!SetForegroundWindow(target_window))
        {
            return false;
        }

        keystrokes_to_send = 2; // one down, one up
        keystroke = new INPUT[keystrokes_to_send];

        keystroke[0].type = INPUT_KEYBOARD;
        keystroke[0].ki.wVk = 0;
        keystroke[0].ki.wScan = key;
        keystroke[0].ki.dwFlags = KEYEVENTF_UNICODE;
        keystroke[0].ki.time = 0;
        keystroke[0].ki.dwExtraInfo = GetMessageExtraInfo();

        keystroke[1].type = INPUT_KEYBOARD;
        keystroke[1].ki.wVk = 0;
        keystroke[1].ki.wScan = key;
        keystroke[1].ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
        keystroke[1].ki.time = 0;
        keystroke[1].ki.dwExtraInfo = GetMessageExtraInfo();

        //Send the keystrokes.
        keystrokes_sent = SendInput((UINT)keystrokes_to_send, keystroke, sizeof(*keystroke));
        delete[] keystroke;

        //SetForegroundWindow(myWindow);

        return keystrokes_sent == 2;
    }

};
 

#endif // _SIGVIEWER_WINDOW_UTILS