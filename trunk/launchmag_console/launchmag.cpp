#pragma warning( disable : 4996 )

#include <stdio.h>
#include <memory.h>

#include "argtable/argtable2.h"

#include "../launchmag_firmware/LM_PacketFlags.h"

#ifdef WIN32
#include <fcntl.h>
#include <io.h>
#include <Windows.h>
#endif

#ifdef WIN32

// Portions of this function stolen from: http://www.naughter.com/enumser.html
// Copyright (c) 1998 - 2010 by PJ Naughter (Web: www.naughter.com, Email: pjna@naughter.com)
void ListComPorts()
{
	//Up to 255 COM ports are supported so we iterate through all of them seeing
	//if we can open them or if we fail to open them, get an access denied or general error error.
	//Both of these cases indicate that there is a COM port at that number. 
	for (unsigned int i = 1; i < 256; i++)
	{
		char portName[256];
		sprintf(portName, "COM%d", i);

		char portPath[256];
		sprintf(portPath, "\\\\.\\%s", portName);

		//Try to open the port
		bool bSuccess = FALSE;
		HANDLE hPort = ::CreateFileA(portPath, GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, 0, 0);
		if (hPort == INVALID_HANDLE_VALUE)
		{
			DWORD dwError = GetLastError();

			//Check to see if the error was because some other app had the port open or a general failure
			if (dwError == ERROR_ACCESS_DENIED || dwError == ERROR_GEN_FAILURE || dwError == ERROR_SHARING_VIOLATION || dwError == ERROR_SEM_TIMEOUT)
				bSuccess = TRUE;
		}
		else
		{
			//The port was opened successfully
			bSuccess = TRUE;

			//Don't forget to close the port, since we are going to do nothing with it anyway
			CloseHandle(hPort);
		}

		if (bSuccess)
			fprintf(stderr, "    %s\n", portName);
	}
}
#endif

enum LM_PrintMode
{
	LM_PRINTMODE_BINARY	= 0,
	LM_PRINTMODE_INTERPRET,
};

#define LM_PRINTFLAG_TRACK2		0x0001
#define LM_PRINTFLAG_TRACK1		0x0002
#define LM_PRINTFLAG_LABELS		0x0004

#define LM_TRACKBUFFER_SIZE		2048

bool LM_PrintInterpret(const char * track, int bitCount, int printFlags)
{
	char printableData[4096];
	memset(printableData, 0, sizeof(printableData));
	char *pPrintableData = printableData;

	if (printFlags & LM_PRINTFLAG_TRACK2)
	{
		unsigned char currentByte = 0;
		int currentBitCount = 0;

		for (; currentBitCount < bitCount; currentBitCount++)
		{
			if (currentByte == 0x0B)
				break;

			currentByte = currentByte >> 1;

			if (track[currentBitCount / 8] & (0x01 << (7 - (currentBitCount % 8))))
				currentByte |= 0x10;
		}

		if (currentByte != 0x0B)
			return false;

		if (pPrintableData - printableData > sizeof(printableData))
			return false;

		*(pPrintableData++) = currentByte + 0x30;

		int bitsRead = 0;
		bool calculatedEvenParity;

		for (; currentBitCount < bitCount; currentBitCount++)
		{
			currentByte = currentByte >> 1;

			if (track[currentBitCount / 8] & (0x01 << (7 - (currentBitCount % 8))))
				currentByte |= 0x10;

			if (++bitsRead == 5)
			{
				calculatedEvenParity = true;
				for (int i = 4; i-->0; )
				{
					if (currentByte & (1 << i))
						calculatedEvenParity = !calculatedEvenParity;
				}

				if (calculatedEvenParity != !!(currentByte & 0x10))
					return false;

				if (pPrintableData - printableData > sizeof(printableData))
					return false;

				*(pPrintableData++) = (currentByte & 0x0F) + 0x30;

				if (currentByte == 0x1F)
					break;

				bitsRead = 0;
			}
		}
		
		if (currentByte != 0x1F)
			return false;
	}
	else if (printFlags & LM_PRINTFLAG_TRACK1)
	{
		unsigned char currentByte = 0;
		int currentBitCount = 0;

		for (; currentBitCount < bitCount; currentBitCount++)
		{
			if (currentByte == 0x45)
				break;

			currentByte = currentByte >> 1;

			if (track[currentBitCount / 8] & (0x01 << (7 - (currentBitCount % 8))))
				currentByte |= 0x40;
		}

		if (currentByte != 0x45)
			return false;

		if (pPrintableData - printableData > sizeof(printableData))
			return false;

		*(pPrintableData++) = (currentByte & 0x3F) + 0x20;

		int bitsRead = 0;
		bool calculatedEvenParity;

		for (; currentBitCount < bitCount; currentBitCount++)
		{
			currentByte = currentByte >> 1;

			if (track[currentBitCount / 8] & (0x01 << (7 - (currentBitCount % 8))))
				currentByte |= 0x40;

			if (++bitsRead == 7)
			{
				calculatedEvenParity = true;
				for (int i = 6; i-->0; )
				{
					if (currentByte & (1 << i))
						calculatedEvenParity = !calculatedEvenParity;
				}

				if (calculatedEvenParity != !!(currentByte & 0x40))
					return false;

				if (pPrintableData - printableData > sizeof(printableData))
					return false;

				*(pPrintableData++) = (currentByte & 0x3F) + 0x20;

				if (currentByte == 0x1F)
					break;

				bitsRead = 0;
			}
		}
		
		if (currentByte != 0x1F)
			return false;
	}

	printf("%s", printableData);

	return true;
}

