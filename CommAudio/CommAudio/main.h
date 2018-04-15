#ifndef _WIN_MAIN_H_
#define _WIN_MAIN_H_

#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <winsock2.h>
#include <Ws2tcpip.h>
#include <windows.h>
#include <windowsx.h>
#include <mmsystem.h>
#include <stdio.h>
#include <conio.h>
#include "commctrl.h"

#pragma comment(lib,"comctl32.lib")
#pragma comment(lib,"ws2_32.lib")
#pragma comment(lib,"winmm.lib")

#define STATUS_BAR		2001

/* Connection Definitions */
#define NUMCONNECTIONS	5
#define WM_TCP_SOCKET	10000
#define WM_UDP_SOCKET	10001
#define TCP_PORT		9000
#define	UDP_PORT		7000
#define	MULTICAST_GROUP	"237.137.137.137"

/* Buffer Definitions */
#define BLOCK_SIZE		44100
#define BLOCK_COUNT		500
#define TEMP_BUFF_SIZE	200
#define FILE_PATH_SIZE	255
#define FILE_BUFF_SIZE	4096
#define WAVE_HEAD_SIZE	128

/* Flag Definitions */
#define LOCALPLAY		500
#define NETWORKPLAY		501

/* Mode macros */
#define SERVER			1
#define CLIENT			2
#define TCP				1
#define IDP				2
#define SINGLE_DL		1
#define SINGLE_UP		2
#define SINGLE_STREAM	3
#define MULTI_STREAM	4
#define MICROPHONE		5

#define INP_BUFFER_SIZE 44100

/* Custom Message Cracker Definition */
#define HANDLE_WM_TCP_SOCKET(hwnd, wParam, lParam, fn) \
	((fn)((hwnd), (wParam), (lParam)), 0L)

/* Connection Information */
typedef struct _CONNECTINFO
{
	char	ip[TEMP_BUFF_SIZE];
	int		tcp_port, udp_port, mode, request, protocol;
	BOOL	newClient;	
	char	DLfileName[FILE_PATH_SIZE];
	char	waveFormat[WAVE_HEAD_SIZE];
	SOCKET	tcpSocket, udpSocket, tcpSock2;
} connectInfo;

/* Microphone Globals */
static BOOL			recording, playing, ending, killing;
static DWORD		dwDataLength;
static HWAVEIN		hWaveIn;
static PBYTE		pBuff1, pBuff2, pSaveBuff, pOutBuff, pNewBuff;
static PWAVEHDR		pWaveHdr1, pWaveHdr2, pWaveHdr3, pWaveHdr4;
static TCHAR		szOpenError[] = "Error: Failed opening waveform audio!";
static TCHAR		szMemError[] = "Error: Failed allocating memory!";
static WAVEFORMATEX	waveform;
static HANDLE		micSend, micRecv;

/* Status Bar Information */
static int nParts = 3;
static int width[] = { 115,225,-1 };
static int parts[] = { 0, 1, 2 };

/* Global Functions */
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
BOOL CALLBACK MainDlgProc(HWND, UINT, WPARAM, LPARAM);
HWND InitializeGUI(HINSTANCE, int);

/* Initialization */
void InitializeWindow(HWND);
BOOL CALLBACK ServerProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
BOOL CALLBACK ClientProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
void initializeClient(HWND hwnd, int type);
void initializeServer (HWND hwnd, int type);
void initializeButtons(void);

/* Local File */
BOOL initializeLocalFile(HWND hwnd, char * filename);
void stopLocalFile(void);
BOOL browseLocalFiles(HWND hwnd, PTSTR pstrFileName, PTSTR pstrTitleName);

/* Networking */
void writeAudio(LPSTR data, int size);
void freeBlocks(WAVEHDR* blockArray);
WAVEHDR* allocateBlocks(int size, int count);

static void CALLBACK waveOutProc(HWAVEOUT hWaveOut, UINT uMsg, DWORD dwInstance, DWORD dwParam1, DWORD dwParam2);
DWORD WINAPI receiveStream(LPVOID iValue);
DWORD WINAPI sendStream(LPVOID iValue);
DWORD WINAPI receiveMicThread(LPVOID iValue);

/* Download Services */
void serverDownload(WPARAM wParam, PTSTR	fileName);
void clientDownload(WPARAM wParam);

/* Helper Functions */
void sendFileList(WPARAM wParam);
void receiveFileList(WPARAM wParam, char buf[]);
void GetSelList(char * selItem);

void disconnectActions(void);
void connectActions(void);
void setActions(void);
void checkMenuItem(int);

/* Microphone */
void beginMicRecord(void);

/* Universal Socket Functions */
void sockClose(HWND hwnd, WPARAM wParam, LPARAM lParam);

void Close(HWND hwnd);
void Command(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify);
int Create(HWND hwnd, LPCREATESTRUCT lpCreateStruct);
void Destroy(HWND hwnd);
void Paint(HWND hwnd);
void Size(HWND hwnd, UINT state, int cx, int cy);
void TCPSocket(HWND hwnd, WPARAM wParam, LPARAM lParam);

#endif