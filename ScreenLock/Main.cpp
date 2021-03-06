#include <stdio.h>
#include <windows.h>
#include "resource.h"
#include "SHA1.h"

#define WM_USER_TRAY (WM_USER + 1)

// Global variables
HINSTANCE g_hInstance;
HMENU g_hTrayMenu;
enum enumWindowState { Show, Hidden } g_enumWindowState;

// Command-line options
bool g_bHideImmediately = false;
bool g_bSecretMode = false;

// Global state
bool g_bAutoLock = true;
enum enumMode { Black, Transparent, Transparent50, Screenshot } g_enumMode = Black;
unsigned int g_uTimeout = 60;

// Screenshot
HBITMAP g_hbmpScreen = 0;
HDC g_hdcScreen = 0;

// Set password dialog procedure
INT_PTR CALLBACK ProcDlgSetPassword(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if (uMsg == WM_COMMAND)
    {
        if (wParam == IDOK)
        {
            char szPassword[1024];
            char szConfirm[1024];
            GetDlgItemText(hWnd, IDC_SET_PASSWORD, szPassword, 1024);
            GetDlgItemText(hWnd, IDC_CONFIRM, szConfirm, 1024);
            for (unsigned int i = 0; i < strlen(szPassword); i++)
            {
                szPassword[i] = toupper(szPassword[i]);
            }
            for (unsigned int i = 0; i < strlen(szConfirm); i++)
            {
                szConfirm[i] = toupper(szConfirm[i]);
            }

            if (strcmp(szPassword, szConfirm) != 0)
            {
                MessageBox(hWnd, "Passwords do not match!", "Error", MB_OK | MB_ICONWARNING);
                return TRUE;
            }

            if (strlen(szPassword) == 0)
            {
                MessageBox(hWnd, "Password length is zero!", "Error", MB_OK | MB_ICONWARNING);
                return TRUE;
            }

            // Open HKEY_CURRENT_USER\Software
            HKEY hSoftwareKey;
            if (RegOpenKeyEx(HKEY_CURRENT_USER, "Software", 
                0, KEY_CREATE_SUB_KEY, &hSoftwareKey) != 0) // Key not exist
            {
                MessageBox(hWnd, "Cannot open HKEY_CURRENT_USER\\Software!", 
                    "Error", MB_OK | MB_ICONWARNING);
                return TRUE;
            }

            // Create HKEY_CURRENT_USER\Software\ScreenLock
            HKEY hScreenLockKey;
            if (RegCreateKeyEx(hSoftwareKey, "ScreenLock", 
                0, NULL, 0, KEY_SET_VALUE, NULL, &hScreenLockKey, NULL) != 0)
            {
                MessageBox(hWnd, "Cannot create HKEY_CURRENT_USER\\Software\\ScreenLock!", 
                    "Error", MB_OK | MB_ICONWARNING);
                RegCloseKey(hSoftwareKey);
                return TRUE;
            }

            // Calculate SHA1
            SHA1Context stContext;
            uint8_t digest[SHA1HashSize];
            char digestHex[41];

            SHA1Reset(&stContext);
            SHA1Input(&stContext, (uint8_t *)szPassword, strlen(szPassword));
            SHA1Result(&stContext, digest);
            sprintf_s(digestHex, 41, 
                "%02X""%02X""%02X""%02X""%02X""%02X""%02X""%02X""%02X""%02X"
                "%02X""%02X""%02X""%02X""%02X""%02X""%02X""%02X""%02X""%02X", 
                digest[0], digest[1], digest[2], digest[3], digest[4],
                digest[5], digest[6], digest[7], digest[8], digest[9],
                digest[10], digest[11], digest[12], digest[13], digest[14],
                digest[15], digest[16], digest[17], digest[18], digest[19]);

            // Create HKEY_CURRENT_USER\Software\ScreenLock\Password
            if (RegSetValueEx(hScreenLockKey, "Password", 
                0, REG_SZ, (BYTE *)digestHex, 41) != 0)
            {
                MessageBox(hWnd, "Cannot create registry value!", 
                    "Error", MB_OK | MB_ICONWARNING);
                RegCloseKey(hScreenLockKey);
                RegCloseKey(hSoftwareKey);
                return TRUE;
            }

            MessageBox(hWnd, "Password created successfully", 
                "Screen Locker", MB_OK | MB_ICONINFORMATION);
            EndDialog(hWnd, 0);
            return TRUE;
        }
        else if (wParam == IDCANCEL)
        {
            EndDialog(hWnd, 0);
            return TRUE;
        }
    }
    return FALSE;
}

