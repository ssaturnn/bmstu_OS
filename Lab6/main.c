#include <windows.h>
#include <stdbool.h>
#include <stdio.h>
#include <time.h>
#include <stdbool.h>


#define READERS_NUM 5
#define WRITERS_NUM 3

#define ITERS 18

HANDLE can_read;
HANDLE can_write;
HANDLE mutex;

HANDLE readers_threads[READERS_NUM];
HANDLE writers_threads[WRITERS_NUM];


int writers_wait_cnt = 0;
int readers_wait_cnt = 0;
int readers_active_cnt = 0;

bool is_writer_active = false;

int readers_id[READERS_NUM];
int writers_id[WRITERS_NUM];

int value = 0;


void start_read()
{
    InterlockedIncrement(&readers_wait_cnt);

    if (is_writer_active || WaitForSingleObject(can_write, 0) == WAIT_OBJECT_0)
    {
        WaitForSingleObject(can_read, INFINITE);
    }

    WaitForSingleObject(mutex, INFINITE); // при декременте ждущих читателей, может начать работать писатель 
	                                      //(потому что не произошел инкремент счетчика активных читателей)
    InterlockedDecrement(&readers_wait_cnt);

    SetEvent(can_read);
    InterlockedIncrement(&readers_active_cnt);

    ReleaseMutex(mutex);
}


void stop_read()
{
    InterlockedDecrement(&readers_active_cnt);

    if (readers_active_cnt == 0)
    {
        SetEvent(can_write);
    }
}


DWORD WINAPI run_reader(const LPVOID parametr)
{
    int id = *(int*) parametr;

    for (int i = 0; i < ITERS; i++)
    {
        int sleep_time = rand() % 3000 + 1000;
        Sleep(sleep_time);

        start_read();

        printf(">>>>> Reader: id = %2d, value = %2d, sleep = %3d ms\n", id, value, sleep_time);

        stop_read();
    }
}


void start_write()
{
    InterlockedIncrement(&writers_wait_cnt);

    if (is_writer_active || WaitForSingleObject(can_read, 0) == WAIT_OBJECT_0)
    {
        WaitForSingleObject(can_write, INFINITE);
    }

    InterlockedDecrement(&writers_wait_cnt);

    is_writer_active = true;
    ResetEvent(can_write);
}


void stop_write()
{
    is_writer_active = false;

    if (readers_wait_cnt == 0)
    {
        SetEvent(can_write);
    }
    else
    {
        SetEvent(can_read);
    }
}


DWORD WINAPI run_writer(const LPVOID parametr)
{
    int id = *(int*) parametr;

    for (int i = 0; i < ITERS; i++)
    {
        int sleep_time = rand() % 4000 + 1000;
        Sleep(sleep_time);

        start_write();

        value++;
        printf("<<<<< Writer: id = %2d, value = %2d, sleep = %3d ms\n", id, value, sleep_time);

        stop_write();
    }
}


int create_threads()
{
    // Readers
    for (int i = 0; i < READERS_NUM; i++)
    {
        readers_id[i] = i;
        readers_threads[i] = CreateThread(NULL, 0, run_reader, readers_id + i, 0, NULL);

        if (readers_threads[i] == NULL)
        {
            perror("\nError: Create thread - reader\n");
            return -1;
        }
    }


    // Writers
    for (int i = 0; i < WRITERS_NUM; i++)
    {
        writers_id[i] = i;
        writers_threads[i] = CreateThread(NULL, 0, run_writer,  writers_id + i, 0, NULL);

        if (writers_threads[i] == NULL)
        {
            perror("\nError: Create thread - writer\n");
            return -1;
        }
    }

    return 0;
}


void close()
{
    for (int i = 0; i < READERS_NUM; i++)
    {
        CloseHandle(readers_threads[i]);
    }

    for (int i = 0; i < WRITERS_NUM; i++)
    {
        CloseHandle(writers_threads[i]);
    }

    CloseHandle(mutex);
    CloseHandle(can_write);
    CloseHandle(can_read);
}


int main(void)
{
    setbuf(stdout, NULL);
    srand(time(NULL));

    // Инициализация
    mutex = CreateMutex(NULL, FALSE, NULL);

    if (mutex == NULL)
    {
        perror("\nError: Create mutex\n");
        return -1;
    }

    can_write = CreateEvent(NULL, TRUE, FALSE, NULL);

    if (can_write == NULL)
    {
        perror("\nError: Create event - can_write\n");
        return -1;
    }

    can_read = CreateEvent(NULL, TRUE, FALSE, NULL);

    if (can_read == NULL)
    {
        perror("\nError: Create event - can_read\n");
        return -1;
    }

    // Создать потоки
    int rc = create_threads();

    if (rc != 0)
    {
        return -1;
    }

    // Wait
    WaitForMultipleObjects(READERS_NUM, readers_threads, TRUE, INFINITE);
    WaitForMultipleObjects(WRITERS_NUM, writers_threads, TRUE, INFINITE);

    close();

    printf("\n\nProgram has finished successfully\n");

    return 0;
}