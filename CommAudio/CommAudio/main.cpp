/*------------------------------------------------------------------------------------------------------------------
-- SOURCE FILE: client.cpp - Application that allows users to upload, 
--
-- PROGRAM: inotd
--
-- FUNCTIONS:
-- LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
-- BOOL CALLBACK MainDlgProc(HWND, UINT, WPARAM, LPARAM);
-- HWND InitializeGUI(HINSTANCE, int);
-- void InitializeWindow(HWND);
-- BOOL CALLBACK ServerProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
-- BOOL CALLBACK ClientProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
-- void initializeClient(HWND hwnd, int type);
-- void initializeServer(HWND hwnd, int type);
-- void initializeButtons(void);
-- BOOL initializeLocalFile(HWND hwnd, char * filename);
-- void stopLocalFile(void);
-- BOOL browseLocalFiles(HWND hwnd, PTSTR pstrFileName, PTSTR pstrTitleName);
-- void writeAudio(LPSTR data, int size);
-- void freeBlocks(WAVEHDR* blockArray);
-- WAVEHDR* allocateBlocks(int size, int count);
-- static void CALLBACK waveOutProc(HWAVEOUT hWaveOut, UINT uMsg, DWORD dwInstance, DWORD dwParam1, DWORD dwParam2);
-- DWORD WINAPI receiveStream(LPVOID iValue);
-- DWORD WINAPI sendStream(LPVOID iValue);
-- DWORD WINAPI receiveMicThread(LPVOID iValue);
-- void serverDownload(WPARAM wParam, PTSTR	fileName);
-- void clientDownload(WPARAM wParam);
-- void sendFileList(WPARAM wParam);
-- void receiveFileList(WPARAM wParam, char buf[]);
-- void GetSelList(char * selItem);
-- void disconnectActions(void);
-- void connectActions(void);
-- void setActions(void);
-- void checkMenuItem(int);
-- void beginMicRecord(void);
-- void sockClose(HWND hwnd, WPARAM wParam, LPARAM lParam);
-- void Close(HWND hwnd);
-- void Command(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify);
-- int Create(HWND hwnd, LPCREATESTRUCT lpCreateStruct);
-- void Destroy(HWND hwnd);
-- void Paint(HWND hwnd);
-- void Size(HWND hwnd, UINT state, int cx, int cy);
-- void TCPSocket(HWND hwnd, WPARAM wParam, LPARAM lParam);
--
-- DATE: April 15 2018
--
-- REVISIONS: None
--
-- DESIGNER: Morgan Ariss, Anthony Vu, John Tee, Li-Yan Tong
--
-- PROGRAMMER: Morgan Ariss, Anthony Vu, John Tee, Li-Yan Tong
--
-- NOTES:
-- This program instantiates a tcp socket that connects to the given ip and port as the username given. Gives the program
-- for 1 second to connect to the server before timing out. After connecting to the server, shows other users who
-- connect to the chatroom and displays all messages received from the server along with who sent the message and
-- the time they sent it. Also allows the user to save the chat log into a file.
----------------------------------------------------------------------------------------------------------------------*/
#include "main.h"
#include "resource.h"

// Global Variables
HINSTANCE	ghInst;
HWND		ghWndMain, ghDlgMain, ghStatus;
HMENU		ghMenu;
HACCEL		ghAccel;
SOCKADDR_IN	remote, local, udp_remote, udp_local;
connectInfo	cInfo;

static CRITICAL_SECTION waveCriticalSection;
static WAVEHDR*			waveBlocks;
static volatile int		waveFreeBlockCount;
static int				waveCurrentBlock;
static int received = FALSE;

TCHAR	szAppName[] = TEXT("Comm Audio");

static OPENFILENAME ofn;
static MCIDEVICEID deviceID;

int				busyFlag;
static			WAVEFORMATEX pwfx;
HWAVEOUT		hWaveOut;
static BOOL		streamActive;
static HANDLE	streamThread;

/*------------------------------------------------------------------------------------------------------------------
-- FUNCTION: WinMain
--
-- DATE: April 15 2018
--
-- REVISIONS: None
--
-- DESIGNER: Morgan Ariss, Anthony Vu, John Tee, Li-Yan Tong
--
-- PROGRAMMER: 
--
-- INTERFACE: int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
--
-- RETURNS: exit code
--
-- NOTES:
-- Starting point of the program. Initializes the GUI and sends messages to WndProc.
----------------------------------------------------------------------------------------------------------------------*/
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	MSG msg;

	ghWndMain = InitializeGUI(hInstance, nCmdShow);

	while (GetMessage(&msg, NULL, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return (int)msg.wParam;
}

/*------------------------------------------------------------------------------------------------------------------
-- FUNCTION: InitializeGUI
--
-- DATE: April 15 2018
--
-- REVISIONS: None
--
-- DESIGNER: Morgan Ariss, Anthony Vu, John Tee, Li-Yan Tong
--
-- PROGRAMMER:
--
-- INTERFACE: HWND InitializeGUI(HINSTANCE hInst, int nCmdShow)
--
-- RETURNS: handle of the GUI
--
-- NOTES:
-- Initializes the GUI.
----------------------------------------------------------------------------------------------------------------------*/
HWND InitializeGUI(HINSTANCE hInst, int nCmdShow) {
	WNDCLASS	wc = { 0 };
	RECT		rect;
	POINT		ptWindow;

	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = (WNDPROC)WndProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = hInst;
	wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);

	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
	wc.lpszMenuName = MAKEINTRESOURCE(IDR_MENU);
	wc.lpszClassName = szAppName;

	if (!RegisterClass(&wc)) {
		return FALSE;
	}

	SystemParametersInfo(SPI_GETWORKAREA, 0, &rect, 0);
	ptWindow.x = (rect.right / 2) - (510 / 2);
	ptWindow.y = (rect.bottom / 2) - (280 / 2);

	ghWndMain = CreateWindow(szAppName, szAppName,
		WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_BORDER,
		ptWindow.x, ptWindow.y,
		510, 280,
		NULL, NULL, hInst, NULL);

	ShowWindow(ghWndMain, nCmdShow);
	UpdateWindow(ghWndMain);

	ghInst = hInst;

	streamActive = FALSE;

	return ghWndMain;
}

/*------------------------------------------------------------------------------------------------------------------
-- FUNCTION: initializeClient
--
-- DATE: April 15 2018
--
-- REVISIONS: None
--
-- DESIGNER: Morgan Ariss, Anthony Vu, John Tee, Li-Yan Tong
--
-- PROGRAMMER:
--
-- INTERFACE: void initializeClient(HWND hwnd, int type)
--
-- RETURNS: void
--
-- NOTES:
-- Sets up the socket for the client. UDP for multicast and TCP for file transfer.
----------------------------------------------------------------------------------------------------------------------*/
void initializeClient(HWND hwnd, int type)
{
	WSADATA	wsaData;
	char	arg, yes = 1;
	struct	ip_mreq mreq;

	if ((WSAStartup(MAKEWORD(2, 2), &wsaData)) == -1)
	{
		MessageBox(NULL, "WSAStartup failed!", "ERROR", MB_OK);
		WSACleanup();
	}

	if (type == SOCK_STREAM) {
		if ((cInfo.tcpSocket = socket(AF_INET, type, IPPROTO_TCP)) == INVALID_SOCKET)
			MessageBox(NULL, "Unable to create TCP socket!", "ERROR", MB_OK);

		if (setsockopt(cInfo.tcpSocket, SOL_SOCKET, SO_REUSEADDR, &arg, sizeof(arg)) == -1)
			MessageBox(NULL, "Unable to set SO_REUSEADDR", "ERROR", MB_OK);

		memset((char *)&remote, 0, sizeof(SOCKADDR_IN));
		remote.sin_family = AF_INET;
		remote.sin_addr.s_addr = inet_addr(cInfo.ip);
		remote.sin_port = htons(cInfo.tcp_port);

		WSAAsyncSelect(cInfo.tcpSocket, hwnd, WM_TCP_SOCKET, FD_CONNECT | FD_WRITE | FD_READ | FD_CLOSE);
	}
	else if (type == SOCK_DGRAM) {
		if ((cInfo.udpSocket = socket(AF_INET, type, IPPROTO_UDP)) == INVALID_SOCKET)
			MessageBox(NULL, "Unable to create UDP socket!", "ERROR", MB_OK);

		memset((char *)&udp_remote, 0, sizeof(SOCKADDR_IN));
		udp_remote.sin_family = AF_INET;
		udp_remote.sin_port = htons(cInfo.udp_port);

		if (cInfo.request == MULTI_STREAM) {
			if (setsockopt(cInfo.udpSocket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0) {
				MessageBox(NULL, "Reusing ADDR failed", "ERROR", MB_OK);
				ExitProcess(1);
			}
			udp_remote.sin_addr.s_addr = htonl(INADDR_ANY);

			if (bind(cInfo.udpSocket, (struct sockaddr *)&udp_remote, sizeof(udp_remote)) < 0) {
				MessageBox(NULL, "Multicast bind() error!", "ERROR", MB_OK);
				ExitProcess(1);
			}

			mreq.imr_multiaddr.s_addr = inet_addr(MULTICAST_GROUP);
			mreq.imr_interface.s_addr = htonl(INADDR_ANY);

			if (setsockopt(cInfo.udpSocket, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)&mreq, sizeof(mreq)) < 0) {
				MessageBox(NULL, "Multicast: Reusing ADDR failed", "ERROR", MB_OK);
				ExitProcess(1);
			}
		}
		else {
			udp_remote.sin_addr.s_addr = inet_addr(cInfo.ip);
		}

	}
}

