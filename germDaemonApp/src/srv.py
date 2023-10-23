#!/usr/bin/python

import socket

s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
s.bind(("127.0.0.1", 32000))
print("UDP bound on port 32000...")

while True:
	data, addr = s.recvfrom(1024)
	print("Receive from %s:%s" % (addr))
	if data == b"exit":
		s.sendto(b"Good bye!\n", addr)
		continue
	s.sendto(b"Hello %s!\n" % data, addr)

s.close()

