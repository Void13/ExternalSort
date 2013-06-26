#include <conio.h>
#include <stdio.h>
#include <queue>
#include <stack>
#include <time.h>
#include <vector>
#include <Windows.h>

using namespace std;

// task 2

#define TEMP_FILE_NAME		"TempFiles\\task2temp.txt"
#define MAX_TEMP_FILES		8
#define MAX_ELEMS_IN_MEM	262
#define AMOUNT_OF_NUMBERS	200000

class HRTimer 
{
public:
	HRTimer() : frequency(GetFrequency()) { }
    LONGLONG GetFrequency()
	{
		LARGE_INTEGER proc_freq;
		QueryPerformanceFrequency(&proc_freq);
		return proc_freq.QuadPart;
	}
    void Reset() 
	{
		DWORD_PTR oldmask = SetThreadAffinityMask(GetCurrentThread(), 0);
		QueryPerformanceCounter(&start);
		SetThreadAffinityMask(GetCurrentThread(), oldmask);
	}
    double GetElapsed() 
	{
		DWORD_PTR oldmask = SetThreadAffinityMask(GetCurrentThread(), 0);
		QueryPerformanceCounter(&stop);
		SetThreadAffinityMask(GetCurrentThread(), oldmask);
		return ((stop.QuadPart - start.QuadPart) / (double)frequency);
	}
private:
    LARGE_INTEGER start;
    LARGE_INTEGER stop;
    LONGLONG frequency;
};
HRTimer timer;

int LogicalRightShift(int _nNumber, int _nShiftStep)
{
	return (_nNumber >> _nShiftStep) & ~(0xFFFFFFFF << (32 - _nShiftStep));
	//return (int)((unsigned int)_nNumber >> _nShiftStep);
}

int ConvToBigLittleEnd(int _nNumber)
{
	return 
		((_nNumber & 0xFF) << 24) | 
		((LogicalRightShift(_nNumber, 8) & 0xFF) << 16) | 
		((LogicalRightShift(_nNumber, 16) & 0xFF) << 8) | 
		LogicalRightShift(_nNumber, 24);
}

void fwriteBigEndian(FILE *_pFile, int _nNumber)
{
	int nNumber = ConvToBigLittleEnd(_nNumber);
	fwrite(&nNumber, 4, 1, _pFile);
}

int freadBigEndian(FILE *_pFile)
{
	int nNumber;
	fread_s(&nNumber, 4, 4, 1, _pFile);

	nNumber = ConvToBigLittleEnd(nNumber);

	return nNumber;
}

/*

	Функция создаёт файл примера с рандомными числами в формате BigEndian

*/
void ContructExample()
{
	srand((unsigned int)time(NULL));

	FILE *pFile;
	fopen_s(&pFile, "task2.txt", "wb");

	for (int i = 0; i < AMOUNT_OF_NUMBERS; i++)
	{
		int nNumber = rand() - rand();
		fwriteBigEndian(pFile, nNumber);

		//printf("%d ", nNumber);
	}

	//printf("\n\n");

	fclose(pFile);
}