/*------------------------------------------------------------------------------------------------------------------
-- FUNCTION: initializeServer
--
-- DATE: April 15 2018
--
-- REVISIONS: None
--
-- DESIGNER: Morgan Ariss, Anthony Vu, John Tee, Li-Yan Tong
--
-- PROGRAMMER:
--
-- INTERFACE: void initializeServer(HWND hwnd, int type)
--
-- RETURNS: void
--
-- NOTES:
-- Accepts connection requests from clients. 
----------------------------------------------------------------------------------------------------------------------*/
void initializeServer(HWND hwnd, int type)
{
	WSADATA	wsaData;
	char	arg;
	HOSTENT *thisHost;
	char	*ip;
	char	ipStatus[TEMP_BUFF_SIZE];

	memset(&ipStatus, '\0', sizeof(ipStatus));

	if ((WSAStartup(MAKEWORD(2, 2), &wsaData)) == -1)
	{
		MessageBox(NULL, "WSAStartup failed!", "ERROR", MB_OK);
		WSACleanup();
	}

	thisHost = gethostbyname("");
	ip = inet_ntoa(*(struct in_addr *)*thisHost->h_addr_list);

	strcat_s(ipStatus, sizeof(ipStatus), "IP: ");
	strcat_s(ipStatus, sizeof(ipStatus), ip);

	SendMessage(ghStatus, SB_SETTEXT, (WPARAM)parts[2], (LPARAM)ipStatus);

	if (type == SOCK_STREAM) {
		if ((cInfo.tcpSocket = socket(AF_INET, type, IPPROTO_TCP)) == INVALID_SOCKET)
			MessageBox(NULL, "Unable to create TCP socket!", "ERROR", MB_OK);

		if (setsockopt(cInfo.tcpSocket, SOL_SOCKET, SO_REUSEADDR, &arg, sizeof(arg)) == -1)
			MessageBox(NULL, "Unable to set SO_REUSEADDR", "ERROR", MB_OK);

		memset((char *)&local, 0, sizeof(SOCKADDR_IN));
		local.sin_family = AF_INET;
		local.sin_addr.s_addr = INADDR_ANY;
		local.sin_port = htons(cInfo.tcp_port);

		if (bind(cInfo.tcpSocket, (SOCKADDR *)&local, sizeof(SOCKADDR)) == SOCKET_ERROR)
			MessageBox(NULL, "Unable to bind address to socket!", "ERROR", MB_OK);

		if (listen(cInfo.tcpSocket, NUMCONNECTIONS))
			MessageBox(NULL, "Unable to set number of connections!", "ERROR", MB_OK);

		WSAAsyncSelect(cInfo.tcpSocket, hwnd, WM_TCP_SOCKET, FD_READ | FD_ACCEPT | FD_CLOSE);
	}
	else if (type == SOCK_DGRAM) {
		if ((cInfo.udpSocket = socket(AF_INET, type, IPPROTO_UDP)) == INVALID_SOCKET)
			MessageBox(NULL, "Unable to create UDP socket!", "ERROR", MB_OK);

		memset((char *)&udp_local, 0, sizeof(SOCKADDR_IN));
		udp_local.sin_family = AF_INET;
		udp_local.sin_port = htons(cInfo.udp_port);

		if (cInfo.request == MULTI_STREAM) {
			udp_local.sin_addr.s_addr = inet_addr(MULTICAST_GROUP);
		}
		else {
			udp_local.sin_addr.s_addr = INADDR_ANY;
			if (bind(cInfo.udpSocket, (SOCKADDR *)&udp_local, sizeof(SOCKADDR)) == SOCKET_ERROR)
				MessageBox(NULL, "Unable to bind address to socket!", "ERROR", MB_OK);
		}
	}
}

/*------------------------------------------------------------------------------------------------------------------
-- FUNCTION: sendFileList
--
-- DATE: April 15 2018
--
-- REVISIONS: None
--
-- DESIGNER: Morgan Ariss, Anthony Vu, John Tee, Li-Yan Tong
--
-- PROGRAMMER:
--
-- INTERFACE: void sendFileList(WPARAM wParam)
--
-- RETURNS: void
--
-- NOTES:
-- If a client has requested to download a file, send all .wav files to the client. 
----------------------------------------------------------------------------------------------------------------------*/
void sendFileList(WPARAM wParam)
{
	char buf[FILE_BUFF_SIZE];
	WIN32_FIND_DATA wfd;
	HANDLE FF;
	memset(buf, '\0', FILE_BUFF_SIZE);
	strcpy_s(buf, sizeof(buf), "FILES:");

	if ((FF = FindFirstFile("*.wav", &wfd)) == INVALID_HANDLE_VALUE)
	{
		MessageBox(ghWndMain, (LPCSTR)"Unable to find any .wav files in current directory.",
			(LPCSTR)"Error!", MB_OK | MB_ICONSTOP);
		return;
	}

	do
	{
		strcat_s(buf, sizeof(buf), wfd.cFileName);
		strcat_s(buf, sizeof(buf), ":");
	} while (FindNextFile(FF, &wfd));

	if (cInfo.request != MULTI_STREAM) {
		if (send(wParam, buf, strlen(buf), 0) == -1)
		{
			if (WSAGetLastError() != WSAEWOULDBLOCK)
			{
				MessageBox(NULL, TEXT("send() failed with error \n"), NULL, MB_OK);
				closesocket(wParam);
			}
		}
	}
	else {
		receiveFileList(wParam, buf);
	}
}

/*------------------------------------------------------------------------------------------------------------------
-- FUNCTION: receiveFileList
--
-- DATE: April 15 2018
--
-- REVISIONS: None
--
-- DESIGNER: Morgan Ariss, Anthony Vu, John Tee, Li-Yan Tong
--
-- PROGRAMMER:
--
-- INTERFACE: void receiveFileList(WPARAM wParam, char buf[])
--
-- RETURNS: void
--
-- NOTES:
-- Display all .wav files to the ListBox in the GUI client-side.
----------------------------------------------------------------------------------------------------------------------*/
void receiveFileList(WPARAM wParam, char buf[])
{
	char *pch, *nextToken;
	HWND list;

	pch = strtok_s(buf, ":", &nextToken);
	pch = strtok_s(NULL, ":", &nextToken);
	while (pch != NULL)
	{
		list = GetDlgItem(ghDlgMain, IDC_LST_PLAY);

		ListBox_InsertString(list, -1, pch);
		ListBox_SetCurSel(list, ListBox_GetCount(list) - 1);

		SetFocus(list);
		pch = strtok_s(NULL, ":", &nextToken);
	}
}

/*------------------------------------------------------------------------------------------------------------------
-- FUNCTION: GetSelList
--
-- DATE: April 15 2018
--
-- REVISIONS: None
--
-- DESIGNER: Morgan Ariss, Anthony Vu, John Tee, Li-Yan Tong
--
-- PROGRAMMER:
--
-- INTERFACE: void GetSelList(char * selItem)
--
-- RETURNS: void
--
-- NOTES:
-- Get all .wav files displayed in the ListBox in the GUI client-side.
----------------------------------------------------------------------------------------------------------------------*/
void GetSelList(char * selItem) {
	HWND list = GetDlgItem(ghDlgMain, IDC_LST_PLAY);

	ListBox_GetText(list, ListBox_GetCurSel(list), selItem);
}

/*------------------------------------------------------------------------------------------------------------------
-- FUNCTION: browseLocalFiles
--
-- DATE: April 15 2018
--
-- REVISIONS: None
--
-- DESIGNER: Morgan Ariss, Anthony Vu, John Tee, Li-Yan Tong
--
-- PROGRAMMER:
--
-- INTERFACE: BOOL browseLocalFiles(HWND hwnd, PTSTR pstrFileName, PTSTR pstrTitleName)
--
-- RETURNS: True if a file has been selected
--
-- NOTES:
-- Opens a dialog box to allow the user to select a file.
----------------------------------------------------------------------------------------------------------------------*/
BOOL browseLocalFiles(HWND hwnd, PTSTR pstrFileName, PTSTR pstrTitleName)
{
	ofn.hwndOwner = hwnd;
	ofn.lpstrFile = pstrFileName;
	ofn.lpstrFileTitle = pstrTitleName;
	ofn.Flags = OFN_HIDEREADONLY | OFN_CREATEPROMPT;
	return GetOpenFileName(&ofn);
}

/*------------------------------------------------------------------------------------------------------------------
-- FUNCTION: initializeButtons
--
-- DATE: April 15 2018
--
-- REVISIONS: None
--
-- DESIGNER: Morgan Ariss, Anthony Vu, John Tee, Li-Yan Tong
--
-- PROGRAMMER:
--
-- INTERFACE: void initializeButtons()
--
-- RETURNS: void
--
-- NOTES:
-- Initializes GUI elements depending on the mode chosen by the user.
----------------------------------------------------------------------------------------------------------------------*/
void initializeButtons() {
	if (cInfo.request == MULTI_STREAM) {
		SetWindowText(GetDlgItem(ghDlgMain, IDC_BTN_PAUSE), (LPCSTR)"Mute");
	}
	else {
		SetWindowText(GetDlgItem(ghDlgMain, IDC_BTN_PAUSE), (LPCSTR)"Pause");
	}
	EnableWindow(GetDlgItem(ghDlgMain, IDC_BTN_PAUSE), FALSE);
	EnableWindow(GetDlgItem(ghDlgMain, IDC_BTN_BROADCAST), FALSE);

	if (cInfo.mode == SERVER) {
		if (cInfo.request == MULTI_STREAM) {
			EnableWindow(GetDlgItem(ghDlgMain, IDC_BTN_BROADCAST), TRUE);
		}
	}
	else if (cInfo.mode == CLIENT) {
		if (cInfo.request == SINGLE_DL) {
			EnableWindow(GetDlgItem(ghDlgMain, IDC_BTN_DOWNLOAD), TRUE);
		}
		else if (cInfo.request == SINGLE_UP) {
		}
		else if (cInfo.request == SINGLE_STREAM) {
			EnableWindow(GetDlgItem(ghDlgMain, IDC_BTN_PAUSE), TRUE);
			EnableWindow(GetDlgItem(ghDlgMain, IDC_BTN_DOWNLOAD), TRUE);
		}
		else if (cInfo.request == MULTI_STREAM) {
			EnableWindow(GetDlgItem(ghDlgMain, IDC_BTN_PAUSE), TRUE);
		}
		else if (cInfo.request == MICROPHONE) {
		}
	}
}

