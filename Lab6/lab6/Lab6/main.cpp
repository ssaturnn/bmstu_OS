#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <windows.h>
#include <time.h>

// HANDLE - дескриптор
HANDLE mutex;
HANDLE can_read;
HANDLE can_write;

// Функции InterlockedIncrement и InterlockedDecrement
// должны принимать параметр типа LONG
// LONG - 32х разрядное знаковое число
LONG waiting_writers_amount = 0; // кол-во ожидающих писателей
LONG waiting_readers_amount = 0; // кол-во ожидающих читателей
LONG active_readers_amount = 0; // кол-во активных писателей


int value = 0;

void start_read()
{
    InterlockedIncrement(&waiting_readers_amount);
    if (WaitForSingleObject(can_write, 0) == WAIT_OBJECT_0 && waiting_writers_amount)
    {
        WaitForSingleObject(can_read, INFINITE);
    }
    WaitForSingleObject(mutex, INFINITE);
    InterlockedDecrement(&waiting_readers_amount);
    InterlockedIncrement(&active_readers_amount);
    SetEvent(can_read);
    ReleaseMutex(mutex);
}

void stop_read()
{
    InterlockedDecrement(&active_readers_amount);
    if (active_readers_amount == 0)
    {
        SetEvent(can_write);
    }
}

void start_write(void)
{
    InterlockedIncrement(&waiting_writers_amount);
    if (active_readers_amount > 0)
    {
        WaitForSingleObject(can_write, INFINITE);
    }
    InterlockedDecrement(&waiting_writers_amount);
}

void stop_write(void)
{
    ResetEvent(can_write);
    if (waiting_readers_amount)
    {
        SetEvent(can_read);
    }
    else
    {
        SetEvent(can_write);
    }
}

// Функция потока читателя
// DWORD WINAPI - возвращаемый тип для функций потоков
DWORD WINAPI reader_run(CONST LPVOID param)
{
    int reader_id = (int)param;
    srand(time(NULL) + reader_id);
    int sleep_time;
    for (size_t i = 0; i < 4; i++)
    {
        sleep_time = 300 + rand() % 2000;
        Sleep(sleep_time);
        start_read();
        printf("Reader[%d] >>>> %d (sleep = %d)\n", reader_id, value, sleep_time);
        stop_read();
    }

    return 0;
}

DWORD WINAPI writer_run(CONST LPVOID param)
{
    int writer_id = (int)param;
    srand(time(NULL) + writer_id);
    int sleep_time;
    for (size_t i = 0; i < 4; i++)
    {
        sleep_time = 100 + rand() % 1500;
        Sleep(sleep_time);
        start_write();
        ++value;
        printf("Writer[%d] >>>> %d (sleep = %d)\n", writer_id, value, sleep_time);
        stop_write();
    }
    return 0;
}

int main()
{
    HANDLE readers_threads[5]; // массив потоков читателей
    HANDLE writers_threads[3]; // массив потоков писателей
    // Создание мьютекса
    // Для защиты разделяемого ресурса от одновременного доступа несколькими потоками
    // NULL1 - дескриптор не может наследоваться дочерними процессами
    // FALSE - вызывающий поток не получит права владения мьютексом,
    // то есть мьютекс будет разлочен
    // NULL2 - мьютекс создается без имени
    if ((mutex = CreateMutex(NULL, FALSE, NULL)) == NULL)
    {
        perror("CreateMutex error");
        exit(1);
    }
    // Создание объекта события автоматического сброса без начального состояния
    // Это нужно для уведомления ожидающего потока о возникновении события
    // NULL1 - дескриптор не может наследоваться дочерними процессами
    // FALSE1 - создание эвента автоматического сброса, и система автоматически
    // сбросит эвент в состояние без сигнала после освобождения одного ожидающего потока
    // FALSE2 - начальное состояние события не назначено
    // NULL2 - событие создается без имени
    if ((can_read = CreateEvent(NULL, FALSE, FALSE, NULL)) == NULL)
    {
        perror("CreateEvent (can_read) error");
        exit(1);
    }
    // TRUE - создается эвент сброса вручную, то есть потребуется использовать
    // функцию ResetEvent, чтобы установить эвент в состояние без сигнала
    if ((can_write = CreateEvent(NULL, TRUE, FALSE, NULL)) == NULL)
    {
        perror("CreateEvent (can_write) error");
        exit(1);
    }

    // Создание потоков для читателей
    // Создает поток чтобы выполнять код в виртуальном адресном
    // пространстве вызывающего процесса
    // NULL - поток получает дескриптор безопасности по умолчанию
    // 0 - начальный размер стека в байтах. Если он равен 0, то
    // поток использует размер стека по умолчанию
    // &reader_run - указатель на функцию, которую будет выполнять поток
    // Иными словами это начальный адрес.
    // (LPVOID)i - указатель на переменную, которая передается в поток
    // 0 - поток запускается сразу после создания
    // NULL - идентификатор потока не будет возвращен
    for (size_t i = 0; i < 5; i++)
    {
        readers_threads[i] = CreateThread(NULL, 0, &reader_run, (LPVOID)i, 0, NULL);
        if (readers_threads[i] == NULL)
        {
            perror("CreateThread (reader) error");
            exit(1);
        }
    }

    // Почему здесь без оператора взятия адреса????????????
    for (size_t i = 0; i < 3; i++)
    {
        writers_threads[i] = CreateThread(NULL, 0, writer_run, (LPVOID)i, 0, NULL);
        if (writers_threads[i] == NULL)
        {
            perror("CreateThread (writer) error");
            exit(1);
        }
    }

    // Ожидают завершения всех потоков из массивов
    // TRUE - ожидание завершения всех, а не хотя бы одного
    // INFINITE - время ожидание неограниченное
    WaitForMultipleObjects(5, readers_threads, TRUE, INFINITE);
    WaitForMultipleObjects(3, writers_threads, TRUE, INFINITE);


    // Closehandle закрывает дескриптор открытого объекта
    // Все открытые хендлы должны быть закрыты
    CloseHandle(mutex);
    CloseHandle(can_read);
    CloseHandle(can_write);

    for (size_t i = 0; i < 5; i++)
        CloseHandle(readers_threads[i]);

    for (size_t i = 0; i < 3; i++)
        CloseHandle(writers_threads[i]);

    return 0;
}
