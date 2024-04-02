#include <stdio.h>
#include <cstdio>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string>
#include <stdlib.h>
#include <unistd.h>
#include <cerrno>
#include <strings.h>
#include <iostream>
#include <cstring>
#include "sqlite3.h"
#include <sstream>
#include <iomanip>
#include <pthread.h>

// constant values
#define SERVER_PORT 7472
#define MAX_PENDING 5
#define MAX_LINE 256

struct threadArgs
{
    int socket;
    int threadId;
    sqlite3 *database;
};

struct userInfo
{
    bool loggedIn = false;
    bool rootUser = false;
    std::string userName;
    int userId;
};

using namespace std;
// function prototypes
static int callback(void *, int, char **, char **);
bool onlyNum(const string);
void *controlLoop(void *);
userInfo login(userInfo *, string, sqlite3 *);
string adduser(sqlite3 *, string);
string search(sqlite3 *, string, userInfo *);
string buyCard(sqlite3 *, string, userInfo *);
string balance(sqlite3 *, string, userInfo *);
string sell(sqlite3 *, string, userInfo *);
string deposit(sqlite3 *, string, userInfo *);
string who();
string lookup(sqlite3 *, string, userInfo *);
//globals
bool SHUTDOWN = false;
bool running[100];
string clientIPs[100];
string userNames[100];

