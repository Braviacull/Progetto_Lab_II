#include "xerrori.h"
#include "libarchivio.h"

void *writer(void *v) {
    Sync* sync = (Sync*) v;

    char* str = get(sync->buffer);
    while (str != NULL) { //fino a che non ricevo un valore di terminazione
        aggiungi(str, sync->hash);
        free(str);
        str = get(sync->buffer);
    }

    pthread_exit(NULL);
}

void *reader(void *v) {
    static pthread_mutex_t log_lock = PTHREAD_MUTEX_INITIALIZER;
    Sync* sync = (Sync*) v;

    char* str = get(sync->buffer);
    while (str != NULL) { //fino a che non ricevo un valore di terminazione
        int occorrenze = conta(str, sync->hash);

        xpthread_mutex_lock(&log_lock,__LINE__,__FILE__);

        fprintf(sync->buffer->log, "%s %d\n", str, occorrenze);

        xpthread_mutex_unlock(&log_lock, __LINE__, __FILE__);

        free(str);

        str = get(sync->buffer);
    }

    pthread_exit(NULL);
}

void* buffer_handler (void* arg) {
    Buffer* buffer = (Buffer*) arg;
    
    if (buffer->fifo < 0)
        xtermina("Errore apertura fifo\n", __LINE__, __FILE__);

    while (1) {
        // leggo la lunghezza della stringa
        int16_t len = 0;
        ssize_t n = readn(buffer->fifo, &len, sizeof(int16_t));

        if (n == -1) 
            xtermina("Errore nella lettura della lunghezza della stringa\n", __LINE__, __FILE__);

        if (n == 0){ // fifo vuota e chiusa
            break;
        }

        assert (n == sizeof(int16_t));
        assert (len > 0);
        assert (len < Max_sequence_length);

        // leggo la stringa
        char line[len+1]; // +1 per il terminatore
        ssize_t byte_letti = readn(buffer->fifo, line, sizeof(char)*len);
        line[len] = '\0'; // terminatore

        if (byte_letti == -1) 
            xtermina("Errore nella lettura della stringa\n", __LINE__, __FILE__);

        assert (byte_letti == sizeof(char)*len);

        char* saveptr = NULL;
        char* s = strtok_r(line, ".,:; \n\r\t", &saveptr);
        while (s != NULL) {
            put(buffer, strdup(s));
            s = strtok_r(NULL, ".,:; \n\r\t", &saveptr);
        }
    }

    if (close(buffer->fifo) < 0)
        xtermina("Errore chiusura fifo\n", __LINE__, __FILE__);
            
    put (buffer, NULL); //valore di terminazione

    pthread_exit(NULL);
}

void *signal_handler(void *arg) {
    Hash* hash = (Hash*) arg;

    sigset_t mask;
    sigfillset (&mask);
    int sig;

    while (1) {
        int e = sigwait(&mask, &sig);

        if (e != 0) 
            xtermina("Errore sigwait", __LINE__, __FILE__);

        if (sig == SIGINT) {

            //Evito che un thread scrittore possa modificare stringhe_distinte mentre voglio leggerla
            //CI PUO ESSERE UN SOLO SCRITTORE ALLA VOLTA
            start_writer(hash);
            int n = hash->stringhe_distinte;
            done_writer(hash);

            fprintf(stderr, "Numero totale di stringhe distinte presenti nella tabella hash: %d\n", n);
        }

        else if (sig == SIGTERM) { //significa che il server ha chiuso la connessione

            xpthread_join(hash->capo_lettore, NULL,__LINE__, __FILE__);
            xpthread_join(hash->capo_scrittore, NULL,__LINE__, __FILE__);

            //Evito che un thread scrittore possa modificare stringhe_distinte mentre voglio leggerla
            //CI PUO ESSERE UN SOLO SCRITTORE ALLA VOLTA
            start_writer(hash);

            int n = hash->stringhe_distinte;
            dealloca_contenuto_tabella_hash(hash);

            done_writer(hash);

            printf("Numero totale di stringhe distinte presenti nella tabella hash: %d\n", n);

            break; // termina il programma 
        }

        else if (sig == SIGUSR1) {

            //ottiene l'accesso in scrittura alla tabella hash
            //deallocazione elementi tabella hash

            start_writer(hash);

            dealloca_contenuto_tabella_hash(hash);
            hdestroy();

            //crea una nuova tabella hash
            if (hcreate(Num_elem) == 0)
                xtermina("Errore creazione tabella hash\n", __LINE__, __FILE__);
            
            // devo necessariamente resettare il numero di stringhe distinte solo dopo aver creato una nuova tabella
            // altrimenti un thread scrittore che sta chiamando la funzione aggiungi e si è bloccato perchè la tabella era piena,
            // potrebbe evitare di chiamare la wait se, dopo un secondo controllo, stringhe_distinte non è più uguale a Num_elem
            // a quel punto, deve esserci una tabella nella quale posso inserire la stringa
            hash->stringhe_distinte = 0;

            done_writer(hash);

            //risveglia i thread in attesa sulla variabile condizionale not_full
            xpthread_mutex_lock(&(hash->full),__LINE__,__FILE__);
            xpthread_cond_broadcast(&(hash->not_full),__LINE__,__FILE__);
            xpthread_mutex_unlock(&(hash->full),__LINE__,__FILE__);
        }

        else { //gestione di default per qualsiasi altro segnale
            signal(sig, SIG_DFL);
        }
    }
    pthread_exit(NULL); 
}

