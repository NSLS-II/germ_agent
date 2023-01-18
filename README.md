This repository contains the following modules:

- EPICS IOC that that provides PVs
- Germanium daemon that receives data from FPGA through UDP connection, calculates spectra, and saves data and spectra to files.

PV prefix is defined in `prefix.cfg` file and should be located in the directory where the test agent program is to be run.



# 1. How to Build

The software modules are created under EPICS framework using `makeBaseApp` command in Linux, and configured per NSLS2 standard. To build them on non-NSLS2 computers, in `configure/RELEASE`, redefine the location of EPICS base. By default it doesn't require extra EPICS modules.

# 2. How to Use


- PV prefix is defined in `prefix.cfg` and should be identical to the definition in `iocBoot/iocgermDaemon/st.cmd`.

- Start the IOC with `./st.cmd` in the IOC directory.

- Start the Germanium daemon using `bin/linux-x86_64/germ_daemon` from the IOC directory.