void LM_PrintBinary(const char * track, int bitCount)
{
	for (int i = 0; i < bitCount; i++)
	{
		if (track[i / 8] & (0x01 << (7 - (i % 8))))
			printf("1");
		else
			printf("0");
	}
}

void LM_ReverseTrackData(char * trackReversed, const char * track, int bitCount)
{
	memset(trackReversed, 0, LM_TRACKBUFFER_SIZE);

	for (int i = 0; i < bitCount; i++)
	{
		if (track[i / 8] & (0x01 << (7 - (i % 8))))
			trackReversed[((bitCount - 1) - i) / 8] |= (0x01 << (7 - (((bitCount - 1) - i) % 8)));
	}
}

void LM_MainLoop(FILE * inputStream, LM_PrintMode printMode, int printFlags)
{
	char track2[LM_TRACKBUFFER_SIZE];
	char track1[LM_TRACKBUFFER_SIZE];

	int track2BitCount = 0;
	int track1BitCount = 0;

	memset(track2, 0, LM_TRACKBUFFER_SIZE);
	memset(track1, 0, LM_TRACKBUFFER_SIZE);

	int track2PacketSize = -1;
	int track1PacketSize = -1;

	while (1)
	{
		int inputByte = fgetc(inputStream);

		char * track = (inputByte & LM_PACKET_FLAG_TRACK2) ? track2 : track1;
		int &bitCount = (inputByte & LM_PACKET_FLAG_TRACK2) ? track2BitCount : track1BitCount;
		int &packetSize = (inputByte & LM_PACKET_FLAG_TRACK2) ? track2PacketSize : track1PacketSize;

		if (inputByte & LM_PACKET_FLAG_STARTSTOPCONTROL)
		{
			if (inputByte & LM_PACKET_FLAG_START)
			{
				memset(track, 0, LM_TRACKBUFFER_SIZE);
				bitCount = 0;
				packetSize = -1;
			}
			else
			{
				if (	(inputByte & LM_PACKET_FLAG_TRACK2 && printFlags & LM_PRINTFLAG_TRACK2)
					||	(!(inputByte & LM_PACKET_FLAG_TRACK2) && printFlags & LM_PRINTFLAG_TRACK1))
				{
					if (printFlags & LM_PRINTFLAG_LABELS)
						printf("%s", (inputByte & LM_PACKET_FLAG_TRACK2) ? "Track 2: " : "Track 1: ");

					if (printMode == LM_PRINTMODE_INTERPRET)
					{
						if (!LM_PrintInterpret(track, bitCount, printFlags & ~((inputByte & LM_PACKET_FLAG_TRACK2) ? LM_PRINTFLAG_TRACK1 : LM_PRINTFLAG_TRACK2)))
						{
							char reversedTrack[LM_TRACKBUFFER_SIZE];
							LM_ReverseTrackData(reversedTrack, track, bitCount);
							if (!LM_PrintInterpret(reversedTrack, bitCount, printFlags & ~((inputByte & LM_PACKET_FLAG_TRACK2) ? LM_PRINTFLAG_TRACK1 : LM_PRINTFLAG_TRACK2)))
							{
								fprintf(stderr, "data read error");
							}
						}
					}
					else if (printMode == LM_PRINTMODE_BINARY)
					{
						LM_PrintBinary(track, bitCount);
					}
					printf("\n");
				}
			}
		}
		else
		{
			if (packetSize == -1)
				packetSize = inputByte & 0x0F;
			else
			{
				if (bitCount / 8 >= LM_TRACKBUFFER_SIZE)
				{
					fprintf(stderr, "Track buffer overflow on %s.", (inputByte & LM_PACKET_FLAG_TRACK2) ? "track2" : "track1");
				}
				else
				{
					for (int i = 0; i < packetSize; i++)
					{
						if ((0x01 << (4 - i)) & inputByte)
							track[bitCount / 8] |= 0x1 << (7 - (bitCount % 8));

						bitCount++;
					}
				}
				packetSize = -1;
			}
		}
	}
}

