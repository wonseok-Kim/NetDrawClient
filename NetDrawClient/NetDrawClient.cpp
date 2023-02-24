#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include "framework.h"
#include "NetDrawClient.h"

#include "Container/RingBuffer.h"

#include "Util/MyLog.h"
#include "Util/Util.h"

#pragma comment(lib, "ws2_32")
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <windowsx.h>

#define UM_NETWORK (WM_USER + 1)

struct Header
{
	USHORT Len;
};

struct DrawPacket
{
	INT StartX;
	INT StartY;
	INT EndX;
	INT EndY;
};

HINSTANCE g_hInst;
HWND g_hWnd;
WCHAR g_szIP[256];
SOCKET g_socket;
RingBuffer g_sendQue;
RingBuffer g_recvQue;
bool g_bConn;

LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    DlgProc(HWND, UINT, WPARAM, LPARAM);
bool InitService(HWND);
void Disconnect();
void ReadProc();
void WriteProc();
void SendPacket(DrawPacket* pPacket);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR    lpCmdLine,
	_In_ int       nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	g_hInst = hInstance;

	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
		return 2;

	WNDCLASSEXW wcex;
	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_NETDRAWCLIENT));
	wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName = nullptr; // MAKEINTRESOURCEW(IDC_NETDRAWCLIENT);
	wcex.lpszClassName = L"NetDrawClient";
	wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));
	RegisterClassExW(&wcex);

	HWND hWnd = CreateWindowW(L"NetDrawClient", L"NetDrawClient", WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, 0, 624, 471, nullptr, nullptr, hInstance, nullptr);
	if (!hWnd)
		return 1;

	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);
	g_hWnd = hWnd;

	HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_NETDRAWCLIENT));

	MSG msg;
	while (GetMessage(&msg, nullptr, 0, 0))
	{
		if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	WSACleanup();
	return (int)msg.wParam;
}

int g_oldX;
int g_oldY;
int g_curX;
int g_curY;
bool g_bDrag;

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_CREATE:
	{
		if (DialogBox(g_hInst, MAKEINTRESOURCE(IDD_INPUT_IP), hWnd, DlgProc) != IDCONNECT)
			PostQuitMessage(0);

		if (!InitService(hWnd))
			PostQuitMessage(0);

		SetWindowTextW(hWnd, L"Try Connecting...");
	}
	break;

	case WM_PAINT:
	{
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(hWnd, &ps);
		// TODO: Add any drawing code that uses hdc here...
		EndPaint(hWnd, &ps);
	}
	break;

	case WM_DESTROY:
		PostQuitMessage(0);
		break;

	case WM_MOUSEMOVE:
	{
		if (g_bDrag && g_bConn)
		{
			g_oldX = g_curX;
			g_oldY = g_curY;
			g_curX = GET_X_LPARAM(lParam);
			g_curY = GET_Y_LPARAM(lParam);

			DrawPacket packet{ g_oldX, g_oldY, g_curX, g_curY };
            wprintf(L"%d %d\n", g_curX, g_curY);
			SendPacket(&packet);
		}
	}
	break;

	case WM_LBUTTONDOWN:
		g_curX = GET_X_LPARAM(lParam);
		g_curY = GET_Y_LPARAM(lParam);
		g_bDrag = true;
		break;

	case WM_LBUTTONUP:
		g_bDrag = false;
		break;

	case UM_NETWORK:
	{
		int err = WSAGETSELECTERROR(lParam);
		int netEvent = WSAGETSELECTEVENT(lParam);

		if (err != 0)
		{
			Log(L"netEvenct: %d, err %d", netEvent, err);
			Disconnect();
			break;
		}

		switch (netEvent)
		{
		case FD_CONNECT:
			SetWindowTextW(hWnd, L"Net Drawing");
			g_bConn = true;
			break;
		case FD_READ:
			ReadProc();
			break;
		case FD_WRITE:
			WriteProc();
			break;
		case FD_CLOSE:
			Disconnect();
			break;
		}
	}
	break;

	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

INT_PTR CALLBACK DlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	switch (message)
	{
	case WM_INITDIALOG:
		return (INT_PTR)TRUE;

	case WM_SETFOCUS:
		return 0;

	case WM_COMMAND:
		if (LOWORD(wParam) == IDCONNECT || LOWORD(wParam) == IDCANCEL)
		{
			if (GetDlgItemTextW(hDlg, IDC_EDIT_IP, g_szIP, _countof(g_szIP)) == 0)
				Log(L"GetDlgItemTextW err: %u", GetLastError());
			EndDialog(hDlg, LOWORD(wParam));
			return (INT_PTR)TRUE;
		}
		break;
	}
	return (INT_PTR)FALSE;
}

