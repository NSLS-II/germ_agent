#!/bin/bash

#ps aux |grep bin/linux-x86_64/germ_test_agent |grep -v grep | awk '{print $2}' |xargs -r kill -9
#ps aux |grep bin/linux-x86_64/germ_udp_agent  |grep -v grep | awk '{print $2}' |xargs -r kill -9
#
#bin/linux-x86_64/germ_test_agent&
#bin/linux-x86_64/germ_udp_agent&

cd iocBoot/iocgermDaemon/
./st.cmd
