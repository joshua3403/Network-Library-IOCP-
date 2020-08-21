#include "stdafx.h"
#include "CNetworkTest.h"

Network testNet;

int main()
{

	LOG_SET(LOG_CONSOLE | LOG_FILE, LOG_DEBUG);
	CMessage::SetMemoryPool(1000, FALSE);

	testNet.Start(6000,TRUE,NULL,NULL,500);

	while (true)
	{
		testNet.PrintPacketCount();
		Sleep(1000);
	}


	return 0;
}