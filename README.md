This repository contains the following two software modules:

- test agent
- UDP agent

## 1. Module descriptions

### 1.1 Test agent

The test agent allows users to enable/disable channels and test pulses on Germamium detectors during test. It's intended to be used to test from CSS screens.

The detector IOC defines the following PVs which are read/wirtten by the EPICS thread created by the data agent:


- `$(Sys)$(Dev)TSEN`
- `$(Sys)$(Dev)CHEN`
- `$(Sys)$(Dev)TSEN_CTRL`
- `$(Sys)$(Dev)CHEN_CTRL`
- `$(Sys)$(Dev)TSEN_PROC`
- `$(Sys)$(Dev)CHEN_PROC`
- `$(Sys)$(Dev)NELM`
- `$(Sys)$(Dev)MONCH`
- `$(Sys)$(Dev)TEST:PID`
- `$(Sys)$(Dev)TEST:WATCHDOG`
- `$(Sys)$(Dev)TEST:TOLERANCE`
- `$(Sys)$(Dev)TEST:ONLINE`


Usage:

- `$(Sys)$(Dev)$(Sys)$(Dev)TSEN` and `$(Sys)$(Dev)$(Sys)$(Dev)CHEN` defines which channels have test pulses enabled, and which channels are enabled. They can be set one-by-one, all all at once, which are defined by `$(Sys)$(Dev)DET{GERM}TSEN_CTRL` and `$(Sys)$(Dev)DET{GERM}CHEN_CTR` separately. 

- The channel to be set is selected by `$(Sys)$(Dev)MONCH`.

- Once a channel is selected and the way to change`xxEN` is set, write `1` to `$(Sys)$(Dev)TSEN_PROC` and `$(Sys)$(Dev)CHEN_PROC`. The thread will then process the change and clear the `_PROC` PVs.

- `$(Sys)$(Dev)TEST:WATCH` is a PV incrementing at 1Hz. Every second, the thread sends `$(Sys)$(Dev)TEST:HEARTBEAT` and clears `$(Sys)$(Dev)TEST:WATCHDOG`. If this is not performed for any reason (e.g., the thread crashes), `$(Sys)$(Dev)TEST:WATCHDOG` will exceeds `$(Sys)$(Dev)TEST:TOLERANCE` which turns `$(Sys)$(Dev)TEST:ONLINE` from 1 to 0. 

PV prefix is defined in `prefix.cfg` file and should be located in the directory where the test agent program is to be run.

### 1.2 UDP agent

The UDP agent runs on data storage machine, receives and saves detector data from Germanium detector via UDP connection.

The detector IOC defines the following PVs which are read/wirtten by the EPICS thread created by the data agent:

- `$(Sys)$(Dev)ScanStatus`
- `$(Sys)$(Dev)FNAM`
- `$(Sys)$(Dev)FNAM_ACK`
- `$(Sys)$(Dev)FSIZ`
- `$(Sys)$(Dev)FSIZ_ACK`
- `$(Sys)$(Dev)RUNNO`
- `$(Sys)$(Dev)RUNNO_ACK`
- `$(Sys)$(Dev)DATA:PID`
- `$(Sys)$(Dev)DATA:HOSTNAME`
- `$(Sys)$(Dev)DATA:DIR`
- `$(Sys)$(Dev)DATA:HEARTBEAT`
- `$(Sys)$(Dev)DATA:WATCHDOG`
- `$(Sys)$(Dev)DATA:TOLERANCE`
- `$(Sys)$(Dev)DATA:ONLINE`

Usage:

- `$(Sys)$(Dev)$(Sys)$(Dev)ScanStatus` is used to indicate the status of a scan: `IDLE`, `STAGE`, and `SCAN`. The information is set when in `IDLE`, and read by the thread in `STAGE`. `STAGE` is separated from `IDLE` to reduce number of CA messages, and can be merged to it if needed.

  - When in `STAGE`, the thread reads `$(Sys)$(Dev)DATA:FNAM`, `$(Sys)$(Dev)DATA:FSIZ` and `$(Sys)$(Dev)DATA:RUNNO`, and responds by writing the corresponding values to `$(Sys)$(Dev):*_ACK`s. A scan should start only after the correct `$(Sys)$(Dev)DATA:RUNNO_ACK` is returned.

- The thread reports its PID, hostname, and absolute path the program is located.

- `$(Sys)$(Dev)DATA:WATCHDOG` is a PV incrementing at 1Hz. Every second, the thread sends `$(Sys)$(Dev)DATA:HEARTBEAT` and clears `$(Sys)$(Dev)DATA:WATCHDOG`. If `$(Sys)$(Dev)DATA:WATCHDOG` is not cleared for any reason (e.g., the thread crashes), `$(Sys)$(Dev)DATA:WATCHDOG` will exceeds `$(Sys)$(Dev)DATA:TOLERANCE`, which turns `$(Sys)$(Dev)DATA:ONLINE` from `1` to `0`. A scan program can rely on `$(Sys)$(Dev)DATA:ONLINE` to decide if the UDP daemon is still running.

PV prefix is defined in `prefix.cfg` file and should be located in the directory where the test agent program is to be run.

## 2. Build

The software modules are created under EPICS framework using `makeBaseApp` command in Linux, and configured per NSLS2 standard. It relies on ezca module (https://github.com/epics-extensions/ezca). To build them on non-NSLS2 computers, in `configure/RELEASE`, redefine the location of EPICS base, and the location of ezca extension if that is not built with EPICS base.