int main(int argc, char const *argv[])
{
    // declarations for sqlite3
    sqlite3 *db;
    sqlite3_stmt *statement;
    char *errmsg = 0;
    int retC;
    string sql;

    // open DB
    retC = sqlite3_open("data.db", &db);

    if (retC)
    {
        cout << "Can't open database: " << sqlite3_errmsg(db) << endl;
        return (0);
    }
    else
    {
        cout << "Opened database successfully.\n";
    }

    // create table USERS
    sql = "CREATE TABLE USERS ("
          "ID	                INTEGER         PRIMARY KEY    AUTOINCREMENT     NOT NULL,"
          "first_name	        TEXT,"
          "last_name	        TEXT,"
          "user_name	        TEXT        NOT NULL,"
          "password	        TEXT,"
          "usd_balance	    REAL     NOT NULL);";

    retC = sqlite3_exec(db, sql.c_str(), callback, 0, &errmsg);

    if (retC != SQLITE_OK)
    {
        cout << "SQL Error: " << errmsg << endl;
        sqlite3_free(errmsg);
    }
    else
    {
        cout << "Table USERS created successfully\n";
    }
    // create table Pokemon_Cards
    sql = "CREATE TABLE Pokemon_Cards ("
          "ID                 INTEGER     PRIMARY KEY     AUTOINCREMENT       NOT NULL,"
          "card_name          TEXT                                            NOT NULL,"
          "card_type          TEXT                                            NOT NULL,"
          "rarity             TEXT                                            NOT NULL,"
          "count              INTEGER,"
          "owner_id           INTEGER,"
          "FOREIGN KEY(owner_id) REFERENCES USERS(ID));";

    retC = sqlite3_exec(db, sql.c_str(), callback, 0, &errmsg);

    if (retC != SQLITE_OK)
    {
        cout << "SQL Error: " << errmsg << endl;
        sqlite3_free(errmsg);
    }
    else
    {
        cout << "Table Pokemon Cards created successfully.\n";
    }
    //populate users in case empty
    sql = "SELECT * FROM USERS WHERE ID = 1;";
    retC = sqlite3_prepare_v2(db, sql.c_str(), sql.length(), &statement, nullptr);
    retC = sqlite3_step(statement);
    if (retC == SQLITE_DONE)
    {
        sql = "INSERT INTO USERS (first_name,last_name,user_name,password,usd_balance) VALUES ('root', 'root', 'root', 'root01', '100.00');";
        retC = sqlite3_exec(db, sql.c_str(), callback, 0, &errmsg);
        sql = "INSERT INTO USERS (first_name,last_name,user_name,password,usd_balance) VALUES ('Mary', 'Jane', 'mary', 'mary01', '100.00');";
        retC = sqlite3_exec(db, sql.c_str(), callback, 0, &errmsg);
        sql = "INSERT INTO USERS (first_name,last_name,user_name,password,usd_balance) VALUES ('John', 'Doe', 'john', 'john01', '100.00');";
        retC = sqlite3_exec(db, sql.c_str(), callback, 0, &errmsg);
        sql = "INSERT INTO USERS (first_name,last_name,user_name,password,usd_balance) VALUES ('Moe', 'Money', 'moe', 'moe01', '100.00');";
        retC = sqlite3_exec(db, sql.c_str(), callback, 0, &errmsg);
        cout << "Starter user have been added to the USERS table.\n";

        if (retC != SQLITE_OK)
        {
            cout << stderr << "SQL error: " << errmsg << endl;
            sqlite3_free(errmsg);
        }
    }
    sqlite3_finalize(statement);
    sql.clear();
    // declarations for socket transfer and primary control of program

    pthread_t threadIds[100];
    threadArgs args;
    int server_fd, new_socket, valread;
    struct sockaddr_in address;
    int opt = 1;
    socklen_t addrlen = sizeof(address);

    for (int k = 0; k < 100; k++)
    {
        threadIds[k] = 0;
        userNames[k] = "Unknown";
    }

    // create socket descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        perror("Socket Failed");
        exit(EXIT_FAILURE);
    }

    cout << "Socket file descriptor created: " << &server_fd << endl;
    // set socket options
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)))
    {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
    cout << "Socket attached to port \n";
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(SERVER_PORT);
    // bind socket to address
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    cout << "Attached socket to port\n";
    string holder;
    string received = "Start";

    // start listening for connections
    if (listen(server_fd, 3) < 0)
    {
        perror("listen");
        exit(EXIT_FAILURE);
    }
    cout << "Waiting for connection from client. . .\n";
    // socket connection loop
    while (1)
    {
        int ready;
        struct timeval timer;
        timer.tv_sec = 0;
        timer.tv_usec = 5;

        fd_set sockets;
        FD_ZERO(&sockets);
        FD_SET(server_fd, &sockets);

        ready = select(server_fd + 1, &sockets, NULL, NULL, &timer);

        if (ready != 0)
        {
            // accept pending connection
            new_socket = accept(server_fd, (struct sockaddr *)&address, &addrlen);
            if (new_socket < 0)
            {
                perror("accept");
                cout << new_socket;
                exit(EXIT_FAILURE);
            }
            //loop to find a good thread slot in 100 slot pool
            for (int k = 0; k < 100; k++)
            {
                if (running[k] == false)
                {
                    args.database = db;
                    args.socket = new_socket;
                    args.threadId = k;
                    running[k] = true;

                    char clientAddr[INET_ADDRSTRLEN];
                    //saves the ip in an array 
                    if(inet_ntop(AF_INET, &(address.sin_addr), clientAddr, INET_ADDRSTRLEN) != NULL)
                    {
                        string IPString = clientAddr;
                        clientIPs[k] = IPString;
                    }
                    //creates a thread that starts in the control loop
                    pthread_create(&threadIds[k], NULL, controlLoop, &args);
                    break;
                }
            }

            cout << "Ready to accept transfers on port: " << SERVER_PORT << " Through thread: " << args.threadId << " With IP: " << clientIPs[args.threadId] << endl;
        }
        //loops through, cleaning up closed threads
        for (int i = 0; i < 100; i++)
        {
            if (!running[i] && threadIds[i] != 0)
            {
                pthread_join(threadIds[i], NULL);
                threadIds[i] = 0;
                clientIPs[i].clear();
                userNames[i] = "Unknown";
                cout << "Thread: " << i << " has been closed.\n";
            }
        }


        //once root calls shutdown this segment runs
        if (SHUTDOWN)
        {
            // shutdown function sends response to client to acknowledge command
            string response = "200 OK. Server shutting down.\n";
            send(new_socket, response.data(), response.size(), 0);
            // close socket and shuts down server
            close(new_socket);
            shutdown(server_fd, SHUT_RDWR);
            // closes database
            sqlite3_close(db);
            // exits program
            return 0;
        }
    }
}
//this function is the start point of all threads and handles the inputs from client
void *controlLoop(void *args)
{

    userInfo user;
    threadArgs *transfer = (threadArgs *)args;

    sqlite3 *db = transfer->database;
    int threadId = transfer->threadId;
    int new_socket = transfer->socket;

    string holder;
    string received;
    string response;
    int valread;
    char buffer[1024] = {0};
    //main control loop
    while (!(holder == "QUIT") && !(holder == "SHUTDOWN"))
    {
        // accept socket transfer from client
        // clear received from inside loop to sterilize input
        received.clear();
        holder.clear();
        // read from socket buffer
        valread = read(new_socket, buffer, 1024);
        // only add message sent to received string based on length of message sent stored in valread
        received.append(buffer, valread);
        // begin if statements based on input from client, building sql statements for each command and sending them
        // to the proper function to get a string response to send back to the client

        int i = 0;
        //pulls first word of message to check for command
        do
        {
            if (received[i] != ' ')
            {
                holder += received[i];
            }
            i++;
        } while (received[i] != ' ' && i < received.length());
        cout << "Command: " << received << " received from thread: " << threadId << endl;
        //series of if statements to direct the input to the correct function
        if (holder == "LOGIN" && !user.loggedIn)
        {
            string response;
            
            user = login(&user, received, db);

            if (user.loggedIn)
            {
                response = "200 OK. Login successful.\n";
                userNames[threadId] = user.userName;
                send(new_socket, response.data(), response.length(), 0);
            }
            else
            {
                response = "403 BAD. Login failed. Please check your information and try again.\n";
                send(new_socket, response.data(), response.length(), 0);
            }

            received.clear();
        }
        else if (holder == "LOGIN" && user.loggedIn)
        {
            string response;

            response = "USER Already logged in.\n";
            send(new_socket, response.data(), response.length(), 0);

            received.clear();

        }
        else if (holder == "LOGOUT" && user.loggedIn)
        {
            string response = "200 OK. Logout successful.\n";
            user.loggedIn = false;
            user.rootUser = false;
            send(new_socket, response.data(), response.length(), 0);

        }
        else if (holder == "ADDUSER" && user.loggedIn && user.rootUser)
        {
            string response;
            // non-required add user functionality to add a user to the database
            // calls adduser function passing database and sql statement built through data transfer inputs
            response = adduser(db, received);
            // sends premade good reply once created
            send(new_socket, response.data(), sizeof(response), 0);
            // clear received string for continued use in the control loop
            received.clear();
        }
        else if (holder == "BALANCE" && user.loggedIn)
        {
            // balance check
            string response;
            // sets response string to hold response created in the balance function
            response = balance(db, received, &user);

            // sends response string
            send(new_socket, response.data(), response.length(), 0);
            received.clear();
        }
        else if (holder == "BUY" && user.loggedIn)
        {
            // buy function
            string response;

            response = buyCard(db, received, &user);
            send(new_socket, response.data(), response.length(), 0);
            received.clear();
        }
        else if (holder == "SELL" && user.loggedIn)
        {
            // sell function
            string response;

            response = sell(db, received, &user);
            send(new_socket, response.data(), response.length(), 0);
            received.clear();
        }
        else if (holder == "LIST" && user.loggedIn)
        {
            // list cards owned by user
            string response;
            response = search(db, received, &user);

            send(new_socket, response.data(), response.length(), 0);
            received.clear();
        }
        else if (holder == "DEPOSIT" && user.loggedIn)
        {
            string response;
            response = deposit(db, received, &user);

            send(new_socket, response.data(), response.length(), 0);
            received.clear();
        }
        else if (holder == "WHO" && user.loggedIn && user.rootUser)
        {
            string response;
            response = who();

            send(new_socket, response.data(), response.length(), 0);
            received.clear();
        }
        else if (holder == "WHO" && !user.rootUser)
        {
            string response;
            response = "401 BAD. Access denied.";

            send(new_socket, response.data(), response.length(), 0);
            received.clear();
        }
        else if (holder == "LOOKUP" && user.loggedIn)
        {
            string response;

            response = lookup(db, received, &user);

            send(new_socket, response.data(), response.length(), 0);
            received.clear();
        }
        else if (holder == "QUIT")
        {
            // quit function sends response to client to acknowledge closure
            string response = "200 OK. Client shutting down.\n";
            send(new_socket, response.data(), response.length(), 0);
            // closes socket
            close(new_socket);
            // doesn't clear received string so that quit exits the inner control loop and goes back to waiting for connections
            running[threadId] = false;
        }
        else if (holder == "SHUTDOWN" && user.loggedIn && user.rootUser)
        {
            SHUTDOWN = true;
        }
        else if (holder == "SHUTDOWN" && user.loggedIn && !user.rootUser)
        {
            string response  = "403 BAD. Unable to process shutdown. Access Denied.\n";
            send(new_socket, response.data(), response.length(), 0);
            holder.clear();
        }
        else if (holder == "SHUTDOWN" && !user.loggedIn && !user.rootUser)
        {
            string response  = "403 BAD. Unable to process shutdown. Access Denied.\n";
            send(new_socket, response.data(), response.length(), 0);
            holder.clear();
        }
        else if (!user.loggedIn && (holder == "ADDUSER" || holder == "BUY" || holder == "SELL" || holder == "LIST" || holder == "BALANCE" || holder == "WHO" || holder == "DEPOSIT" || holder == "LOOKUP" || holder == "LOGOUT"))
        {
            string response = "403 BAD. User not logged in.\n";
            send(new_socket, response.data(), response.length(), 0);
        }
        else
        {
            string response = "403 BAD. Incorrect input, please follow the instructions.\n";
            send(new_socket, response.data(), response.length(), 0);
        }
    }
}


