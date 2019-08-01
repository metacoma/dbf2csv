define print_uint64_t 
printf "%d %d %d %d, %d %d %d %d\n", buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7]
end

define hex_quad
printf "%02X %02X %02X %02X  %02X %02X %02X %02X",                          \
               *(unsigned char*)($arg0), *(unsigned char*)($arg0 + 1),      \
               *(unsigned char*)($arg0 + 2), *(unsigned char*)($arg0 + 3),  \
               *(unsigned char*)($arg0 + 4), *(unsigned char*)($arg0 + 5),  \
               *(unsigned char*)($arg0 + 6), *(unsigned char*)($arg0 + 7)
end
#set args /media/backup/share/FISSION.DBF
set args /media/backup/share/DeviceLock.dbf
#b main
#b 331
b 390
run
#hex_quad buf