/*------------------------------------------------------------------------------------------------------------------
-- FUNCTION: connectActions
--
-- DATE: April 15 2018
--
-- REVISIONS: None
--
-- DESIGNER: Morgan Ariss, Anthony Vu, John Tee, Li-Yan Tong
--
-- PROGRAMMER:
--
-- INTERFACE: void connectActions()
--
-- RETURNS: void
--
-- NOTES:
-- Enables and disables menu items once the user clicks the connect button.
----------------------------------------------------------------------------------------------------------------------*/
void connectActions() {
	EnableMenuItem(ghMenu, ID_FILE_CONNECT, MF_GRAYED);
	EnableMenuItem(ghMenu, ID_FILE_DISCONNECT, MF_ENABLED);
	EnableMenuItem(ghMenu, ID_FILE_LOCAL, MF_GRAYED);

	busyFlag = NETWORKPLAY;
	stopLocalFile();
}

/*------------------------------------------------------------------------------------------------------------------
-- FUNCTION: disconnectActions
--
-- DATE: April 15 2018
--
-- REVISIONS: None
--
-- DESIGNER: Morgan Ariss, Anthony Vu, John Tee, Li-Yan Tong
--
-- PROGRAMMER:
--
-- INTERFACE: void disconnectActions()
--
-- RETURNS: void
--
-- NOTES:
-- Enables and disables menu items once the user clicks the disconnect button.
----------------------------------------------------------------------------------------------------------------------*/
void disconnectActions() {
	EnableMenuItem(ghMenu, ID_FILE_CONNECT, MF_GRAYED);
	EnableMenuItem(ghMenu, ID_FILE_DISCONNECT, MF_GRAYED);
	EnableMenuItem(ghMenu, ID_FILE_LOCAL, MF_ENABLED);

	checkMenuItem(0);
}

/*------------------------------------------------------------------------------------------------------------------
-- FUNCTION: setActions
--
-- DATE: April 15 2018
--
-- REVISIONS: None
--
-- DESIGNER: Morgan Ariss, Anthony Vu, John Tee, Li-Yan Tong
--
-- PROGRAMMER:
--
-- INTERFACE: void setActions()
--
-- RETURNS: void
--
-- NOTES:
-- Enables and disables menu items depending on the mode that the user has chosen.
----------------------------------------------------------------------------------------------------------------------*/
void setActions() {
	if (cInfo.mode == CLIENT) {
		CheckMenuItem(ghMenu, ID_MODE_CLIENT, MF_CHECKED);
		CheckMenuItem(ghMenu, ID_MODE_SERVER, MF_UNCHECKED);
		EnableMenuItem(ghMenu, ID_FILE_CONNECT, MF_GRAYED);
		EnableMenuItem(ghMenu, ID_FILE_DISCONNECT, MF_GRAYED);

		EnableMenuItem(ghMenu, ID_SINGLE_DL, MF_ENABLED);
		EnableMenuItem(ghMenu, ID_SINGLE_UP, MF_ENABLED);
		EnableMenuItem(ghMenu, ID_SINGLE_STREAM, MF_ENABLED);
		EnableMenuItem(ghMenu, ID_MULTI_STREAM, MF_ENABLED);
		EnableMenuItem(ghMenu, ID_2WAY_MICROPHONE, MF_ENABLED);
	}
	else if (cInfo.mode == SERVER) {
		CheckMenuItem(ghMenu, ID_MODE_CLIENT, MF_UNCHECKED);
		CheckMenuItem(ghMenu, ID_MODE_SERVER, MF_CHECKED);
		EnableMenuItem(ghMenu, ID_FILE_CONNECT, MF_ENABLED);
		EnableMenuItem(ghMenu, ID_FILE_DISCONNECT, MF_GRAYED);

		EnableMenuItem(ghMenu, ID_SINGLE_DL, MF_GRAYED);
		EnableMenuItem(ghMenu, ID_SINGLE_UP, MF_GRAYED);
		EnableMenuItem(ghMenu, ID_SINGLE_STREAM, MF_GRAYED);
		EnableMenuItem(ghMenu, ID_MULTI_STREAM, MF_GRAYED);
		EnableMenuItem(ghMenu, ID_2WAY_MICROPHONE, MF_GRAYED);
	}
	else {
		CheckMenuItem(ghMenu, ID_MODE_CLIENT, MF_UNCHECKED);
		CheckMenuItem(ghMenu, ID_MODE_SERVER, MF_UNCHECKED);
		EnableMenuItem(ghMenu, ID_FILE_CONNECT, MF_GRAYED);
		EnableMenuItem(ghMenu, ID_FILE_DISCONNECT, MF_GRAYED);

		EnableMenuItem(ghMenu, ID_SINGLE_DL, MF_GRAYED);
		EnableMenuItem(ghMenu, ID_SINGLE_UP, MF_GRAYED);
		EnableMenuItem(ghMenu, ID_SINGLE_STREAM, MF_GRAYED);
		EnableMenuItem(ghMenu, ID_MULTI_STREAM, MF_GRAYED);
		EnableMenuItem(ghMenu, ID_2WAY_MICROPHONE, MF_GRAYED);
	}
}

/*------------------------------------------------------------------------------------------------------------------
-- FUNCTION: checkMenuItem
--
-- DATE: April 15 2018
--
-- REVISIONS: None
--
-- DESIGNER: Morgan Ariss, Anthony Vu, John Tee, Li-Yan Tong
--
-- PROGRAMMER:
--
-- INTERFACE: void checkMenuItem(int item)
--
-- RETURNS: void
--
-- NOTES:
-- Sets the state of the specified menu item's check-mark attribute to either selected or clear. 
----------------------------------------------------------------------------------------------------------------------*/
void checkMenuItem(int item) {
	CheckMenuItem(ghMenu, ID_SINGLE_DL, MF_UNCHECKED);
	CheckMenuItem(ghMenu, ID_SINGLE_UP, MF_UNCHECKED);
	CheckMenuItem(ghMenu, ID_SINGLE_STREAM, MF_UNCHECKED);
	CheckMenuItem(ghMenu, ID_MULTI_STREAM, MF_UNCHECKED);
	CheckMenuItem(ghMenu, ID_2WAY_MICROPHONE, MF_UNCHECKED);

	if (item == 0) {
		CheckMenuItem(ghMenu, ID_MODE_CLIENT, MF_UNCHECKED);
		CheckMenuItem(ghMenu, ID_MODE_SERVER, MF_UNCHECKED);
		EnableMenuItem(ghMenu, ID_FILE_CONNECT, MF_GRAYED);
		EnableMenuItem(ghMenu, ID_FILE_DISCONNECT, MF_GRAYED);
	}
	else {
		EnableMenuItem(ghMenu, ID_FILE_CONNECT, MF_ENABLED);
		CheckMenuItem(ghMenu, item, MF_CHECKED);
	}
}

/*------------------------------------------------------------------------------------------------------------------
-- FUNCTION: initializeLocalFile
--
-- DATE: April 15 2018
--
-- REVISIONS: None
--
-- DESIGNER: Morgan Ariss, Anthony Vu, John Tee, Li-Yan Tong
--
-- PROGRAMMER:
--
-- INTERFACE: BOOL initializeLocalFile(HWND hwnd, char * fileName)
--
-- RETURNS: true if initialization of file is successful
--
-- NOTES:
-- Sets up the device information and opens audio file, then plays the audio
----------------------------------------------------------------------------------------------------------------------*/
BOOL initializeLocalFile(HWND hwnd, char * fileName)
{
	MCI_WAVE_OPEN_PARMS	openParams;
	MCI_PLAY_PARMS		playParams;
	int					errno;

	memset(&openParams, 0, sizeof(MCI_WAVE_OPEN_PARMS));
	openParams.lpstrDeviceType = "waveaudio";
	openParams.lpstrElementName = fileName;
	errno = mciSendCommand(0, MCI_OPEN, MCI_WAIT | MCI_OPEN_TYPE | MCI_OPEN_ELEMENT,
		(DWORD_PTR)&openParams);
	if (errno != 0)
	{
		MessageBox(ghWndMain, (LPCSTR)"Unable to open file.",
			(LPCSTR)"Error!", MB_OK | MB_ICONSTOP);
		return FALSE;
	}
	else
	{
		deviceID = openParams.wDeviceID;
		memset(&playParams, 0, sizeof(playParams));
		playParams.dwCallback = (DWORD_PTR)hwnd;
		errno = mciSendCommand(deviceID, MCI_PLAY, MCI_NOTIFY, (DWORD_PTR)&playParams);
		if (errno != 0)
			return FALSE;
	}

	return TRUE;
}

/*------------------------------------------------------------------------------------------------------------------
-- FUNCTION: stopLocalFile
--
-- DATE: April 15 2018
--
-- REVISIONS: None
--
-- DESIGNER: Morgan Ariss, Anthony Vu, John Tee, Li-Yan Tong
--
-- PROGRAMMER:
--
-- INTERFACE: void stopLocalFile(void)
--
-- RETURNS: void
--
-- NOTES:
-- Stops the audio file and close the device
----------------------------------------------------------------------------------------------------------------------*/
void stopLocalFile(void)
{
	MCI_GENERIC_PARMS stop;
	MCI_GENERIC_PARMS close;

	if (deviceID)
	{
		memset(&stop, 0, sizeof(MCI_GENERIC_PARMS));
		memset(&close, 0, sizeof(MCI_GENERIC_PARMS));

		mciSendCommand(deviceID, MCI_STOP, MCI_WAIT, (DWORD_PTR)&stop);
		mciSendCommand(deviceID, MCI_CLOSE, MCI_WAIT, (DWORD_PTR)&close);

		deviceID = 0;
	}
}

