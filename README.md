This repository contains the following content:

- An EPICS IOC that that provides PVs;
  
- A UDP daemon that receives data from FPGA through a UDP connection and saves data to files;
  
- A Phoebus screen for the detector (in `${IOC-DIR}/opi/` directory).

PV prefix is defined in `${IOC-DIR}/prefix.cfg` file.

# 1. Build

The software modules are created under EPICS framework using `makeBaseApp` command in Linux, and configured per NSLS2 standard. To build on non-NSLS2 computers, in `configure/RELEASE`, redefine the location of EPICS base (`EPICS_BASE`) and ezca module (`EZCA`).

# 2. System requirement

The program receives data from 1G Ethernet connection, which causes significant amount of data being written to the disk in short time, and there could be potential data loss when writing to slow disks (even SSDs). For better disk access performance, it is recommended to save data file to a RAM disk temporarily, and then move them to permanent storage. This is performed by the program automatically. See #3 for details.

# 3. Use

## 3.1 IOC

- Define the beamline specific PV prefix and type of the detector (96, 192, or 384) in `${IOC-DIR}/iocBoot/iocgermDaemon/unique.cmd`.

- Define PV prefix in `prefix.cfg`. It should be identical to the definition in `${IOC-DIR}/iocBoot/iocgermDaemon/unique.cmd`. Follow the format in the existing `prefix.cfg`.

- Start the IOC by running `${IOC-DIR}/st.cmd`.

#### 3.2.1 PV definition

The values of the following PVs need to be given:

- `$(Sys)$(Dev).IPADDR`
  
  IP address of the UDP data connection.

- `$(Sys)$(Dev).FNAM`
  
  Name of data file. The UDP daemon will append run number and segment number to construct the full names of data files.

- `$(Sys)$(Dev):TMP_DATAFILE_DIR`
  
  Temporary location of data files. A directory on a RAM disk is preffered. Be sure the user running the UDP daemon have write access to it.

- `$(Sys)$(Dev):DATAFILE_DIR`
  
  Permanent location of the data files. Be sure the user running the UDP daemon have write access to it.

### 3.2 UDP daemon

- Start the Germanium daemon by running `${IOC-DIR}/bin/linux-x86_64/germ_daemon`. Restart the daemon if the detector is rebooted or has the IP address of the UDP data connection is changed.

### 3.3 Phoebus screen

The Germanium detector can be controlled from a Phonebus screen.

#### 3.3.1 Germanium software set status

- Make sure all the IOCs (the IOC in the detector and the IOC mentioned in this document) are running by checking if all the PVs are connected.
  
- Make sure the UDP daemon is running. This is indicated by the `Online` LED in Tab 2.

#### 3.3.2 Enable/disable test pulses

Turning on/off the test pulses is done by changing `$(Sys)$(Dev).TSEN`. From Tab2 of the screen, choose `Enable`, `Enable All`, `Disable`, or `Disable All` for `Test Pulses`, then click `Set`, the LED willl be lit. Once `$(Sys)$(Dev).TSEN` is set, the LED will be off.

#### 3.3.3 Enable/disable channels

Turning on/off the channels is done by changing `$(Sys)$(Dev).CHEN`. From Tab2 of the screen, choose `Enable`, `Enable All`, `Disable`, or `Disable All` under `Channels`, then click `Set`, the LED willl be lit. Once `$(Sys)$(Dev).CHEN` is set, the LED will be off.

## 4. Troubleshooting

- If the UDP daemon reports network errors when starting, or if no data is received from the UDP connection, please check if the computer is aware of the device's MAC address:

```
arp | grep "00:01:02:03:04:05"
```

If no entries are returned, manually to add the ARP entry:

```
sudo arp -s ${UDP-IP} 00:01:02:03:04:05
```

Where `${UDP-IP}` is the IP address of the UDP data connection, i.e., the value of `$(Sys)$(Dev).IPADDR`.
