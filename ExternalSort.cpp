#include <conio.h>
#include <stdio.h>
#include <queue>
#include <set>
#include <time.h>
#include <vector>
#include <map>
#include <Windows.h>
#include <functional>

#ifdef _DEBUG
	#define _CRTDBG_MAP_ALLOC
	#include <stdlib.h>
	#include <crtdbg.h>
#endif
// task 2

//#define DEBUG_INFO 1

#define KB					(1024)
#define MB					(1024 * KB)

#define SIZE_OF_FILE		(400 * MB)
#define AMOUNT_OF_NUMBERS	(SIZE_OF_FILE / 4)
#define MEMORY_SIZE			(512 * MB)
#define MAX_ELEMS_IN_MEM	(MEMORY_SIZE / 4)

int g_nNumFiles = 0;

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

/*

	Функция создаёт файл примера с рандомными числами в формате BigEndian

*/
void ContructExample()
{
	FILE *pFile = nullptr;
	fopen_s(&pFile, "task2.txt", "wb");

	int *pTempArray = new int[MAX_ELEMS_IN_MEM];

	for (int k = 0; k < AMOUNT_OF_NUMBERS / MAX_ELEMS_IN_MEM; k++)
	{
		for (int i = 0; i < MAX_ELEMS_IN_MEM; i++)
		{
			int nNumber = rand() - rand();

			pTempArray[i] = nNumber;
#ifdef DEBUG_INFO
			printf("%d ", nNumber);
#endif
		}

		fwrite(pTempArray, 4, MAX_ELEMS_IN_MEM, pFile);
	}

	memset(pTempArray, 0, MAX_ELEMS_IN_MEM * 4);

	int nCount = AMOUNT_OF_NUMBERS - (AMOUNT_OF_NUMBERS / MAX_ELEMS_IN_MEM) * MAX_ELEMS_IN_MEM;
	for (int i = 0; i < nCount; i++)
	{
		int nNumber = rand() - rand();

		pTempArray[i] = nNumber;

#ifdef DEBUG_INFO
		printf("%d ", nNumber);
#endif
	}

	fwrite(pTempArray, 4, nCount, pFile);

	delete[] pTempArray;
	fclose(pFile);
	
#ifdef DEBUG_INFO
	printf("\n\n");
#endif
}

/*

	Функция разбивает файл с числами на несколько файлов, в которых числа отсортированы по возрастанию

*/
bool FirstStep()
{
	FILE *pSourceFile = nullptr;
	FILE *pTempFile = nullptr;

	int *pTempArray = new int[MAX_ELEMS_IN_MEM];
	if (!pTempArray)
		return false;

	fopen_s(&pSourceFile, "task2.txt", "rb");
	if (!pSourceFile)
	{
		delete[] pTempArray;
		return false;
	}

	char sName[40];
	int nIDTempFile = 0;

	while (!feof(pSourceFile))
	{
		int nNumReadedNumbs = 0;

		if (!(nNumReadedNumbs = fread_s(pTempArray, MAX_ELEMS_IN_MEM * 4, 4, MAX_ELEMS_IN_MEM, pSourceFile)))
			break;

		std::sort(pTempArray, pTempArray + nNumReadedNumbs);

#ifdef DEBUG_INFO
		for (int i = 0; i < nNumReadedNumbs; i++)
		{
			printf("%d ", pTempArray[i]);
		}
		printf("\n\n");
#endif

		sprintf_s(sName, 40, "TempFiles\\test%d.txt", nIDTempFile);
		fopen_s(&pTempFile, sName, "wb");
		if (!pTempFile)
		{
			fclose(pSourceFile);
			delete[] pTempArray;

			return false;
		}

		if (!fwrite(pTempArray, 4, nNumReadedNumbs, pTempFile))
		{
			fclose(pSourceFile);
			fclose(pTempFile);
			delete[] pTempArray;

			return false;
		}

		fclose(pTempFile);

		nIDTempFile++;
	}

#ifdef DEBUG_INFO
	printf("\n\n");
#endif

	fclose(pSourceFile);
	delete[] pTempArray;

	g_nNumFiles = nIDTempFile;

	return true;
}

/*

	Функция собирает временные файлы в один(похожа техника на merge sort).

*/

