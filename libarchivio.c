#include "libarchivio.h"

// crea un oggetto di tipo entry
// con chiave s e valore n e next==NULL
ENTRY *crea_entry(char *s, int n) {
  ENTRY *e = malloc(sizeof(ENTRY));
  if(e==NULL) xtermina("errore malloc entry 1", __LINE__, __FILE__);
  e->key = strdup(s); // salva copia di s
  e->data = malloc(sizeof(coppia));
  if(e->key==NULL || e->data==NULL)
    xtermina("errore malloc entry 2", __LINE__, __FILE__);
  // inizializzo coppia
  coppia *data = (coppia *) e->data; // cast obbligatorio
  data->valore  = n;
  data->next = NULL;
  return e;
}

void distruggi_entry(ENTRY *e) {
  free(e->key); free(e->data); free(e);
}


void Hash_initializer (Hash* hash) {
    xpthread_mutex_init(&hash->access, NULL,__LINE__, __FILE__);
    xpthread_mutex_init(&hash->ordering, NULL,__LINE__, __FILE__);
    xpthread_cond_init(&hash->go, NULL,__LINE__, __FILE__);
    hash->active_readers = 0;
    hash->active_writers = 0;

    xpthread_mutex_init(&hash->full, NULL,__LINE__, __FILE__);
    xpthread_cond_init(&hash->not_full, NULL,__LINE__, __FILE__);

    xpthread_mutex_init(&hash->mutex_stringhe_distinte, NULL,__LINE__, __FILE__);
    hash->stringhe_distinte = 0;
}

void Hash_destroyer (Hash* hash) {
    xpthread_mutex_destroy(&hash->access,__LINE__, __FILE__);
    xpthread_mutex_destroy(&hash->ordering,__LINE__, __FILE__);
    xpthread_cond_destroy(&hash->go,__LINE__, __FILE__);

    xpthread_mutex_destroy(&hash->full,__LINE__, __FILE__);
    xpthread_cond_destroy(&hash->not_full,__LINE__, __FILE__);

    xpthread_mutex_destroy(&hash->mutex_stringhe_distinte,__LINE__, __FILE__);

    free(hash);
}

void dealloca_contenuto_tabella_hash (Hash* hash) {
    coppia* data = (coppia*) (hash->testa)->data;

    while (data->next != NULL) {
        ENTRY* tmp = data->next;
        distruggi_entry (hash->testa);
        (hash->testa) = tmp;
        data = (coppia*) tmp->data;
    }

}

void buffer_initializer (Buffer* buffer) {
    xpthread_mutex_init(&buffer->mutex, NULL,__LINE__, __FILE__);
    xpthread_cond_init(&buffer->not_empty, NULL,__LINE__, __FILE__);
    xpthread_cond_init(&buffer->not_full, NULL,__LINE__, __FILE__);
    buffer->head = 0;
    buffer->tail = 0;
    buffer->nelem = 0;
}

void buffer_destroyer (Buffer* buffer) {
    xpthread_mutex_destroy(&buffer->mutex,__LINE__, __FILE__);
    xpthread_cond_destroy(&buffer->not_empty,__LINE__, __FILE__);
    xpthread_cond_destroy(&buffer->not_full,__LINE__, __FILE__);

    free(buffer);
}


// permettono l'accesso alla tabella hash a più thread lettori o un singolo thread scrittore 
void start_reader(Hash* hash) {
    xpthread_mutex_lock(&hash->ordering,__LINE__,__FILE__);//uno alla volta
    xpthread_mutex_lock(&hash->access,__LINE__,__FILE__);

    // il lettore che si sospende non rilascia ordering ma solo mutex
    while(hash->active_writers>0) {
        xpthread_cond_wait(&hash->go,&hash->access,__LINE__,__FILE__);
    }
    hash->active_readers++;

    xpthread_mutex_unlock(&hash->ordering,__LINE__,__FILE__);//avanti il prossimo
    xpthread_mutex_unlock(&hash->access,__LINE__,__FILE__);
}

void done_reader(Hash* hash) {
    xpthread_mutex_lock(&hash->access,__LINE__,__FILE__);
    hash->active_readers--;
    if(hash->active_readers==0) {
        xpthread_cond_broadcast(&hash->go,__LINE__,__FILE__);
    }
    xpthread_mutex_unlock(&hash->access,__LINE__,__FILE__);
}

void start_writer(Hash* hash) {
    xpthread_mutex_lock(&hash->ordering,__LINE__,__FILE__);
    xpthread_mutex_lock(&hash->access,__LINE__,__FILE__);

    // lo scrittore che si sospende non rilascia orddering ma solo mutex
    while(hash->active_readers>0 || hash->active_writers>0) {
        xpthread_cond_wait(&hash->go,&hash->access,__LINE__,__FILE__);
    }
    hash->active_writers++;

    xpthread_mutex_unlock(&hash->ordering,__LINE__,__FILE__);//avanti il prossimo
    xpthread_mutex_unlock(&hash->access,__LINE__,__FILE__);
}

