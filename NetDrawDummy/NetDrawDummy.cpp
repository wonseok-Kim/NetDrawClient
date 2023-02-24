#pragma comment(lib, "ws2_32")
#include <WS2tcpip.h>
#include <WinSock2.h>

#pragma comment(lib, "winmm.lib")
#include <timeapi.h>

#include <algorithm>

#include "List.h"

#define Crash()                                                                                                        \
    do                                                                                                                 \
    {                                                                                                                  \
        *(int*)0 = 0;                                                                                                  \
    } while (0)

struct DrawPacket
{
    INT OldX;
    INT OldY;
    INT CurX;
    INT CurY;
};

struct Dummy
{
    SOCKET Socket;
    DrawPacket DrawPacket;
    POINT Dir;
    DWORD Duration;
    DWORD StartTick;
};

POINT directions[] = {{-5, -5}, {-5, 0}, {-5, 5}, {0, -5}, {0, 5}, {5, -5}, {5, 0}, {5, 5}};
constexpr int numDir = _countof(directions);

LPCWSTR g_szIP = L"3.36.247.75";
constexpr int NUM_DUMMIES = 60;
List<Dummy*> g_dummyList;

int RandInt(int min, int max)
{
    double uniform = (double)rand() / (RAND_MAX);

    return (int)(uniform * (max - min)) + min;
}

int wmain()
{
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
        return 1;

    for (int i = 0; i < NUM_DUMMIES; ++i)
    {
        Dummy* pDummy = new Dummy;
        pDummy->Socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (pDummy->Socket == INVALID_SOCKET)
            Crash();
        pDummy->DrawPacket.CurX = RandInt(0, 620);
        pDummy->DrawPacket.CurY = RandInt(0, 470);
        pDummy->Dir = directions[RandInt(0, numDir - 1)];
        pDummy->Duration = RandInt(100, 1000);

        SOCKADDR_IN serverAddr{};
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(25000);
        if (InetPtonW(AF_INET, g_szIP, &serverAddr.sin_addr) != 1)
            Crash();

        int ret1 = connect(pDummy->Socket, (SOCKADDR*)&serverAddr, sizeof(serverAddr));
        if (ret1 == SOCKET_ERROR)
            Crash();

        u_long on = 1;
        int ret2 = ioctlsocket(pDummy->Socket, FIONBIO, &on);
        if (ret2 == SOCKET_ERROR)
            Crash();

        g_dummyList.push_back(pDummy);
    }

    for (Dummy* pDummy : g_dummyList)
        pDummy->StartTick = timeGetTime();

    char buf[1000];
    for (;;)
    {
        for (Dummy* pDummy : g_dummyList)
        {
            for (;;)
            {
                int ret = recv(pDummy->Socket, buf, sizeof(buf), 0);
                if (ret == SOCKET_ERROR)
                {
                    if (WSAGetLastError() == WSAEWOULDBLOCK)
                        break;
                    else
                        Crash();
                }
            }            
            USHORT header = 16;
            pDummy->DrawPacket.OldX = pDummy->DrawPacket.CurX;
            pDummy->DrawPacket.OldY = pDummy->DrawPacket.CurY;
            pDummy->DrawPacket.CurX = std::clamp((int)(pDummy->DrawPacket.CurX + pDummy->Dir.x), 0, 620);
            pDummy->DrawPacket.CurY = std::clamp((int)(pDummy->DrawPacket.CurY + pDummy->Dir.y), 0, 470);

            /*wprintf(L"(%d, %d) => (%d, %d) \n", pDummy->DrawPacket.OldX, pDummy->DrawPacket.OldY,
                    pDummy->DrawPacket.CurX, pDummy->DrawPacket.CurY);*/
            int ret1 = send(pDummy->Socket, (char*)&header, sizeof(header), 0);
            if (ret1 == SOCKET_ERROR)
                Crash();
            int ret2 = send(pDummy->Socket, (char*)&pDummy->DrawPacket, sizeof(Dummy::DrawPacket), 0);
            if (ret2 == SOCKET_ERROR)
                Crash();

            DWORD now = timeGetTime();
            if (now - pDummy->StartTick >= pDummy->Duration)
            {
                pDummy->Dir = directions[RandInt(0, numDir - 1)];
                pDummy->Duration = RandInt(100, 1000);
                pDummy->StartTick = now;
            }
        }
        Sleep(30);
    }

    WSACleanup();
    return 0;
}