int main(int argc, char *argv[]) {
    sigset_t mask;
    sigfillset(&mask);  // insieme di tutti i segnali
    pthread_sigmask(SIG_BLOCK,&mask,NULL); // blocca tutti i segnali

    ENTRY* testa = crea_entry("Sono il thread che gestisce i segnali", -1);

    Hash* hash = malloc(sizeof(Hash));
    if (hash == NULL)
        xtermina("Errore allocazione hash\n", __LINE__, __FILE__);
    Hash_initializer(hash);
    hash->testa = testa;

    //crea un thread gestore_segnali
    pthread_t gestore_segnali;
    xpthread_create(&gestore_segnali, NULL, &signal_handler, hash,__LINE__, __FILE__);

    if (argc != 5) //controllo argomenti
        xtermina("Uso: ./archivio -w numero_scrittori -r numero_lettori\n", __LINE__, __FILE__);
    
    int w = atoi(argv[2]); //numero scrittori
    int r = atoi(argv[4]); //numero lettori

    if (hcreate(Num_elem) == 0) //creazione tabella hash
        xtermina("Errore creazione tabella hash\n", __LINE__, __FILE__);

    //crea un buffer per capo_scrittore e scrittori
    Buffer* wbuffer = malloc(sizeof(Buffer));
    if (wbuffer == NULL)
        xtermina("Errore allocazione buffer\n", __LINE__, __FILE__);

    buffer_initializer(wbuffer);

    wbuffer->fifo = open("./caposc", O_RDONLY);
    if (wbuffer->fifo < 0)
        xtermina("Errore apertura fifo\n", __LINE__, __FILE__);

    Sync* wsync = malloc(sizeof(Sync));
    if (wsync == NULL)
        xtermina("Errore allocazione wsync\n", __LINE__, __FILE__);

    //sync initializer
    wsync->buffer = wbuffer;
    wsync->hash = hash;

    //crea un buffer per capo_lettore e lettori
    Buffer* rbuffer = malloc(sizeof(Buffer));
    if (rbuffer == NULL)
        xtermina("Errore allocazione buffer\n", __LINE__, __FILE__);

    buffer_initializer(rbuffer);

    rbuffer->fifo = open("./capolet", O_RDONLY);
    if (rbuffer->fifo < 0)
        xtermina("Errore apertura fifo\n", __LINE__, __FILE__);

    rbuffer->log = fopen("lettori.log", "w"); //azzera il file di log e lo apre in scrittura
    if (rbuffer->log == NULL)
        xtermina("Errore apertura lettori.log\n", __LINE__, __FILE__);

    Sync* rsync = malloc(sizeof(Sync));
    if (rsync == NULL)
        xtermina("Errore allocazione rsync\n", __LINE__, __FILE__);

    //sync initializer
    rsync->buffer = rbuffer;
    rsync->hash = hash;

    //crea w thread scrittori che eseguiranno l'operazione aggiungi 
    pthread_t writers[w];
    for (int i = 0; i < w; i++) {
        xpthread_create(&writers[i], NULL, &writer, wsync,__LINE__, __FILE__);
    }

    //crea r thread lettori che eseguiranno l'operazione conta
    pthread_t readers[r];
    for (int i = 0; i < r; i++) {
        xpthread_create(&readers[i], NULL, &reader, rsync,__LINE__, __FILE__);
    }

    //crea un thread capo_scrittore che legge dalla fifo caposc e scrive sul buffer
    xpthread_create(&(hash->capo_scrittore), NULL, &buffer_handler, wbuffer,__LINE__, __FILE__);

    //crea un thread capo_lettore che legge dalla fifo capolet e scrive sul buffer
    xpthread_create(&(hash->capo_lettore), NULL, &buffer_handler, rbuffer,__LINE__, __FILE__);

    //gestore_segnali effettua la join di capo_scrittore e capo_lettore
    xpthread_join (gestore_segnali, NULL,__LINE__, __FILE__);

    //aspetto che gli scrittori ricevano il valore NULL sul buffer, inviato dal capo_scrittore, e terminino la loro esecuzione
    for (int i = 0; i < w; i++) {
        xpthread_join (writers[i], NULL,__LINE__, __FILE__);
    }

    //aspetto che i lettori ricevano il valore NULL sul buffer, inviato dal capo_lettore, e terminino la loro esecuzione
    for (int i = 0; i < r; i++) {
        xpthread_join (readers[i], NULL,__LINE__, __FILE__);
    }

    assert (testa == hash->testa);
 
    distruggi_entry(testa);

    free(wsync);
    free(rsync);

    Hash_destroyer(hash);

    close(wbuffer->fifo);
    close(rbuffer->fifo);
    if (fclose(rbuffer->log) < 0)
        xtermina("Errore chiusura file\n", __LINE__, __FILE__);
    buffer_destroyer(wbuffer);
    buffer_destroyer(rbuffer);

    return 0;
}