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

#define SERVER_PORT 7472
#define MAX_LINE 1024

using namespace std;
// function prototypes
string stringCleaner(string);

int main(int argc, char *argv[])
{
    // socket transfer declarations
    FILE *fp;
    struct hostent *hp;
    struct sockaddr_in sin;
    char *host;
    char buf[MAX_LINE];
    int s;
    int len;
    // code for setting host
    if (argc == 2)
    {
        host = argv[1];
    }
    else
    {
        cout << "Usage: Simplex-talk host\n"
             << stderr << endl;
        exit(1);
    }

    hp = gethostbyname(host);
    if (!hp)
    {
        cout << "Simplex-talk: Unknown host: " << host << endl
             << stderr << endl;
        exit(1);
    }
    // socket settings
    bzero((char *)&sin, sizeof(sin));
    sin.sin_family = AF_INET;
    bcopy(hp->h_addr, (char *)&sin.sin_addr, hp->h_length);
    sin.sin_port = htons(SERVER_PORT);
    // socket creation and connection
    if ((s = socket(PF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("Simplex-talk: Socket");
        exit(1);
    }
    if (connect(s, (struct sockaddr *)&sin, sizeof(sin)) < 0)
    {
        perror("Simplex-talk: Connect");
        close(s);
        exit(1);
    }
    // declarations for control loop
    string option;
    string holder;
    string response;
    int valread;
    // control loop
    while (1)
    {
        option.clear();
        holder.clear();
        response.clear();
        cout << "\nPlease select one of the available options: \n";
        cout << "LOGIN <username> <password>\n";
        cout << "LOGOUT\n";
        cout << "LOOKUP <search term>\n";
        cout << "LIST\n";
        cout << "BALANCE\n";
        cout << "DEPOSIT <amount>\n";
        cout << "BUY <card name> <card type> <card rarity> <card quantity> <card cost>\n";
        cout << "SELL <card name> <card quantity> <card cost>\n";
        cout << "WHO (requires root access)\n";
        cout << "QUIT\n";
        cout << "SHUTDOWN (requires root access)\n";
        getline(cin, option);

        int i = 0;
        do
        {
            if (option[i] != ' ')
            {
                holder += option[i];
            }
            i++;

        } while (option[i] != ' ' && i < option.length());

        

        if (holder == "QUIT")
        {
            // quit option
            send(s, option.data(), option.length(), 0);
            cout << "Exiting client.\n";
            // receive response
            valread = read(s, buf, 1024);
            response.append(buf, valread);
            cout << response;
            response.clear();
            // close socket
            close(s);
            // end client program
            return 0;
        }
        else if (holder == "SHUTDOWN")
        {
            // shutdown option
            string holding;
            int i = 0;
            send(s, option.data(), option.length(), 0);
            // receive response
            valread = read(s, buf, 1024);
            response.append(buf, valread);

            cout << response << endl;
            // cout << response.length() << endl;
             do
            {
                 if (response[i] != ' ')
                 {
                     holding += response[i];
                 }
                 i++;
             } while (response[i] != ' ' && i < response.length());

            // cout << holding << endl;
            if (holding == "200")
            {
                cout << "Shutting down server and client.\n";
                // close socket
                close(s);
                // end client program
                return 0;
            }

            response.clear();
        }
        else
        {
            send(s, option.c_str(), option.length(), 0);
            valread = read(s, buf, 1024);
            response.append(buf, valread);
            cout << response;
            response.clear();
        }
    }
}
