#pragma once
#include "stdafx.h"
#include "CCrashDumpClass.h"
#include "CLog.h"
#include "MemoryPool(LockFree).h"

#define CHECKCODE 0x0000000019921107

template<class DATA>
class CLFFreeList_TLS
{
private:
	class CDataDump;
	struct st_DataDump_Node
	{
		CDataDump* pDataDump;
		LONG64 lCheck;
		DATA Data;
	};

public:
	CLFFreeList_TLS(LONG lDumpCount = 200, BOOL bPlacementNew = false);
	virtual ~CLFFreeList_TLS();
	DATA* Alloc();
	bool Free(DATA* data);

	int GetUseCount() { return m_dwUseCount; }
	int GetAllocCount() { return m_pDataDump->GetAllocCount(); }

private:
	CDataDump* DataDumpAlloc()
	{
		CDataDump* DataDump = m_pDataDump->Alloc();

		if (DataDump->m_pDataDumpNode == nullptr)
		{
			DataDump->Initial(this, m_dwDumpCount);
		}
		else
			DataDump->Clear(m_dwDumpCount);

		TlsSetValue(m_dwTlsIndex, DataDump);

		return DataDump;
	}

private:
	DWORD m_dwTlsIndex;

	DWORD m_dwDumpCount;

	DWORD m_dwUseCount;

	CLFFreeList<CDataDump>* m_pDataDump;
	

private:

	class CDataDump
	{
	public:
		CDataDump();
		virtual ~CDataDump() { delete[] m_pDataDumpNode; };
		void Initial(CLFFreeList_TLS<DATA>* MemoryPool, DWORD DataCount);
		DATA* Alloc();
		bool Clear(DWORD size);
		bool Free();
	private:
		friend class CLFFreeList_TLS<DATA>;
		CLFFreeList_TLS<DATA>* m_pFreeList;
		st_DataDump_Node* m_pDataDumpNode;
		// 노드 덤프 내의 Data 개수
		DWORD m_dwNodeCount;
		// 총 데이터의 개수 중에서 할당된 개수
		DWORD m_dwUsingCount;
		// 총 데이터의 개수 중에서 남은 개수
		DWORD m_dwFreeCount;

	};
};

template<class DATA>
inline CLFFreeList_TLS<DATA>::CDataDump::CDataDump()
{
	m_pFreeList = nullptr;
	m_pDataDumpNode = nullptr;
	m_dwNodeCount = 0;
	m_dwUsingCount = 0;
	m_dwFreeCount = 0;
}

template<class DATA>
inline void CLFFreeList_TLS<DATA>::CDataDump::Initial(CLFFreeList_TLS<DATA>* MemoryPool, DWORD DataCount)
{
	m_pFreeList = MemoryPool;
	m_dwNodeCount = DataCount;
	m_dwFreeCount = DataCount;

	m_pDataDumpNode = new st_DataDump_Node[m_dwNodeCount];
	for (int i = 0; i < m_dwNodeCount; i++)
	{
		m_pDataDumpNode[i].pDataDump = this;
		m_pDataDumpNode[i].lCheck = CHECKCODE;
	}
}

template<class DATA>
inline DATA* CLFFreeList_TLS<DATA>::CDataDump::Alloc()
{
	DATA* allocData = &m_pDataDumpNode[m_dwUsingCount].Data;
	if (m_dwNodeCount == InterlockedIncrement(&m_dwUsingCount))
		m_pFreeList->DataDumpAlloc();
	return allocData;
}

template<class DATA>
inline bool CLFFreeList_TLS<DATA>::CDataDump::Clear(DWORD size)
{
	m_dwFreeCount = size;
	m_dwUsingCount = 0;
	return true;
}

template<class DATA>
inline bool CLFFreeList_TLS<DATA>::CDataDump::Free()
{
	if (InterlockedDecrement(&m_dwFreeCount) == 0)
	{
		m_pFreeList->m_pDataDump->Free(m_pDataDumpNode->pDataDump);

		return true;
	}
	return false;
}

template<class DATA>
inline CLFFreeList_TLS<DATA>::CLFFreeList_TLS(LONG lDumpCount, BOOL bPlacementNew)
{
	m_dwTlsIndex = TlsAlloc();
	if (m_dwTlsIndex == TLS_OUT_OF_INDEXES)
		CRASH();

	m_pDataDump = new CLFFreeList<CDataDump>(lDumpCount, bPlacementNew);
	m_dwDumpCount = lDumpCount;
	m_dwUseCount = 0;
}

template<class DATA>
inline CLFFreeList_TLS<DATA>::~CLFFreeList_TLS()
{
	TlsFree(m_dwTlsIndex);
	delete[] m_pDataDump;
}

template<class DATA>
inline DATA* CLFFreeList_TLS<DATA>::Alloc()
{
	CDataDump* DataDump = (CDataDump*)TlsGetValue(m_dwTlsIndex);
	if (DataDump == nullptr)
	{
		DataDump = DataDumpAlloc();
	}
	InterlockedIncrement(&m_dwUseCount);

	return DataDump->Alloc();	
}

template<class DATA>
inline bool CLFFreeList_TLS<DATA>::Free(DATA* data)
{
	st_DataDump_Node* DataDump = (st_DataDump_Node*)((char*)data - (sizeof(st_DataDump_Node::pDataDump) + sizeof(st_DataDump_Node::lCheck)));
	if (DataDump->lCheck != CHECKCODE)
		CRASH();

	InterlockedDecrement(&m_dwUseCount);

	DataDump->pDataDump->Free();

	return false;
}




