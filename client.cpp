#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include "diary1.h"
#include <signal.h>
#include <unistd.h>

// Определимся с портом, адресом сервера и другими константами.
// В данном случае берем произвольный порт и адрес обратной связи
// (тестируем на одной машине).
#define  SERVER_PORT     5555
#define  SERVER_NAME    "127.0.0.1"
#define  BUFLEN          512

// Две вспомогательные функции для чтения/записи (см. ниже)
int  writeToServer  (int fd);
int  readFromServer (int fd);

bool readAll(int fd, size_t N, vector<char> &buf);

volatile sig_atomic_t W;

void handler(int signo)
{
    W = signo;
}

int  main (void)
{
    struct sigaction a;
    a.sa_handler = handler;
    a.sa_flags = 0;
    sigemptyset(&a.sa_mask);

    if(sigaction(SIGPIPE, &a, 0) == -1)
        return -1;

//    int i;
    int err;
    int sock;
    struct sockaddr_in server_addr;
    struct hostent    *hostinfo;

    // Получаем информацию о сервере по его DNS имени
    // или точечной нотации IP адреса.
    hostinfo = gethostbyname(SERVER_NAME);
    if ( hostinfo==NULL ) {
        fprintf (stderr, "Unknown host %s.\n",SERVER_NAME);
        exit (EXIT_FAILURE);
    }

    // Заполняем адресную структуру для последующего
    // использования при установлении соединения
    server_addr.sin_family = PF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    server_addr.sin_addr = *(struct in_addr*) hostinfo->h_addr;

    // Создаем TCP сокет.
    sock = socket(PF_INET, SOCK_STREAM, 0);
    if ( sock<0 ) {
        perror ("Client: socket was not created");
        exit (EXIT_FAILURE);
    }

    // Устанавливаем соединение с сервером
    err = connect (sock, (struct sockaddr*)&server_addr, sizeof(server_addr));
    if ( err<0 ) {
        perror ("Client:  connect failure");
        exit (EXIT_FAILURE);
    }
    fprintf (stdout,"Connection is ready\n");

    // Обмениваемся данными
    while(1) {
        if (writeToServer(sock) < 0) break;
        if (readFromServer(sock) < 0) break;
    }
    fprintf (stdout,"The end\n");


    // Закрываем socket
    close (sock);
    exit (EXIT_SUCCESS);
}



int  writeToServer (int fd)
{
    int   nbytes;
    char  buf[BUFLEN];

    fprintf(stdout,"> ");
    if (fgets(buf,BUFLEN,stdin)==nullptr) {
        int r = -1;
        write(fd, &r, 4);
        return -1;
    }
    buf[strlen(buf)-1] = 0;

    int len = strlen(buf) + 1;

    nbytes = write(fd, &len, 4);

    nbytes = write (fd, buf, strlen(buf) + 1);
    if ( nbytes<0 )
    {
        perror("write");
        return -1;
    }
//    if (strstr(buf,"stop")) return -1;
    return 0;
}


int  readFromServer (int fd)
{
    int   nbytes, n;

    nbytes = read(fd, &n, 4);
    if ( nbytes<0 ) {
        // ошибка чтения
        perror ("read");
        return -1;
    } else if ( nbytes==0 ) {
        // нет данных для чтения
        fprintf (stderr,"Client: no message\n");
    } else
        {
            // ответ успешно прочитан
            fprintf (stdout,"Server's replay:\n Status: %d\n", n);

            if (n == 0)
            {
                nbytes = read(fd, &n, 4);
                fprintf(stdout," The number of filters: %d\n", n);

//                vector<diary1> record(n);
                int j;

                for(int i = 0; i < n; i++)
                {
                    diary1 record;
                    read(fd, &j, sizeof(int));

                    vector<char> v(j);
                    //read(fd, &v[0], j);
                    readAll(fd, j, v);

                    toDiary1(record, v);
                    print(record);
                }
                //int aaa;
                //read(fd, &aaa, 4);
            }

//            fprintf(stdout, "\n");
        }
    return 0;
}

bool readAll(int fd, size_t N, vector<char> &buf)
{
    buf.clear();
    buf.resize(N);
    size_t n = 0, nbytes = 0;
    
    for(;;)
    {
        nbytes = read(fd, &buf[n], N - n);
        n += nbytes;
        
        if(n == N)
            return true;
    }
    return false;
}