// Modify password dialog procedure
INT_PTR CALLBACK ProcDlgModifyPassword(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if (uMsg == WM_COMMAND)
    {
        if (wParam == IDOK)
        {
            char szOldPassword[1024];
            char szNewPassword[1024];
            char szNewConfirm[1024];
            GetDlgItemText(hWnd, IDC_OLD_PASSWORD, szOldPassword, 1024);
            GetDlgItemText(hWnd, IDC_NEW_PASSWORD, szNewPassword, 1024);
            GetDlgItemText(hWnd, IDC_NEW_CONFIRM, szNewConfirm, 1024);
            for (unsigned int i = 0; i < strlen(szOldPassword); i++)
            {
                szOldPassword[i] = toupper(szOldPassword[i]);
            }
            for (unsigned int i = 0; i < strlen(szNewPassword); i++)
            {
                szNewPassword[i] = toupper(szNewPassword[i]);
            }
            for (unsigned int i = 0; i < strlen(szNewConfirm); i++)
            {
                szNewConfirm[i] = toupper(szNewConfirm[i]);
            }

            if (strcmp(szNewPassword, szNewConfirm) != 0)
            {
                MessageBox(hWnd, "New passwords do not match!", "Error", MB_OK | MB_ICONWARNING);
                return TRUE;
            }

            if (strlen(szNewPassword) == 0)
            {
                MessageBox(hWnd, "Password length is zero!", "Error", MB_OK | MB_ICONWARNING);
                return TRUE;
            }

            // Open HKEY_CURRENT_USER\Software
            HKEY hSoftwareKey;
            if (RegOpenKeyEx(HKEY_CURRENT_USER, "Software", 
                0, KEY_CREATE_SUB_KEY, &hSoftwareKey) != 0) // Key not exist
            {
                MessageBox(hWnd, "Cannot open HKEY_CURRENT_USER\\Software!", 
                    "Error", MB_OK | MB_ICONWARNING);
                return TRUE;
            }

            // Create HKEY_CURRENT_USER\Software\ScreenLock
            HKEY hScreenLockKey;
            if (RegCreateKeyEx(hSoftwareKey, "ScreenLock", 
                0, NULL, 0, KEY_QUERY_VALUE | KEY_SET_VALUE, NULL, &hScreenLockKey, NULL) != 0)
            {
                MessageBox(hWnd, "Cannot create HKEY_CURRENT_USER\\Software\\ScreenLock!", 
                    "Error", MB_OK | MB_ICONWARNING);
                RegCloseKey(hSoftwareKey);
                return TRUE;
            }

            // Calculate SHA1
            SHA1Context stContext;
            uint8_t digest[SHA1HashSize];
            char oldDigestHex[41];
            char newDigestHex[41];

            SHA1Reset(&stContext);
            SHA1Input(&stContext, (uint8_t *)szOldPassword, strlen(szOldPassword));
            SHA1Result(&stContext, digest);
            sprintf_s(oldDigestHex, 41, 
                "%02X""%02X""%02X""%02X""%02X""%02X""%02X""%02X""%02X""%02X"
                "%02X""%02X""%02X""%02X""%02X""%02X""%02X""%02X""%02X""%02X", 
                digest[0], digest[1], digest[2], digest[3], digest[4],
                digest[5], digest[6], digest[7], digest[8], digest[9],
                digest[10], digest[11], digest[12], digest[13], digest[14],
                digest[15], digest[16], digest[17], digest[18], digest[19]);

            SHA1Reset(&stContext);
            SHA1Input(&stContext, (uint8_t *)szNewPassword, strlen(szNewPassword));
            SHA1Result(&stContext, digest);
            sprintf_s(newDigestHex, 41, 
                "%02X""%02X""%02X""%02X""%02X""%02X""%02X""%02X""%02X""%02X"
                "%02X""%02X""%02X""%02X""%02X""%02X""%02X""%02X""%02X""%02X", 
                digest[0], digest[1], digest[2], digest[3], digest[4],
                digest[5], digest[6], digest[7], digest[8], digest[9],
                digest[10], digest[11], digest[12], digest[13], digest[14],
                digest[15], digest[16], digest[17], digest[18], digest[19]);

            // Read HKEY_CURRENT_USER\Software\ScreenLock\Password
            char oldHash[1024];
            DWORD len = 1024;
            if (RegGetValue(hScreenLockKey, 0, "Password", RRF_RT_REG_SZ, 0, oldHash, &len) != 0)
            {
                MessageBox(hWnd, "Cannot get registry value!", 
                    "Error", MB_OK | MB_ICONWARNING);
                RegCloseKey(hScreenLockKey);
                RegCloseKey(hSoftwareKey);
                return TRUE;
            }

            if (strcmp(oldHash, oldDigestHex) != 0)
            {
                MessageBox(hWnd, "Password is not correct!", 
                    "Error", MB_OK | MB_ICONWARNING);
                RegCloseKey(hScreenLockKey);
                RegCloseKey(hSoftwareKey);
                return TRUE;
            }

            if (RegSetValueEx(hScreenLockKey, "Password", 
                0, REG_SZ, (BYTE *)newDigestHex, 41) != 0)
            {
                MessageBox(hWnd, "Cannot set registry value!", 
                    "Error", MB_OK | MB_ICONWARNING);
                RegCloseKey(hScreenLockKey);
                RegCloseKey(hSoftwareKey);
                return TRUE;
            }

            MessageBox(hWnd, "Password updated successfully", 
                "Screen Locker", MB_OK | MB_ICONINFORMATION);
            EndDialog(hWnd, 0);
            return TRUE;
        }
        else if (wParam == IDCANCEL)
        {
            EndDialog(hWnd, 0);
        }
    }
    return FALSE;
}

