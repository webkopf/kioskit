kioskit: src/Kioskit.c
	gcc -O src/Kioskit.c -o kioskit `pkg-config --cflags gtk+-3.0 --libs webkitgtk-3.0`
	
clean:
	rm kioskit