/*------------------------------------------------------------------------------------------------------------------
-- FUNCTION: beginMicRecord
--
-- DATE: April 15 2018
--
-- REVISIONS: None
--
-- DESIGNER: Morgan Ariss, Anthony Vu, John Tee, Li-Yan Tong
--
-- PROGRAMMER:
--
-- INTERFACE: void beginMicRecord()
--
-- RETURNS: void
--
-- NOTES:
-- Set up buffers, wave format, and WAVEHDR struct.
----------------------------------------------------------------------------------------------------------------------*/
void beginMicRecord()
{
	pWaveHdr1 = reinterpret_cast<PWAVEHDR> (malloc(sizeof(WAVEHDR)));
	pWaveHdr2 = reinterpret_cast<PWAVEHDR> (malloc(sizeof(WAVEHDR)));

	pSaveBuff = reinterpret_cast<PBYTE>(malloc(1));

	pBuff1 = reinterpret_cast<PBYTE> (malloc(INP_BUFFER_SIZE));
	pBuff2 = reinterpret_cast<PBYTE> (malloc(INP_BUFFER_SIZE));

	if (!pBuff1 || !pBuff2)
	{
		if (pBuff1)
			free(pBuff1);
		if (pBuff2)
			free(pBuff2);

		MessageBox(ghWndMain, szMemError, NULL, MB_ICONEXCLAMATION | MB_OK);

		return;
	}

	waveform.nSamplesPerSec = 44100; /* sample rate */
	waveform.wBitsPerSample = 16; /* sample size */
	waveform.nChannels = 2; /* channels*/
	waveform.cbSize = 0; /* size of _extra_ info */
	waveform.wFormatTag = WAVE_FORMAT_PCM;
	waveform.nBlockAlign = (waveform.wBitsPerSample * waveform.nChannels) >> 3;
	waveform.nAvgBytesPerSec = waveform.nBlockAlign * waveform.nSamplesPerSec;

	waveInClose(hWaveIn);
	if (waveInOpen(&hWaveIn, WAVE_MAPPER, &waveform, (DWORD)ghWndMain, 0, CALLBACK_WINDOW))
	{
		free(pBuff1);
		free(pBuff2);
		MessageBox(ghWndMain, szOpenError, NULL, MB_ICONEXCLAMATION | MB_OK);
	}

	/* set up headers and prepare them */
	pWaveHdr1->lpData = reinterpret_cast<LPSTR> (pBuff1);
	pWaveHdr1->dwBufferLength = INP_BUFFER_SIZE;
	pWaveHdr1->dwBytesRecorded = 0;
	pWaveHdr1->dwUser = 0;
	pWaveHdr1->dwFlags = 0;
	pWaveHdr1->dwLoops = 1;
	pWaveHdr1->lpNext = NULL;
	pWaveHdr1->reserved = 0;
	waveInPrepareHeader(hWaveIn, pWaveHdr1, sizeof(WAVEHDR));

	pWaveHdr2->lpData = reinterpret_cast<LPSTR> (pBuff2);
	pWaveHdr2->dwBufferLength = INP_BUFFER_SIZE;
	pWaveHdr2->dwBytesRecorded = 0;
	pWaveHdr2->dwUser = 0;
	pWaveHdr2->dwFlags = 0;
	pWaveHdr2->dwLoops = 1;
	pWaveHdr2->lpNext = NULL;
	pWaveHdr2->reserved = 0;
	waveInPrepareHeader(hWaveIn, pWaveHdr2, sizeof(WAVEHDR));
}

/*-------------------------------------------------------------------------------------------------
--	FUNCTION:	ReceiveStream()
--
--	DATE:		April 15, 2018
--
--	DESIGNER:	Morgan Ariss, Anthony Vu, John Tee, Li-Yan Tong
--
--	PROGRAMMER:	Morgan Ariss
--
--	INTERFACE:	receiveStream(LPVOID iValue)
--
--	ARGUMENTS:	LPVOID iValue
--
--	RETURNS:	DWORD; always void
--
--	NOTES:
--		This function handles sending the receiving the audio stream. A circular buffer is created
--		using memory allocation, where blocks are assigned and freed in a circular manner. This
--		allows clients to join into the server late and still be able to listen to a normal stream.
--		The memory allocation is used to overwrite the least recent blocks.
--
-------------------------------------------------------------------------------------------------*/
DWORD WINAPI receiveStream(LPVOID iValue)
{
	WAVEFORMATEX	wfx;
	char			buff[BLOCK_SIZE];
	int				i, n, rLen;
	DWORD			outBytes = 0;
	const char*		play_byte = "1";
	BOOL			firstRun = TRUE;

	rLen = sizeof(udp_remote);

	/* initialise the module variables */
	waveBlocks = allocateBlocks(BLOCK_SIZE, BLOCK_COUNT);
	waveFreeBlockCount = BLOCK_COUNT;

	waveCurrentBlock = 0;
	InitializeCriticalSection(&waveCriticalSection);

	while (TRUE)
	{
		if (cInfo.request != MULTI_STREAM)
		{
			sendto(cInfo.udpSocket, play_byte, sizeof(play_byte), 0, (struct sockaddr *)&udp_remote, rLen);
		}

		if ((n = recvfrom(cInfo.udpSocket, buff, sizeof(buff), 0, (struct sockaddr *)&udp_remote, &rLen)) <= 0)
		{
			waveOutClose(hWaveOut);
			ExitThread(0);
		}

		if (strncmp(buff, "RIFF", 4) == 0)
		{
			memcpy(&wfx, buff + 20, sizeof(wfx));

			if (cInfo.request != MULTI_STREAM || firstRun == TRUE) {
				waveOutClose(hWaveOut);

				if (waveOutOpen(&hWaveOut, WAVE_MAPPER, &wfx, (DWORD_PTR)waveOutProc,
					(DWORD_PTR)&waveFreeBlockCount, CALLBACK_FUNCTION) != MMSYSERR_NOERROR)
				{
					ExitProcess(1);
				}
				firstRun = FALSE;
			}
		}

		if (n == 0)
			break;
		else if (n < sizeof(buff) && n != WAVE_HEAD_SIZE)
		{
			memset(buff + n, 0, sizeof(buff) - n);
			writeAudio(buff, n);
			break;
		}

		writeAudio(buff, n);
	}

	while (waveFreeBlockCount < BLOCK_COUNT)
		Sleep(10);

	for (i = 0; i < waveFreeBlockCount; i++)
	{
		if (waveBlocks[i].dwFlags & WHDR_PREPARED)
			waveOutUnprepareHeader(hWaveOut, &waveBlocks[i], sizeof(WAVEHDR));
	}
	DeleteCriticalSection(&waveCriticalSection);
	freeBlocks(waveBlocks);
	waveOutClose(hWaveOut);
	streamActive = FALSE;
	ExitThread(0);
}

/*------------------------------------------------------------------------------------------------------------------
-- FUNCTION: sendStream
--
-- DATE: April 15 2018
--
-- REVISIONS: None
--
-- DESIGNER: Morgan Ariss, Anthony Vu, John Tee, Li-Yan Tong
--
-- PROGRAMMER:
--
-- INTERFACE: DWORD WINAPI sendStream(LPVOID iValue)
--
-- RETURNS: DWORD
--
-- NOTES:
-- Thread for streaming audio to all connected clients (multicast).
----------------------------------------------------------------------------------------------------------------------*/
DWORD WINAPI sendStream(LPVOID iValue)
{
	HANDLE	hFile;
	int		rLen;
	DWORD	readBytes;

	char	buff[BLOCK_SIZE];

	if ((hFile = CreateFile(cInfo.DLfileName, GENERIC_READ, FILE_SHARE_READ, NULL,
		OPEN_EXISTING, 0, NULL)) == INVALID_HANDLE_VALUE)
	{
		MessageBox(ghWndMain, (LPCSTR)"Unable to open file.",
			(LPCSTR)"Error!", MB_OK | MB_ICONSTOP);
		ExitProcess(1);
	}

	rLen = sizeof(udp_local);

	if (cInfo.request != MULTI_STREAM) {
		recvfrom(cInfo.udpSocket, 0, 0, 0, (struct sockaddr *)&udp_remote, &rLen);
	}

	while (TRUE)
	{
		if (!ReadFile(hFile, buff, sizeof(buff), &readBytes, NULL))
		{
			closesocket(cInfo.udpSocket);
			CloseHandle(hFile);
			ExitThread(0);
		}

		if (strncmp(buff, "RIFF", 4) == 0)
		{
			memcpy(&cInfo.waveFormat, buff, sizeof(WAVEFORMATEX) + 20);
		}

		if (cInfo.newClient == TRUE) {
			cInfo.newClient = FALSE;
			sendto(cInfo.udpSocket, cInfo.waveFormat, sizeof(cInfo.waveFormat), 0, (struct sockaddr *)&udp_local, rLen);
		}

		if (readBytes == 0)
		{
			CloseHandle(hFile);
			ExitThread(0);
		}
		if (readBytes < sizeof(buff))
			memset(buff + readBytes, 0, sizeof(buff) - readBytes);


		if (cInfo.request == MULTI_STREAM) {
			sendto(cInfo.udpSocket, buff, BLOCK_SIZE, 0, (struct sockaddr *)&udp_local, rLen);
			Sleep(225);
		}
		else {
			sendto(cInfo.udpSocket, buff, BLOCK_SIZE, 0, (struct sockaddr *)&udp_remote, rLen);

			recvfrom(cInfo.udpSocket, 0, 0, 0, (struct sockaddr *)&udp_remote, &rLen);
		}
	}
	ExitThread(0);
}

static void CALLBACK waveOutProc(HWAVEOUT hWaveOut, UINT uMsg, DWORD dwInstance, DWORD dwParam1, DWORD dwParam2)
/*-----------------------------------------------------------------------------
--	FUNCTION:		waveOutProc
--
--	DATE:			April 5th, 2018
--
--	REVISIONS:
--
--	DESIGNER(S):	Morgan Ariss
--	PROGRAMMER(S):	Morgan Ariss
--
--	INTERFACE:		waveOutProc(HWAVEOUT hWaveOut, UINT uMsg, DWORD dwInstance,
--								DWORD dwParam1, DWORD dwParam2)
--
--	ARGUMENTS:		HWAVEOUT hWaveOut: Handle to the audio output device
--					UINT uMesg: Message that the procedure handles
--					DWORD dwInstance: The procedure's instance
--					DWORD dwParam1 & dwParam2: Unknown and forgotten.
--
--	RETURNS:		void
--
--	NOTES: 
--		The callback function which is used in the asynchronous call to play
--		the audio file this work is done in a separate thread. It tracks free
--		blocks in a critical section counteracted by the writeAudio function.
-----------------------------------------------------------------------------*/
{
	int* freeBlockCounter = (int*)dwInstance;

	if (uMsg != WOM_DONE)
		return;

	/* Critical Section */
	EnterCriticalSection(&waveCriticalSection);
	(*freeBlockCounter)++;
	LeaveCriticalSection(&waveCriticalSection);
}