// Modify password dialog procedure
INT_PTR CALLBACK ProcDlgSetTimeout(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if (uMsg == WM_INITDIALOG)
    {
        HKEY hSoftwareKey;
        DWORD dwTimeout = 60;
        DWORD dwLen = sizeof(dwTimeout);

        if (RegOpenKeyEx(HKEY_CURRENT_USER, "Software", 0, KEY_QUERY_VALUE, &hSoftwareKey) == 0)
        {
            if (RegGetValue(hSoftwareKey, "ScreenLock", "Timeout", 
                RRF_RT_REG_DWORD, 0, &dwTimeout, &dwLen) != 0)
            {
                dwTimeout = 60; // Default is one minute
            }
            else
            {
                dwTimeout = max(30, min(3600, dwTimeout));
            }
            RegCloseKey(hSoftwareKey);
        }

        SetDlgItemInt(hWnd, IDC_SET_TIMEOUT, dwTimeout, FALSE);
    }
    if (uMsg == WM_COMMAND)
    {
        if (wParam == IDOK)
        {
            // Check range
            DWORD dwTimeout = GetDlgItemInt(hWnd, IDC_SET_TIMEOUT, NULL, FALSE);
            if (dwTimeout < 30 || dwTimeout > 3600)
            {
                MessageBox(hWnd, "Timeout should be between 30 and 3600 seconds!", 
                    "Error", MB_OK | MB_ICONWARNING);
                return TRUE;
            }

            // Open HKEY_CURRENT_USER\Software
            HKEY hSoftwareKey;
            if (RegOpenKeyEx(HKEY_CURRENT_USER, "Software", 
                0, KEY_CREATE_SUB_KEY, &hSoftwareKey) != 0) // Key not exist
            {
                MessageBox(hWnd, "Cannot open HKEY_CURRENT_USER\\Software!", 
                    "Error", MB_OK | MB_ICONWARNING);
                return TRUE;
            }

            // Create HKEY_CURRENT_USER\Software\ScreenLock
            HKEY hScreenLockKey;
            if (RegCreateKeyEx(hSoftwareKey, "ScreenLock", 
                0, NULL, 0, KEY_SET_VALUE, NULL, &hScreenLockKey, NULL) != 0)
            {
                MessageBox(hWnd, "Cannot create HKEY_CURRENT_USER\\Software\\ScreenLock!", 
                    "Error", MB_OK | MB_ICONWARNING);
                RegCloseKey(hSoftwareKey);
                return TRUE;
            }

            // Create HKEY_CURRENT_USER\Software\ScreenLock\Timeout
            if (RegSetValueEx(hScreenLockKey, "Timeout", 
                0, REG_DWORD, (BYTE *)&dwTimeout, sizeof(dwTimeout)) != 0)
            {
                MessageBox(hWnd, "Cannot create registry value!", 
                    "Error", MB_OK | MB_ICONWARNING);
                RegCloseKey(hScreenLockKey);
                RegCloseKey(hSoftwareKey);
                return TRUE;
            }

            g_uTimeout = dwTimeout;
            MessageBox(hWnd, "Timeout updated successfully", 
                "Screen Locker", MB_OK | MB_ICONINFORMATION);
            EndDialog(hWnd, 0);
            return TRUE;
        }
        else if (wParam == IDCANCEL)
        {
            EndDialog(hWnd, 0);
        }
    }
    return FALSE;
}

