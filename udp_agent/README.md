This is the repository of the EPICS related operations for the data agent of Germanium detector. The overall idea is to have an EPICS thread to acquire the parameters for data file creation, and report its running environment and status.

The detector IOC defines the following PVs which are read/wirtten by the EPICS thread created by the data agent:

- `$(Sys)$(Dev)ScanStatus`
- `$(Sys)$(Dev)Data:Filename`
- `$(Sys)$(Dev)Data:Filename_ACK`
- `$(Sys)$(Dev)Data:Filesize`
- `$(Sys)$(Dev)Data:Filesize_ACK`
- `$(Sys)$(Dev)Data:Runno`
- `$(Sys)$(Dev)Data:Runno_ACK`
- `$(Sys)$(Dev)Data:AgentPid`
- `$(Sys)$(Dev)Data:AgentHostname`
- `$(Sys)$(Dev)Data:AgentDir`
- `$(Sys)$(Dev)Data:Heartbeat`
- `$(Sys)$(Dev)Data:Watchdog`
- `$(Sys)$(Dev)Data:Tolerance`
- `$(Sys)$(Dev)Data:AgentOnline`

Usage:

- `$(Sys)$(Dev)$(Sys)$(Dev)ScanStatus` is used to indicate the status of a scan: `IDLE`, `STAGE`, and `SCAN`. The information is set when in `IDLE`, and read by the thread in `STAGE`. `STAGE` is separated from `IDLE` to reduce number of CA messages, and can be merged to it if needed.

- When in `STAGE`, the thread reads `$(Sys)$(Dev)Data:Filename`, `$(Sys)$(Dev)Data:Filesize` and `$(Sys)$(Dev)Data:Runno`, and responds by writing the corresponding values to `$(Sys)$(Dev):*_ACK`s. A scan should start only after the correct `$(Sys)$(Dev)Data:Runno_ACK` is returned.

- The thread reports its PID, hostname, and absolute path the program is located.

- `$(Sys)$(Dev)Data:Watchdog` is a PV incrementing at 1Hz. Every second, the thread sends `$(Sys)$(Dev)Data:Heartbeat` and clears `$(Sys)$(Dev)Data:Watchdog`. If this is not performed for any reason (e.g., the thread crashes), `$(Sys)$(Dev)Data:Watchdog` will exceeds `$(Sys)$(Dev)Data:Tolerance` which turns `$(Sys)$(Dev)Data:AgentOnline` from 1 to 0. A scan program can rely on `$(Sys)$(Dev)Data:AgentOnline` to decide if the UDP daemon is still running.

PV prefix is defined in `prefix.cfg` file located in the same directory as the UDP daemon program.
