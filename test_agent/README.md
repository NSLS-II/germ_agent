This is the repository of the test agent of Germanium detector that disables channels and test pulses. It's intended to be used to test from CSS screens.

The detector IOC defines the following PVs which are read/wirtten by the EPICS thread created by the data agent:

DET{GERM}TSEN
DET{GERM}CHEN
DET{GERM}TSEN_CTRL
DET{GERM}CHEN_CTRL
DET{GERM}TSEN_PROC
DET{GERM}CHEN_PROC
DET{GERM}TEST:AgentPid
DET{GERM}NELM
DET{GERM}MONCH
DET{GERM}TEST:WATCHDOG
DET{GERM}TEST:ONLINE
DET{GERM}TEST:TOLERANCE

Usage:

- `$(Sys)$(Dev)$(Sys)$(Dev)TSEN` and `$(Sys)$(Dev)$(Sys)$(Dev)CHEN` defines which channels have test pulses enabled, and which channels are enabled. They can be set one-by-one, all all at once, which are defined by `$(Sys)$(Dev)DET{GERM}TSEN_CTRL` and `$(Sys)$(Dev)DET{GERM}CHEN_CTR` separately.

- The channel to be set is selected by `$(Sys)$(Dev)MONCH`.

- Once a channel is selected and the way to change`xxEN` is set, write `1` to `$(Sys)$(Dev)TSEN_PROC` and `$(Sys)$(Dev)CHEN_PROC`. The thread will then process the change and clear the `_PROC` PVs.

- `$(Sys)$(Dev)TEST:WATCH` is a PV incrementing at 1Hz. Every second, the thread sends `$(Sys)$(Dev)TEST:HEARTBEAT` and clears `$(Sys)$(Dev)TEST:WATCHDOG`. If this is not performed for any reason (e.g., the thread crashes), `$(Sys)$(Dev)TEST:WATCHDOG` will exceeds `$(Sys)$(Dev)TEST:TOLERANCE` which turns `$(Sys)$(Dev)TEST:ONLINE` from 1 to 0. 

PV prefix is defined in `prefix.cfg` file located in the same directory as the test agent program.