bool InitService(HWND hWnd)
{
	int errSocket, errSelect, errPton, errConn;

	g_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (g_socket == INVALID_SOCKET)
	{
		errSocket = WSAGetLastError();
		Log(L"socket err %d", errSocket);
		Crash();
	}

	int retSelect = WSAAsyncSelect(g_socket, hWnd, UM_NETWORK, FD_CONNECT | FD_WRITE | FD_READ | FD_CLOSE);
	if (retSelect != 0)
	{
		errSelect = WSAGetLastError();
		Log(L"WSAAsyncSelect err %d", errSelect);
		Crash();
	}

	SOCKADDR_IN serverAddr;
	ZeroMemory(&serverAddr, sizeof(serverAddr));
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(25000);
	if (InetPtonW(AF_INET, g_szIP, &serverAddr.sin_addr) != 1)
	{
		errPton = WSAGetLastError();
		Log(L"InetPtonW(%s) err %d", g_szIP, errPton);
		MessageBox(hWnd, L"올바르지 않은 IP 주소 입니다.", nullptr, MB_OK);
		return false;
	}

	int retConn = connect(g_socket, (SOCKADDR*)&serverAddr, sizeof(serverAddr));
	if (retConn == SOCKET_ERROR)
	{
		errConn = WSAGetLastError();
		if (errConn != WSAEWOULDBLOCK)
		{
			Log(L"connect err %d", errConn);
			Crash();
		}
	}
}

void Disconnect()
{
	g_bConn = false;
	closesocket(g_socket);
}

void ReadProc()
{
	int errRecv;
	int retRecv, retPeek, retDeque, retMoveRear, retMoveFront;
	int directEnqueSize;
	int useSize;
	Header header;
	DrawPacket drawPacket;
	for (;;)
	{
		directEnqueSize = g_recvQue.DirectEnqueueSize();
		retRecv = recv(g_socket, g_recvQue.GetRearPtr(), directEnqueSize, 0);
		if (retRecv == SOCKET_ERROR)
		{
			errRecv = WSAGetLastError();
			if (errRecv == WSAEWOULDBLOCK)
				break;
			Log(L"recv err %d", errRecv);
            if (errRecv == 10054)
                break;
			Crash();
		}
		else if (retRecv == 0)
		{
			Log(L"recv return 0");
			Disconnect();
			break;
		}
		retMoveRear = g_recvQue.MoveRear(retRecv);
        if (retMoveRear != retRecv)
            Crash();

		useSize = g_recvQue.GetUseSize();
        while (useSize > sizeof(header))
        {
            retPeek = g_recvQue.Peek(&header, sizeof(header));
            if (retPeek != sizeof(header))
                Crash();

            if (g_recvQue.GetUseSize() < header.Len + sizeof(header))
                break;

            retMoveFront = g_recvQue.MoveFront(retPeek);
            if (retMoveFront != retPeek)
                Crash();
            useSize -= retPeek;

            retDeque = g_recvQue.Dequeue(&drawPacket, sizeof(drawPacket));
            if (retDeque != sizeof(drawPacket))
                Crash();
            useSize -= retDeque;

            if (useSize != g_recvQue.GetUseSize())
            {
                Log(L"useSize != g_recvQue.GetUseSize() %s %d", __FILEW__, __LINE__);
                Crash();
			}

			HDC hdc = GetDC(g_hWnd);
			{
				MoveToEx(hdc, drawPacket.StartX, drawPacket.StartY, nullptr);
				LineTo(hdc, drawPacket.EndX, drawPacket.EndY);
			}
			ReleaseDC(g_hWnd, hdc);
		}
	}
}

void WriteProc()
{
	int errSend;
	int retSend;
	int directDequeSize;

	int useSize = g_sendQue.GetUseSize();
	while (useSize > 0)
	{
		directDequeSize = g_sendQue.DirectDequeueSize();
		retSend = send(g_socket, g_sendQue.GetFrontPtr(), directDequeSize, 0);
		if (retSend == SOCKET_ERROR)
		{
			errSend = WSAGetLastError();
			if (errSend == WSAEWOULDBLOCK)
			{
				break;
			}
			else
			{
				Log(L"send err in WriteProc: %d", errSend);
				Crash();
			}
		}
		else if (retSend < directDequeSize)
		{
			g_sendQue.MoveFront(retSend);
			break;
		}

		g_sendQue.MoveFront(retSend);
		useSize -= retSend;
        if (useSize != g_sendQue.GetUseSize())
            Crash();
	}
}

void SendPacket(DrawPacket* pPacket)
{
	Header header{ sizeof(DrawPacket) };

	int ret1 = g_sendQue.Enqueue(&header, sizeof(header));
	if (ret1 != sizeof(header))
	{
		Log("%s, %d lines g_sendQue.Enqueue err", __FILEW__, __LINE__);
		Crash();
	}

	int ret2 = g_sendQue.Enqueue(pPacket, sizeof(DrawPacket));
	if (ret2 != sizeof(DrawPacket))
	{
		Log("%s, %d lines g_sendQue.Enqueue err", __FILEW__, __LINE__);
		Crash();
	}

	WriteProc();
}
