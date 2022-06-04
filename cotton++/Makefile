
all: nqueens_cotton sor_cotton heat_cotton
cotton: cotton-runtime.cpp
	g++ -shared -fPIC cotton-runtime.cpp -o libcotton.so -lpthread
nqueens_cotton: nqueens_cotton.cpp
	g++ -I./ -L./ nqueens_cotton.cpp -o nqueens_cotton ./libcotton.so -Wl,-rpath,/usr/lib,-rpath,./
sor_cotton: sor_cotton.cpp
	g++ -I./ -L./ sor_cotton.cpp -o sor_cotton ./libcotton.so -Wl,-rpath,/usr/lib,-rpath,./
heat_cotton: heat_cotton.cpp
	g++ -I./ -L./ heat_cotton.cpp -o heat_cotton ./libcotton.so -Wl,-rpath,/usr/lib,-rpath,./
	
clean:  nqueens_cotton sor_cotton heat_cotton
	rm nqueens_cotton sor_cotton heat_cotton