// callback function used for printing to console when testing list check originally
static int callback(void *unused, int argc, char **argv, char **azColName)
{
    int i;
    for (i = 0; i < argc; i++)
    {
        cout << azColName[i] << " = " << (argv[i] ? argv[i] : "NULL") << endl;
    }
    cout << endl;
    return 0;
}
// checks a string to see if it's entirely numerical
bool onlyNum(const string input)
{
    return (input.find_first_not_of("1234567890.") == string::npos);
}
//login function, returns userInfo struct instance with pertinent information
userInfo login(userInfo *user, string received, sqlite3 *db)
{
    userInfo temporary;

    temporary.loggedIn = false;
    temporary.rootUser = false;
    temporary.userName = user->userName;
    
    string holder, password, sql;
    int i = 0;
    int j = 0;
    
    while (i < received.length())
    {
        holder.clear();
        do
        {
            if (received[i] != ' ')
            {
                holder += received[i];
            }
            i++;

        } while (received[i] != ' ' && i < received.length());
        j++;
        if (j == 1)
            j = j;
        else if (j == 2)
            temporary.userName = holder;
        else if (j == 3)
            password = holder;
        else
        {
            temporary.loggedIn = false;
            return temporary;
        }
        i++;
    }
    //sql statement to check user and password for login
    sql = "SELECT ID FROM USERS WHERE user_name= '";
    sql += temporary.userName;
    sql += "' AND password= '";
    sql += password;
    sql += "'";

    sqlite3_stmt *stmt;
    char *errmsg = 0;

    int retC = sqlite3_prepare_v2(db, sql.c_str(), sql.length(), &stmt, nullptr);

    if (retC != SQLITE_OK)
    {
        cout << stderr << "SQL error: " << errmsg << endl;
        sqlite3_free(errmsg);
        temporary.loggedIn = false;
        return temporary;
    }
    else
    {
        int status = sqlite3_step(stmt);
        if (status != SQLITE_DONE)
        {
            temporary.userId = sqlite3_column_int(stmt, 0);
            temporary.loggedIn = true;
            if(temporary.userName == "root")
                temporary.rootUser = true;
        sqlite3_finalize(stmt);
        return temporary;        
        }
        else
        {
            temporary.loggedIn = false;
            return temporary;
        }
    }
    return temporary;
}


