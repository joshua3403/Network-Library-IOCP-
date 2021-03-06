#pragma once
#include "stdafx.h"
#include "CNewRingBuffer.h"
#include "CMessage.h"
//#include "CLog.h"

#define READ 3
#define WRITE 5
#define MAX_CLIENT_COUNT 200
#define CLIENT_COUNT	 100
#define MAX_PACKET_COUNT 500

namespace joshua
{


	typedef struct st_WSAOVERLAPPED
	{
		// 3 = READ, 5 = WRITE
		WSAOVERLAPPED Overlapped;
		DWORD ID;
	} NEWWSAOVERLAPPED, * LPNEWWSAOVERLAPPED;

	struct st_SESSION
	{
		LPNEWWSAOVERLAPPED SendOverlapped;
		LPNEWWSAOVERLAPPED RecvOverlapped;
		RingBuffer* SendBuffer;
		RingBuffer* RecvBuffer;
		DWORD dwIOCount;
		DWORD dwPacketCount;
		volatile LONG bIsSend;
		std::list<CMessage*> lMessageList;

		// SessionID가 0이면 사용하지 않는 세션
		DWORD SessionID;
		SOCKET socket;
		SOCKADDR_IN clientaddr;
		st_SESSION()
		{
			SendBuffer = new RingBuffer();
			RecvBuffer = new RingBuffer();
			SendOverlapped = new NEWWSAOVERLAPPED;
			RecvOverlapped = new NEWWSAOVERLAPPED;
			SendOverlapped->ID = WRITE;
			RecvOverlapped->ID = READ;
			dwIOCount = dwPacketCount = SessionID = 0;
			bIsSend = FALSE;
			socket = INVALID_SOCKET;
			ZeroMemory(&clientaddr, sizeof(clientaddr));
			//wprintf(L"session created\n");
		}

		~st_SESSION()
		{
			delete SendOverlapped;
			delete RecvOverlapped;
			delete SendBuffer;
			delete RecvBuffer;
			//wprintf(L"session deleted\n");

		}

	};


	class NetworkLibrary
	{
	private:
		SOCKET _listen_socket;
		SOCKADDR_IN _serveraddr;
		LONG64 _dwSessionCount;
		DWORD _dwSessionMax;
		LONG64 _dwCount;
		DWORD packetCount;

		// 입출력 완료 포트 생성// IOCP 변수
		HANDLE _hCP;
		DWORD _dwSessionID;
		std::vector<HANDLE> _ThreadVector;

		st_SESSION* _SessionArray;
		std::stack<DWORD> _ArrayIndex;

		CRITICAL_SECTION _IndexStackCS;

		LONG64 sendtest;
		LONG64 recvtest;

	private:
		// 소켓 초기화
		BOOL InitialNetwork(const WCHAR* ip, DWORD port, BOOL Nagle);
		// 스레드 생성
		// 0이면 프로세서 * 2
		BOOL CreateThread(DWORD threadCount);
		// 세션 할당 및 생성
		BOOL CreateSession();

		void PushIndex(DWORD index);

		DWORD PopIndex();

		// 빈 세션 공간에 새로 생성된 세션 세팅
		DWORD InsertSession(SOCKET sock, SOCKADDR_IN* sockaddr);

		// Accept를 전담할 스레드의 시작 함수
		static unsigned int WINAPI AcceptThread(LPVOID lpParam);
		static unsigned int WINAPI WorkerThread(LPVOID lpParam);

		void AcceptThread(void);
		void WorkerThread(void);

	public:

		NetworkLibrary()
		{
			_listen_socket = INVALID_SOCKET;
			_dwSessionID = 1;
			_dwSessionCount = _dwSessionMax = 0;
			_hCP = INVALID_HANDLE_VALUE;
			_SessionArray = nullptr;
			ZeroMemory(&_serveraddr, sizeof(_serveraddr));
			InitializeCriticalSection(&_IndexStackCS);
			_dwCount = 0;
		}
		
		~NetworkLibrary()
		{
			delete[] _SessionArray;
			closesocket(_listen_socket);
			CloseHandle(_hCP);
			WSACleanup();
		}

		bool PostSend(st_SESSION* session);
		bool PostRecv(st_SESSION* session);


		BOOL Start(DWORD port, BOOL nagle, const WCHAR* ip = nullptr, DWORD threadCount = 0, DWORD MaxClient = 0);

		void ReStart();

		// Accept후 접속 처리 완료후 호출하는 함수
		virtual void OnClientJoin(SOCKADDR_IN* sockAddr, DWORD sessionID) = 0; 
		virtual void OnClientLeave(DWORD sessionID) = 0;

		// accept직후
		// TRUE면 접속 허용
		// FALSE면 접속 불허
		virtual bool OnConnectionRequest(SOCKADDR_IN* sockAddr) = 0; 

		virtual void OnRecv(DWORD sessionID, CMessage* message) = 0; 
		virtual void OnSend(DWORD sessionID, int sendsize) = 0;

			//	virtual void OnWorkerThreadBegin() = 0;                    
			//	virtual void OnWorkerThreadEnd() = 0;                      

		virtual void OnError(int errorcode, WCHAR*) = 0;

		void SessionRelease(DWORD id);

		void SendPacket(DWORD id, CMessage* message);

		void PrintPacketCount();

		void DisconnectSession(DWORD id);
	};
}