/*-----------------------------------------------------------------------------
--	FUNCTION:		allocateBlocks
--
--	DATE:			April 7th, 2018
--
--	REVISIONS:
--
--	DESIGNER(S):	Morgan Ariss
--
--	PROGRAMMER(S):	Morgan Ariss
--
--	INTERFACE:		allocateBlocks(int size, int count)
--						
--	ARGUMENTS:		int size: size of each block
--					int count: number of blocks
--
--	RETURNS:		WAVEHDR*: Pointer to the allocated blocks.
--
--	NOTES: 
--		Allocates a buffer for data to be received in. Frees up any existing blocks.
--		Performs a heapAlloc to ready the entire set; if this fails the process exits.
--		Sets up pointers for each bit, returns the WAVEHDR* of blocks.
-----------------------------------------------------------------------------*/
WAVEHDR* allocateBlocks(int size, int count)
{
	unsigned char* buff;
	int i;
	WAVEHDR* blocks;
	DWORD totalbuffSize = (size + sizeof(WAVEHDR)) * count;

	/* Free any current memory blocks */
	freeBlocks(waveBlocks);

	/* allocate memory for the entire set in one go */
	if ((buff = reinterpret_cast<unsigned char *> (HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, totalbuffSize))) == NULL)
	{
		// Memory allocation error
		ExitProcess(1);
	}

	/*  set up pointers to each bit */
	blocks = (WAVEHDR*)buff;
	buff += sizeof(WAVEHDR) * count;

	for (i = 0; i < count; i++)
	{
		blocks[i].dwBufferLength = size;
		blocks[i].lpData = reinterpret_cast<LPSTR> (buff);
		buff += size;
	}

	return blocks;
}

/*------------------------------------------------------------------------------------------------------------------
-- FUNCTION: freeBlocks
--
-- DATE: April 15 2018
--
-- REVISIONS: None
--
-- DESIGNER: Morgan Ariss, Anthony Vu, John Tee, Li-Yan Tong
--
-- PROGRAMMER:
--
-- INTERFACE: void freeBlocks(WAVEHDR* blockArray)
--
-- RETURNS: void
--
-- NOTES:
-- Free WAVEHDR struct
----------------------------------------------------------------------------------------------------------------------*/
void freeBlocks(WAVEHDR* blockArray)
{
	HeapFree(GetProcessHeap(), 0, blockArray);
}

/*------------------------------------------------------------------------------------------------------------------
-- FUNCTION: writeAudio
--
-- DATE: April 15 2018
--
-- REVISIONS: None
--
-- DESIGNER: Morgan Ariss, Anthony Vu, John Tee, Li-Yan Tong
--
-- PROGRAMMER:
--
-- INTERFACE: void writeAudio(LPSTR data, int size)
--
-- RETURNS: void
--
-- NOTES:
-- Write audio data in blocks to the WAVEHDR struct.
----------------------------------------------------------------------------------------------------------------------*/
void writeAudio(LPSTR data, int size)
{
	WAVEHDR* current;
	int remain;
	current = &waveBlocks[waveCurrentBlock];

	while (size > 0)
	{
		/* first make sure the header we're going to use is unprepared */
		if (current->dwFlags & WHDR_PREPARED)
			waveOutUnprepareHeader(hWaveOut, current, sizeof(WAVEHDR));

		if (size < (int)(BLOCK_SIZE - current->dwUser))
		{
			memcpy(current->lpData + current->dwUser, data, size);
			current->dwUser += size;
			break;
		}

		remain = BLOCK_SIZE - current->dwUser; /* Comment out? */
		memcpy(current->lpData + current->dwUser, data, remain);
		size -= remain;
		data += remain;
		current->dwBufferLength = BLOCK_SIZE;
		waveOutPrepareHeader(hWaveOut, current, sizeof(WAVEHDR));
		waveOutWrite(hWaveOut, current, sizeof(WAVEHDR));

		/* Critical section */
		EnterCriticalSection(&waveCriticalSection);
		waveFreeBlockCount--;
		LeaveCriticalSection(&waveCriticalSection);


		/* wait for a block to become free */
		while (!waveFreeBlockCount)
			Sleep(10);

		/* point to the next block */
		waveCurrentBlock++;
		waveCurrentBlock %= BLOCK_COUNT;
		current = &waveBlocks[waveCurrentBlock];
		current->dwUser = 0;
	}
}

/*------------------------------------------------------------------------------------------------------------------
-- FUNCTION: receiveMicThread
--
-- DATE: April 15 2018
--
-- REVISIONS: None
--
-- DESIGNER: Morgan Ariss, Anthony Vu, John Tee, Li-Yan Tong
--
-- PROGRAMMER:
--
-- INTERFACE: DWORD WINAPI receiveMicThread(LPVOID iValue)
--
-- RETURNS: DWORD
--
-- NOTES:
-- Thread to receive and send audio from microphones.
----------------------------------------------------------------------------------------------------------------------*/
DWORD WINAPI receiveMicThread(LPVOID iValue) {
	char micBuff[BLOCK_SIZE];
	int rLen;
	const char* play_byte = "1";

	waveBlocks = allocateBlocks(BLOCK_SIZE, BLOCK_COUNT);
	waveFreeBlockCount = BLOCK_COUNT;
	waveCurrentBlock = 0;
	InitializeCriticalSection(&waveCriticalSection);

	rLen = sizeof(udp_remote);

	waveform.nSamplesPerSec = 44100; /* sample rate */
	waveform.wBitsPerSample = 16; /* sample size */
	waveform.nChannels = 2; /* channels*/
	waveform.cbSize = 0; /* size of _extra_ info */

	waveform.wFormatTag = WAVE_FORMAT_PCM;
	waveform.nBlockAlign = (waveform.wBitsPerSample * waveform.nChannels) >> 3;
	waveform.nAvgBytesPerSec = waveform.nBlockAlign * waveform.nSamplesPerSec;

	if (waveOutOpen(&hWaveOut, WAVE_MAPPER, &waveform, (DWORD)ghWndMain, 0, CALLBACK_WINDOW))
		MessageBox(ghWndMain, szOpenError, NULL, MB_OK);

	for (;;) {
		if (1 != 1) {
			continue;
		}
		recvfrom(cInfo.udpSocket, micBuff, BLOCK_SIZE, 0, (struct sockaddr *)&udp_remote, &rLen);

		writeAudio(micBuff, sizeof(micBuff));

		Sleep(225);
	}
}

/*------------------------------------------------------------------------------------------------------------------
-- FUNCTION: serverDownload
--
-- DATE: April 15 2018
--
-- REVISIONS: None
--
-- DESIGNER: Morgan Ariss, Anthony Vu, John Tee, Li-Yan Tong
--
-- PROGRAMMER:
--
-- INTERFACE: void serverDownload(WPARAM wParam, PTSTR fileName)
--
-- RETURNS: void
--
-- NOTES:
-- Called when client is uploading an audio file to the server.
----------------------------------------------------------------------------------------------------------------------*/
void serverDownload(WPARAM wParam, PTSTR fileName)
{
	char outBuf[FILE_BUFF_SIZE];
	DWORD bytesRead;
	HANDLE hFile;
	DWORD Flags = 0;

	/* Open the file */
	if ((hFile = CreateFile(fileName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_ALWAYS, 0, NULL)) == INVALID_HANDLE_VALUE)
	{
		MessageBox(ghWndMain, (LPCSTR)"Error opening file!",
			(LPCSTR)"Error!", MB_OK | MB_ICONSTOP);
		return;
	}

	while (TRUE)
	{
		memset(outBuf, '\0', FILE_BUFF_SIZE);
		ReadFile(hFile, outBuf, FILE_BUFF_SIZE, &bytesRead, NULL);
		if (bytesRead == 0)
		{
			send(wParam, "Last Pkt\0", 9, 0);
			CancelIo(hFile);
			break;
		}

		if (send(wParam, outBuf, bytesRead, 0) == -1)
		{
			if (WSAGetLastError() != WSAEWOULDBLOCK)
			{
				MessageBox(ghWndMain, (LPCSTR)"send() failed.",
					(LPCSTR)"Error!", MB_OK | MB_ICONSTOP);
				closesocket(wParam);
			}
		}
		Sleep(1); 
	}
}



