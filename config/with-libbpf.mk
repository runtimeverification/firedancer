CFLAGS+=-DFD_HAS_LIBBPF=1
LDFLAGS+=-lbpf -lelf
FD_HAS_LIBBPF:=1