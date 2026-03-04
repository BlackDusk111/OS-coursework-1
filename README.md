# OS-coursework-1
# ATM Monitoring and Alert System
# ATM OS  Codes

This repo has my Operating System code for Linux and Windows.  
It includes scheduling algorithm demos, ATM synchronization, and IPC between ATM transaction and monitor processes.

## Folder structure

- `linux/`
  - `atm_ipc_linux.c`
  - `atm_sync.c`
  - `atm_sync_solution_linux.c`
  - `linfcfs.c`
  - `linps.c`
  - `linrr.c`
  - `linsjf.c`

- `windows/`
  - `atm_ipc_windows.c`
  - `atm_sync.c`
  - `atm_sync_solution_windows.c`
  - `winfcfs.c`
  - `winps.c`
  - `winrr.c`
  - `winsjf.c`

## What is included

- CPU scheduling examples:
  - FCFS
  - Priority Scheduling
  - Round Robin
  - SJF
- ATM synchronization examples with proper locking
- ATM IPC examples using shared memory and synchronization primitives

## Run on Linux

```bash
cd linux
gcc atm_sync_solution_linux.c -o atm_sync_solution_linux -pthread
./atm_sync_solution_linux

gcc atm_ipc_linux.c -o atm_ipc_linux -pthread
./atm_ipc_linux monitor
./atm_ipc_linux writer
./atm_ipc_linux cleanup
```

## Run on Windows (PowerShell + MinGW gcc)

```powershell
cd windows
gcc atm_sync_solution_windows.c -o atm_sync_solution_windows.exe
.\atm_sync_solution_windows.exe

gcc atm_ipc_windows.c -o atm_ipc_windows.exe
.\atm_ipc_windows.exe monitor
.\atm_ipc_windows.exe writer
```

Simple goal of this repo: show OS concepts with real ATM-style examples in both Linux and Windows.
