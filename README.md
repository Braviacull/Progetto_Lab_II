# Progetto Laboratorio2

Il progetto consiste in una comunicazione client server. 
I client inviano righe di testo che vengono ricevute dal server su un socket dedicato ad ogni connessione.
Ci sono 2 tipi di client e quindi 2 tipi di connessioni differenti.
Il primo tipo di connessione (connessione di tipo A) è la connessione usata dal client di tipo 1 (client1), invia una singola riga al server e chiude la connessione. 
client1 prende in input un file di testo contenente n>0 righe e avvia n connessioni di tipo A, quindi una connessione per riga.
Il secondo tipo di connessione (connessione di tipo B) è la connessione usata dal client di tipo 2 (client2), invia tutte le righe di un file di testo al server e chiude la connessione.
client2 riceve in input f>0 file di testo e avvia f connessioni di tipo B.

Il server, in base al tipo di connessione gestita, scrive in una fifo tutte le righe di testo ricevute dal client.
La fifo è caposc per connessioni di tipo B e capolet per connessioni di tipo A.
Successivamente, il programma archivio dedica un thread per fifo, quindi 2 thread, chiamati capo_scrittore e capo_lettore, che leggono rispettivamente dalla fifo caposc e capolet.
Per ogni fifo è dedicato un buffer di dimensione fissata: un buffer condiviso tra capo_scrittore e w>0 thread writer e un buffer condiviso tra capo_lettore e r>0 thread reader.
Per ogni riga di testo letta dalla fifo, vengono creati t tokens e inseriti nel buffer corrispondente.
I thread writer possono aggiungere o modificare entry all'interno della tabella hash mentre i thread lettori possono cercare entry all'interno della tabella.
Nello specifico, i thread writer, estratto un token dal buffer condiviso col capo_scrittore, controllano se esiste una entry corrispondente al token e, se presente, incrementano il valore corrispondente di uno, altrimenti aggiungono un entry, associata al token, con valore intero corrispondente uguale ad 1 mentre i thread reader, estratto un token dal buffer dedicato, eseguono l'operazione conta sul token, ovvero, controllano se esiste una entry associata a quel token e, se presente, ne leggono il valore intero corrispondente, altrimenti leggono il valore intero 0.

server.py, se inesistente, crea un file server.log. in questo file si tiene nota di ogni connessione gestita salvandone il tipo e la quantità di byte ricevuti.

archivio, se inesistente, crea un file lettori.log. I thread reader registrano,  all'interno di lettori.log, il token analizzato e il risultato della chiamata alla funzione conta sul token.

Se il server.py viene eseguito con opzione -v il programma archivio viene eseguito tramite il programma valgrind e viene generato un file di log contenente l'output di valgrind.

La gestione dei segnali è effettuata da un thread dedicato gestore_segnali. 

IMPORTANTE: Per la gestione dei segnali, dato che viene usata la funzione sigwait(), vengono utilizzate funzioni non async-signal-safe. Questo non crea problemi perché sigwait() sospende l'esecuzione del thread fino a quando uno dei segnali specificati diventa pendente. Non causa l'interruzione di altre funzioni, quindi non si verificano le condizioni di gara che rendono pericoloso l'uso di funzioni non signal-safe nei gestori di segnali.

I programmi terminano quando il gestore_segnali riceve il segnale SIGINT.

## Scelte implementative

(1) RICEZIONE/INVIO DI RIGHE DI TESTO
Ogni riga di testo, inviata o ricevuta, è preceduta da un intero di 2 byte contenente la lunghezza in byte della riga di testo. SI ASSUME CHE LA LUNGHEZZA IN BYTE DELLE RIGHE NON SIA SUPERIORE A 2048.

(2) ACCESSO CONCORRENTE ALLA TABELLA HASH
L'accesso concorrente di thread writer e reader è gestito dalle funzioni start_reader(), start_writer(), done_reader(), done_writer().
Un thread reader, prima di accedere alla tabella hash in lettura, chiama la funzione start_reader() e, una volta effettuata l'operazione di lettura, chiama la funzione done_reader(). L'utilizzo di start_writer() e done_writer per i thread writer è analogo.

Con questo approccio, i casi possibili sono 2: ci sono uno o più thread in lettura allo stesso tempo oppure un singolo thread in scrittura. Inoltre, i thread vengono serviti con il loro ordine di arrivo grazie alla variabile di lock ordering.

(3) TABELLA HASH PIENA
Se, nel momento in cui un thread writer cerca di inserire una nuova entry all'interno della tabella hash, questa dovesse risultare piena, il thread in questione si mette in attesa aspettando che venga inviato il segnale SIGUSR1 che, in pratica, svuota la tabella hash, dopodiché l'esecuzione riprende.

(4) DEALLOCAZIONE CONTENUTO TABELLA HASH
La funzione hdestroy() si limita a deallocare la tabella hash. 
Allo scopo di deallocare il contenuto della tabella hash, ogni entry contiene un puntatore alla entry successiva (in ordine di inserimento) creando una sorta di linked list. La testa di questa lista, quando la tabella hash è vuota, contiene una entry che punta a NULL, mentre, quando viene aggiunta una nuova entry, la testa viene aggiornata con quest'ultima, tramite l'utilizzo di una variabile temporanea, e punterà alla vecchia testa.

(5) PERMESSI DI ESECUZIONE server.py
I permessi di esecuzione per server.py sono concessi dopo l'esecuzione dell'istruzione make. Questa istruzione, oltre a creare gli eseguibili necessari con dipendenze annesse, esegue il comando "chmod +x server.py".

(6) DISTINZIONE TIPO DI CONNESSIONE
La prima sequenza di byte inviata da un client è un intero.
Il server riceve questo intero, se è 1, capisce che si tratta di un client di tipo 1 e quindi di una connessione di tipo A, se è 2, capisce che si tratta di una connessione di tipo 2 e quindi di una connessione di tipo B, altrimenti genera un errore.

(7) TERMINAZIONE capo_scrittore e capo_lettore
Quando server.py riceve il segnale SIGINT, genera l'eccezione KeyboardInterrupt, e, tra le varie operazioni, chiude le fifo caposc e capolet.
A questo punto, capo_scrittore e capo_lettore leggeranno il valore 0 dalla fifo (si veda il manuale della funzione read) e termineranno.


### Esempio di utilizzo

$ make
$ ./server.py 5 -r 2 -w 4 -v &  # parte il server con 5 thread che 
                                # a sua volta fa partire archivio
$ ./client2 file1 file2         # scrive dati su archivio
$ ./client1 file3               # interroga archivio
$ pkill -INT -f server.py       # invia SIGINT a server.py
                                # che a sua volta termina archivio
