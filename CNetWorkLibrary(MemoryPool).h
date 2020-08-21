#pragma once
#include "stdafx.h"
#include "CNewRingBuffer.h"
#include "CMessage.h"
#include "CLog.h"

#define READ 3
#define WRITE 5
#define MAX_CLIENT_COUNT 1000

namespace joshua
{


	struct st_SESSION
	{
		WSAOVERLAPPED SendOverlapped;
		WSAOVERLAPPED RecvOverlapped;
		RingBuffer SendBuffer;
		RingBuffer RecvBuffer;
		DWORD dwIOCount;
		DWORD dwPacketCount;
		LONG bIsSend;
		LONG bIsReleased;
		std::list<CMessage*> lMessageList;
		BOOL bIsSendDisconnect;
		

		// SessionID�� 0�̸� ������� �ʴ� ����
		int index;
		LONG64 SessionID;
		SOCKET socket;
		SOCKADDR_IN clientaddr;
		st_SESSION()
		{
			SendBuffer;
			RecvBuffer;
			dwIOCount = dwPacketCount = 0;
			SessionID = 0;
			bIsSend = FALSE;
			socket = INVALID_SOCKET;
			bIsReleased = FALSE;
			bIsSendDisconnect = FALSE;
			ZeroMemory(&clientaddr, sizeof(clientaddr));
			index = 0;
			//wprintf(L"session created\n");
		}

		~st_SESSION()
		{
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

		// ����� �Ϸ� ��Ʈ ����// IOCP ����
		HANDLE _hCP;
		LONG64 _dwSessionID;
		std::vector<HANDLE> _ThreadVector;

		st_SESSION* _SessionArray;
		std::stack<DWORD> _ArrayIndex;

		CRITICAL_SECTION _IndexStackCS;

		BOOL _bServerOn;


	private:
		// ���� �ʱ�ȭ
		BOOL InitialNetwork(const WCHAR* ip, DWORD port, BOOL Nagle);
		// ������ ����
		// 0�̸� ���μ��� * 2
		BOOL CreateThread(DWORD threadCount);
		// ���� �Ҵ� �� ����
		BOOL CreateSession();

		void PushIndex(DWORD index);

		DWORD PopIndex();

		// �� ���� ������ ���� ������ ���� ����
		DWORD InsertSession(SOCKET sock, SOCKADDR_IN* sockaddr);

		// Accept�� ������ �������� ���� �Լ�
		static unsigned int WINAPI AcceptThread(LPVOID lpParam);
		static unsigned int WINAPI WorkerThread(LPVOID lpParam);

		void AcceptThread(void);
		void WorkerThread(void);

		bool PostSend(st_SESSION* session);
		bool PostRecv(st_SESSION* session);

		void SessionRelease(st_SESSION* session);
		void DisconnectSession(DWORD id);
		joshua::st_SESSION* SessionReleaseCheck(LONG64 iSessionID);

		// IOCP completion notice
		void RecvComplete(st_SESSION* pSession, DWORD dwTransferred);
		void SendComplete(st_SESSION* pSession, DWORD dwTransferred);

	protected:
		// Accept�� ���� ó�� �Ϸ��� ȣ���ϴ� �Լ�
		virtual void OnClientJoin(SOCKADDR_IN* sockAddr, DWORD sessionID) = 0;
		virtual void OnClientLeave(DWORD sessionID) = 0;

		// accept����
		// TRUE�� ���� ���
		// FALSE�� ���� ����
		virtual bool OnConnectionRequest(SOCKADDR_IN* sockAddr) = 0;

		virtual void OnRecv(DWORD sessionID, CMessage* message) = 0;
		virtual void OnSend(DWORD sessionID, int sendsize) = 0;

		//	virtual void OnWorkerThreadBegin() = 0;                    
		//	virtual void OnWorkerThreadEnd() = 0;                      

		virtual void OnError(int errorcode, WCHAR*) = 0;

		void SendPacket(LONG64 id, CMessage* message);
		bool SendPacket_Dissconnect(LONG64 id, CMessage* message);
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

		BOOL Start(DWORD port, BOOL nagle, const WCHAR* ip = nullptr, DWORD threadCount = 0, DWORD MaxClient = 0);

		void ReStart();

		void PrintPacketCount();


	};
}
