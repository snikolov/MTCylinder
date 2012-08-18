#! /bin/sh
#PBS -j oe
#PBS -m e
#PBS -M snikolov@lusi.uni-sb.de
#PBS -r n

# By default, the working directory is set to the
# home directory of the user. Usually it is more
# convenient to change to the directory from which
# the script was submitted. The magical incantation:
cd $PBS_O_WORKDIR

CC=icpc
CFLAGS="-O3 -xP"

# compile the program...
$(CC) -c -g src/sampler.cpp -I ./NRHeaders
$(CC) -c -g src/ran2.cpp -I ./NRHeaders
$(CC) -c -g src/cylinder.cpp -I ./NRHeaders
$(CC) -g -o cylinder cylinder.o ran2.o sampler.o

# ...and run it.
./cylinder