// adds user to database, responses are handled in control loop
string adduser(sqlite3 *db, string received)
{
    int retC;
    int i = 0;
    int j = 0;
    char *errmsg = 0;

    string sql = "INSERT INTO USERS (first_name,last_name,user_name,password,usd_balance) "
                 "VALUES ('";
    errmsg = 0;
    string firstName;
    string lastName;
    string userName;
    string password;
    string balance;
    string holder;

    while (i < received.length())
    {
        holder.clear();
        do
        {
            if (received[i] != ' ')
            {
                holder += received[i];
            }
            i++;

        } while (received[i] != ' ' && i < received.length());
        j++;
        if (j == 1)
            j = j;
        else if (j == 2)
            firstName = holder;
        else if (j == 3)
            lastName = holder;
        else if (j == 4)
            userName = holder;
        else if (j == 5)
            password = holder;
        else if (j == 6 && onlyNum(holder))
            balance = holder;
        else
            return "403 Bad Input. Unable to Complete Add User.\n";

        i++;
    }

    // first name
    sql += firstName;
    sql += "', '";
    // last name
    sql += lastName;
    sql += "', '";
    // username
    sql += userName;
    sql += "', '";
    // password
    sql += password;
    sql += "', '";
    // starting balance
    sql += balance;
    sql += "');";

    // executes passed sql statement on passed database
    retC = sqlite3_exec(db, sql.c_str(), callback, 0, &errmsg);

    if (retC != SQLITE_OK)
    {
        cout << stderr << "SQL error: " << errmsg << endl;
        sqlite3_free(errmsg);
    }

    return "200 OK. User Added.\n";
}
// checks balance and returns a string response to send to client
string balance(sqlite3 *db, string received, userInfo *user)
{
    int retC;
    int i = 0;
    int j = 0;
    char *errmsg = 0;
    sqlite3_stmt *stmt;
    const unsigned char *fName;
    const unsigned char *lName;
    double balance;
    string response;
    string holder;
    int uId;
    string sql = "SELECT first_name, last_name, usd_balance FROM USERS WHERE ID=";


    uId = user->userId;
    sql += to_string(uId);
    // runs passed prepared statement on passed database
    retC = sqlite3_prepare_v2(db, sql.c_str(), sql.length(), &stmt, nullptr);

    if (retC != SQLITE_OK)
    {
        cout << stderr << "SQL error: " << errmsg << endl;
        sqlite3_free(errmsg);
    }
    else
    {
        if (sqlite3_step(stmt) != SQLITE_DONE)
        {

            fName = sqlite3_column_text(stmt, 0);
            lName = sqlite3_column_text(stmt, 1);
            balance = sqlite3_column_double(stmt, 2);
            // builds balance response
            response = "Balance for ";
            response += (const char *)fName;
            response += " ";
            response += (const char *)lName;
            response += " is: $";
            ostringstream holder;
            holder << fixed;
            holder << setprecision(2);
            holder << balance;
            response += holder.str();
            response += "\n";
        }
        else
            response = "400 BAD. User not found.\n"; // negative response

        return response; // returns response to send to client
    }
}
// buy card function returns string to be sent to client
string buyCard(sqlite3 *db, string received, userInfo *user)
{
    int retC;
    int i = 0;
    int j = 0;
    char *errmsg = 0;
    string response;
    string holder;

    string sql = "INSERT INTO Pokemon_Cards (card_name,card_type,rarity,count,owner_id) "
                 "VALUES ('";
    string cardName, cardType, rarity, ownerId;
    int uId, numCards;
    double cost;
    string cardCost;
    string count;

    errmsg = 0;
    //loop to pull apart input and assign to variables for parsing
    while (i < received.length())
    {
        holder.clear();
        do
        {
            if (received[i] != ' ')
            {
                holder += received[i];
            }
            i++;

        } while (received[i] != ' ' && i < received.length());
        j++;
        if (j == 1)
            j = j;
        else if (j == 2)
            cardName = holder;
        else if (j == 3)
            cardType = holder;
        else if (j == 4)
            rarity = holder;
        else if (j == 5 && onlyNum(holder))
            count = holder;
        else if (j == 6 && onlyNum(holder))
            cardCost = holder;
        else
            return "403 Bad Input. Unable to Complete Purchase.\n";

        i++;
    }

    ownerId = to_string(user->userId);

    // card name
    sql += cardName;
    sql += "', '";
    // card type
    sql += cardType;
    sql += "', '";
    // card rarity
    sql += rarity;
    sql += "', '";
    // card count
    sql += count;
    sql += "', '";

    // save owner id in uId as an integer to use for calculation
    uId = stoi(ownerId);
    numCards = stoi(count);
    sql += ownerId;
    sql += "');";
    // save card cost in cardCost as an integer to use for calculations
    cost = stod(cardCost);
    cost = cost * numCards;
    // second sql statement for balance checking
    string buysql;
    double balance = 0;
    buysql = "SELECT usd_balance FROM USERS where ID=";
    buysql += ownerId;
    sqlite3_stmt *stmt;
    // prepared sql statement to search for balance of user buying
    retC = sqlite3_prepare_v2(db, buysql.c_str(), buysql.length(), &stmt, nullptr);

    if (retC != SQLITE_OK)
    {
        cout << stderr << "SQL error: " << errmsg << endl;
        sqlite3_free(errmsg);
    }
    else
    {
        sqlite3_step(stmt);
        balance = sqlite3_column_double(stmt, 0);
        sqlite3_finalize(stmt);
    }
    balance = balance - cost;
    // if user has enough money, buy. If user doesn't, return error message to client
    if (balance >= 0)
    {
        // checks for card and owner id, if entry already exists, adjust number instead of create new entry
        string checksql = "SELECT ID,count FROM Pokemon_Cards WHERE owner_id=";
        checksql += ownerId;
        checksql += " AND card_name= '";
        checksql += cardName;
        checksql += "'";

        int checkId = 0, checkCount;
        retC = sqlite3_prepare_v2(db, checksql.c_str(), checksql.length(), &stmt, nullptr);

        if (retC != SQLITE_OK)
        {
            cout << stderr << "SQL error: " << errmsg << endl;
            sqlite3_free(errmsg);
        }
        else
        {
            sqlite3_step(stmt);
            checkId = sqlite3_column_int(stmt, 0);
            checkCount = sqlite3_column_int(stmt, 1);
            sqlite3_finalize(stmt);
        }

        if (checkId != 0)
        {
            sql = "UPDATE Pokemon_Cards SET count = ";
            sql += to_string(checkCount + numCards);
            sql += " WHERE ID=";
            sql += to_string(checkId);
        }
    }
    else
        return "403 BAD. Unable to complete purchase, not enough money or user not found.\n";

    // executes statement sent, decision is made in control loop
    retC = sqlite3_exec(db, sql.c_str(), callback, 0, &errmsg);

    if (retC != SQLITE_OK)
    {
        cout << stderr << "SQL error: " << errmsg << endl;
        sqlite3_free(errmsg);
    }

    // statement designed to update balance for response
    sql = "UPDATE USERS set usd_balance =";
    sql += to_string(balance);
    sql += " where ID=";
    sql += to_string(uId);

    retC = sqlite3_prepare_v2(db, sql.c_str(), sql.length(), &stmt, nullptr);

    if (retC != SQLITE_OK)
    {
        cout << stderr << "SQL error: " << errmsg << endl;
        sqlite3_free(errmsg);
    }
    else
    {
        sqlite3_step(stmt);
        balance = sqlite3_column_int(stmt, 0);
        sqlite3_finalize(stmt);
    }
    // statment to get updated balance for response
    sql = "SELECT usd_balance FROM USERS where ID=";
    sql += to_string(uId);
    retC = sqlite3_prepare_v2(db, sql.c_str(), sql.length(), &stmt, nullptr);

    if (retC != SQLITE_OK)
    {
        cout << stderr << "SQL error: " << errmsg << endl;
        sqlite3_free(errmsg);
    }
    else
    {
        sqlite3_step(stmt);
        balance = sqlite3_column_double(stmt, 0);
        sqlite3_finalize(stmt);
        // response crafted and returned for sending to client
        response = "200 OK. Purchase complete.\nNew balance: $";
        ostringstream holder;
        holder << fixed;
        holder << setprecision(2);
        holder << balance;
        response += holder.str();
        return response;
    }
    return response;
}
// sell function returns string to be sent back to client
string sell(sqlite3 *db, string received, userInfo *user)
{
    int retC;
    int i = 0;
    int j = 0;
    double newBalance;
    char *errmsg = 0;
    string response;
    sqlite3_stmt *stmt;
    string cardName;
    string count;
    int total, id, cardCount, userid;
    double cost;
    string price;
    string uId;
    string holder;
    string sql = "SELECT ID, count FROM Pokemon_Cards WHERE card_name= '";
    //loop to pull command into variables for parsing
    while (i < received.length())
    {
        holder.clear();
        do
        {
            if (received[i] != ' ')
            {
                holder += received[i];
            }
            i++;

        } while (received[i] != ' ' && i < received.length());
        j++;
        if (j == 1)
            j = j;
        else if (j == 2)
            cardName = holder;
        else if (j == 3 && onlyNum(holder))
            count = holder;
        else if (j == 4 && onlyNum(holder))
            price = holder;
        else
            return "403 Bad Input. Unable to Complete Sale.\n";

        i++;
    }

    uId = to_string(user->userId);
    // card name
    sql += cardName;
    sql += "' AND owner_id = ";
    sql += uId;
    // card count
    // count saved to int
    cardCount = stoi(count);
    // card price
    // price saved to double
    cost = stod(price);
    cost = cost * cardCount;
    // user id

    // id saved to integer
    userid = stoi(uId);
    // prepared statement to get id and count from cards with same name as sell option
    retC = sqlite3_prepare_v2(db, sql.c_str(), sql.length(), &stmt, nullptr);

    if (retC != SQLITE_OK)
    {
        cout << stderr << "SQL error: " << errmsg << endl;
        sqlite3_free(errmsg);
    }
    else
    {
        sqlite3_step(stmt);
        id = sqlite3_column_int(stmt, 0);
        total = sqlite3_column_int(stmt, 1);
        sqlite3_finalize(stmt);

        if (total > cardCount)
        {
            // if enough cards exist to sell and still have some, update
            sql = "UPDATE Pokemon_Cards SET count =";
            sql += to_string(total - cardCount);
            sql += " WHERE ID=";
            sql += to_string(id);
        }
        else if (total == cardCount)
        {
            // if sell order is same as stock, delete entry
            sql = "DELETE FROM Pokemon_Cards WHERE ID =";
            sql += to_string(id);
        }
        else
            return "400 BAD. Unable to complete sale, not enough stock.\n"; // not enough stock to sell
    }
    // executes the sql statement sent by the main control function
    retC = sqlite3_exec(db, sql.c_str(), callback, 0, &errmsg);

    if (retC != SQLITE_OK)
    {
        cout << stderr << "SQL error: " << errmsg << endl;
        sqlite3_free(errmsg);
    }
    // new sql statement to get balance from the user selling
    sql = "SELECT usd_balance FROM USERS WHERE ID = ";
    sql += to_string(userid);

    retC = sqlite3_prepare(db, sql.c_str(), sql.length(), &stmt, nullptr);

    if (retC != SQLITE_OK)
    {
        cout << stderr << "SQL error: " << errmsg << endl;
        sqlite3_free(errmsg);
    }
    else
    {
        sqlite3_step(stmt);
        newBalance = sqlite3_column_double(stmt, 0);
        newBalance += cost;
        sqlite3_finalize(stmt);
    }
    // update the user's balance
    sql = "UPDATE USERS SET usd_balance = ";
    sql += to_string(newBalance);
    sql += " WHERE ID = ";
    sql += to_string(userid);

    retC = sqlite3_exec(db, sql.c_str(), callback, 0, &errmsg);

    if (retC != SQLITE_OK)
    {
        cout << stderr << "SQL error: " << errmsg << endl;
        sqlite3_free(errmsg);
    }
    else
    {
        // create response string to return with balance
        response = "200 OK. Sale complete. New balance is: $";
        ostringstream holderS;
        holderS << fixed;
        holderS << setprecision(2);
        holderS << newBalance;
        response += holderS.str();
    }
    return response;
}
// list function
string search(sqlite3 *db, string received, userInfo *user)
{
    
    int retC, status;
    int i = 0;
    int j = 0;
    char *errmsg = 0;
    string response;
    string holder;
    sqlite3_stmt *stmt;
    // list cards owned by user
    string sql;
    int owner;
    errmsg = 0;

    owner = user->userId;
    if (user->userName == "root")
    {
        sql = "SELECT * FROM Pokemon_Cards";
    }
    else
    {
        sql = "SELECT * FROM Pokemon_Cards WHERE owner_id=";
        sql += to_string(owner);
        sql += "";
    }

    
    // run prepared statement from main control loop
    retC = sqlite3_prepare(db, sql.c_str(), sql.length(), &stmt, nullptr);

    if (retC != SQLITE_OK)
    {
        cout << stderr << "SQL error: " << errmsg << endl;
        sqlite3_free(errmsg);
    }
    else
    {
        status = sqlite3_step(stmt);
        // loops until no more rows
        while (status != SQLITE_DONE)
        {
            // for loop that goes until the column is empty
            for (int i = 0; i < sqlite3_column_count(stmt); i++)
            {
                // gets column name
                response += (const char *)sqlite3_column_name(stmt, i);
                response += " = ";
                // gets column text
                response += (const char *)sqlite3_column_text(stmt, i);
                response += "\n;";
            }
            response += "\n";
            // progresses the statement to the next column
            status = sqlite3_step(stmt);
        }
        sqlite3_finalize(stmt);
        // search complete addition to response
        response += "\n200 OK. Search Complete.\n";
    }
    return response;
}

