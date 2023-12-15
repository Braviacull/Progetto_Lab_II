#include "xerrori.h"

// host e port a cui connettersi
#define HOST "127.0.0.1"
#define PORT 58363

#define CONN_TYPE 2

void *send_file(void *v) {
    char* nomefile = (char*) v;

    printf("File da inviare: %s\n", nomefile);

    FILE *fp = fopen(nomefile, "r");
    if (fp == NULL) xtermina("Errore nell'apertura del file\n",__LINE__, __FILE__);

    //UNA CONNESSIONE PER FILE
    int fd_skt = xcrea_socket(HOST, PORT);

    //COMUNICO IL TIPO DI CONNESSIONE UNA VOLTA SOLA
    xinvio_intero (CONN_TYPE, fd_skt);

    char *line = NULL;
    size_t len = 0; // dimensione del buffer per allocare la line
    int linee = 0; // contatore delle linee lette e da inviare

    //leggi il file riga per riga
    //e inviale al server con una sola connessione di Tipo B
    while (1) {
        size_t bytes_read = 0;
        errno = 0;
        bytes_read = getline(&line, &len, fp);
        
        if (bytes_read != -1) {

            assert (bytes_read == strlen(line));
            assert (strlen(line) <= len);

            //invio la lunghezza della linea
            xinvio_intero(strlen(line), fd_skt);
            //invio la linea
            xinvio_stringa(line, fd_skt);

            linee++;
        }
        else {
            //controlla che non ci siano errori
            if (errno != 0) {
                free(line);
                xtermina("Errore nella lettura del file\n", __LINE__, __FILE__);
            }
            else { //ho finito di leggere il file
                free(line);
                fclose(fp);
                break;
            }
        }
    }

    xinvio_intero(0, fd_skt); // comunico al server che ho finito di inviare le linee
    

    //ricevo il numero di sequenze/linee ricevute dal server
    uint32_t num_seq_network;
    readn(fd_skt, &num_seq_network, sizeof(uint32_t));
    int num_seq = ntohl(num_seq_network);

    assert(num_seq == linee);
    
    if(close(fd_skt)<0)
        xtermina("Errore chiusura socket", __LINE__, __FILE__);

    puts("Connessione di Tipo B chiusa con successo");

    pthread_exit(NULL);
}

int main(int argc, char const* argv[])
{
    //puoi passare un numero arbitrario di file
    if (argc < 2) {
        fprintf(stderr, "Uso: %s <nomefile1> <nomefile2> ... <nomefileN>\n", argv[0]);
        xtermina("Errore nei parametri\n", __LINE__, __FILE__);
    }

    pthread_t t[argc-1];
    for (int i = 0; i < argc-1; i++) {
        //crea un thread per ogni file passato su linea di comando
        xpthread_create(&t[i], NULL, &send_file, (void*) argv[i+1],__LINE__, __FILE__);
    }

    for (int i = 0; i < argc-1; i++) {
        //attendo che i thread abbiano finito
        xpthread_join(t[i], NULL,__LINE__, __FILE__);
    }

    return 0;
}