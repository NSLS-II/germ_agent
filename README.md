This repository contains the following content for Germanium detector control and UDP data receiving:

- An EPICS IOC;
  
- A UDP daemon;
  
- A Phoebus screen (in `opi/` directory).

PV prefix is defined in `prefix.cfg` file.

# 1. Build

The software was created under EPICS framework in Linux, and configured per NSLS2 environment. To build on non-NSLS2 computers, redefine the location of EPICS base (`EPICS_BASE`) in `configure/RELEASE`.

# 2. Use

## 2.1 IOC

- Define the network environment, the PV prefix, and the type of the detector (96, 192, or 384) in `iocBoot/iocgermDaemon/unique.cmd`.

- Start the IOC by running `st.cmd`.

- Assign values to the following PVs:

  - `$(Sys)$(Dev).IPADDR`
    
    IP address of the UDP data connection.
  
  - `$(Sys)$(Dev).FNAM`
    
    Name of data file, include the directory where the data files are to be saved. The UDP daemon will append run number and segment number to construct the full names of data files.

### 2.2 UDP daemon

- Define PV prefix in `prefix.cfg`. It should be identical to the definition in `iocBoot/iocgermDaemon/unique.cmd`. Follow the format in the existing `prefix.cfg`.

- Start the Germanium daemon by running `./start-udp`. Restart the daemon if the detector is rebooted or the IP address of the UDP data connection is changed.

### 2.3 Phoebus screen

The Germanium detector can be controlled from a Phonebus screen. It should have the following macros defined:

- $(Sys) Should be identical to the value defined in `iocBoot/iocgermDaemon/unique.cmd`.
  
- $(Dev) Should be identical to the value defined in `iocBoot/iocgermDaemon/unique.cmd`.

- $(NELM) Depends on the detector type: 96, 192, or 384.

See `opi/Open-GeRM.bob` for examples.

#### 2.3.1 Germanium software set status

- Make sure all the IOCs (the IOC in the detector and the IOC mentioned in this document) are running by checking if all the PVs are connected.
  
- Make sure the UDP daemon is running. This is indicated by the `UDP Ready` LED in Tab 1.

#### 2.3.2 Enable/disable test pulses

Turning on/off the test pulses is done by changing `$(Sys)$(Dev).TSEN`. From Tab2 of the screen, choose `Enable`, `Enable All`, `Disable`, or `Disable All` for `Test Pulses`, then click `Set`, the LED willl be lit. Once `$(Sys)$(Dev).TSEN` is set by the UDP daemon, the LED will be off.

#### 2.3.3 Enable/disable channels

Turning on/off the channels is done by changing `$(Sys)$(Dev).CHEN`. From Tab2 of the screen, choose `Enable`, `Enable All`, `Disable`, or `Disable All` under `Channels`, then click `Set`, the LED willl be lit. Once `$(Sys)$(Dev).CHEN` is set by the UDP daemon, the LED will be off.

## 3. Troubleshooting

- If the UDP daemon reports network errors when starting, or if no data is received from the UDP connection, please check if the computer is aware of the device's MAC address:

```
arp | grep "00:01:02:03:04:05"
```

If no entries are returned, manually to add the ARP entry:

```
sudo arp -s ${UDP-IP} 00:01:02:03:04:05
```

Where `${UDP-IP}` is the IP address of the UDP data connection, i.e., the value of `$(Sys)$(Dev).IPADDR`.