/*-------------------------------------------------------------------------------------------------
--	FUNCTION:	clientDownload()
--
--	DATE:		April 3rd, 2018
--
--	DESIGNER:	Morgan Ariss
--
--	PROGRAMMER:	Morgan Ariss
--
--	INTERFACE:	clientDownload(WPARAM wParam)
--
--	ARGUMENTS:	LPVOID iValue
--
--	RETURNS:	void
--
--	NOTES:
--		Called asynchronously upon a request type set to download if the
--		connection has already been established.  It opens the file and writes the
--		incoming packet to the end of it, and displays a message when we are done
--		transferring.
--
-------------------------------------------------------------------------------------------------*/
void clientDownload(WPARAM wParam)
{
	char buff[FILE_BUFF_SIZE];
	DWORD bytesWritten;
	HANDLE hFile;
	OVERLAPPED lpOver;
	int bytesRead;
	DWORD Flags = 0;

	memset(&lpOver, 0, sizeof(OVERLAPPED));
	lpOver.Offset = lpOver.OffsetHigh = 0xFFFFFFFF;
	memset(buff, 0, sizeof(buff));

	if ((bytesRead = recv(wParam, buff, FILE_BUFF_SIZE, 0)) == -1)
	{
		if (WSAGetLastError() != WSAEWOULDBLOCK)
		{
			MessageBox(ghWndMain, (LPCSTR)"WSARecv() failed.",
				(LPCSTR)"Error!", MB_OK | MB_ICONSTOP);
			closesocket(wParam);
		}
	}
	else
	{
		if (strncmp(buff, "FILE", 4) == 0)
		{
			receiveFileList(wParam, buff);
			return;
		}
		else if (strncmp(buff, "NO", 2) == 0) {
			MessageBox(ghWndMain, (LPCSTR)"Server says NO!",
				(LPCSTR)"Error!", MB_OK | MB_ICONSTOP);
			ExitProcess(1);
		}

		if ((hFile = CreateFile(cInfo.DLfileName, GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_ALWAYS, 0, NULL)) == INVALID_HANDLE_VALUE)
		{
			MessageBox(ghWndMain, (LPCSTR)"Error opening file!",
				(LPCSTR)"Error!", MB_OK | MB_ICONSTOP);
			return;
		}

		WriteFile(hFile, buff, bytesRead, &bytesWritten, &lpOver);
		CloseHandle(hFile);
	}
	if (!strcmp(buff, "Last Pkt")) { /* Last packet, but not an empty packet. Needs to be fixed. */
		MessageBox(ghWndMain, (LPCSTR)"File download completed!",
			(LPCSTR)"Operation Completed", MB_OK | MB_ICONINFORMATION);
	}
}

void sockClose(HWND hwnd, WPARAM wParam, LPARAM lParam)
{
	if (lParam != -1) {
		shutdown(wParam, 2);
		closesocket(wParam);
	}
	else if (cInfo.request != MULTI_STREAM) {
		if (streamThread != NULL)
			TerminateThread(streamThread, 0);

		cInfo.request = 0;

		Sleep(200);
	}
}

void Close(HWND hwnd)
{
	PostQuitMessage(0);
}

void Command(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify)
{
	TCHAR fileName[FILE_BUFF_SIZE];
	TCHAR pathName[FILE_BUFF_SIZE];
	char ip[TEMP_BUFF_SIZE] = "IP: ";

	HWND list;
	int count, i;

	switch (id)
	{
	case ID_FILE_CONNECT:
		if (cInfo.mode == SERVER)
		{
			initializeServer(hwnd, SOCK_STREAM);
			initializeServer(hwnd, SOCK_DGRAM);

			if (cInfo.request == MULTI_STREAM) {
				sendFileList(0);
			}

			if (cInfo.tcpSocket == INVALID_SOCKET)
			{
				MessageBox(ghWndMain, (LPCSTR)"Invalid Socket!",
					(LPCSTR)"Error!", MB_OK | MB_ICONSTOP);
			}
		}
		else if (cInfo.mode == CLIENT)
		{
			initializeClient(hwnd, SOCK_STREAM);
			initializeClient(hwnd, SOCK_DGRAM);

			if (cInfo.request == MULTI_STREAM) {
				if (streamThread != NULL) {
					TerminateThread(streamThread, 0);
				}

				Sleep(100);

				if ((streamThread = CreateThread(NULL, 0,
					(LPTHREAD_START_ROUTINE)receiveStream, (LPVOID)cInfo.udpSocket, 0, 0)) == NULL)
				{
					MessageBox(NULL, "Thread creation failed", NULL, MB_OK);
					ExitProcess(1);
				}
			}
			else {
				connect(cInfo.tcpSocket, (SOCKADDR *)&remote, sizeof(remote));

				if (cInfo.tcpSocket == INVALID_SOCKET)
				{
					if (WSAGetLastError() != WSAEWOULDBLOCK) {
						MessageBox(ghWndMain, (LPCSTR)"Unable to connect to server!",
							(LPCSTR)"Error!", MB_OK | MB_ICONSTOP);
					}
				}
			}
		}

		connectActions();
		initializeButtons();
		SendMessage(ghStatus, SB_SETTEXT, (WPARAM)parts[0], (LPARAM)"Status: Connected");
		break;

	case ID_FILE_DISCONNECT:
		if (streamThread != NULL)
			TerminateThread(streamThread, 0);

		Sleep(200);

		sockClose(hwnd, cInfo.tcpSocket, 0);
		sockClose(hwnd, cInfo.udpSocket, 0);

		cInfo.mode = 0;
		cInfo.request = 0;

		disconnectActions();
		setActions();
		initializeButtons();

		list = GetDlgItem(ghDlgMain, IDC_LST_PLAY);
		count = ListBox_GetCount(list);

		for (i = 0; i < count; i++) {
			ListBox_DeleteString(list, 0);
		}


		SendMessage(ghStatus, SB_SETTEXT, (WPARAM)parts[0], (LPARAM)"Status: Disconnected");
		SendMessage(ghStatus, SB_SETTEXT, (WPARAM)parts[1], (LPARAM)"Mode: ");
		SendMessage(ghStatus, SB_SETTEXT, (WPARAM)parts[2], (LPARAM)"IP: ");
		break;

	case ID_FILE_EXIT:
		PostMessage(hwnd, WM_CLOSE, 0, 0);
		break;

	case ID_MODE_CLIENT:
		DialogBox(ghInst, MAKEINTRESOURCE(IDD_CLIENT), hwnd, (DLGPROC)ClientProc);
		cInfo.mode = CLIENT;

		checkMenuItem(0);
		setActions();

		SendMessage(ghStatus, SB_SETTEXT, (WPARAM)parts[1], (LPARAM)"Mode: Client");
		strcat_s(ip, sizeof(ip), cInfo.ip);
		SendMessage(ghStatus, SB_SETTEXT, (WPARAM)parts[2], (LPARAM)ip);
		break;

	case ID_MODE_SERVER:
		DialogBox(ghInst, MAKEINTRESOURCE(IDD_SERVER), hwnd, (DLGPROC)ServerProc);
		cInfo.mode = SERVER;

		checkMenuItem(0);
		setActions();

		SendMessage(ghStatus, SB_SETTEXT, (WPARAM)parts[1], (LPARAM)"Mode: Server");
		break;

	case ID_SINGLE_DL:
		checkMenuItem(ID_SINGLE_DL);
		cInfo.request = SINGLE_DL;
		break;
	case ID_SINGLE_UP:
		checkMenuItem(ID_SINGLE_UP);
		cInfo.request = SINGLE_UP;
		break;
	case ID_SINGLE_STREAM:
		checkMenuItem(ID_SINGLE_STREAM);
		cInfo.request = SINGLE_STREAM;
		break;
	case ID_MULTI_STREAM:
		checkMenuItem(ID_MULTI_STREAM);
		cInfo.request = MULTI_STREAM;
		break;

	case ID_2WAY_MICROPHONE:
		checkMenuItem(ID_2WAY_MICROPHONE);
		cInfo.request = MICROPHONE;
		break;

	case ID_FILE_LOCAL:
		stopLocalFile();
		EnableWindow(GetDlgItem(ghDlgMain, IDC_BTN_PAUSE), TRUE);
		SetWindowText(GetDlgItem(ghDlgMain, IDC_BTN_PAUSE), (LPCSTR)"Pause");
		busyFlag = LOCALPLAY;
		memset(fileName, 0, FILE_BUFF_SIZE);
		memset(pathName, 0, FILE_BUFF_SIZE);
		browseLocalFiles(hwnd, fileName, pathName);
		if (initializeLocalFile(hwnd, fileName) == FALSE) {
			busyFlag = 0;
		}
		break;

	default:
		break;
	}
}

int Create(HWND hwnd, LPCREATESTRUCT lpCreateStruct)
{
	static TCHAR szFilter[] = TEXT("All Files (*.*)\0*.*\0\0");

	ghDlgMain = CreateDialog(ghInst, MAKEINTRESOURCE(IDD_MAIN), hwnd, MainDlgProc);

	ofn.lStructSize = sizeof(OPENFILENAME);
	ofn.hwndOwner = hwnd;
	ofn.hInstance = NULL;
	ofn.lpstrFilter = szFilter;
	ofn.lpstrCustomFilter = NULL;
	ofn.nMaxCustFilter = 0;
	ofn.nFilterIndex = 0;
	ofn.lpstrFile = NULL;
	ofn.nMaxFile = MAX_PATH;
	ofn.lpstrFileTitle = NULL;
	ofn.nMaxFileTitle = MAX_PATH;
	ofn.lpstrInitialDir = NULL;
	ofn.lpstrTitle = NULL;
	ofn.Flags = 0;
	ofn.nFileOffset = 0;
	ofn.nFileExtension = 0;
	ofn.lpstrDefExt = TEXT("wav");
	ofn.lCustData = 0L;
	ofn.lpfnHook = NULL;
	ofn.lpTemplateName = NULL;

	busyFlag = 0;
	memset((char *)&cInfo, 0, sizeof(connectInfo));

	cInfo.tcp_port = TCP_PORT;
	cInfo.udp_port = UDP_PORT;

	ghMenu = GetMenu(hwnd);

	EnableMenuItem(ghMenu, ID_FILE_CONNECT, MF_GRAYED);
	EnableMenuItem(ghMenu, ID_FILE_DISCONNECT, MF_GRAYED);
	EnableMenuItem(ghMenu, ID_SINGLE_DL, MF_GRAYED);
	EnableMenuItem(ghMenu, ID_SINGLE_UP, MF_GRAYED);
	EnableMenuItem(ghMenu, ID_SINGLE_STREAM, MF_GRAYED);
	EnableMenuItem(ghMenu, ID_MULTI_STREAM, MF_GRAYED);
	EnableMenuItem(ghMenu, ID_2WAY_MICROPHONE, MF_GRAYED);
	checkMenuItem(0);

	ghStatus = CreateStatusWindow(WS_CHILD | WS_VISIBLE, "Status: Disconnected", hwnd, STATUS_BAR);

	SendMessage(ghStatus, SB_SETPARTS, (WPARAM)nParts, (LPARAM)&width);
	SendMessage(ghStatus, SB_SETTEXT, (WPARAM)parts[1], (LPARAM)"Mode:");
	SendMessage(ghStatus, SB_SETTEXT, (WPARAM)parts[2], (LPARAM)"IP:");

	return TRUE;
}

