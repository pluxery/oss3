#pragma once

#include <thread>
#include <fstream>
#include <chrono>
#include <iostream>
#include <ctime>
#include <atomic>
#include <stdlib.h>
#include "SharedMemory.hpp"
#include "TimeUtils.hpp"

#if defined(WIN32)

#   include <windows.h>

#   define MEMORY_NAME "Local\\memory_name"
#   define INVALID_MEMORY NULL
#   define SEM_TYPE HANDLE
#else

#   include <unistd.h>
#   include <fcntl.h>
#   include <utility>
#   include <filesystem>
#   include <semaphore.h>
#   include <sys/mman.h>
#include <sys/wait.h>

#   define MEMORY_NAME "/memory_name"
#   define INVALID_MEMORY (-1)
#   define SEM_TYPE sem_t *
#   define HANDLE int
#endif
#define SEM_NAME "semaphore_name"
#define SEP '/'
#define EXT_EXE ".exe"
#define COPY_NAME_1 "copy_1"
#define COPY_NAME_2 "copy_2"
#define LOG_NAME "log.txt"

enum CopyType {
    COPY_TYPE_1 = 1,
    COPY_TYPE_2 = 2,
};


class Program {
public:
    explicit Program(const std::string &dir) {
#if defined(WIN32)
        char path[MAX_PATH];
        char filename[MAX_PATH];
        _splitpath(dir.c_str(), NULL, path, filename, NULL);

        this->executableName = filename;
        this->_path = path;
#else
        this->_path = std::filesystem::path(dir).parent_path();
#endif
        this->OpenSharedMemory();
        if (this->_mem == INVALID_MEMORY) {
            this->_isNewProcess = true;
            this->CreateSharedMemory();
        }
        this->MapCounter();
        this->_log.open(this->_path + SEP + LOG_NAME,
                        std::fstream::out | std::fstream::app | std::fstream::in);
    };

    void StartProgram(int copyType = 0) {
        if (copyType == COPY_TYPE_1) {
            this->WaitSema();
            this->_log << "copy #1 pid: " << getpid() << "start time: " << TimeUtils::TimeNow() << std::endl;
            this->_sharedMemory->counter += 10;
            this->_log << "end time: " << TimeUtils::TimeNow() << std::endl;
            this->ReleaseSema();
            return;
        }
        if (copyType == COPY_TYPE_2) {
            this->WaitSema();
            this->_log << "copy #2 pid: " << getpid() << "start time: " << TimeUtils::TimeNow() << std::endl;
            this->_sharedMemory->counter *= 2;
            this->ReleaseSema();

            TimeUtils::Sleep(2);

            this->WaitSema();
            this->_sharedMemory->counter /= 2;
            this->_log << "end time: " << TimeUtils::TimeNow() << std::endl;
            this->ReleaseSema();
            return;
        }
        std::thread thCnt(&Program::AddOneToCounter, this);
        std::thread thCmdEdit(&Program::EditCounterFromCmd, this);
        std::thread *thWriteLog = NULL;
        std::thread *thCopies = NULL;

        if (this->_isNewProcess) {
            if (this->_log.is_open()) {
                this->WaitSema();
                this->_log << "pid:" << getpid() << std::endl << "start time: " << TimeUtils::TimeNow() << std::endl;
                this->ReleaseSema();
            }
            thWriteLog = new std::thread(&Program::WriteLog, this);
            thCopies = new std::thread(&Program::CreateCopies, this);
        }
        thCnt.join();
        thCmdEdit.join();
        if (thCopies != nullptr) {
            thWriteLog->join();
            thCopies->join();
        }

    };

    ~Program() {
        this->_sharedMemory->countOfOpenedPrograms--;
        if (this->_sharedMemory->countOfOpenedPrograms == 0) {
            this->RemoveSharedMemory();
        }
#if defined(WIN32)
        CloseHandle(this->_sema);
#else
        sem_close(this->_sema);
#endif
        this->_log.close();
    };
private:
#if defined(WIN32)
    std::string executableName;
#endif
    std::atomic<bool> _continue = true;
    SEM_TYPE _sema = nullptr;
    std::fstream _log;
    std::string _path;
    HANDLE _mem;
    bool _isNewProcess = false;
    SharedMemory *_sharedMemory;

    void AddOneToCounter() {
        while (this->_continue) {
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
            this->WaitSema();
            this->_sharedMemory->counter++;
            this->ReleaseSema();
        }
    };

    void WriteLog() {
        while (this->_continue) {
            TimeUtils::Sleep(1);
            if (this->_log.is_open()) {
                this->WaitSema();
                this->_log << "pid:" << getpid() << std::endl << "time: " << TimeUtils::TimeNow() << " counter = "
                           << this->_sharedMemory->counter << std::endl;
                this->ReleaseSema();
            }
        }
    };

