# definizione del compilatore e dei flag di compilazione
# che vengono usate dalle regole implicite
CC=gcc
CFLAGS=-std=c11 -Wall -g
LDLIBS=-lm -lpthread -lrt

# su https://www.gnu.org/software/make/manual/make.html#Implicit-Rules
# sono elencate le regole implicite e le variabili 
# usate dalle regole implicite 

# Variabili automatiche: https://www.gnu.org/software/make/manual/make.html#Automatic-Variables
# nei comandi associati ad ogni regola:
#  $@ viene sostituito con il nome del target
#  $< viene sostituito con il primo prerequisito
#  $^ viene sostituito con tutti i prerequisiti

# elenco degli eseguibili da creare
EXECS=archivio client1 client2

# primo target: gli eseguibili sono precondizioni
# quindi verranno tutti creati
all: $(EXECS) permissions

client1: client1.o xerrori.o
	$(CC) client1.o xerrori.o -o client1 $(LDLIBS)

client2: client2.o xerrori.o
	$(CC) client2.o xerrori.o -o client2 $(LDLIBS)

archivio: archivio.o xerrori.o libarchivio.o
	$(CC) archivio.o xerrori.o libarchivio.o -o archivio $(LDLIBS)

#.o
archivio.o: archivio.c libarchivio.h xerrori.h
	$(CC) $(CFLAGS) -c archivio.c -o archivio.o $(LDLIBS)

libarchivio.o: libarchivio.c libarchivio.h xerrori.h
	$(CC) $(CFLAGS) -c libarchivio.c -o libarchivio.o $(LDLIBS)

xerrori.o: xerrori.c xerrori.h
	$(CC) $(CFLAGS) -c xerrori.c -o xerrori.o $(LDLIBS)

permissions:
	chmod +x server.py

 

# esempio di target che non corrisponde a una compilazione
# ma esegue la cancellazione dei file oggetto, degli eseguibili e dei file di log
clean: 
	rm *.o $(EXECS) *.log
		
zip:
	zip funziona.zip *.c *.h makefile

	
	
	
	

