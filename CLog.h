#pragma once
#include "stdafx.h"


#define MAX_FILE_NAME 32
#define MAX_LOG_LENGTH 128
#define MAX_FILE_PATH 128

enum class LOG_LEVEL
{
	e_DEBUG = 0,
	e_WARNNING,
	e_ERROR,
};

class SYSLOGCLASS
{

private:
	FILE* _hFile;
	WCHAR _wFileName[MAX_FILE_NAME];
	WCHAR _wLOGBUF[MAX_LOG_LENGTH];
	WCHAR _wFilePath[MAX_FILE_PATH] = L"\\";
	LOG_LEVEL _eDefaultLevel;
	int count;

	DWORD64 _dw64LogCount;

	static SYSLOGCLASS* _pSysLog;

	SYSLOGCLASS()
	{
		_hFile = nullptr;
		_wFilePath[0] = { '\0' };
		_wFileName[0] = { '\0' };
		_wLOGBUF[0] = { '\0' };
		_dw64LogCount = 0;
		_eDefaultLevel = LOG_LEVEL::e_DEBUG;
	}

	~SYSLOGCLASS()
	{

	}

public:

	void SYSLOG_DIRECTORY(const WCHAR* path)
	{
		StringCchCat(_wFilePath, MAX_FILE_PATH, path);
		StringCchCat(_wFilePath, MAX_FILE_PATH, L"\\");

	}

	void SYSLOG_LEVEL(LOG_LEVEL level)
	{
		_eDefaultLevel = level;
	}

	void LOG(const WCHAR* szType, LOG_LEVEL level,const WCHAR* szStringFormat,...)
	{
		HRESULT hResult;
		va_list va;
		WCHAR szInMessage[256]{ '\0' };
		va_start(va, szStringFormat);
		hResult  = StringCchVPrintf(szInMessage, 256, szStringFormat, va);
		va_end(va);

		if (count <= 0)
		{
			time_t now = time(NULL);
			struct tm date;
			localtime_s(&date, &now);
			WCHAR time[64] = { 0 };
			wcsftime(time, 64, L"%Y-%m-%d %H-%M-%S", &date);
			wcsftime(_wFileName, 64, L"%Y%m_", &date);

			StringCchCat(_wFileName, MAX_FILE_PATH, szType);
			StringCchCat(_wFileName, MAX_FILE_PATH, L".txt");
			StringCchCat(_wFilePath, MAX_FILE_PATH, _wFileName);
			count++;
		}

		time_t now = time(NULL);
		struct tm date;
		localtime_s(&date, &now);
		WCHAR time2[64] = { 0 };
		wcsftime(time2, 64, L"%Y-%m-%d %H-%M-%S", &date);
		if (level >= _eDefaultLevel)
		{			
			_hFile = _wfopen(_wFilePath, L"a+");
			if (_hFile == INVALID_HANDLE_VALUE)
			{
				wprintf(L"LogFile open failed\n");
				return;
			}

			switch (level)
			{
			case LOG_LEVEL::e_DEBUG:
				InterlockedIncrement(&_dw64LogCount);
				StringCchPrintf(_wLOGBUF, MAX_LOG_LENGTH, L"[%s] [%s / DEBUG / %08d] %s\n", szType, time2, _dw64LogCount, szInMessage);
				wprintf(L"%s", _wLOGBUF);
				break;
			case LOG_LEVEL::e_WARNNING:
				InterlockedIncrement(&_dw64LogCount);
				StringCchPrintf(_wLOGBUF, MAX_LOG_LENGTH, L"[%s] [%s / WARNNING / %08d] %s\n", szType, time2, _dw64LogCount, szInMessage);
				break;
			case LOG_LEVEL::e_ERROR:
				InterlockedIncrement(&_dw64LogCount);
				StringCchPrintf(_wLOGBUF, MAX_LOG_LENGTH, L"[%s] [%s / ERROR / %08d] %s\n", szType, time2, _dw64LogCount, szInMessage);
				wprintf(L"%s", _wLOGBUF);

				break;
			default:
				break;
			}

			fwprintf(_hFile, L"%s", _wLOGBUF);
			fclose(_hFile);
		}
	}

	static SYSLOGCLASS* GetInstance(void)
	{
		if (_pSysLog == nullptr)
		{
			_pSysLog = new SYSLOGCLASS();
			atexit(Destroy);
		}

		return _pSysLog;
	}

	static void Destroy()
	{
		delete _pSysLog;
	}
};

SYSLOGCLASS* SYSLOGCLASS::_pSysLog = nullptr;