    void CreateCopies() {
        while (this->_continue) {
            TimeUtils::Sleep(3);
#if defined(WIN32)
            STARTUPINFO startupinfo, startupinfo2;
            PROCESS_INFORMATION pi, pi2;
            ZeroMemory(&startupinfo, sizeof(startupinfo));
            ZeroMemory(&pi, sizeof(pi));
            ZeroMemory(&startupinfo2, sizeof(startupinfo2));
            ZeroMemory(&pi2, sizeof(pi2));
            std::string progPath = this->_path + this->executableName + EXT_EXE;
            std::string cmdArgCopy1 = progPath + " " + COPY_NAME_1;
            std::string cmdArgCopy2 = progPath + " " + COPY_NAME_2;
            if (!CreateProcess(
                    progPath.c_str(),
                    (LPSTR) cmdArgCopy1.c_str(),
                    NULL,
                    NULL,
                    FALSE,
                    0,
                    NULL,
                    NULL,
                    &startupinfo,
                    &pi
            )) {
                std::cerr << "copy 1 start failed!" << std::endl;
            }
            if (!CreateProcess(
                    progPath.c_str(),
                    (LPSTR) cmdArgCopy2.c_str(),
                    NULL,
                    NULL,
                    FALSE,
                    0,
                    NULL,
                    NULL,
                    &startupinfo2,
                    &pi2
            )) {
                std::cerr << "copy 2 start failed!" << std::endl;;
            }
            WaitForSingleObject(pi.hProcess, INFINITE);
            WaitForSingleObject(pi2.hProcess, INFINITE);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
            CloseHandle(pi2.hProcess);
            CloseHandle(pi2.hThread);

#else
            int status;
            HANDLE fd[2];
            HANDLE fd2[2];
            pipe(fd2);
            pipe(fd);
            pid_t childPid1 = fork();
            pid_t childPid2 = 0;

            if (childPid1) {
                close(fd[0]);
                write(fd[1], &childPid1, sizeof(int));
                close(fd[1]);
                childPid2 = fork();
            } else {
                close(fd[1]);
                read(fd[0], &childPid1, sizeof(int));
                close(fd[0]);
            }
            if (childPid2) {
                close(fd2[0]);
                write(fd2[1], &childPid2, sizeof(int));
                close(fd2[1]);
            } else if (getpid() != childPid1) {
                close(fd2[1]);
                read(fd2[0], &childPid2, sizeof(int));
                close(fd2[0]);
            }

            if (getpid() == childPid1) {
                this->WaitSema();
                this->_log << "copy #1 pid: " << getpid() << " time start" << TimeUtils::TimeNow() << std::endl;
                this->_sharedMemory->counter += 10;
                this->_log << "end time: " << TimeUtils::TimeNow() << std::endl;
                this->ReleaseSema();
                return;
            }
            if (getpid() == childPid2) {
                this->WaitSema();
                this->_log << "copy #2 pid" << getpid() << " time start: " << TimeUtils::TimeNow() << std::endl;
                this->_sharedMemory->counter *= 2;
                this->ReleaseSema();

                sleep(2);

                this->WaitSema();
                this->_sharedMemory->counter /= 2;
                this->_log << "end time: " << TimeUtils::TimeNow() << std::endl;
                this->ReleaseSema();
                return;
            }

            waitpid(childPid1, &status, WUNTRACED);
#endif
        }
    };

    void EditCounterFromCmd() {
        while (this->_continue) {
            int newCntVal;
            std::cin >> newCntVal;
            if (newCntVal == -1) {
                this->_continue = false;
                return;
            }
            this->WaitSema();
            this->_sharedMemory->counter = newCntVal;
            this->ReleaseSema();
        }
    };

    void CreateSharedMemory() {
#if defined(WIN32)
        this->_mem = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(SharedMemory),
                                       MEMORY_NAME);
#else
        this->_mem = shm_open(MEMORY_NAME, O_CREAT | O_EXCL | O_RDWR, 0644);
        ftruncate(this->_mem, sizeof(SharedMemory));
        this->_sema = sem_open(SEM_NAME, O_CREAT | O_EXCL | O_RDWR, 0644);
#endif
    };

    void MapCounter() {
#if defined(WIN32)
        this->_sharedMemory = reinterpret_cast<SharedMemory *>(MapViewOfFile(this->_mem, FILE_MAP_WRITE, 0, 0,
                                                                             sizeof(SharedMemory)));
#else
        void *res = mmap(nullptr, sizeof(SharedMemory), PROT_WRITE | PROT_READ, MAP_SHARED, this->_mem, 0);
        this->_sharedMemory = reinterpret_cast<SharedMemory *>(res);
#endif
        if (this->_isNewProcess) {
            this->_sharedMemory->counter = 0;
            this->_sharedMemory->countOfOpenedPrograms = 0;
        }
        this->_sharedMemory->countOfOpenedPrograms++;
    };

    void UnmapCounter() {
#if defined(WIN32)
        UnmapViewOfFile(this->_sharedMemory);
#else
        munmap(this->_sharedMemory, sizeof(SharedMemory));
#endif
    };

    void OpenSharedMemory() {
#if defined(WIN32)
        this->_mem = OpenFileMapping(FILE_MAP_WRITE, true, MEMORY_NAME);
        this->_sema = OpenSemaphore(SEMAPHORE_ALL_ACCESS, false, SEM_NAME);
#else
        this->_mem = shm_open(MEMORY_NAME, O_RDWR, 0644);
        this->_sema = sem_open(SEM_NAME, O_RDWR, 0644);
#endif
    };

    void RemoveSharedMemory() {
        this->CloseSharedMemory();
#if defined(WIN32)
#else
        shm_unlink(MEMORY_NAME);
#endif
    };

    void CloseSharedMemory() {
        this->UnmapCounter();
#if defined(WIN32)
        CloseHandle(this->_mem);
#else
        close(this->_mem);
#endif
    };

    void WaitSema() {
#if defined(WIN32)
        WaitForSingleObject(this->_sema, 0);
#else
        sem_wait(this->_sema);
#endif
    };

    void ReleaseSema() {
#if defined (WIN32)
        ReleaseSemaphore(this->_sema, 1, NULL);
#else
        sem_post(this->_sema);
#endif
    };
};
