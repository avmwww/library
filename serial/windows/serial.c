/*
 *
 */

#include <stdio.h>
#include <windows.h>

#include "serial.h"

//#define DEBUG

#ifdef DEBUG
# define dbg		printf
#else
# define dbg(...)
#endif

/*
 *
 */

void ErrorExit(LPTSTR lpszFunction) 
{
	// Retrieve the system error message for the last-error code

	LPVOID lpMsgBuf;
	LPVOID lpDisplayBuf;
	DWORD dw = GetLastError(); 

	FormatMessage(
			FORMAT_MESSAGE_ALLOCATE_BUFFER | 
			FORMAT_MESSAGE_FROM_SYSTEM |
			FORMAT_MESSAGE_IGNORE_INSERTS,
			NULL,
			dw,
			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
			(LPTSTR) &lpMsgBuf,
			0, NULL );

	// Display the error message and exit the process

	lpDisplayBuf = (LPVOID)LocalAlloc(LMEM_ZEROINIT, 
			(lstrlen((LPCTSTR)lpMsgBuf) + lstrlen((LPCTSTR)lpszFunction) + 40) * sizeof(TCHAR)); 
	sprintf((LPTSTR)lpDisplayBuf, TEXT("%s failed with error %ld:\n%s"), lpszFunction, dw, (char *)lpMsgBuf); 
	MessageBox(NULL, (LPCTSTR)lpDisplayBuf, TEXT("Error"), MB_OK); 

	LocalFree(lpMsgBuf);
	LocalFree(lpDisplayBuf);
	ExitProcess(dw); 
}



/**
	\brief Opens a new connection to a serial port
	\param portname		name of the serial port(COM1 - COM9 or \\\\.\\COM1-COM256)
	\param baudrate		the baudrate of this port (for example 9600)
	\param stopbits		th nuber of stoppbits (one, onePointFive or two)
	\param parity		the parity (even, odd, off or mark)
	\return			HANDLE to the serial port
	*/
HANDLE openSerialPort(LPCSTR portname)
{
	dbg("Open serial port %s\n", portname);

	HANDLE hSerial = CreateFile(portname,
		GENERIC_READ | GENERIC_WRITE,
		0,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL);
	if (hSerial == INVALID_HANDLE_VALUE) {
		ErrorExit("Open Port: ");
	}
	dbg("Open serial port %s success\n", portname);
	return hSerial;
}

BOOL setTimeoutSerialPort(HANDLE hSerial, float tmo)
{
	COMMTIMEOUTS timeouts={0};
	dbg("Set timeout of serial port to %f sec\n", tmo);

	timeouts.ReadIntervalTimeout = 0xFFFFFFFF;
	timeouts.ReadTotalTimeoutConstant = (DWORD)(tmo * 1000);
	timeouts.ReadTotalTimeoutMultiplier = 0;
	timeouts.WriteTotalTimeoutConstant = (DWORD)(tmo * 1000);
	timeouts.WriteTotalTimeoutMultiplier = 0;
	if(!SetCommTimeouts(hSerial, &timeouts)) {
		ErrorExit("Setting timeouts failed");
	}
	dbg("Set timeout of serial port success\n");
	return TRUE;
}

BOOL setupSerialPort(HANDLE hSerial, enum Baudrate baudrate, enum Stopbits stopbits, enum Paritycheck parity)
{
	DCB dcbSerialParams = {0};
	dcbSerialParams.DCBlength = sizeof(dcbSerialParams);

	dbg("Set baud rate of serial port to %d sec\n", baudrate);

	if (!GetCommState(hSerial, &dcbSerialParams)) {
		 ErrorExit("Retrieving serial port parameters failed");
	}
	dcbSerialParams.BaudRate = baudrate;
	dcbSerialParams.ByteSize = 8;
	dcbSerialParams.StopBits = stopbits;
	dcbSerialParams.Parity = parity;
	if(!SetCommState(hSerial, &dcbSerialParams)){
		 ErrorExit("Setting Parameters failed");
	}
	if(!setTimeoutSerialPort(hSerial, 0.2)){
		ErrorExit("Setting timeouts failed");
	}
	dbg("Set baud rate of serial port success\n");
	return TRUE;
}

/**
	\brief Read data from the serial port
	\param hSerial		File HANDLE to the serial port
	\param buffer		pointer to the area where the read data will be written
	\param buffersize	maximal size of the buffer area
	\return				amount of data that was read
	*/
DWORD readFromSerialPort(HANDLE hSerial, char * buffer, int buffersize)
{
	DWORD dwBytesRead = 0;

	dbg("Start read from serial port\n");
	if(!ReadFile(hSerial, buffer, buffersize, &dwBytesRead, NULL)){
		ErrorExit("ReadFile");
	}
	dbg("Read from serial port %ld bytes\n", dwBytesRead);
	return dwBytesRead;
}

/**
	\brief write data to the serial port
	\param hSerial	File HANDLE to the serial port
	\param buffer	pointer to the area where the read data will be read
	\param length	amount of data to be read
	\return			amount of data that was written
	*/
DWORD writeToSerialPort(HANDLE hSerial, const char * data, int length)
{
	DWORD dwBytesRead = 0;

	dbg("Start write to serial port %d bytes\n", length);

	if(!WriteFile(hSerial, data, length, &dwBytesRead, NULL)){
		ErrorExit("WriteFile");
	}
	dbg("Writed to serial port %ld bytes\n", dwBytesRead);
	return dwBytesRead;
}

BOOL closeSerialPort(HANDLE hSerial)
{
	return CloseHandle(hSerial);
}
