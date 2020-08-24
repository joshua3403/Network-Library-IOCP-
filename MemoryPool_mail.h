/*---------------------------------------------------------------

	procademy MemoryPool.

	메모리 풀 클래스 (오브젝트 풀 / 프리리스트)
	특정 데이타(구조체,클래스,변수)를 일정량 할당 후 나눠쓴다.

	- 사용법.

	procademy::CMemoryPool<DATA> MemPool(300, FALSE);
	DATA *pData = MemPool.Alloc();

	pData 사용

	MemPool.Free(pData);

				
----------------------------------------------------------------*/
#pragma once
#include "stdafx.h"


template <class DATA>
class CFreeList
{
private:

	/* **************************************************************** */
	// 각 블럭 앞에 사용될 노드 구조체.
	/* **************************************************************** */
	struct st_BLOCK_NODE
	{
		st_BLOCK_NODE()
		{
			stpNextBlock = nullptr;
			check = 0x1107;
			//wprintf(L"st_BLOCK_NODE Contructor\n");
		}

		~st_BLOCK_NODE()
		{
			//wprintf(L"st_BLOCK_NODE Destroyer\n");
		}
		DWORD check;
		st_BLOCK_NODE *stpNextBlock;
	};

public:

	//////////////////////////////////////////////////////////////////////////
	// 생성자, 파괴자.
	//
	// Parameters:	(int) 초기 블럭 개수.
	//				(bool) Alloc 시 생성자 / Free 시 파괴자 호출 여부
	// Return:
	//////////////////////////////////////////////////////////////////////////
			CFreeList(int iBlockNum, bool bPlacementNew = false);
	virtual	~CFreeList();


	//////////////////////////////////////////////////////////////////////////
	// 블럭 하나를 할당받는다.  
	//
	// Parameters: 없음.
	// Return: (DATA *) 데이타 블럭 포인터.
	//////////////////////////////////////////////////////////////////////////
	DATA	*Alloc(void);

	//////////////////////////////////////////////////////////////////////////
	// 사용중이던 블럭을 해제한다.
	//
	// Parameters: (DATA *) 블럭 포인터.
	// Return: (BOOL) TRUE, FALSE.
	//////////////////////////////////////////////////////////////////////////
	bool	Free(DATA *pData);


	//////////////////////////////////////////////////////////////////////////
	// 현재 확보 된 블럭 개수를 얻는다. (메모리풀 내부의 전체 개수)
	//
	// Parameters: 없음.
	// Return: (int) 메모리 풀 내부 전체 개수
	//////////////////////////////////////////////////////////////////////////
	int		GetAllocCount(void) { return m_iMaxCount; }

	//////////////////////////////////////////////////////////////////////////
	// 현재 사용중인 블럭 개수를 얻는다.
	//
	// Parameters: 없음.
	// Return: (int) 사용중인 블럭 개수.
	//////////////////////////////////////////////////////////////////////////
	int		GetUseCount(void) { return m_iUseCount; }


public:
	//friend class CMessage;

private :
	// 스택 방식으로 반환된 (미사용) 오브젝트 블럭을 관리.

	st_BLOCK_NODE* _pFreeNode;
	int m_iFreeCount;
	int m_iMaxCount;
	int m_iUseCount;
	bool  m_bUsingPlacementNew;
	CRITICAL_SECTION m_CS;

};

template<class DATA>
inline CFreeList<DATA>::CFreeList(int iBlockNum, bool bPlacementNew)
{
	// 새로히 생성할 객체 블럭
	DATA* newObject = nullptr;
	st_BLOCK_NODE* newNode = nullptr;

	// 맴버 변수 초기화
	m_iMaxCount = m_iFreeCount = iBlockNum;
	m_iUseCount = 0;
	_pFreeNode = new st_BLOCK_NODE;
	m_bUsingPlacementNew = bPlacementNew;
	InitializeCriticalSection(&m_CS);
	//wprintf(L"%d\n", sizeof(st_BLOCK_NODE));
	// 지역변수
	int count = m_iMaxCount;

	if (m_iMaxCount != 0)
	{

		while (count > 0)
		{
			void* newBlock = malloc(sizeof(st_BLOCK_NODE) + sizeof(DATA));
			newNode = new(newBlock) st_BLOCK_NODE;
			newNode->stpNextBlock = this->_pFreeNode->stpNextBlock;
			this->_pFreeNode->stpNextBlock = newNode;
			count--;
			//wprintf(L"CFreeList() : MemoryPool Header Pointer : %p, newNode Pointer : %p newObject Pointer : %p\n", this->_pFreeNode->stpNextBlock, newNode, newObject);

		}
	}
}