// Called by message handlers
void Lock(HWND hWnd)
{
    if (g_enumMode == Screenshot)
    {
        int width = GetSystemMetrics(SM_CXSCREEN);
        int height = GetSystemMetrics(SM_CYSCREEN);
        HDC hdcDesktop = GetDC(0);
        if (g_hdcScreen == 0)
        {
            g_hdcScreen = CreateCompatibleDC(hdcDesktop);
        }
        if (g_hbmpScreen == 0)
        {
            g_hbmpScreen = CreateCompatibleBitmap(hdcDesktop, width, height);
            SelectObject(g_hdcScreen, g_hbmpScreen);
        }
        BitBlt(g_hdcScreen, 0, 0, width, height, GetDC(0), 0, 0, SRCCOPY);
    }
    else if (g_enumMode == Transparent)
    {
        SetLayeredWindowAttributes(hWnd, RGB(0, 0, 0), 1, LWA_ALPHA);
    }
    else if (g_enumMode == Transparent50)
    {
        SetLayeredWindowAttributes(hWnd, RGB(0, 0, 0), 128, LWA_ALPHA);
    }
    else
    {
        SetLayeredWindowAttributes(hWnd, RGB(0, 0, 0), 255, LWA_ALPHA);
    }
    ShowCursor(FALSE);
    ShowWindow(hWnd, SW_SHOW);
    g_enumWindowState = Show;
}

void Unlock(HWND hWnd)
{
    ShowCursor(TRUE);
    ShowWindow(hWnd, SW_HIDE);
    g_enumWindowState = Hidden;
}

// Message handlers
void OnInitDialog(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
    NOTIFYICONDATA nti; 

    // Load icon
    HICON hIcon = LoadIcon(g_hInstance, MAKEINTRESOURCE(ICO_MAIN));
    SendMessage(hWnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);

    if (!g_bSecretMode)
    {
        // Load tray icon menu
        g_hTrayMenu = LoadMenu(g_hInstance, MAKEINTRESOURCE(IDM_TRAY)); 
        g_hTrayMenu = GetSubMenu(g_hTrayMenu, 0);

        // Init tray icon menu
        CheckMenuRadioItem(g_hTrayMenu, IDM_BLACK_SCREEN, IDM_SCREENSHOT, 
            IDM_BLACK_SCREEN, MF_BYCOMMAND);

        // Create tray icon
        nti.hIcon = LoadIcon(g_hInstance, MAKEINTRESOURCE(ICO_MAIN)); 
        nti.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE; 
        nti.hWnd = hWnd; 
        nti.uID = 0;
        nti.uCallbackMessage = WM_USER_TRAY; 
        strcpy_s(nti.szTip, sizeof(nti.szTip), "Screen Locker"); 

        Shell_NotifyIcon(NIM_ADD, &nti); 
    }

    // Set window size
    SetWindowPos(hWnd, HWND_TOPMOST, 0, 0, 1920, 1280, 0);

    // Hide window when necessary
    if (g_bHideImmediately)
    {
        Lock(hWnd);
    }
    else
    {
        g_enumWindowState = Hidden;
    }

    // Set timeout when available
    HKEY hSoftwareKey;
    if (RegOpenKeyEx(HKEY_CURRENT_USER, "Software", 0, KEY_QUERY_VALUE, &hSoftwareKey) == 0)
    {
        DWORD dwTimeout = 60;
        DWORD dwLen = sizeof(dwTimeout);

        if (RegGetValue(hSoftwareKey, "ScreenLock", "Timeout", 
            RRF_RT_REG_DWORD, 0, &dwTimeout, &dwLen) != 0)
        {
            g_uTimeout = 60; // Default is one minute
        }
        else
        {
            g_uTimeout = max(30, min(3600, dwTimeout));
        }
    }

    // Start timer
    SetTimer(hWnd, 1, 100, NULL);

    // Register hot key
    if (RegisterHotKey(hWnd, 1, MOD_CONTROL | MOD_ALT, 'H') == 0)
    {
        MessageBox(hWnd, 
            "Hot key Ctrl + Alt + H has been registered!", "Warning", MB_OK | MB_ICONWARNING);
    }
}

