#include "xerrori.h"

// host e port a cui connettersi
#define HOST "127.0.0.1"
#define PORT 58363

#define CONN_TYPE 1

int main(int argc, char const* argv[])
{
    //puoi passare solo il nome del file
    if (argc != 2) {
        fprintf(stderr, "Uso: %s <nomefile>\n", argv[0]);
        xtermina("Errore nei parametri\n", __LINE__, __FILE__);
    }

    FILE *fp = fopen(argv[1], "r");
    if (fp == NULL) xtermina("Errore nell'apertura del file\n", __LINE__, __FILE__);

    char *line = NULL;
    size_t len = 0;

    //leggi il file riga per riga
    //e inviale al server con connessioni di Tipo A separate
    int connessioni = 0;
    while(1) {
        size_t bytes_read = 0;
        errno = 0;
        bytes_read = getline(&line, &len, fp);

        if (bytes_read != -1) {
            assert (bytes_read == strlen(line));
            assert (strlen(line) <= len);
            
            connessioni++;

            //UNA CONNESSIONE PER RIGA
            int fd_skt = xcrea_socket(HOST, PORT);

            //COMUNICO IL TIPO DI CONNESSIONE PER OGNI RIGA/CONNESSIONE
            xinvio_intero (CONN_TYPE, fd_skt);
            
            //ho letto una riga
            //invio la lunghezza della linea
            xinvio_intero (strlen(line), fd_skt);
            // invio la linea
            xinvio_stringa (line, fd_skt);
            
            if(close(fd_skt)<0)
                xtermina("Errore chiusura socket", __LINE__, __FILE__);

        }
        else {
            // controlla che non ci siano errori
            if (errno != 0) {
                free(line);
                xtermina("Errore nella lettura del file\n", __LINE__, __FILE__);
            }
            else { // ho finito di leggere il file
                free(line);
                fclose(fp);
                break;
            }
        }
    }
    
    return 0;
}