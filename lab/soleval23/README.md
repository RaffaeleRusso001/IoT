# Evaluation lab - Contiki-NG

## Group number: 14

## Group members

- Andrea Caravano 
- Biagio Cancelliere
- Marco Pozzi

## Solution description
Client:

Firstly, the client starts the udp_server_process PROTOTHREAD. Here it initialize a connection with the server 
by simple_udp_register.
After, for every iteration, we set an etimer, so the temperatures are sent every minute more or less. 
At each iteration, it checks if the server is reachable.
If it is, the client also checks if the average of the batched temperatures is needed to be sent 
(case client was disconnected, and it has just reconnected) and then sends a new temperature (in a different message).
In case the client can't reach the server, every minute (more or less, as before) it calculates a new temperature, 
and it is saved in a circular array which represents the batched temperatures.
After sending the average of the batched temperatures, an etimer is set so multiple calls of simple_udp_sendto in sequence are unlikely
to succeed.

Server:

After the process has begun we can specify the temperature buffer and DAG root initialization and then define the UDP connection.
The received callback is defined in the following way:
1) The server receive the temperature reading from a client;
2) Then, it indicates receivers that are knew and stored. When new receivers show up, if the number of known receivers 
is not greater than "MAX_RECEIVERS" (so total count of receivers is still valid), server stores them.
3) After, it stores the new temperature reading in a dedicated array;
4) In the end it computes the average.