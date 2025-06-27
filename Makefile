CC=gcc
CFLAGS=`pkg-config --cflags gtk+-3.0`
LDFLAGS=`pkg-config --libs gtk+-3.0`

all: constructor backend frontend backend_inventory

constructor: constructor.c
	$(CC) constructor.c -o constructor

backend: backend.c
	$(CC) backend.c -o backend

backend_inventory: backend_inventory.c
	$(CC) backend_inventory.c -o backend_inventory

frontend: frontend.c
	$(CC) frontend.c -o frontend $(CFLAGS) $(LDFLAGS)

clean:
	rm -f constructor backend frontend backend_inventory 