template<class DATA>
inline CFreeList<DATA>::~CFreeList()
{
	wprintf(L"~CFreeList()\n");
	int count = m_iMaxCount - m_iUseCount;
	void* temp = this->_pFreeNode->stpNextBlock;
	void* next = nullptr;

	while (count > 0)
	{
		wprintf(L"~CFreeList() : MemoryPool Header Pointer : %p\n", temp);
		next = (*(st_BLOCK_NODE*)temp).stpNextBlock;
		free(temp);
		temp = next;
		count--;

	}
}

template<class DATA>
inline DATA* CFreeList<DATA>::Alloc(void)
{
	EnterCriticalSection(&m_CS);
	// 새로히 생성할 객체 블럭
	DATA* newObject = nullptr;
	st_BLOCK_NODE* newNode = nullptr;

	// 현재 사용가능한 블럭 확인
	int FreeCount = m_iMaxCount - m_iUseCount;

	// 메모리 새로 확보
	if (FreeCount <= 0)
	{
		if (m_bUsingPlacementNew)
		{
			void* newBlock = malloc(sizeof(st_BLOCK_NODE) + sizeof(DATA));
			newNode = new(newBlock) st_BLOCK_NODE;
			newNode->stpNextBlock = this->_pFreeNode->stpNextBlock;
			this->_pFreeNode->stpNextBlock = newNode;
			newObject = new((char*)newBlock + sizeof(st_BLOCK_NODE)) DATA;
		}
		else
		{
			void* newBlock = malloc(sizeof(st_BLOCK_NODE) + sizeof(DATA));
			newNode = new(newBlock) st_BLOCK_NODE;
			newNode->stpNextBlock = this->_pFreeNode->stpNextBlock;
			this->_pFreeNode->stpNextBlock = newNode;
			newObject = new((char*)newBlock + sizeof(st_BLOCK_NODE)) DATA;
		}
		m_iMaxCount++;
	}

	void* blockPointer = this->_pFreeNode->stpNextBlock;
	// 블럭 포인터 반환
	if (m_bUsingPlacementNew)
	{
		newObject = new((char*)blockPointer + sizeof(st_BLOCK_NODE)) DATA;
	}
	this->_pFreeNode->stpNextBlock = (*(st_BLOCK_NODE*)blockPointer).stpNextBlock;
	//wprintf(L"Alloc() : MemoryPool Header Pointer : %p\n", this->_pFreeNode->stpNextBlock);
	m_iUseCount++;
	LeaveCriticalSection(&m_CS);
	return (DATA*)((char*)blockPointer + sizeof(st_BLOCK_NODE));
}

template<class DATA>
inline bool CFreeList<DATA>::Free(DATA* pData)
{
	EnterCriticalSection(&m_CS);
	void* returnedBlock = (char*)pData - sizeof(st_BLOCK_NODE);
	if ((*(st_BLOCK_NODE*)returnedBlock).check != 0x1107)
	{
		return false;
	}
	if (m_bUsingPlacementNew)
	{
		pData->~DATA();
	}
	(*(st_BLOCK_NODE*)returnedBlock).stpNextBlock = this->_pFreeNode->stpNextBlock;
	this->_pFreeNode->stpNextBlock = (st_BLOCK_NODE*)returnedBlock;
	m_iUseCount--;
	LeaveCriticalSection(&m_CS);
	return true;
}