void done_writer(Hash* hash) {
    xpthread_mutex_lock(&hash->access,__LINE__,__FILE__);
    hash->active_writers--;
    assert (hash->active_writers==0);
    xpthread_cond_broadcast(&hash->go,__LINE__,__FILE__);
    xpthread_mutex_unlock(&hash->access,__LINE__,__FILE__);
}

void aggiungi (char* s, Hash* hash) {
    //cerca s nella tabella hash
    ENTRY* new = crea_entry(s, 1);

    start_reader(hash);

    ENTRY* e = hsearch (*new, FIND);

    done_reader(hash);

    if (e == NULL) { // s non è presente nella tabella hash
        // Se la tabella hash è piena
        while (hash->stringhe_distinte == Num_elem) {
            printf("Tabella hash piena, aspetto SIGURS1...\n");

            xpthread_mutex_lock(&(hash->full),__LINE__,__FILE__);
            if (hash->stringhe_distinte == Num_elem) //controllo che la tabella hash sia ancora piena
                xpthread_cond_wait(&(hash->not_full), &(hash->full),__LINE__,__FILE__); //aspetto che la tabella hash non sia piena
            xpthread_mutex_unlock(&(hash->full),__LINE__,__FILE__);

        }
        
        coppia* new_data = (coppia*) new->data;
        
        start_writer(hash);

        //aggiorno la testa della lista per la deallocazione della tabella hash
        new_data->next = (hash->testa);
        (hash->testa) = new;

        e = hsearch (*new, ENTER);

        (hash->stringhe_distinte)++;

        done_writer(hash);

        //printf("NUOVA TESTA %s\n", (*testa)->key);
        //printf("CHE PUNTA A %s\n", ((coppia*)(*testa)->data)->next->key);
        
        if (e == NULL)
            xtermina("Errore inserimento stringa nella tabella hash\n", __LINE__, __FILE__);

    }
    else { // s è presente nella tabella hash
        assert(strcmp(e->key, new->key) == 0);
        assert(strcmp(e->key, s) == 0);

        distruggi_entry(new);
        coppia* data = (coppia*) e->data;
        data->valore++;
    }
}

int conta (char* s, Hash* hash) {
    //cerca s nella tabella hash
    ENTRY* tmp = crea_entry(s, -1); // usata solo per la ricerca
    start_reader(hash);
    ENTRY* e = hsearch(*tmp, FIND);
    done_reader(hash);
    distruggi_entry(tmp);
    if (e == NULL) { // s non è presente nella tabella hash
        return 0;
    }
    else { // s è presente nella tabella hash
        assert(strcmp(e->key, s) == 0);
        coppia* data = (coppia*) e->data;
        //printf("CONTA: Trovata stringa %s con %d occorrenze\n", s, c->valore);
        return data->valore;
    }
}

char* get (Buffer* buffer) {
    //preleva una stringa dal buffer
    char* s = NULL;
    
    xpthread_mutex_lock(&buffer->mutex,__LINE__,__FILE__);

    while (buffer->nelem == 0) {
        //aspetto che il buffer non sia vuoto
        xpthread_cond_wait(&buffer->not_empty,&buffer->mutex,__LINE__,__FILE__);
    }
    //prelevo una stringa dal buffer
    s = buffer->buf[buffer->head];
    //printf("GET : Ho prelevato la stringa %s dal buffer\n", s);
    if (s == NULL) {
        //se è NULL, è un valore di terminazione
        xpthread_mutex_unlock(&buffer->mutex,__LINE__,__FILE__);
        return s;
    }
    buffer->head = (buffer->head+1)%PC_buffer_len;
    buffer->nelem--;

    xpthread_cond_signal(&buffer->not_full,__LINE__,__FILE__);

    xpthread_mutex_unlock(&buffer->mutex,__LINE__,__FILE__);

    return s;
}

void put(Buffer* buffer, char*s) {
    //inserisci una stringa nel buffer
    xpthread_mutex_lock(&buffer->mutex,__LINE__,__FILE__);

    while (buffer->nelem == PC_buffer_len) {
        //aspetto che il buffer non sia pieno
        xpthread_cond_wait(&buffer->not_full,&buffer->mutex,__LINE__,__FILE__);
    }
    //inserisco una stringa nel buffer
    buffer->buf[buffer->tail] = s;
    buffer->tail = (buffer->tail+1)%PC_buffer_len;
    buffer->nelem++;

    //una signal non basta, devo fare una broadcast così da svegliare tutti i thread in attesa quando dovranno leggere il valore di terminazione
    xpthread_cond_broadcast(&buffer->not_empty,__LINE__,__FILE__);

    xpthread_mutex_unlock(&buffer->mutex,__LINE__,__FILE__);
}