void Destroy(HWND hwnd)
{
	PostQuitMessage(0);
}

void Paint(HWND hwnd)
{
	HDC hdc;
	PAINTSTRUCT ps;

	hdc = BeginPaint(hwnd, &ps);
	EndPaint(hwnd, &ps);
}

void Size(HWND hwnd, UINT state, int cy, int cx)
{
	WORD wTop = 0;
	WORD wHeight = cx;
	WORD wWidth = cy;
	RECT wRect;

	GetWindowRect(ghWndMain, &wRect);

	MoveWindow(ghDlgMain, 0, wTop, wWidth, wHeight, TRUE);
}

void TCPSocket(HWND hwnd, WPARAM wParam, LPARAM lParam)
{
	SOCKET sock;
	TCHAR	fileName[FILE_PATH_SIZE], pathName[FILE_PATH_SIZE];
	static BOOL firstTime = TRUE;

	char buff[FILE_BUFF_SIZE];
	int bytesRead;
	int cSize;

	switch (WSAGETSELECTEVENT(lParam))
	{
	case FD_ACCEPT:
		if (wParam == INVALID_SOCKET)
		{
			break;
		}

		sock = accept(wParam, NULL, NULL);

		if (sock == INVALID_SOCKET) {
			MessageBox(ghWndMain, (LPCSTR)"Unable to create accept socket!",
				(LPCSTR)"Error!", MB_OK | MB_ICONSTOP);
		}
		break;

	case FD_CLOSE:
		sockClose(hwnd, wParam, -1);
		break;

	case FD_CONNECT:
		break;

	case FD_READ:
		if (wParam == INVALID_SOCKET)
			break;

		cSize = sizeof(SOCKADDR);
		memset(buff, '\0', FILE_BUFF_SIZE);

		if (cInfo.mode == SERVER)
		{
			if (cInfo.request == SINGLE_UP)
			{
				strcpy_s(cInfo.DLfileName, sizeof(cInfo.DLfileName), "upload_file.wav");
				clientDownload(wParam);
				break;
			}

			if (recv(wParam, buff, sizeof(buff), 0) == -1)
			{
				if (WSAGetLastError() != WSAEWOULDBLOCK)
				{
					MessageBox(ghWndMain, (LPCSTR)"recv() failed.",
						(LPCSTR)"Error!", MB_OK | MB_ICONSTOP);
					closesocket(wParam);
				}
			}

			if (cInfo.request == SINGLE_DL)
			{
				serverDownload(wParam, buff);
			}
			else if (cInfo.request == SINGLE_STREAM)
			{
				strcpy_s(cInfo.DLfileName, sizeof(cInfo.DLfileName), buff);

				if (streamThread != NULL)
					TerminateThread(streamThread, 0);

				streamThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)sendStream, (LPVOID)wParam, 0, 0);
				if (streamThread == NULL)
				{
					// Fatal error
					ExitProcess(1);
				}
			}
			else if (cInfo.request == MICROPHONE) {
				waveOutWrite(hWaveOut, (WAVEHDR *)buff, sizeof(WAVEHDR));
			}

			if (cInfo.request != MULTI_STREAM) {
				if (strcmp(buff, "Single Download") == 0)
				{
					cInfo.request = SINGLE_DL;
					sendFileList(wParam);
				}
				else if (strcmp(buff, "Single Upload") == 0)
				{
					cInfo.request = SINGLE_UP;
				}
				else if (strcmp(buff, "Stream") == 0)
				{
					cInfo.request = SINGLE_STREAM;
					sendFileList(wParam);
				}
				else if (strcmp(buff, "Microphone") == 0)
				{
					cInfo.request = MICROPHONE;
					beginMicRecord();
					streamThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)receiveMicThread,
						(LPVOID)wParam, 0, 0);
				}
			}
			else {
				if (strcmp(buff, "Multicast") == 0)
				{
					cInfo.request = MULTI_STREAM;
				}
				else {
					strcpy_s(buff, FILE_BUFF_SIZE, "NO");
					send(wParam, buff, strlen(buff), 0);
				}
			}
		}
		else if (cInfo.mode == CLIENT)
		{
			if (cInfo.request == SINGLE_DL)
			{
				Sleep(1);
				clientDownload(wParam);
			}
			else if (cInfo.request == SINGLE_UP)
			{
				if ((bytesRead = recv(wParam, buff, FILE_BUFF_SIZE, 0)) == -1)
				{
					if (WSAGetLastError() != WSAEWOULDBLOCK)
					{
						MessageBox(ghWndMain, (LPCSTR)"WSARecv() failed.",
							(LPCSTR)"Error!", MB_OK | MB_ICONSTOP);
						closesocket(wParam);
					}
				}
				if (strncmp(buff, "NO", 2) == 0) {
					MessageBox(ghWndMain, (LPCSTR)"Server has no mic!",
						(LPCSTR)"Error!", MB_OK | MB_ICONSTOP);
					ExitProcess(1);
				}
			}
			else if (cInfo.request == SINGLE_STREAM)
			{
				if ((bytesRead = recv(wParam, buff, FILE_BUFF_SIZE, 0)) == -1)
				{
					if (WSAGetLastError() != WSAEWOULDBLOCK)
					{
						MessageBox(ghWndMain, (LPCSTR)"WSARecv() failed.",
							(LPCSTR)"Error!", MB_OK | MB_ICONSTOP);
						closesocket(wParam);
					}
				}
				if (strncmp(buff, "NO", 2) == 0) {
					MessageBox(ghWndMain, (LPCSTR)"Server says NO!",
						(LPCSTR)"Error!", MB_OK | MB_ICONSTOP);
					ExitProcess(1);
				}
				receiveFileList(wParam, buff);
			}
			else if (cInfo.request == MICROPHONE)
			{
				if ((bytesRead = recv(wParam, buff, FILE_BUFF_SIZE, 0)) == -1)
				{
					if (WSAGetLastError() != WSAEWOULDBLOCK)
					{
						MessageBox(ghWndMain, (LPCSTR)"WSARecv() failed.",
							(LPCSTR)"Error!", MB_OK | MB_ICONSTOP);
						closesocket(wParam);
					}
				}
				if (strncmp(buff, "NO", 2) == 0) {
					MessageBox(ghWndMain, (LPCSTR)"Server says NO!",
						(LPCSTR)"Error!", MB_OK | MB_ICONSTOP);
					ExitProcess(1);
				}
			}
		}
		initializeButtons();
		break;

	case FD_WRITE:
		memset(buff, '\0', FILE_BUFF_SIZE);
		memset(fileName, '\0', FILE_PATH_SIZE);
		memset(pathName, '\0', FILE_PATH_SIZE);

		if (cInfo.mode == CLIENT)
		{
			if (cInfo.request == SINGLE_DL)
			{
				strcpy_s(buff, FILE_BUFF_SIZE, "Single Download");
				Sleep(10);
			}
			else if (cInfo.request == SINGLE_UP)
			{
				strcpy_s(buff, FILE_BUFF_SIZE, "Single Upload");
				Sleep(10);
				browseLocalFiles(hwnd, fileName, pathName);
			}
			else if (cInfo.request == SINGLE_STREAM)
				strcpy_s(buff, FILE_BUFF_SIZE, "Stream");
			else if (cInfo.request == MULTI_STREAM)
				strcpy_s(buff, FILE_BUFF_SIZE, "Multicast");
			else if (cInfo.request == MICROPHONE)
				strcpy_s(buff, FILE_BUFF_SIZE, "Microphone");

			if (send(wParam, buff, strlen(buff), 0) == -1)
			{
				if (WSAGetLastError() != WSAEWOULDBLOCK)
				{
					MessageBox(ghWndMain, (LPCSTR)"send() failed.",
						(LPCSTR)"Error!", MB_OK | MB_ICONSTOP);
					closesocket(wParam);
				}
			}

			if (cInfo.request == SINGLE_UP)
			{
				Sleep(1);
				serverDownload(wParam, fileName);
			}
			else if (cInfo.request == MICROPHONE)
			{
				beginMicRecord();
				streamThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)receiveMicThread,
					(LPVOID)wParam, 0, 0);
			}
		}
		else if (cInfo.mode == SERVER) {
			MessageBox(ghWndMain,
				(LPCSTR)"Warning: Asyncronous TCP write called by the server.\n",
				(LPCSTR)"Error!", MB_OK | MB_ICONSTOP);
		}
		break;

	default:
		break;
	}
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
	switch (message)
	{
		HANDLE_MSG(hWnd, WM_CREATE, Create);
		HANDLE_MSG(hWnd, WM_SIZE, Size);
		HANDLE_MSG(hWnd, WM_COMMAND, Command);
		HANDLE_MSG(hWnd, WM_PAINT, Paint);
		HANDLE_MSG(hWnd, WM_CLOSE, Close);
		HANDLE_MSG(hWnd, WM_TCP_SOCKET, TCPSocket);
		HANDLE_MSG(hWnd, WM_DESTROY, Destroy);

	case MM_WIM_OPEN:
		pSaveBuff = reinterpret_cast<PBYTE> (realloc(pSaveBuff, 1));

		waveInAddBuffer(hWaveIn, pWaveHdr1, sizeof(WAVEHDR));
		waveInAddBuffer(hWaveIn, pWaveHdr2, sizeof(WAVEHDR));

		recording = TRUE;
		ending = FALSE;
		dwDataLength = 0;

		waveInStart(hWaveIn);

		break;

	case MM_WIM_DATA:
		if (received == FALSE) {
			received = TRUE;
			sendto(cInfo.udpSocket, "1", sizeof("1"), 0, (struct sockaddr *)&udp_remote, 0);
			Sleep(100);
		}
		pNewBuff = reinterpret_cast<PBYTE> (realloc(pSaveBuff, dwDataLength + ((PWAVEHDR)lParam)->dwBytesRecorded));

		if (pNewBuff == NULL)
		{
			waveInClose(hWaveIn);
			MessageBox(ghWndMain, szMemError, (LPCSTR)"Error!", MB_OK | MB_ICONSTOP);

			break;
		}

		pSaveBuff = pNewBuff;
		CopyMemory(pSaveBuff + dwDataLength, ((PWAVEHDR)lParam)->lpData, ((PWAVEHDR)lParam)->dwBytesRecorded);

		dwDataLength += ((PWAVEHDR)lParam)->dwBytesRecorded;

		if (ending)
		{
			waveInClose(hWaveIn);
			break;
		}

		sendto(cInfo.udpSocket, ((PWAVEHDR)lParam)->lpData, BLOCK_SIZE, 0, (struct sockaddr *)&udp_remote, sizeof(udp_remote));

		waveInAddBuffer(hWaveIn, (PWAVEHDR)lParam, sizeof(WAVEHDR));

		break;

	case MM_WIM_CLOSE:
		waveInUnprepareHeader(hWaveIn, pWaveHdr1, sizeof(WAVEHDR));
		waveInUnprepareHeader(hWaveIn, pWaveHdr2, sizeof(WAVEHDR));

		free(pBuff1);
		free(pBuff2);

		recording = FALSE;

		if (killing)
			SendMessage(ghWndMain, WM_SYSCOMMAND, SC_CLOSE, 0L);
		break;

	case MM_WOM_OPEN:
		pWaveHdr3 = reinterpret_cast<PWAVEHDR> (malloc(sizeof(WAVEHDR)));
		pWaveHdr4 = reinterpret_cast<PWAVEHDR> (malloc(sizeof(WAVEHDR)));
		pOutBuff = reinterpret_cast<PBYTE> (GlobalAlloc(0, BLOCK_SIZE));

		/* Set up header */
		pWaveHdr3->lpData = reinterpret_cast<LPSTR> (pOutBuff);
		pWaveHdr3->dwBufferLength = BLOCK_SIZE;
		pWaveHdr3->dwBytesRecorded = 0;
		pWaveHdr3->dwUser = 0;
		pWaveHdr3->dwFlags = WHDR_PREPARED;
		pWaveHdr3->dwLoops = 0;
		pWaveHdr3->lpNext = NULL;
		pWaveHdr3->reserved = 0;

		pWaveHdr4->lpData = reinterpret_cast<LPSTR> (pOutBuff);
		pWaveHdr4->dwBufferLength = BLOCK_SIZE;
		pWaveHdr4->dwBytesRecorded = 0;
		pWaveHdr4->dwUser = 0;
		pWaveHdr4->dwFlags = WHDR_PREPARED;
		pWaveHdr4->dwLoops = 0;
		pWaveHdr4->lpNext = NULL;
		pWaveHdr4->reserved = 0;

		waveOutPrepareHeader(hWaveOut, pWaveHdr3, sizeof(WAVEHDR));
		waveOutWrite(hWaveOut, pWaveHdr3, sizeof(WAVEHDR));

		ending = FALSE;
		playing = TRUE;
		break;

	case WM_SYSCOMMAND:
		switch (LOWORD(lParam))
		{
		case SC_CLOSE:
			if (recording)
			{
				killing = TRUE;
				ending = TRUE;
				waveInReset(hWaveIn);
				break;
			}

			if (playing)
			{
				killing = TRUE;
				ending = TRUE;
				waveOutReset(hWaveOut);
				break;
			}

			free(pWaveHdr1);
			free(pWaveHdr2);
			free(pSaveBuff);
			break;

		default:
			break;
		}

		break;
	}
	return DefWindowProc(hWnd, message, wParam, lParam);
}

