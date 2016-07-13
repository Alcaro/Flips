cd lib
gcc -O3 -gdwarf-2 -DHAVE_CONFIG_H -I../include -c divsufsort.c -o divsufsort.o
gcc -O3 -gdwarf-2 -DHAVE_CONFIG_H -I../include -c sssort.c -o sssort.o
gcc -O3 -gdwarf-2 -DHAVE_CONFIG_H -I../include -c trsort.c -o trsort.o
gcc -O3 -gdwarf-2 -DHAVE_CONFIG_H -I../include -c utils.c -o utils.o
cd ..
del libdivsufsort-2.0.1.a
ar rcs libdivsufsort-2.0.1.lib lib/*.o
