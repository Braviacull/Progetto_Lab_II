#! /usr/bin/env python3
import time, os, stat, struct, socket, argparse, subprocess, signal, concurrent.futures, logging, threading
# valori di default per host e port
HOST = "127.0.0.1"  # interfaccia su cui mettersi in ascolto
PORT = 58363  # Port to listen on (non-privileged ports are > 1023)
MAX_SEQUENCE_LENGTH = 2048

connessioni = 0

log_lock = threading.Lock()
capolet_lock = threading.Lock()
caposc_lock = threading.Lock()

# Crea un logger di nome 'my_logger'
logger = logging.getLogger('my_logger')

# Setta un livello di log, questo può essere DEBUG, INFO, WARNING, ERROR, CRITICAL
logger.setLevel(logging.INFO)

# crea un file handler
handler = logging.FileHandler('server.log')

# Crea un formatter
formatter = logging.Formatter('%(asctime)s - %(name)s - %(levelname)s - %(message)s')

# Add the formatter to the handler
handler.setFormatter(formatter)

# Add the handler to the logger
logger.addHandler(handler)

# Now you can log messages with various severity levels

def main(host=HOST,port=PORT):
    
    connessioni = 0

    crea_fifo ("./caposc")
    crea_fifo ("./capolet")


    # Accetto argomenti da linea di comando
    parser = argparse.ArgumentParser()
    parser.add_argument('m', type=int, help='Il massimo numero di thread')
    parser.add_argument('-w', type=int, default=3, help='Numero writers')
    parser.add_argument('-r', type=int, default=3, help='Numero readers')
    parser.add_argument('-v', action='store_true', help='Lancia archivio mediante valgrind')

    args = parser.parse_args()
    m = args.m
    w = args.w
    r = args.r
    v = args.v

    if m is None: # -m è obbligatorio
        raise ValueError('Il massimo numero di thread (-m) deve essere specificato')

    # Lancio `archivio` 
    if v:
        p = subprocess.Popen(["valgrind","--leak-check=full", 
                       "--show-leak-kinds=all",
                       "--log-file=valgrind-%p.log", 
                       "./archivio", "-w", str(w), "-r", str(r)])
    else:
        p = subprocess.Popen(["./archivio", "-w", str(w), "-r", str(r)]) 
   

    with concurrent.futures.ThreadPoolExecutor(max_workers=m) as executor:
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as server:
            try:
                # Permette di riutilizzare la porta se il server viene chiuso
                server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)      
                server.bind((host, port))
                server.listen()
                # Apro la FIFO caposc in modalità scrittura binaria
                caposc = open("./caposc", "wb")
                capolet = open("./capolet", "wb")
                while True:
                    sock, c_addr = server.accept()

                    # Assegno un thread dedicato al client
                    executor.submit(gestisci_connessione, sock, c_addr, caposc, capolet)
                    connessioni += 1
            except KeyboardInterrupt:
                # shutdown del server (la close viene fatta dalla with)  
                server.shutdown(socket.SHUT_RDWR)

                # rimozione delle FIFO
                os.unlink("./caposc")
                os.unlink("./capolet")

                # mando un segnale SIGTERM a archivio
                os.kill(p.pid, signal.SIGTERM)

def gestisci_connessione(sock,addr,caposc,capolet):
    byte_scritti = 0
    with sock:

        client_type = ricevi_intero(sock)

        if client_type == 1:
            client_type = 'Tipo A'
        elif client_type == 2:
            client_type = 'Tipo B'
        else: 
            raise ValueError(f'Unknown client type: {client_type}')


        if client_type == 'Tipo A':

            #ricevo la dimensione della linea che il client sta per inviare
            dim_line = ricevi_intero(sock)

            # ricevo la linea
            line = ricevi_stringa(sock, dim_line)
            assert len(line)==dim_line

            # NECESSARIO ARCHIVIO CHE LEGGA DA FIFO
            with capolet_lock:
                # invia la dimensione della linea ad archivio
                capolet.write(struct.pack('h', dim_line))
                # invia la linea ad archivio
                capolet.write(line.encode())
                capolet.flush()

            with log_lock: # +2 byte per la dimensione della linea
                logger.info(f"Tipo connessione: {client_type} - {dim_line + 2} byte scritti in capolet")


        elif client_type == 'Tipo B':
            sequenze_lette = 0
            byte_scritti = 0
            while (1):
                #ricevo la dimensione della linea che il client sta per inviare
                
                dim_line = ricevi_intero(sock)
                    
                assert dim_line <= 2048

                if dim_line == 0: # Se la dimensione è 0, il client ha finito di inviare le linee
                    with log_lock:
                        logger.info(f"Tipo connessione: {client_type} - {byte_scritti} byte scritti in caposc")
                    
                    # manda il numero di sequenze/linee lette al client
                    invia_intero(sock, sequenze_lette)

                    break 

                # ricevo la linea
                line = ricevi_stringa(sock, dim_line)
                    
                sequenze_lette += 1

                assert len(line)==dim_line

                # NECESSARIO ARCHIVIO CHE LEGGA DA FIFO
                # Non posso separare l'invio della dimensione della linea e della linea stessa
                # perché altrimenti potrebbe succedere che più thread inviino la dimensione della linea
                # prima che il thread che ha inviato la dimensione della linea precedente abbia inviato la linea
                with caposc_lock:
                    # invia la dimensione della linea ad archivio
                    caposc.write(struct.pack('h', dim_line))
                    # invia la linea ad archivio
                    caposc.write(line.encode())
                    caposc.flush()

                byte_scritti += dim_line + 2 # 2 byte per la dimensione della linea
        else:
            raise ValueError(f'Unknown client type: {client_type}')

# Se non presente nella directory corrente, crea fifo
def crea_fifo (fifo):
    if (not os.path.exists(fifo)):
        os.mkfifo(fifo)
    elif (not is_fifo(fifo)):
        os.unlink(fifo)
        os.mkfifo(fifo)

# Riceve esattamente n byte dal socket conn e li restituisce
# il tipo restituto è "bytes": una sequenza immutabile di valori 0-255
# Questa funzione è analoga alla readn che abbiamo visto nel C
def recv_all(conn,n):
  chunks = b''
  bytes_recd = 0
  while bytes_recd < n:
    chunk = conn.recv(min(n - bytes_recd, 1024))
    if len(chunk) == 0:
      raise RuntimeError("socket connection broken")
    chunks += chunk
    bytes_recd = bytes_recd + len(chunk)
  return chunks

def is_fifo(file_path):
    return stat.S_ISFIFO(os.stat(file_path).st_mode)

def ricevi_intero(conn): #riceve 2 byte e li converte in un intero
    data = recv_all(conn, 2) #riceve 2 byte
    return struct.unpack('!h', data)[0] # Converto i 2 byte in un intero

def invia_intero(conn, intero): #invia 4 byte che rappresentano l'intero
    data = struct.pack('!i', intero) #Converto l'intero in 4 byte
    conn.sendall(data)

def ricevi_stringa(conn, dim): #riceve dim byte e li converte in una stringa
    data = recv_all(conn, dim)
    return data.decode('utf-8') # Converto i byte in una stringa

main (HOST, PORT)