BOOL CALLBACK ClientProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_INITDIALOG:
		return FALSE;

	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDOK:
			GetDlgItemText(hDlg, IDC_EDT_IPADDR, cInfo.ip, 26);

			if (cInfo.ip[0] != '\0')
				EnableMenuItem(ghMenu, ID_FILE_CONNECT, MF_ENABLED);

			EndDialog(hDlg, TRUE);
			return TRUE;

		case IDCANCEL:
			EndDialog(hDlg, FALSE);
			return TRUE;
		}

		break;
	}

	return FALSE;
}

BOOL CALLBACK ServerProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_INITDIALOG:
		if (cInfo.request == MULTI_STREAM) {
			SendMessage(GetDlgItem(hDlg, IDC_MULTICAST), BM_SETCHECK, BST_CHECKED, 0);
		}
		return TRUE;

	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDOK:
			if (SendMessage(GetDlgItem(hDlg, IDC_MULTICAST), BM_GETCHECK, 0, 0) == BST_CHECKED) {
				cInfo.request = MULTI_STREAM;
			}

			EndDialog(hDlg, TRUE);
			return TRUE;

		case IDCANCEL:
			EndDialog(hDlg, FALSE);
			return TRUE;
		}
	}

	return FALSE;
}

BOOL CALLBACK MainDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	HMENU hMenu;
	char	fileName[FILE_BUFF_SIZE];
	char	outBuf[TEMP_BUFF_SIZE];
	DWORD	errNo;
	static	HANDLE streamThread;
	char	pause_play[10];

	MCI_GENERIC_PARMS pause;
	MCI_GENERIC_PARMS resume;

	hMenu = GetMenu(ghWndMain);

	switch (message) {
	case WM_INITDIALOG:
		return FALSE;

	case WM_SETFOCUS:
		break;

	case WM_COMMAND: // Process user input
		switch (LOWORD(wParam))
		{
		case IDC_BTN_PAUSE:
			GetWindowText(GetDlgItem(ghDlgMain, IDC_BTN_PAUSE), pause_play, sizeof(pause_play));
			if (cInfo.request != MULTI_STREAM) {
				if (strcmp(pause_play, "Pause") == 0) {
					SetWindowText(GetDlgItem(ghDlgMain, IDC_BTN_PAUSE), (LPCSTR)"Play");
					if (busyFlag == LOCALPLAY)
					{
						if (deviceID)
						{
							memset(&pause, 0, sizeof(MCI_GENERIC_PARMS));
							mciSendCommand(deviceID, MCI_PAUSE, MCI_WAIT, (DWORD_PTR)&pause);
						}
					}
					else if (cInfo.request == SINGLE_STREAM)
					{
						if (waveOutPause(hWaveOut) != MMSYSERR_NOERROR)
						{
							errNo = GetLastError();
							MessageBox(NULL, "Can't pause", "Error", 0);

							return FALSE;
						}
					}
				}
				else {
					SetWindowText(GetDlgItem(ghDlgMain, IDC_BTN_PAUSE), (LPCSTR)"Pause");
					if (busyFlag == LOCALPLAY)
					{
						if (deviceID)
						{
							memset(&resume, 0, sizeof(MCI_GENERIC_PARMS));
							mciSendCommand(deviceID, MCI_RESUME, MCI_WAIT, (DWORD_PTR)&resume);
						}
					}
					else if (busyFlag == NETWORKPLAY)
						waveOutRestart(hWaveOut);
					else if (cInfo.request == SINGLE_STREAM)
						waveOutRestart(hWaveOut);
				}
			}
			else {
				if (strcmp(pause_play, "Mute") == 0) {
					SetWindowText(GetDlgItem(ghDlgMain, IDC_BTN_PAUSE), (LPCSTR)"Un-mute");
					waveOutSetVolume(hWaveOut, 0x00000000);
				}
				else {
					SetWindowText(GetDlgItem(ghDlgMain, IDC_BTN_PAUSE), (LPCSTR)"Mute");
					waveOutSetVolume(hWaveOut, 0xFFFFFFFF);
				}
			}
			return FALSE;

		case IDC_BTN_DOWNLOAD:

			if (cInfo.mode == CLIENT && (cInfo.request == SINGLE_STREAM
				|| cInfo.request == SINGLE_DL)) {

				EnableWindow(GetDlgItem(ghDlgMain, IDC_BTN_DOWNLOAD), FALSE);

				GetSelList(fileName);
				memset(outBuf, '\0', TEMP_BUFF_SIZE);
				strcpy_s(outBuf, sizeof(outBuf), fileName);
				strcpy_s(cInfo.DLfileName, sizeof(cInfo.DLfileName), fileName);

				if (send(cInfo.tcpSocket, outBuf, strlen(outBuf), 0) == -1)
				{
					if (WSAGetLastError() != WSAEWOULDBLOCK)
					{
						MessageBox(ghWndMain, (LPCSTR)"send() failed.",
							(LPCSTR)"Error!", MB_OK | MB_ICONSTOP);
						closesocket(wParam);
					}
				}

				if (cInfo.request == SINGLE_STREAM) {
					if (streamThread != NULL)
						TerminateThread(streamThread, 0);

					Sleep(200);

					streamThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)receiveStream, (LPVOID)wParam, 0, 0);
					if (streamThread == NULL)
					{
						MessageBox(ghWndMain, (LPCSTR)"Thread creation failed.",
							(LPCSTR)"Error!", MB_OK | MB_ICONSTOP);
						ExitProcess(1);
					}

					SetFocus(GetDlgItem(ghDlgMain, IDC_LST_PLAY));

					Sleep(400);
				}
				EnableWindow(GetDlgItem(ghDlgMain, IDC_BTN_DOWNLOAD), TRUE);
			}
			return FALSE;

		case IDC_BTN_BROADCAST:
			GetSelList(cInfo.DLfileName);

			if (streamThread != NULL) {
				TerminateThread(streamThread, 0);
				Sleep(200);
			}

			if ((streamThread = CreateThread(NULL, 0,
				(LPTHREAD_START_ROUTINE)sendStream, (LPVOID)wParam, 0, 0)) == NULL)
			{
				MessageBox(NULL, "Thread creation failed", NULL, MB_OK);
				ExitProcess(1);
			}

			SetFocus(GetDlgItem(ghDlgMain, IDC_LST_PLAY));
			return FALSE;

		default:
			return FALSE;
		}
		return TRUE;
	}
	return FALSE;
}