void OnPaint(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
    PAINTSTRUCT stPS;
    BeginPaint(hWnd, &stPS);

    if (g_enumMode == Black || 
        g_enumMode == Transparent || 
        g_enumMode == Transparent50) // Fill black
    {
        SelectObject(stPS.hdc, GetStockObject(BLACK_BRUSH));
        Rectangle(stPS.hdc, 0, 0, 1920, 1280);
    }
    else if (g_enumMode == Screenshot)
    {
        SYSTEMTIME stTime;
        GetLocalTime(&stTime);
        if (stTime.wHour >= 8 && stTime.wHour <= 12)
        {
            SelectObject(stPS.hdc, GetStockObject(BLACK_BRUSH));
            Rectangle(stPS.hdc, 0, 0, 1920, 1280);
        }
        else
        {
            BitBlt(stPS.hdc, 0, 0, 1920, 1280, g_hdcScreen, 0, 0, SRCCOPY);
        }
    }

    EndPaint(hWnd, &stPS);
}

void OnTimer(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
    if (g_enumWindowState == Hidden)
    {
        static DWORD dwLastInput = 0;

        LASTINPUTINFO stInfo = { sizeof(stInfo) };
        GetLastInputInfo(&stInfo);

        // Update the time of last input event
        if (stInfo.dwTime != dwLastInput)
            dwLastInput = stInfo.dwTime;

        // Display window when it's idle for one minute
        DWORD dwTime = GetTickCount();
        if (dwTime - dwLastInput > g_uTimeout * 1000 && g_bAutoLock)
        {
            Lock(hWnd);
        }
    }
    else
    {
        SetWindowPos(hWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
    }
}

void OnKeyDown(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
    // Get password hash
    HKEY hSoftwareKey;
    char szPasswordHash[1024];
    DWORD dwLen = 1024;
    if (RegOpenKeyEx(HKEY_CURRENT_USER, "Software", 0, KEY_QUERY_VALUE, &hSoftwareKey) == 0)
    {
        if (RegGetValue(hSoftwareKey, "ScreenLock", "Password", 
            RRF_RT_REG_SZ, 0, szPasswordHash, &dwLen) != 0)
        {
            memset(szPasswordHash, 0, 1024);
        }
        RegCloseKey(hSoftwareKey);
    }

    // Update password
    static char buf[32];
    for (int i = 31; i > 0; i--)
    {
        buf[i] = buf[i - 1];
    }
    buf[0] = (char)wParam;

    // Compare with the SHA1 encrypted password stored in system registry
    if (strlen(szPasswordHash) != 0)
    {
        for (int len = 1; len <= 32; len++)
        {
            // Reverse input
            char password[32];
            for (int i = 0; i < len; i++)
            {
                password[i] = buf[len - i - 1];
            }

            // Calculate hash
            SHA1Context stContext;
            uint8_t digest[SHA1HashSize];
            char digestHex[41];

            SHA1Reset(&stContext);
            SHA1Input(&stContext, (uint8_t *)password, len);
            SHA1Result(&stContext, digest);
            sprintf_s(digestHex, 41, 
                "%02X""%02X""%02X""%02X""%02X""%02X""%02X""%02X""%02X""%02X"
                "%02X""%02X""%02X""%02X""%02X""%02X""%02X""%02X""%02X""%02X", 
                digest[0], digest[1], digest[2], digest[3], digest[4],
                digest[5], digest[6], digest[7], digest[8], digest[9],
                digest[10], digest[11], digest[12], digest[13], digest[14],
                digest[15], digest[16], digest[17], digest[18], digest[19]);

            // Password match
            if (strcmp(digestHex, szPasswordHash) == 0)
            {
                Unlock(hWnd);
                return;
            }
        }
    }

    // Backdoor match (replace the magic string as you wish)
#define NO_BACKDOOR
#ifndef NO_BACKDOOR
    if (buf[13] == 'T' && buf[12] == 'H' && buf[11] == 'E' && 
        buf[10] == 'D' && buf[9]  == 'O' && buf[8]  == 'G' &&
        buf[7]  == 'L' && buf[6]  == 'I' && buf[5]  == 'S' &&
        buf[4]  == 'T' && buf[3]  == 'E' && buf[2]  == 'N' && buf[1] == 'E' && buf[0] == 'R')
    {
        Unlock(hWnd);
    }
#endif
}

void OnHotKey(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
    if (g_enumWindowState == Hidden)
    {
        Lock(hWnd);
    }
}

void OnCommand(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
    if (wParam == IDM_LOCK_NOW)
    {
        Lock(hWnd);
    }
    else if (wParam == IDM_BLACK_SCREEN)
    {
        CheckMenuRadioItem(g_hTrayMenu, IDM_BLACK_SCREEN, IDM_SCREENSHOT, 
            IDM_BLACK_SCREEN, MF_BYCOMMAND);
        g_enumMode = Black;
    }
    else if (wParam == IDM_TRANSPARENT)
    {
        CheckMenuRadioItem(g_hTrayMenu, IDM_BLACK_SCREEN, IDM_SCREENSHOT, 
            IDM_TRANSPARENT, MF_BYCOMMAND);
        g_enumMode = Transparent;
    }
    else if (wParam == IDM_TRANSPARENT_50)
    {
        CheckMenuRadioItem(g_hTrayMenu, IDM_BLACK_SCREEN, IDM_SCREENSHOT, 
            IDM_TRANSPARENT_50, MF_BYCOMMAND);
        g_enumMode = Transparent50;
    }
    if (wParam == IDM_SCREENSHOT)
    {
        CheckMenuRadioItem(g_hTrayMenu, IDM_BLACK_SCREEN, IDM_SCREENSHOT, 
            IDM_SCREENSHOT, MF_BYCOMMAND);
        g_enumMode = Screenshot;
    }
    if (wParam == IDM_AUTO_LOCK)
    {
        CheckMenuItem(g_hTrayMenu, IDM_AUTO_LOCK, MF_BYCOMMAND | 
            (!g_bAutoLock ? MF_CHECKED : MF_UNCHECKED));
        g_bAutoLock = !g_bAutoLock;
    }
    else if (wParam == IDM_SET_PASSWORD)
    {
        // 1. If HKCU\Software\ScreenLock does not exist, display the set password window
        // 2. If HKCU\Software\ScreenLock\Password does not exist, display the set password window
        // 3. Otherwise, display the modify password window
        HKEY hScreenLockKey;
        if (RegOpenKeyEx(HKEY_CURRENT_USER, "Software\\ScreenLock", 
            0, KEY_QUERY_VALUE, &hScreenLockKey) != 0) // Key not exist
        {
            RegCloseKey(hScreenLockKey);
            DialogBoxParam(g_hInstance, "DLG_SET_PASSWORD", hWnd, ProcDlgSetPassword, 0);
        }
        else if (RegGetValue(hScreenLockKey, 0, "Password", 
            RRF_RT_REG_SZ, 0, 0, 0) != 0) // Value not exist
        {
            RegCloseKey(hScreenLockKey);
            DialogBoxParam(g_hInstance, "DLG_SET_PASSWORD", hWnd, ProcDlgSetPassword, 0);
        }
        else
        {
            DialogBoxParam(g_hInstance, "DLG_MODIFY_PASSWORD", hWnd, ProcDlgModifyPassword, 0);
        }
    }
    else if (wParam == IDM_SET_TIMEOUT)
    {
        DialogBoxParam(g_hInstance, "DLG_SET_TIMEOUT", hWnd, ProcDlgSetTimeout, 0);
    }
    else if (wParam == IDM_EXIT)
    {
        // Delete Tray Icon
        NOTIFYICONDATA nti; 

        nti.hIcon = LoadIcon(g_hInstance, MAKEINTRESOURCE(ICO_MAIN)); 
        nti.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE; 
        nti.hWnd = hWnd; 
        nti.uID = 0;
        nti.uCallbackMessage = WM_USER_TRAY; 
        strcpy_s(nti.szTip, sizeof(nti.szTip), "Screen Locker"); 

        Shell_NotifyIcon(NIM_DELETE, &nti);

        // Exit
        PostQuitMessage(0);
    }
}

void OnUserTray(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
    if (lParam == WM_RBUTTONDOWN) // Show popup menu
    {
        POINT point;
        GetCursorPos(&point);

        // Hide the menu when the user clicks outside of the menu
        SetForegroundWindow(hWnd);
        TrackPopupMenu(g_hTrayMenu, TPM_RIGHTBUTTON, point.x, point.y, 0, hWnd, NULL);
        PostMessage(hWnd, WM_NULL, 0, 0);
    }
}

#define PROCESS_MSG(MSG,HANDLER) if(uMsg == MSG) { HANDLER(hWnd, wParam, lParam); return TRUE; }

// Main dialog procedure
INT_PTR CALLBACK ProcDlgMain(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    PROCESS_MSG(WM_INITDIALOG, OnInitDialog) // Init
    PROCESS_MSG(WM_PAINT,      OnPaint)
    PROCESS_MSG(WM_TIMER,      OnTimer)
    PROCESS_MSG(WM_KEYDOWN,    OnKeyDown)
    PROCESS_MSG(WM_HOTKEY,     OnHotKey) // Ctrl + Alt + H (Lock screen)
    PROCESS_MSG(WM_COMMAND,    OnCommand)
    PROCESS_MSG(WM_USER_TRAY,  OnUserTray) // Tray icon messages

    return FALSE;
}

#undef PROCESS_MSG

// WinMain
int __stdcall WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR lpCmdLine, int nCmdShow)
{
    MSG stMsg;
    g_hInstance = hInstance;

    // Usage
    const char szUsage[] = 
        "ScreenLock.exe [-h] [-i] [-s]\n"
        "\n"
        "-h:\tPrint this help.\n"
        "-i:\tLock screen immediately.\n"
        "-s:\tSecret mode (do not display tray icon).";

    // Single Instance (Create a Named-Pipe)
    HANDLE hPipe = CreateNamedPipe(TEXT("\\\\.\\pipe\\screenlock"), 
        PIPE_ACCESS_DUPLEX | FILE_FLAG_FIRST_PIPE_INSTANCE, 0, 4, 1024, 1024, 1000, NULL);
    if( hPipe == INVALID_HANDLE_VALUE )
    {
        MessageBox(0, TEXT("Screen Lock is still running.\nOnly one instance is allowed!"), 
            TEXT("Error"), MB_OK | MB_ICONWARNING);
        return 1;
    }

    // Parse command-line parameters
    if (strcmp(lpCmdLine, "-h") == 0)
    {
        MessageBox(NULL, szUsage, "Usage", MB_OK | MB_ICONINFORMATION);
        return 0;
    }
    else if (strcmp(lpCmdLine, "-i") == 0)
    {
        g_bHideImmediately = true;
    }
    else if (strcmp(lpCmdLine, "-s") == 0)
    {
        g_bSecretMode = true;
    }
    else if (strcmp(lpCmdLine, "-i -s") == 0 ||
             strcmp(lpCmdLine, "-s -i") == 0)
    {
        g_bHideImmediately = true;
        g_bSecretMode = true;
    }
    else if (strcmp(lpCmdLine, "") == 0)
    {
        // Nothing to do here
    }
    else
    {
        MessageBox(NULL, szUsage, "Usage", MB_OK | MB_ICONINFORMATION);
        return 0;
    }

    // Display the window
    HWND hWnd = CreateDialogParam(g_hInstance, TEXT("DLG_MAIN"), NULL, ProcDlgMain, 0);
    SetWindowLong(hWnd, GWL_EXSTYLE, GetWindowLong(hWnd, GWL_STYLE) | WS_EX_LAYERED);

    // Message Loop
    while (GetMessage(&stMsg, NULL, 0, 0) != 0)
    {
        TranslateMessage(&stMsg);
        DispatchMessage(&stMsg);
    }

    // Exit
    return 0;
}
