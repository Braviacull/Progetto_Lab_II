#include "xerrori.h"

#define Num_elem 1000000
#define PC_buffer_len 10
#define Max_sequence_length 2048


typedef struct {
    //per start_reader, done_reader, start_writer, done_writer
    pthread_mutex_t access, ordering;
    pthread_cond_t go;
    int active_readers;
    int active_writers;

    //puntatore al primo elemento della tabella hash che punta al secondo elemento e così via
    ENTRY* testa;

    //per gestire il caso in cui la tabella hash sia piena quando un thread scrittore vuole aggiungere una stringa
    pthread_mutex_t full; // uso se la tabella hash è piena
    pthread_cond_t not_full; // aspetto che la tabella hash non sia piena

    //per tenere conto del numero di stringhe distinte presenti nella tabella hash
    int stringhe_distinte;
    pthread_mutex_t mutex_stringhe_distinte;

    // per effettuare la join in gestore_segnali
    pthread_t capo_scrittore, capo_lettore;
} Hash;

typedef struct {
    // per sincronizzare l'accesso al buffer tramite get e put
    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
    //buffer circolare
    char* buf[PC_buffer_len];
    int head;
    int tail;
    int nelem;

    //fifo su dalla quale i thread capo_lettore e capo_scrittore leggono le sequenze inviate dal server
    int fifo; //caposc o capolet

    //per scrivere su file i risultati delle chiamate a conta;
    FILE* log;
} Buffer;

typedef struct {
    Buffer* buffer;
    Hash* hash;
} Sync;

typedef struct {
  int valore;    // numero di occorrenze della stringa 
  ENTRY *next;  
} coppia;


ENTRY *crea_entry(char *s, int n);
void distruggi_entry(ENTRY *e);

void Hash_initializer (Hash* hash);

void Hash_destroyer (Hash* hash);

void dealloca_contenuto_tabella_hash (Hash* hash);

void buffer_initializer (Buffer* buffer);

void buffer_destroyer (Buffer* buffer);


// permettono l'accesso alla tabella hash a più thread lettori o un singolo thread scrittore 
void start_reader(Hash* hash);

void done_reader(Hash* hash);

void start_writer(Hash* hash);

void done_writer(Hash* hash);


void aggiungi (char* s, Hash* hash);

int conta (char* s, Hash* hash);

char* get (Buffer* buffer);

void put(Buffer* buffer, char*s);