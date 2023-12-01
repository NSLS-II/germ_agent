
## Flag definition in packet_buff_t

- Type: atomic_char flag;

- Bits
  7:   unused
  6:   udp_conn_thread accessing. This prevents other threads from reading the buffer.
  5:   data_writer_thread accessing. This prevents udp_conn_thread from reading the buffer.
  4:   data_proc_thread accessing. This prevents udp_conn_thread from reading the buffer.
  3-2: unused
  1:   data_write_thread access finished. This allows udp_conn_thread to read the buffer.
  0:   data_proc_thread access finished

  The udp_conn_thread clears the flag after writing to the buffer. A 0 value of the flag prevents data_writing_thread and data_proc_thread from reading the buffer.