//deposit function adds funds to logged in user
string deposit(sqlite3 *db, string received, userInfo *user)
{
    int i = 0;
    int j = 0;
    char *errmsg = 0;
    userInfo temporary;
    temporary.userName = user->userName;
    string response;
    string holder;
    double balance;
    string deposit;
    sqlite3_stmt *stmt;
    string sql = "SELECT usd_balance FROM USERS WHERE user_name = '";

    sql += temporary.userName;
    sql += "'";

    int retC = sqlite3_prepare(db, sql.c_str(), sql.length(), &stmt, nullptr);

    if (retC != SQLITE_OK)
    {
        cout << stderr << "SQL error: " << errmsg << endl;
        sqlite3_free(errmsg);
    }
    else
    {
        sqlite3_step(stmt);
        balance = sqlite3_column_double(stmt, 0);
    }

    while (i < received.length())
    {
        holder.clear();
        do
        {
            if (received[i] != ' ')
            {
                holder += received[i];
            }
            i++;

        } while (received[i] != ' ' && i < received.length());
        j++;
        if (j == 1)
            j = j;
        else if (j == 2 && onlyNum(holder))
            deposit = holder;
        else
            return "403 Bad Input. Unable to Complete deposit.\n";

        i++;
    }

    balance = balance + stod(deposit);

    sql = "UPDATE USERS SET usd_balance = ";

    sql += to_string(balance);
    sql += " WHERE user_name = '";
    sql += temporary.userName;
    sql += "'";

    retC = sqlite3_exec(db, sql.c_str(), callback, 0, &errmsg);

    if (retC != SQLITE_OK)
    {
        cout << stderr << "SQL error: " << errmsg << endl;
        sqlite3_free(errmsg);
    }
    else
    {
        response = "200 OK. Deposit complete. New balance: $";
        response += to_string(balance);
        return response;
    }


}
//lists connections for root with 'Unknown' as username for non-logged users
string who()
{
    string response = "Users connected:\n";

    for (int i = 0; i < 100; i++)
    {
        if (running[i] == true)
        {
            response += userNames[i];
            response += "\t";
            response += clientIPs[i];
            response += "\n";
        }
    }

    response += "\nEnd of List\n";
    return response;
}
//search function for either name or type of pokemon cards belonging to user
string lookup(sqlite3 *db, string received, userInfo *user)
{
    userInfo temporary;
    
    temporary.userName = user->userName;
    temporary.userId = user->userId;

    string response;
    string holder;
    string searchTerm;
    int i = 0;
    int j = 0;
    int retC, status;
    char *errmsg = 0;
    sqlite3_stmt *stmt;
    string sql = "SELECT * FROM Pokemon_Cards WHERE owner_id = ";

    sql += to_string(temporary.userId);
    sql += " AND (card_name LIKE '%";

    while (i < received.length())
    {
        holder.clear();
        do
        {
            if (received[i] != ' ')
            {
                holder += received[i];
            }
            i++;

        } while (received[i] != ' ' && i < received.length());
        j++;
        if (j == 1)
            j = j;
        else if (j == 2)
            searchTerm = holder;
        else
            return "403 Bad Input. Unable to complete search.\n";

        i++;
    }

    sql += searchTerm;
    sql += "%' OR card_type LIKE '%";
    sql += searchTerm;
    sql += "%')";

    retC = sqlite3_prepare(db, sql.c_str(), sql.length(), &stmt, nullptr);

    if (retC != SQLITE_OK)
    {
        cout << stderr << "SQL error: " << errmsg << endl;
        sqlite3_free(errmsg);
    }
    else
    {
        int runs = 0;
        status = sqlite3_step(stmt);
        // loops until no more rows
        while (status != SQLITE_DONE)
        {
            runs++;
            // for loop that goes until the column is empty
            for (int i = 0; i < sqlite3_column_count(stmt); i++)
            {
                // gets column name
                response += (const char *)sqlite3_column_name(stmt, i);
                response += " = ";
                // gets column text
                response += (const char *)sqlite3_column_text(stmt, i);
                response += "\n;";
            }
            response += "\n";
            // progresses the statement to the next row
            status = sqlite3_step(stmt);
        }
        sqlite3_finalize(stmt);
        // search complete addition to response
        if (runs == 0)
        {
            response = "404. no results found.\n";
        }
        else
            response += "\n200 OK. Search Complete.\n";
    }
    return response; 

}