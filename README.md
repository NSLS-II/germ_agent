This repository contains a CA client program that works as the UDP daemon that saves data from Germanium detectors.
  
PV prefix is defined in `prefix.cfg` file.

# 1. Build

This program was created under EPICS framework in Linux, and configured per NSLS2 environment. To build on non-NSLS2 computers, redefine the location of EPICS base (`EPICS_BASE`) in `configure/RELEASE`.

# 2. Install

This software can be installed like a regular EPICS IOC using `systemd-softioc`/`manage-iocs` package. Refer to [https://github.com/NSLS-II/systemd-softioc](https://github.com/NSLS-II/systemd-softioc) for details on how to install the package and the IOC.

# 3. Use

- Define PV prefix in `prefix.cfg`. It should be identical to the prefix used in the Germanium detector IOC. Follow the format in the existing `prefix.cfg`.

- Make sure this program is running before start counting on the detector, if data saving is desired. This is indicated by `$(Sys)$(Dev):UDP_ONLINE`.

- Restart the program if the detector is rebooted or the IP address of the UDP data connection is changed. This can be done either in the IOC shell, through `manage-iocs restart germ_agent` command, or by write `1` to `$(Sys)$(Dev):UDP_RESTART`. The program restarts automatically if the Germanium detector IOC has been restarted.

# 4. Troubleshooting

- If the UDP daemon reports network errors when starting, or if no data is received from the UDP connection, please check if the computer is aware of the device's MAC address:

```
arp | grep "00:01:02:03:04:05"
```

If no entries are returned, manually to add the ARP entry:

```
sudo arp -s ${UDP-IP} 00:01:02:03:04:05
```

Where `${UDP-IP}` is the IP address of the UDP data connection, i.e., the value of `$(Sys)$(Dev).IPADDR`.
