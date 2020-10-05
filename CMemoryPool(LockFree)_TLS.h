#pragma once
#include "stdafx.h"
#include "CCrashDumpClass.h"
#include "CLog.h"
#include "MemoryPool(LockFree).h"
#include "Profiler(TLS).h"

#define MAXDUMPCOUNT 3000
#define CHECKCODE 0x0000000019921107

template<class DATA>
class CLFFreeList_TLS
{
private:
	class CDataDump;
	struct st_DataDump_Node
	{
		CDataDump* pDataDump;
		DATA Data;
	};

public:
	CLFFreeList_TLS(BOOL bPlacementNew = false);
	virtual ~CLFFreeList_TLS();
	DATA* Alloc();
	bool Free(DATA* data);

	int GetUseCount() { return m_dwUseCount; }
	int GetAllocCount() { return m_pDataDump->GetAllocCount(); }

private:
	CDataDump* DataDumpAlloc()
	{
		CDataDump* DataDump = m_pDataDump->Alloc();

		if (!TlsSetValue(m_dwTlsIndex, DataDump))
			return nullptr;

		new (DataDump)CDataDump();

		DataDump->m_pFreeList = this;

		InterlockedIncrement(&m_dwUseCount);

		return DataDump;
	}

private:
	DWORD m_dwTlsIndex;

	DWORD m_dwUseCount;

	CLFFreeList<CDataDump>* m_pDataDump;	

private:

	class CDataDump
	{
	public:
		CDataDump() : m_dwNodeCount(MAXDUMPCOUNT), m_dwFreeCount(MAXDUMPCOUNT), m_dwUsingCount(0) {};
		virtual ~CDataDump() { delete[] m_pDataDumpNode; };
		DATA* Alloc();
		bool Free();
	private:
		friend class CLFFreeList_TLS<DATA>;
		CLFFreeList_TLS<DATA>* m_pFreeList;
		st_DataDump_Node m_pDataDumpNode[MAXDUMPCOUNT];
		// 노드 덤프 내의 Data 개수
		DWORD m_dwNodeCount;
		// 총 데이터의 개수 중에서 할당된 개수
		DWORD m_dwUsingCount;
		// 총 데이터의 개수 중에서 남은 개수
		DWORD m_dwFreeCount;

	};
};

template<class DATA>
inline DATA* CLFFreeList_TLS<DATA>::CDataDump::Alloc()
{
	m_dwNodeCount--;
	m_pDataDumpNode[m_dwNodeCount].pDataDump = this;
	
	if (0 ==  m_dwNodeCount)
		m_pFreeList->DataDumpAlloc();

	return &m_pDataDumpNode[m_dwNodeCount].Data;
}


template<class DATA>
inline bool CLFFreeList_TLS<DATA>::CDataDump::Free()
{
	if (!(--m_dwFreeCount))
	{
		m_pFreeList->m_pDataDump->Free(this);

		return true;
	}
	return false;
}

template<class DATA>
inline CLFFreeList_TLS<DATA>::CLFFreeList_TLS(BOOL bPlacementNew)
{
	m_dwTlsIndex = TlsAlloc();
	if (m_dwTlsIndex == TLS_OUT_OF_INDEXES)
		CRASH();

	m_pDataDump = new CLFFreeList<CDataDump>();
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
	st_DataDump_Node* DataDump = (st_DataDump_Node*)((char*)data - sizeof(st_DataDump_Node::pDataDump));

	InterlockedDecrement(&m_dwUseCount);
	
	return DataDump->pDataDump->Free();
}