int main(int argc, char* argv[])
{
#ifdef WIN32
	struct arg_str  *comPortArg						= arg_str0("c", NULL, NULL,          "com port to use");
	struct arg_lit  *listCOMPortsArg        		= arg_lit0("l", "list",              "list com ports");
#endif
	struct arg_lit  *printBinaryArg					= arg_lit0("b", "binary",            "print card data in binary");
	struct arg_lit  *printNoLabelsArg				= arg_lit0("n", "no-labels",         "do not print track labels");
	struct arg_lit  *printTrack2Arg				    = arg_lit0("2", "print-2",           "print track 2");
	struct arg_lit  *printTrack1Arg				    = arg_lit0("1", "print-1",           "print track 1");
	struct arg_lit  *helpArg						= arg_lit0("h", "help",              "print this help and exit");
	struct arg_end  *endArg							= arg_end(20);

	void* argtable[] = {
#ifdef WIN32
		comPortArg,
		listCOMPortsArg,
#endif
		printBinaryArg,
		printNoLabelsArg,
		printTrack2Arg,
		printTrack1Arg,
		helpArg, 
		endArg};
		const char* progname = "launchmag";

		try 
		{
			fprintf(stderr, "\n**** launchmag                       Built: " __DATE__ " ****\n\n");

			if (arg_nullcheck(argtable) != 0)
			{
				fprintf(stderr, "%s: insufficient memory\n", progname);
				throw 1;
			}

			int nErrors = arg_parse(argc, argv, argtable);

			if (nErrors > 0)
			{
				arg_print_errors(stderr, endArg, progname);
				throw 0;
			}

			if (helpArg->count > 0)
			{
				fprintf(stderr, "Usage: %s\n", progname);
				throw 0;
			}

			FILE * inputStream = stdin;

#ifdef WIN32
			if (listCOMPortsArg->count > 0)
			{
				fprintf(stderr, "Open COM Ports:\n\n");
				ListComPorts();
				throw 1;
			}

			if (comPortArg->count < 0)
			{
				fprintf(stderr, "You must list a COM port to connect to. Use -%c", *(((*listCOMPortsArg).hdr).shortopts));
				throw 0;
			}

			char portPath[256];
			sprintf(portPath, "\\\\.\\%s", comPortArg->sval[0]);

			HANDLE hPort = ::CreateFileA(portPath, GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, 0, 0);
			if (hPort == INVALID_HANDLE_VALUE)
			{
				DWORD dwError = GetLastError();

				switch (dwError)
				{
				case ERROR_ACCESS_DENIED:
					fprintf(stderr, "Access denied to %s.\n", comPortArg->sval[0]);
					break;
				case ERROR_SHARING_VIOLATION:
					fprintf(stderr, "%s is already open in another program.\n", comPortArg->sval[0]);
					break;
				case ERROR_SEM_TIMEOUT:
					fprintf(stderr, "Timeout when attempting to open %s.\n", comPortArg->sval[0]);
					break;
				case ERROR_GEN_FAILURE:
				default:
					fprintf(stderr, "Could not open %s for an unknown reason.\n", comPortArg->sval[0]);
					break;
				}
				throw 0;
			}

			DCB dcb;
			dcb.DCBlength = sizeof(DCB);

			if (!::GetCommState(hPort, &dcb))
			{
				fprintf(stderr, "Unable to obtain serial port configuration on %s.\n", comPortArg->sval[0]);
				throw 0;
			}

			dcb.fBinary = TRUE;
			dcb.fDsrSensitivity = FALSE;
			dcb.fTXContinueOnXoff = FALSE;
			dcb.fErrorChar = FALSE;
			dcb.fNull = FALSE;
			dcb.fAbortOnError = FALSE;

			dcb.BaudRate = DWORD(CBR_9600);
			dcb.ByteSize = BYTE(8);
			dcb.Parity   = BYTE(NOPARITY);
			dcb.StopBits = BYTE(ONESTOPBIT);
			dcb.fParity  = false; 
			dcb.fOutxCtsFlow = false;
			dcb.fOutxDsrFlow = false;
			dcb.fDtrControl = DTR_CONTROL_DISABLE;
			dcb.fOutX = true;						
			dcb.fInX = true;						
			dcb.fRtsControl = RTS_CONTROL_DISABLE;

			if (!::SetCommState(hPort, &dcb))
			{
				fprintf(stderr, "Unable to configure serial port on %s.\n", comPortArg->sval[0]);
				throw 0;
			}

			COMMTIMEOUTS timeouts;
			timeouts.ReadIntervalTimeout = 1;
			timeouts.ReadTotalTimeoutMultiplier = 0;
			timeouts.ReadTotalTimeoutConstant = 0;
			timeouts.WriteTotalTimeoutMultiplier = 0;
			timeouts.WriteTotalTimeoutConstant = 0;
			if (!::SetCommTimeouts(hPort, &timeouts))
			{
				fprintf(stderr, "Unable to configure serial port timeouts on %s.\n", comPortArg->sval[0]);
				throw 0;
			}
		
			int libraryHandle = _open_osfhandle((intptr_t)hPort, _O_RDONLY);
			if (libraryHandle < 0)
			{
				fprintf(stderr, "Could not call _open_osfhandle on %s.\n", comPortArg->sval[0]);
				::CloseHandle(hPort);
				return false;
			}

			inputStream = fdopen(libraryHandle, "rb");
			if (!inputStream)
			{
				fprintf(stderr, "Could not call fdopen on %s.\n", comPortArg->sval[0]);
				_close(libraryHandle);
				return false;
			}
#endif

			LM_PrintMode printMode = LM_PRINTMODE_INTERPRET;

			if (printBinaryArg->count)
				printMode = LM_PRINTMODE_BINARY;

			int printFlags = 0;

			if (	!printTrack1Arg->count
				&&	!printTrack2Arg->count)
			{
				printFlags |= LM_PRINTFLAG_TRACK2 | LM_PRINTFLAG_TRACK1;
			}
			else
			{
				if (printTrack1Arg->count)
					printFlags |= LM_PRINTFLAG_TRACK1;

				if (printTrack2Arg->count)
					printFlags |= LM_PRINTFLAG_TRACK2;
			}

			if (!printNoLabelsArg->count)
				printFlags |= LM_PRINTFLAG_LABELS;

			LM_MainLoop(inputStream, printMode, printFlags);
		}
		catch (int e)
		{
			if (e == 0)
			{
				fprintf(stderr, "\n");
				arg_print_syntax(stderr, argtable, "\n");
				arg_print_glossary(stderr, argtable, "  %-25s %s\n");
			}
			arg_freetable(argtable, sizeof(argtable)/sizeof(argtable[0]));	
			return e;
		}
		arg_freetable(argtable, sizeof(argtable)/sizeof(argtable[0]));
		return 1;
}