bool SecondStep()
{
	class CFileAndData
	{
	public:
		CFileAndData() 
		{
			pFile = nullptr;
			pData = nullptr;
			pDataTile = nullptr;
			pDataHeapToRead = nullptr;
		};

		~CFileAndData()
		{
			if (pFile)
			{
				fclose(pFile);
				pFile = nullptr;
			}

			if (pData)
			{
				delete[] pData;
				pData = nullptr;
				pDataTile = nullptr;
				pDataHeapToRead = nullptr;
			}
		};

		FILE *pFile;
		int *pData;
		int *pDataHeapToRead;
		int *pDataTile;
	};

	struct _Comparator
	{
		bool operator()(CFileAndData const *const _pFirst, CFileAndData const *const _pSecond)
		{
			return *_pFirst->pDataHeapToRead < *_pSecond->pDataHeapToRead;
		};
	};

	// набор файлов с сортировкой по первому элементу
	std::vector<CFileAndData *> Files;

	FILE *pSourceFile;
	fopen_s(&pSourceFile, "task2result.txt", "wb");
	if (!pSourceFile)
		return false;

	// тут проблема
	// если кол-во файлов перевалило за размер памяти - то ничего не получится
	size_t dwSizeOfFileData = MAX_ELEMS_IN_MEM / (g_nNumFiles + 1);
	if (dwSizeOfFileData == 0)
	{
		fclose(pSourceFile);
		return false;
	}

	char sName[40];

	// читальщик файла, который читает файл с текущей позиции во всю длину куска памяти
	auto _ReadFile = [&](CFileAndData *_pFAD) -> bool
	{
		size_t dwNumReadedBytes = 0;
		if (!(dwNumReadedBytes = fread_s(_pFAD->pData, dwSizeOfFileData * 4, 4, dwSizeOfFileData, _pFAD->pFile)))
			return false;

		_pFAD->pDataHeapToRead = _pFAD->pData;
		_pFAD->pDataTile = _pFAD->pData + dwNumReadedBytes;

		return true;
	};

	// открываем каждый файл и читаем туда 
	for (int i = 0; i < g_nNumFiles; i++)
	{
		CFileAndData *pFAD = new CFileAndData;
		sprintf_s(sName, 40, "TempFiles\\test%d.txt", i);
		fopen_s(&pFAD->pFile, sName, "rb");
		if (!pFAD->pFile)
		{
			fclose(pSourceFile);
			Files.clear();

			return false;
		}

		pFAD->pData = new int[dwSizeOfFileData];
		Files.emplace_back(pFAD);

		_ReadFile(pFAD);
	}

	size_t dwSizeOfDstData = MAX_ELEMS_IN_MEM - dwSizeOfFileData * g_nNumFiles;
	int *pDataToWrite = new int[dwSizeOfDstData];
	int *pDataToWriteHeap = pDataToWrite;

	while (!Files.empty())
	{
		auto itMin = Files.begin();
		for (auto it = Files.begin(), itEnd = Files.end(); it != itEnd; it++)
		{
			if (*(*it)->pDataHeapToRead < *(*itMin)->pDataHeapToRead)
				itMin = it;
		}

		CFileAndData *pFileAndData = *itMin;

#ifdef DEBUG_INFO
		printf("%d ", *pFileAndData->pDataHeapToRead);
#endif

		*pDataToWriteHeap = *pFileAndData->pDataHeapToRead;
		pFileAndData->pDataHeapToRead++;
		pDataToWriteHeap++;

		// если достигли конца - надо загрузить ещё данных
		if (pFileAndData->pDataHeapToRead == pFileAndData->pDataTile)
		{
			// если файл кончился - выгрузить его и удалить
			if (!_ReadFile(pFileAndData))
			{
				delete pFileAndData;
				pFileAndData = nullptr;

				Files.erase(itMin);
			}
		}

		// если закончилась память для записи, записываем
		if (pDataToWriteHeap == pDataToWrite + dwSizeOfDstData)
		{
			fwrite(pDataToWrite, 4, dwSizeOfDstData, pSourceFile);

			// после записи нужно сбросить указатель на начало и опять в память писать числа
			pDataToWriteHeap = pDataToWrite;
		}
	}

	if (pDataToWrite != pDataToWriteHeap)
		fwrite(pDataToWrite, 4, pDataToWriteHeap - pDataToWrite, pSourceFile);

#ifdef DEBUG_INFO
	printf("\n\n");
#endif

	for (auto *pFAD : Files)
	{
		delete pFAD;
		pFAD = nullptr;
	}

	Files.clear();

	delete[] pDataToWrite;
	fclose(pSourceFile);

	return true;
}

int main()
{
#ifdef _DEBUG
	_CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
	_CrtDumpMemoryLeaks();
#endif

	srand((unsigned int)time(NULL));

	_wmkdir(L"TempFiles");

	//ContructExample();

	printf("Timer is started!\n");
	timer.Reset();

	if (!FirstStep())
	{
		printf("First step: error occured!\n");
		_getch();
		
		return 0;
	}

	if (!SecondStep())
	{
		printf("Second step: error occured!\n");
		_getch();
		
		return 0;
	}

#ifdef DEBUG_INFO
	FILE *pFile = nullptr;
	fopen_s(&pFile, "task2result.txt", "rb");
	if (!pFile)
	{
		printf("task2result file hasn't readed: error occured!\n");
		_getch();
		
		return 0;
	}

	int *pTempArray = new int[MAX_ELEMS_IN_MEM];

	while (!feof(pFile))
	{
		int nNumReadedNumbs = 0;

		if (!(nNumReadedNumbs = fread_s(pTempArray, MAX_ELEMS_IN_MEM * 4, 4, MAX_ELEMS_IN_MEM, pFile)))
			break;

		for (int i = 0; i < nNumReadedNumbs; i++)
		{
			printf("%d ", pTempArray[i]);
		}
	}
	printf("\n\n");

	delete[] pTempArray;
	fclose(pFile);
#endif
	//printf("Test: %d\n", i);

	printf("The task has done!\n");
	printf("Time: %0.5f\n", timer.GetElapsed());

	//_getch();
	return 0;
}