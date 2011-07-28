#include <windows.h>
#include <commctrl.h>
#include <map>

#include "vaultmp.h"
#include "Bethesda.h"
#include "ServerEntry.h"
#include "Data.h"
#include "ufmod.h"

#include "RakNet/RakPeerInterface.h"
#include "RakNet/MessageIdentifiers.h"
#include "RakNet/BitStream.h"
#include "RakNet/RakString.h"
#include "RakNet/RakSleep.h"
#include "RakNet/GetTime.h"

using namespace RakNet;
using namespace Data;
using namespace std;

HINSTANCE instance;
HANDLE mutex;
HFONT hFont;
HWND wndsortcur;
HWND wndchiptune;
HWND wndlistview;
HWND wndlistview2;
HWND wndprogressbar;
HDC hdc, hdcMem;
HBITMAP hBitmap;
BITMAP bitmap;
PAINTSTRUCT ps;
BOOL sort;

RakPeerInterface* peer;
SocketDescriptor* sockdescr;

typedef map<SystemAddress, ServerEntry> ServerMap;
ServerMap serverList;

SystemAddress* selectedServer = NULL;
char inipath[256];
char player_name[MAX_PLAYER_NAME];
char server_master[MAX_MASTER_SERVER];
bool foundFallout3 = false;
bool foundNewVegas = false;

