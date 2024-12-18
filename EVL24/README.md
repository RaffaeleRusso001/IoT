# Evaluation lab - Contiki-NG

## Group number: 25

## Group members

- Marra Biagio 
- Polito Attilio
- Russo Raffaele

## Solution description
- Client: nel client troviamo l'indirizzo ip del root e l'indirizzo ip del genitore e alla fine mandiamo un messaggio al root con all'interno l'indizzo ip del genitore. 

- Server: si occupa di mantenere una tabella dei nodi attivi tramite una structure con gli indirizzi ipv6 del nodo figlio e del suo genitore. Riceve il pacchetto udp e chiama la callback, in cui ottiene l'indirizzo ip del nodo figlio e del genitore, chiama la funzione update_node() che aggiunge o aggiorna il nodo. Ogni 3 minuti chiama la funzione remove_inactive_nodes(), che rimuove i nodi che hanno superato il timeout. Ogni minuto stampa la tabella con all'interno i nodi e i genitori.
