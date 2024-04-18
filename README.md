Introduction:


This networking project involves creating an online Pokémon card trading application that enables multiple clients to connect to the server simultaneously. The client and server applications will communicate via TCP sockets, with a multithreaded server implemented using Pthread. The project is developed in C++ and utilizes SSH terminals for connecting clients and server programs.




Running Instructions:


1.	Make sure to have these files in your directory before beginning.
                       Sqlite3.c
                       Sqlite3.h
                       Sqlite.lib
                       Sqlite3.dll
                       Server.cpp
                       Client.cpp

      
2.	Once all files are placed correctly begin by compiling in the server terminal(console) the sql files and server file with these commands
•	gcc -c sqlite3.c 
•	g++ -c server.cpp -std=c++11
•	g++ -o server.o sqlite.o -lpthread -ldl
•	./server to begin running server

3.	After starting the server console, we do the same with the client
•	g++ -o client client.cpp -std=c++11
•	./client localhost or 127.0.0.1 can be used for starting the client.


Description of Functionalities: 


Buy: Prompts the client to enter the information required to process a purchase of a card in varying quantities. The server processes the request and returns with an updated balance after the transaction. If the transaction cannot be completed the server 
alerts the client.

Sell: Prompts the client to enter the information required to process a sale of a particular in a specific quantity. The card and quantity are checked against the user's stock before processing. If the transaction can occur the entry for the card is updated and the user’s balance is updated and displayed.

Balance: Displays the balance of the currently logged-in user.


Login: Prompts the user to enter userID and password then checks the stored information in the server to see if they match if yes the server creates a new thread for this client.

List: Lists only the cards of the current logged-in user unless the user is the root it lists all cards in the database.

Logout: Logs out the user and they aren’t allowed to send buy, sell, list, balance, and shutdown commands after.

Who: Lists all active users including userID and IP address

Lookup: Prompts user to enter card name (can either be partial or full name) and will return card info if found.


Deposit: Prompts the user to enter the amount in USD to add to the balance. Then displays the updated balance.