HWND CreateMainWindow();
int RegisterClasses();
int MessageLoop();
void InitRakNet();
void CreateWindowContent(HWND parent);
void CleanUp();
LRESULT CALLBACK WindowProcedure(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

void seDebugPrivilege()
{
    TOKEN_PRIVILEGES priv;
    HANDLE hThis, hToken;
    LUID luid;
    hThis = GetCurrentProcess();
    OpenProcessToken(hThis, TOKEN_ADJUST_PRIVILEGES, &hToken);
    LookupPrivilegeValue(0, "seDebugPrivilege", &luid);
    priv.PrivilegeCount = 1;
    priv.Privileges[0].Luid = luid;
    priv.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    AdjustTokenPrivileges(hToken, false, &priv, 0, 0, 0);
    CloseHandle(hToken);
    CloseHandle(hThis);
}

void MinimizeToTray(HWND hwnd)
{
    NOTIFYICONDATA nid = {0};

    ZeroMemory(&nid, sizeof(NOTIFYICONDATA));
    nid.cbSize = sizeof(NOTIFYICONDATA);
    nid.hWnd = hwnd;
    nid.uID = ICON_MAIN;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = MSG_MINTRAYICON;
    nid.hIcon = (HICON) LoadImage(GetModuleHandle(NULL), MAKEINTRESOURCE(ICON_MAIN), IMAGE_ICON, 16, 16, 0);
    strcpy(nid.szTip, "Vault-Tec Multiplayer Mod");

    Shell_NotifyIcon(NIM_ADD, &nid);

    ShowWindow(hwnd, SW_HIDE);
}

void Maximize(HWND hwnd)
{
    NOTIFYICONDATA nid = {0};

    ZeroMemory(&nid, sizeof(NOTIFYICONDATA));
    nid.cbSize = sizeof(NOTIFYICONDATA);
    nid.hWnd = hwnd;
    nid.uID = ICON_MAIN;
    Shell_NotifyIcon(NIM_DELETE, &nid);
    ShowWindow(hwnd, SW_RESTORE);
    SetForegroundWindow(hwnd);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR cmdline, int show)
{
#ifdef VAULTMP_DEBUG
    if (LoadLibrary("exchndl.dll") == NULL)
        return MessageBox(NULL, "Could not find exchndl.dll!", "Error", MB_OK | MB_ICONERROR);
#endif

    FILE* Fallout = NULL;

    Fallout = fopen("Fallout3.exe", "rb");

    if (Fallout != NULL)
    {
        long int size = 0;

        fseek(Fallout, 0, SEEK_END);
        size = ftell(Fallout);
        fclose(Fallout);

        if (size != FALLOUT3_EXE_SIZE)
            return MessageBox(NULL, "Fallout3.exe is either corrupted or not supported!", "Error", MB_OK | MB_ICONERROR);
        else
        {
            Fallout = fopen("fose_1_7.dll", "rb");

            if (Fallout != NULL)
            {
                fclose(Fallout);

                Fallout = fopen("xlive.dll", "rb");

                if (Fallout != NULL)
                {
                    size = 0;

                    fseek(Fallout, 0, SEEK_END);
                    size = ftell(Fallout);
                    fclose(Fallout);

                    if (size != XLIVE_DLL_SIZE)
                        return MessageBox(NULL, "xlive.dll is unpatched!", "Error", MB_OK | MB_ICONERROR);
                    else
                        foundFallout3 = true;
                }
                else
                    return MessageBox(NULL, "xlive.dll is missing!", "Error", MB_OK | MB_ICONERROR);
            }
            else
                return MessageBox(NULL, "Could not find FOSE 1.7!\nhttp://fose.silverlock.org/", "Error", MB_OK | MB_ICONERROR);
        }
    }

    Fallout = fopen("FalloutNV.exe", "rb");

    if (Fallout != NULL)
    {
        long int size = 0;

        fseek(Fallout, 0, SEEK_END);
        size = ftell(Fallout);
        fclose(Fallout);

        if (size != FALLOUTNV_EXE1_SIZE && size != FALLOUTNV_EXE2_SIZE && size != FALLOUTNV_EXE3_SIZE)
            return MessageBox(NULL, "FalloutNV.exe is either corrupted or not supported!", "Error", MB_OK | MB_ICONERROR);
        else
        {
            Fallout = fopen("nvse_1_1.dll", "rb");

            if (Fallout != NULL)
            {
                foundNewVegas = true;
                fclose(Fallout);
            }
            else
                return MessageBox(NULL, "Could not find NVSE 1.1!\nhttp://nvse.silverlock.org/", "Error", MB_OK | MB_ICONERROR);
        }
    }

    Fallout = fopen("vaultmp.dll", "rb");

    if (Fallout != NULL)
    {
        long int size = 0;

        fseek(Fallout, 0, SEEK_END);
        size = ftell(Fallout);
        fclose(Fallout);

        if (size != VAULTMP_DLL_SIZE)
            return MessageBox(NULL, "vaultmp.dll is either corrupted or not up to date!", "Error", MB_OK | MB_ICONERROR);
    }
    else
        return MessageBox(NULL, "Could not find vaultmp.dll!", "Error", MB_OK | MB_ICONERROR);

    mutex = CreateMutex(NULL, TRUE, "vaultmp");
    if (GetLastError() == ERROR_ALREADY_EXISTS)
        return MessageBox(NULL, "Vault-Tec Multiplayer Mod is already running!", "Error", MB_OK | MB_ICONERROR);

    instance = hInstance;

    GetCurrentDirectory(sizeof(inipath), inipath);
    strcat(inipath, "\\vaultmp.ini");

    seDebugPrivilege();
    InitCommonControls();
    RegisterClasses();
    InitRakNet();

    hFont = CreateFont(-11, 0, 0, 0, FW_NORMAL, 0, 0, 0, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Verdana");
    CreateMainWindow();

    return MessageLoop();
}

HWND CreateMainWindow()
{
    HWND wnd;
    char wndtitle[sizeof(CLIENT_VERSION) + 64];
    sprintf(wndtitle, "Vault-Tec Multiplayer Mod %s (FOR TESTING PURPOSES ONLY)", CLIENT_VERSION);
    wnd = CreateWindowEx(0, WND_CLASS_NAME, wndtitle, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, (GetSystemMetrics(SM_CXSCREEN) / 2) - 392, (GetSystemMetrics(SM_CYSCREEN) / 2) - 221, 785, 442, HWND_DESKTOP, NULL, instance, NULL);
    ShowWindow(wnd, SW_SHOWNORMAL);
    UpdateWindow(wnd);
    return wnd;
}

void InitRakNet()
{
    sockdescr = new SocketDescriptor();
    peer = RakPeerInterface::GetInstance();
    peer->Startup(RAKNET_CONNECTIONS, sockdescr, 1, THREAD_PRIORITY_NORMAL);
}

void CreateWindowContent(HWND parent)
{
    HWND wnd;
    LV_COLUMN col;
    sort = true;

    col.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
    col.fmt = LVCFMT_LEFT;

    wnd = CreateWindowEx(0x00000000, "Button", "Server details", 0x50020007, 543, 0, 229, 214, parent, (HMENU) IDC_GROUP0, instance, NULL);
    SendMessage(wnd, WM_SETFONT, (WPARAM) hFont, TRUE);

    wnd = CreateWindowEx(0x00000000, "Button", "Vault-Tec Multiplayer Controls", 0x50020007, 543, 218, 229, 190, parent, (HMENU) IDC_GROUP1, instance, NULL);
    SendMessage(wnd, WM_SETFONT, (WPARAM) hFont, TRUE);

    wnd = CreateWindowEx(0x00000200, "SysListView32", "", 0x50010005 | LVS_SINGLESEL | LVS_SHOWSELALWAYS, 6, 6, 531, 285, parent, (HMENU) IDC_GRID0, instance, NULL);
    SendMessage(wnd, WM_SETFONT, (WPARAM) hFont, TRUE);
    SendMessage(wnd, (LVM_FIRST + 54), 0, 64 | 32);
    wndlistview = wnd;

    col.cx = 299;
    col.pszText = (char*) "Name";
    col.iSubItem = 0;
    SendMessage(wnd, LVM_INSERTCOLUMN, 0, (LPARAM) &col);

    col.cx = 62;
    col.pszText = (char*) "Players";
    col.iSubItem = 1;
    SendMessage(wnd, LVM_INSERTCOLUMN, 1, (LPARAM) &col);

    col.cx = 50;
    col.pszText = (char*) "Ping";
    col.iSubItem = 2;
    SendMessage(wnd, LVM_INSERTCOLUMN, 2, (LPARAM) &col);

    col.cx = 116;
    col.pszText = (char*) "Map";
    col.iSubItem = 3;
    SendMessage(wnd, LVM_INSERTCOLUMN, 3, (LPARAM) &col);

    wnd = CreateWindowEx(0x00000200, "SysListView32", "", 0x50010001, 553, 19, 210, 157, parent, (HMENU) IDC_GRID1, instance, NULL);
    SendMessage(wnd, WM_SETFONT, (WPARAM) hFont, TRUE);
    wndlistview2 = wnd;

    col.cx = 103;
    col.pszText = (char*) "Key";
    col.iSubItem = 0;
    SendMessage(wnd, LVM_INSERTCOLUMN, 0, (LPARAM) &col);

    col.cx = 103;
    col.pszText = (char*) "Value";
    col.iSubItem = 1;
    SendMessage(wnd, LVM_INSERTCOLUMN, 1, (LPARAM) &col);

    wnd = CreateWindowEx(0x00000000, "msctls_progress32", "", 0x50000000, 553, 184, 210, 16, parent, (HMENU) IDC_PROGRESS0, instance, NULL);
    SendMessage(wnd, WM_SETFONT, (WPARAM) hFont, TRUE);
    wndprogressbar = wnd;

    wnd = CreateWindowEx(0x00000000, "Static", "Fallout is a trademark or registered trademark of Bethesda Softworks LLC in the U.S. and/or", 0x5000030C, 6, 374, 531, 18, parent, (HMENU) IDC_STATIC0, instance, NULL);
    SendMessage(wnd, WM_SETFONT, (WPARAM) hFont, TRUE);

    wnd = CreateWindowEx(0x00000000, "Static", "other countries. Vault-Tec Multiplayer Mod is not affliated with Bethesda Softworks LLC.", 0x50000300, 6, 392, 531, 18, parent, (HMENU) IDC_STATIC1, instance, NULL);
    SendMessage(wnd, WM_SETFONT, (WPARAM) hFont, TRUE);

    wnd = CreateWindowEx(0x00000000, "Button", "Powered by", 0x50020007, 6, 294, 531, 78, parent, (HMENU) IDC_GROUP2, instance, NULL);
    SendMessage(wnd, WM_SETFONT, (WPARAM) hFont, TRUE);

    wnd = CreateWindowEx(0x00000000, "Button", "mantronix - the wasteland", 0x50010003, 565, 374, 180, 32, parent, (HMENU) IDC_CHECK0, instance, NULL);
    SendMessage(wnd, WM_SETFONT, (WPARAM) hFont, TRUE);
    wndchiptune = wnd;

    wnd = CreateWindowEx(0x00000000, "Button", "Join Server", WS_BORDER | WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 555, 239, 100, 25, parent, (HMENU) IDC_BUTTON0, instance, NULL);
    SendMessage(wnd, WM_SETFONT, (WPARAM) hFont, TRUE);

    wnd = CreateWindowEx(0x00000000, "Button", "Update Server", WS_BORDER | WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 660, 239, 100, 25, parent, (HMENU) IDC_BUTTON1, instance, NULL);
    SendMessage(wnd, WM_SETFONT, (WPARAM) hFont, TRUE);

    wnd = CreateWindowEx(0x00000000, "Button", "Master Query", WS_BORDER | WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 555, 272, 100, 25, parent, (HMENU) IDC_BUTTON2, instance, NULL);
    SendMessage(wnd, WM_SETFONT, (WPARAM) hFont, TRUE);

    wnd = CreateWindowEx(0x00000000, "Button", "Credits", WS_BORDER | WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 660, 272, 100, 25, parent, (HMENU) IDC_BUTTON3, instance, NULL);
    SendMessage(wnd, WM_SETFONT, (WPARAM) hFont, TRUE);

    wnd = CreateWindowEx(0x00000200, "Edit", "vaultmp.com", 0x50010080, 611, 305, 146, 20, parent, (HMENU) IDC_EDIT3, instance, NULL);
    SendMessage(wnd, WM_SETFONT, (WPARAM) hFont, TRUE);
    SendMessage(wnd, EM_SETLIMITTEXT, (WPARAM) MAX_MASTER_SERVER, 0);

    wnd = CreateWindowEx(0x00000200, "Edit", "", 0x50010080, 611, 331, 146, 20, parent, (HMENU) IDC_EDIT0, instance, NULL);
    SendMessage(wnd, WM_SETFONT, (WPARAM) hFont, TRUE);
    SendMessage(wnd, EM_SETLIMITTEXT, (WPARAM) MAX_PLAYER_NAME, 0);

    wnd = CreateWindowEx(0x00000200, "Edit", "", 0x500100A0, 611, 357, 146, 20, parent, (HMENU) IDC_EDIT1, instance, NULL);
    SendMessage(wnd, WM_SETFONT, (WPARAM) hFont, TRUE);
    SendMessage(wnd, EM_SETLIMITTEXT, (WPARAM) MAX_PASSWORD_SIZE, 0);

    wnd = CreateWindowEx(0x00000000, "Static", "Master", 0x50000300, 570, 302, 38, 24, parent, (HMENU) IDC_STATIC4, instance, NULL);
    SendMessage(wnd, WM_SETFONT, (WPARAM) hFont, TRUE);

    wnd = CreateWindowEx(0x00000000, "Static", "Name", 0x50000300, 575, 328, 35, 24, parent, (HMENU) IDC_STATIC2, instance, NULL);
    SendMessage(wnd, WM_SETFONT, (WPARAM) hFont, TRUE);

    wnd = CreateWindowEx(0x00000000, "Static", "Password", 0x50000300, 554, 354, 57, 24, parent, (HMENU) IDC_STATIC3, instance, NULL);
    SendMessage(wnd, WM_SETFONT, (WPARAM) hFont, TRUE);
}

int RegisterClasses()
{
    WNDCLASSEX wc;

    wc.hInstance = instance;
    wc.lpszClassName = WND_CLASS_NAME;
    wc.lpfnWndProc = WindowProcedure;
    wc.style = CS_DBLCLKS;
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.hIcon = LoadIcon(instance, MAKEINTRESOURCE(ICON_MAIN));
    wc.hIconSm = LoadIcon(instance, MAKEINTRESOURCE(ICON_MAIN));
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszMenuName = NULL;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;

    wc.hbrBackground = (HBRUSH)(COLOR_3DFACE + 1);

    return RegisterClassEx(&wc);
}

int MessageLoop()
{
    MSG msg;

    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    CleanUp();

    WritePrivateProfileString("General", "name", player_name, inipath);
    WritePrivateProfileString("General", "master", server_master, inipath);

    return msg.wParam;
}

void CleanUp()
{
    DeleteObject(hFont);
    DeleteObject(hBitmap);
    peer->Shutdown(300);
    RakPeerInterface::DestroyInstance(peer);
    CloseHandle(mutex);
}

int Create2ColItem(HWND hwndList, char* text1, char* text2)
{
    LVITEM lvi = {0};
    int ret;
    lvi.mask = LVIF_TEXT;
    lvi.pszText = text1;
    ret = ListView_InsertItem(hwndList, &lvi);

    if (ret >= 0)
        ListView_SetItemText(hwndList, ret, 1, text2);

    return ret;
}

int Create4ColItem(HWND hwndList, const SystemAddress* addr, char* text1, char* text2, char* text3, char* text4)
{
    LVITEM lvi = {0};
    int ret;
    lvi.mask = LVIF_TEXT | LVIF_PARAM;
    lvi.pszText = text1;
    lvi.lParam = (LPARAM) addr;
    ret = ListView_InsertItem(hwndList, &lvi);

    if (ret >= 0)
    {
        ListView_SetItemText(hwndList, ret, 1, text2);
        ListView_SetItemText(hwndList, ret, 2, text3);
        ListView_SetItemText(hwndList, ret, 3, text4);
    }

    return ret;
}

void RefreshServerList()
{
    SendMessage(wndlistview, LVM_DELETEALLITEMS, 0, 0);
    SendMessage(wndlistview2, LVM_DELETEALLITEMS, 0, 0);
    selectedServer = NULL;

    for (map<SystemAddress, ServerEntry>::iterator i = serverList.begin(); i != serverList.end(); ++i)
    {
        const SystemAddress* addr = &i->first;
        ServerEntry entry = i->second;

        char players[16];
        char ping[16];
        sprintf(players, "%d / %d", entry.GetServerPlayers().first, entry.GetServerPlayers().second);
        sprintf(ping, "%d", entry.GetServerPing());

        Create4ColItem(wndlistview, addr, (char*) entry.GetServerName().c_str(), players, ping, (char*) entry.GetServerMap().c_str());
    }
}

int CALLBACK CompareProc(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort)
{
    static char buf[64], buf2[64];

    ListView_GetItemText(wndsortcur, lParam1, lParamSort, buf, sizeof(buf));
    ListView_GetItemText(wndsortcur, lParam2, lParamSort, buf2, sizeof(buf2));

    if (sort)
        return (stricmp(buf, buf2));
    else
        return (stricmp(buf, buf2) * -1);
}

LRESULT CALLBACK WindowProcedure(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    bool update = false;

    switch (message)
    {
    case WM_CREATE:
        CreateWindowContent(hwnd);
        hBitmap = LoadBitmap(GetModuleHandle(NULL), MAKEINTRESOURCE(POWERED));
        GetObject(hBitmap, sizeof(BITMAP), &bitmap);
        GetPrivateProfileString("General", "name", NULL, player_name, sizeof(player_name), inipath);
        GetPrivateProfileString("General", "master", NULL, server_master, sizeof(server_master), inipath);
        if (strlen(player_name) > 0) SetDlgItemText(hwnd, IDC_EDIT0, player_name);
        if (strlen(server_master) > 0) SetDlgItemText(hwnd, IDC_EDIT3, server_master);
        break;

    case WM_PAINT:
        hdc = BeginPaint(hwnd, &ps);
        hdcMem = CreateCompatibleDC(hdc);
        SelectObject(hdcMem, hBitmap);
        BitBlt(hdc, 13, 310, bitmap.bmWidth, bitmap.bmHeight, hdcMem, 0, 0, SRCCOPY);
        DeleteDC(hdcMem);
        EndPaint(hwnd, &ps);
        break;

    case WM_SIZE:
        if (wParam == SIZE_MINIMIZED)
            MinimizeToTray(hwnd);
        break;

    case WM_COMMAND:
        switch (wParam)
        {
        case IDC_BUTTON0:

            /* RakNet Game */

            if (peer->NumberOfConnections() == 0)
            {
                if (selectedServer != NULL /*|| Direct IP */)
                {
                    SystemAddress addr = *selectedServer;
                    char name[MAX_PLAYER_NAME], pwd[MAX_PASSWORD_SIZE];
                    GetDlgItemText(hwnd, IDC_EDIT0, name, sizeof(name));
                    GetDlgItemText(hwnd, IDC_EDIT1, pwd, sizeof(pwd));

                    if (strlen(name) < 3)
                    {
                        MessageBox(NULL, "Please sepcify a player name of at least 3 characters.", "Error", MB_OK | MB_ICONERROR);
                        break;
                    }

                    map<SystemAddress, ServerEntry>::iterator i;
                    i = serverList.find(*selectedServer);

                    bool NewVegas = (&i->second)->IsNewVegas();

                    if (NewVegas && !foundNewVegas)
                    {
                        MessageBox(NULL, "Could not find FalloutNV.exe!", "Error", MB_OK | MB_ICONERROR);
                        break;
                    }
                    else if (!NewVegas && !foundFallout3)
                    {
                        MessageBox(NULL, "Could not find Fallout3.exe!", "Error", MB_OK | MB_ICONERROR);
                        break;
                    }

                    MinimizeToTray(hwnd);
                    Bethesda::InitializeVaultMP(peer, addr, string(name), string(pwd), NewVegas);
                    Maximize(hwnd);

                    selectedServer = NULL;
                }
            }

            break;

        case IDC_BUTTON1:
            if (selectedServer != NULL) update = true;
            else break;

        case IDC_BUTTON2:

            /* RakNet Master Query */

            if (peer->NumberOfConnections() == 0)
            {
                if (!update) serverList.clear();

                SystemAddress master;
                char maddr[32];
                GetDlgItemText(hwnd, IDC_EDIT3, maddr, sizeof(maddr));

                if (strcmp(maddr, "") == 0)
                {
                    SetDlgItemText(hwnd, IDC_EDIT3, (char*) RAKNET_MASTER_ADDRESS);
                    master.SetBinaryAddress((char*) RAKNET_MASTER_ADDRESS);
                    master.SetPort(RAKNET_MASTER_PORT);
                }
                else
                {
                    master.SetBinaryAddress(strtok(maddr, ":"));
                    char* cport = strtok(NULL, ":");
                    master.SetPort(cport != NULL ? atoi(cport) : RAKNET_MASTER_PORT);
                }

                if (peer->Connect(master.ToString(false), master.GetPort(), MASTER_VERSION, sizeof(MASTER_VERSION), 0, 0, 3, 500, 0) == CONNECTION_ATTEMPT_STARTED)
                {
                    bool query = true;

                    bool lock = false;

                    Packet* packet;

                    while (query)
                    {
                        for (packet = peer->Receive(); packet; peer->DeallocatePacket(packet), packet = peer->Receive())
                        {
                            switch (packet->data[0])
                            {
                            case ID_CONNECTION_REQUEST_ACCEPTED:
                            {
                                BitStream query;

                                if (update)
                                {
                                    query.Write((MessageID) ID_MASTER_UPDATE);
                                    SystemAddress addr = *selectedServer;

                                    SystemAddress self = peer->GetExternalID(packet->systemAddress);

                                    if (strcmp(addr.ToString(false), packet->systemAddress.ToString(false)) == 0)
                                        addr.SetBinaryAddress("127.0.0.1");
                                    else if (strcmp(addr.ToString(false), "127.0.0.1") == 0)
                                        addr.SetBinaryAddress(self.ToString(false));

                                    query.Write(addr);
                                }
                                else
                                    query.Write((MessageID) ID_MASTER_QUERY);

                                peer->Send(&query, HIGH_PRIORITY, RELIABLE, 0, packet->systemAddress, false, 0);
                                break;
                            }
                            case ID_MASTER_QUERY:
                            {
                                BitStream query(packet->data, packet->length, false);
                                query.IgnoreBytes(sizeof(MessageID));

                                unsigned int size;
                                query.Read(size);

                                SendMessage(wndprogressbar, PBM_SETPOS, 0, 0);
                                SendMessage(wndprogressbar, PBM_SETRANGE, 0, MAKELONG(0, size));
                                SendMessage(wndprogressbar, PBM_SETSTEP, 1, 0);

                                for (int i = 0; i < size; i++)
                                {
                                    SystemAddress addr;
                                    RakString name, map;
                                    int players, playersMax, rsize;
                                    bool NewVegas;
                                    std::map<string, string> rules;

                                    query.Read(addr);
                                    query.Read(name);
                                    query.Read(map);
                                    query.Read(players);
                                    query.Read(playersMax);
                                    query.Read(NewVegas);
                                    query.Read(rsize);

                                    ServerEntry entry(name.C_String(), map.C_String(), pair<int, int>(players, playersMax), 999, NewVegas);

                                    for (int j = 0; j < rsize; j++)
                                    {
                                        RakString key, value;
                                        query.Read(key);
                                        query.Read(value);
                                        entry.SetServerRule(key.C_String(), value.C_String());
                                    }

                                    SystemAddress self = peer->GetExternalID(packet->systemAddress);

                                    if (strcmp(addr.ToString(false), "127.0.0.1") == 0)
                                        addr.SetBinaryAddress(packet->systemAddress.ToString(false));
                                    else if (strcmp(addr.ToString(false), self.ToString(false)) == 0)
                                        addr.SetBinaryAddress("127.0.0.1");

                                    serverList.insert(pair<SystemAddress, ServerEntry>(addr, entry));

                                    peer->Ping(addr.ToString(false), addr.GetPort(), false);

                                    SendMessage(wndprogressbar, PBM_STEPIT, 0, 0);
                                }

                                peer->CloseConnection(packet->systemAddress, true, 0, LOW_PRIORITY);

                                query = false;
                                break;
                            }
                            case ID_MASTER_UPDATE:
                            {
                                BitStream query(packet->data, packet->length, false);
                                query.IgnoreBytes(sizeof(MessageID));

                                SystemAddress addr;
                                query.Read(addr);

                                SystemAddress self = peer->GetExternalID(packet->systemAddress);

                                if (strcmp(addr.ToString(false), "127.0.0.1") == 0)
                                    addr.SetBinaryAddress(packet->systemAddress.ToString(false));
                                else if (strcmp(addr.ToString(false), self.ToString(false)) == 0)
                                    addr.SetBinaryAddress("127.0.0.1");

                                std::map<SystemAddress, ServerEntry>::iterator i;
                                i = serverList.find(addr);

                                if (query.GetNumberOfUnreadBits() > 0)
                                {
                                    RakString name, map;
                                    int players, playersMax, rsize;
                                    bool NewVegas;

                                    query.Read(name);
                                    query.Read(map);
                                    query.Read(players);
                                    query.Read(playersMax);
                                    query.Read(NewVegas);
                                    query.Read(rsize);

                                    ServerEntry* entry;

                                    if (i != serverList.end())
                                    {
                                        entry = &i->second;
                                        entry->SetServerName(name.C_String());
                                        entry->SetServerMap(map.C_String());
                                        entry->SetServerPlayers(pair<int, int>(players, playersMax));
                                    }
                                    else
                                    {
                                        std::pair<std::map<SystemAddress, ServerEntry>::iterator, bool> k;
                                        k = serverList.insert(pair<SystemAddress, ServerEntry>(addr, ServerEntry(name.C_String(), map.C_String(), pair<int, int>(players, playersMax), 999, NewVegas)));
                                        entry = &(k.first)->second;
                                    }

                                    for (int j = 0; j < rsize; j++)
                                    {
                                        RakString key, value;
                                        query.Read(key);
                                        query.Read(value);
                                        entry->SetServerRule(key.C_String(), value.C_String());
                                    }

                                    peer->Ping(addr.ToString(false), addr.GetPort(), false);
                                }
                                else if (i != serverList.end())
                                    serverList.erase(i);

                                peer->CloseConnection(packet->systemAddress, true, 0, LOW_PRIORITY);

                                query = false;
                                break;
                            }
                            case ID_UNCONNECTED_PONG:
                            {
                                BitStream query(packet->data, packet->length, false);
                                query.IgnoreBytes(sizeof(MessageID));

                                TimeMS ping;
                                query.Read(ping);

                                map<SystemAddress, ServerEntry>::iterator i;
                                i = serverList.find(packet->systemAddress);

                                if (i != serverList.end())
                                {
                                    ServerEntry* entry = &i->second;
                                    entry->SetServerPing(GetTimeMS() - ping);
                                }
                                break;
                            }
                            case ID_NO_FREE_INCOMING_CONNECTIONS:
                            case ID_CONNECTION_ATTEMPT_FAILED:
                            {
                                if (update && !lock)
                                {
                                    peer->Connect(selectedServer->ToString(false), selectedServer->GetPort(), 0, 0, 0, 0, 3, 500, 0);
                                    lock = true;
                                }
                                else
                                {
                                    if (update)
                                    {
                                        map<SystemAddress, ServerEntry>::iterator i;
                                        i = serverList.find(*selectedServer);

                                        if (i != serverList.end())
                                            serverList.erase(*selectedServer);
                                    }

                                    query = false;
                                }
                                break;
                            }
                            case ID_INVALID_PASSWORD:
                                MessageBox(NULL, "MasterServer version mismatch.\nPlease download the most recent binaries from www.vaultmp.com", "Error", MB_OK | MB_ICONERROR);
                            case ID_DISCONNECTION_NOTIFICATION:
                            case ID_CONNECTION_BANNED:
                            case ID_CONNECTION_LOST:
                                query = false;
                                break;
                            }
                        }

                        RakSleep(2);
                    }
                }
            }

            update = false;

            RefreshServerList();
            break;

        case IDC_BUTTON3:
            MessageBox(NULL, "Vault-Tec Multiplayer Mod is an Open-Source project.\n\ncode: Recycler (www.brickster.net)\nnetwork: RakNet (www.jenkinssoftware.com)\nscripting: The PAWN language (www.compuphase.com)\nmusic: uFMOD (ufmod.sourceforge.net)\n\nGreetings fly out to:\nmqidx, benG, ArminSeiko\n\nThanks to everyone contributing to the project (ideas, code, moral support etc). You guys keep it going :-)\n\nwww.vaultmp.com", "vaultmp credits", MB_OK | MB_ICONINFORMATION);
            break;

        case IDC_CHECK0:
            if (SendMessage(wndchiptune, BM_GETCHECK, 0, 0))
                uFMOD_PlaySong(MAKEINTRESOURCE(CHIPTUNE), GetModuleHandle(NULL), XM_RESOURCE);
            else
                uFMOD_StopSong();
            break;
        }
        break;

    case MSG_MINTRAYICON:
        if (wParam == ICON_MAIN && lParam == WM_LBUTTONUP)
            Maximize(hwnd);
        break;

    case WM_NOTIFY:
        switch (((LPNMHDR) lParam)->code)
        {
        case (LVN_FIRST - 14): // LVN_ITEMACTIVATE
        {
            HWND hwndFrom = (HWND)((LPNMHDR) lParam)->hwndFrom;

            if (hwndFrom == wndlistview)
            {
                LVITEM item;
                item.mask = LVIF_PARAM;
                item.iItem = ListView_GetNextItem(hwndFrom, -1, LVNI_SELECTED);
                item.iSubItem = 0;
                ListView_GetItem(hwndFrom, &item);

                selectedServer = (SystemAddress*) item.lParam;

                SendMessage(wndlistview2, LVM_DELETEALLITEMS, 0, 0);

                map<SystemAddress, ServerEntry>::iterator i;
                i = serverList.find(*selectedServer);

                if (i != serverList.end())
                {
                    ServerEntry* entry = &i->second;
                    std::map<string, string> rules = entry->GetServerRules();

                    for (map<string, string>::iterator k = rules.begin(); k != rules.end(); ++k)
                    {
                        string key = k->first;
                        string value = k->second;

                        char c_key[key.length()];
                        char c_value[value.length()];

                        strcpy(c_key, key.c_str());
                        strcpy(c_value, value.c_str());

                        Create2ColItem(wndlistview2, c_key, c_value);
                    }
                }
            }
            break;
        }
        case LVN_COLUMNCLICK:
        {
            NMLISTVIEW* nmlv = (NMLISTVIEW*) lParam;
            wndsortcur = (HWND)((LPNMHDR) lParam)->hwndFrom;
            ListView_SortItemsEx(wndsortcur, CompareProc, nmlv->iSubItem);
            sort = !sort;
            break;
        }
        }
        break;

    case WM_DESTROY:
        GetDlgItemText(hwnd, IDC_EDIT0, player_name, sizeof(player_name));
        GetDlgItemText(hwnd, IDC_EDIT3, server_master, sizeof(server_master));
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hwnd, message, wParam, lParam);
    }

    return 0;
}
