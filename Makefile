CC = g++
LD = g++

cylinder: cylinder.o ran2.o sampler.o
	$(LD) -g -o cylinder cylinder.o ran2.o sampler.o

cylinder.o: ./src/cylinder.cpp ./src/constants.h ./src/node.h ./src/Point.h ./src/Filament.h ./src/Axon.h ./src/Grid3D.h
	$(CC) -c -g ./src/cylinder.cpp -I ./NRHeaders

ran2.o: ./src/ran2.cpp
	$(CC)  -c -g ./src/ran2.cpp  -I ./NRHeaders

sampler.o: ./src/sampler.cpp ./src/constants.h
	$(CC)  -c -g ./src/sampler.cpp -I ./NRHeaders

clean:
	rm -f cylinder cylinder.o ran2.o sampler.o