/*

	Функция разбивает файл с числами на несколько файлов, в которых числа отсортированы по возрастанию

*/
void FirstStep()
{
	FILE *pTempFiles[MAX_TEMP_FILES];
	FILE *pSourceFile;
	FILE *pTempFile;

	int *pTempArray = new int[MAX_ELEMS_IN_MEM];

	fopen_s(&pSourceFile, "task2.txt", "rb");
	if (!pSourceFile)
		return;

	fopen_s(&pTempFile, TEMP_FILE_NAME, "wb");
	if (!pTempFile)
		return;

	char sName[40];
	for (int i = 0; i < MAX_TEMP_FILES; i++)
	{
		sprintf_s(sName, 40, "TempFiles\\test%d.txt", i);
		fopen_s(&pTempFiles[i], sName, "wb");
		if (!pTempFiles[i])
			return;

		fclose(pTempFiles[i]);

		fopen_s(&pTempFiles[i], sName, "rb");
	}

	int nIDTempFile = 0;

	while (!feof(pSourceFile))
	{
		int nNumReadedNumbs = 0;
		int nTmp = 0;

		for (int i = 0; i < MAX_ELEMS_IN_MEM; i++)
		{
			if (!fread_s(&pTempArray[i], 4, 4, 1, pSourceFile))
				break;

			pTempArray[i] = ConvToBigLittleEnd(pTempArray[i]);

			nNumReadedNumbs++;
		}

		sort(pTempArray, pTempArray + nNumReadedNumbs);

		bool bWaitingWriting = false;
		for (int i = 0; i < nNumReadedNumbs; i++)
		{
			while (1)
			{
				if (!bWaitingWriting)
				{
					bWaitingWriting = fread_s(&nTmp, 4, 4, 1, pTempFiles[nIDTempFile]) != 0;
				}

				if (!bWaitingWriting || nTmp > pTempArray[i])
					break;

				fwrite(&nTmp, 4, 1, pTempFile);
				bWaitingWriting = false;
			}

			fwrite(&pTempArray[i], 4, 1, pTempFile);
		}

		if (bWaitingWriting)
		{
			fwrite(&nTmp, 4, 1, pTempFile);
		}

		while (fread_s(&nTmp, 4, 4, 1, pTempFiles[nIDTempFile]))
		{
			fwrite(&nTmp, 4, 1, pTempFile);
		}
		
		fclose(pTempFiles[nIDTempFile]);
		fclose(pTempFile);

		sprintf_s(sName, 40, "TempFiles\\test%d.txt", nIDTempFile);
		remove(sName);
		rename(TEMP_FILE_NAME, sName);

		fopen_s(&pTempFile, TEMP_FILE_NAME, "wb");
		fopen_s(&pTempFiles[nIDTempFile], sName, "rb");
		if (!pTempFiles[nIDTempFile])
			return;

		nIDTempFile = (nIDTempFile + 1) % MAX_TEMP_FILES;
	}

	fclose(pSourceFile);
	fclose(pTempFile);
	
	for (int i = 0; i < MAX_TEMP_FILES; i++)
	{
		fclose(pTempFiles[i]);
	}

	delete[] pTempArray;
}

/*

	Функция собирает временные файлы в один(похожа техника на merge sort).

*/
void SecondStep()
{
	class CData
	{
	public:
		CData() 
		{
			pFile = nullptr;
		}
		~CData()
		{
			if (pFile)
			{
				fclose(pFile);
				pFile = nullptr;
			}
		}

		int nCurElem;
		FILE *pFile;
	};
	vector<CData> Files(MAX_TEMP_FILES);

	FILE *pSourceFile;
	fopen_s(&pSourceFile, "task2Result.txt", "wb");
	if (!pSourceFile)
		return;

	char sName[40];

	stack<vector<CData>::iterator> StackToErase;
	for (int i = 0; i < MAX_TEMP_FILES; i++)
	{
		sprintf_s(sName, 40, "TempFiles\\test%d.txt", i);
		fopen_s(&Files[i].pFile, sName, "rb");
		if (!Files[i].pFile)
			return;

		if (!fread_s(&Files[i].nCurElem, 4, 4, 1, Files[i].pFile))
			StackToErase.push(Files.begin() + i);
	}

	while (!StackToErase.empty())
	{
		Files.erase(StackToErase.top());
		StackToErase.pop();
	}

	while (!Files.empty())
	{
		int nMinimum = Files.begin()->nCurElem;
		auto FileWithMinimum = Files.begin();

		for (auto it = Files.begin(); it != Files.end(); it++)
		{
			if (nMinimum > it->nCurElem)
			{
				nMinimum = it->nCurElem;
				FileWithMinimum = it;
			}
		}

		if (!fread_s(&FileWithMinimum->nCurElem, 4, 4, 1, FileWithMinimum->pFile))
			Files.erase(FileWithMinimum);

		//printf("%d ", nMinimum);
		fwriteBigEndian(pSourceFile, nMinimum);
	}

	//printf("\n");

	fclose(pSourceFile);
}

int main()
{
	_wmkdir(L"TempFiles");

	timer.Reset();

	ContructExample();

	FirstStep();

	SecondStep();

	printf("The task has done!\n");
	printf("Time: %0.5f\n", timer.GetElapsed());

	_getch();
	return 0;
}