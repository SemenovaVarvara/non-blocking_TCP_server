#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <limits.h>
#include "diary1.h"
#include <signal.h>
#include <unistd.h>

// Определимся с номером порта и другими константами.
#define PORT    5555
#define BUFLEN  512

// Две вспомогательные функции для чтения/записи (см. ниже)
int   readFromClient(int fd, char *buf);
void  writeToClient (int fd, char *buf, int N, vector<diary1> &record);

int toInt(const char* str, int* ptr);

int parser(string str);
int isDate(string str);
int isTime(string str);

volatile sig_atomic_t W;

void handler(int signo)
{
    W = signo;
}

int  main (int ac, char* av[])
{
    int     i, err, opt=1;
    int     sock, new_sock;
    fd_set  active_set, read_set;
    struct  sockaddr_in  addr;
    struct  sockaddr_in  client;
    char    buf[BUFLEN];
    socklen_t  size;
    int N;

    if (ac == 1 || ac > 2)
    {
        perror ("Server: enter parameter N");
        exit(EXIT_FAILURE);
    }

    err = toInt(av[1], &N);
    if(err == -1 || N <= 0)
    {
        perror ("Server: wrong parameter N");
        exit(EXIT_FAILURE);
    }

	

    vector<diary1> record(N);
    for(i = 0; i < N; i++)
    {
        record[i].setRand();
        print(record[i]);
    }

    struct sigaction a;
    a.sa_handler = handler;
    a.sa_flags = 0;
    sigemptyset(&a.sa_mask);

    if(sigaction(SIGINT, &a, 0) == -1)
    {
        return -1;
    }

    // Создаем TCP сокет для приема запросов на соединение
    sock = socket (PF_INET, SOCK_STREAM, 0);
    if ( sock<0 ) {
        perror ("Server: cannot create socket");
        exit (EXIT_FAILURE);
    }
    setsockopt(sock,SOL_SOCKET,SO_REUSEADDR,(char*)&opt,sizeof(opt));

    // Заполняем адресную структуру и
    // связываем сокет с любым адресом
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    err = bind(sock,(struct sockaddr*)&addr,sizeof(addr));
    if ( err<0 )
    {
        perror ("Server: cannot bind socket");
        exit (EXIT_FAILURE);
    }

    // Создаем очередь на 3 входящих запроса соединения
    err = listen(sock,4);
    if ( err<0 )
    {
        perror ("Server: listen queue failure");
        exit(EXIT_FAILURE);
    }

    // Подготавливаем множества дескрипторов каналов ввода-вывода.
    // Для простоты не вычисляем максимальное значение дескриптора,
    // а далее будем проверять все дескрипторы вплоть до максимально
    // возможного значения FD_SETSIZE.
    FD_ZERO(&active_set);
    FD_SET(sock, &active_set);

    //return 0;

    // Основной бесконечный цикл проверки состояния сокетов
    while (!W) {
// 	    puts("A1");
        // Проверим, не появились ли данные в каком-либо сокете.
        // В нашем варианте ждем до фактического появления данных.
        read_set = active_set;
        if ( select(FD_SETSIZE,&read_set,NULL,NULL,NULL)<0 ) {
            perror("Server: select  failure");
            //exit (EXIT_FAILURE);
	    //goto done;
	    break;
        }
    //return 0;

        // Данные появились. Проверим в каком сокете.
        for (i=0; i<FD_SETSIZE; i++) {
            if ( FD_ISSET(i,&read_set) ) {
                if ( i==sock ) {
                    // пришел запрос на новое соединение
                    size = sizeof(client);
		    //return 0;
                    new_sock = accept(sock,(struct sockaddr*)&client,&size);
                    if ( new_sock<0 ) {
                        perror("accept");
//                         exit (EXIT_FAILURE);
                        break;
                    }
                    fprintf (stdout, "Server: connect from host %s, port %hu.\n",
                             inet_ntoa(client.sin_addr),
                             ntohs(client.sin_port));
                    FD_SET(new_sock, &active_set);
                } else {
                    // пришли данные в уже существующем соединени
                    err = readFromClient(i,buf);
                    if ( err<0 ) {
                        // ошибка или конец данных
                        close (i);
                        FD_CLR(i,&active_set);
                    } else {
                            // данные прочитаны нормально
                            writeToClient(i,buf, N, record);
                    }
                }
            }
        }
    }

//done:


    puts("Done.");

    return 0;
}

int  readFromClient (int fd, char *buf)
{
    int  nbytes, len;
    
    nbytes = read(fd, &len, sizeof(int));

    if (len == -1)
        return -1;

    nbytes = read(fd, buf, len);
    if ( nbytes<0 ) {
        // ошибка чтения
        perror ("Server: read failure");
        return -1;
    } else if ( nbytes==0 ) {
        // больше нет данных
        return -1;
    } else {
        // есть данные
        fprintf(stdout,"Server got message: %s\n", buf);
        return 0;
    }
}



void  writeToClient (int fd, char *buf, int N, vector<diary1> &record)
{
    int  nbytes, n;
    string str(buf);

    n = parser(str);

    if (n > N) {
        int k = -1;
        nbytes = write(fd, &k, sizeof k);

        fprintf(stdout,"Write back:\n Status: -1\n nbytes=%d\n\n", nbytes);
    } else if (n < 0)
    {
        int k = -2;
        nbytes = write(fd, &k, sizeof k);

        fprintf(stdout,"Write back:\n Status: -2\n  nbytes=%d\n\n", nbytes);
    } else
    {
        int k = 0;
        nbytes = write(fd, &k, sizeof k);

        fprintf(stdout,"Write back:\n Status: 0\n  nbytes=%d\n", nbytes);

        nbytes = write(fd, &n, sizeof n);
        fprintf(stdout," The number of filters: %d\n  nbytes=%d\n", n, nbytes);

        for(int i = 0; i < n; i++)
        {
            vector<char> v;
            int j;
            j = toChar(record[i], v);
//            fprintf(stdout, " Size of event: %d\n", j);
            fprintf(stdout, " Size of vector: %ld\n", v.size());
            j = v.size();
            write(fd, &j, sizeof j);

            nbytes = write(fd, &v[0], v.size());
//            fprintf(stdout, "nbytes = %d\n", nbytes);
            print(record[i]);
            v.clear();
        }

        fprintf(stdout, "\n");

    }

    if ( nbytes<0 ) {
        perror ("Server: write failure");
    }
}

int toInt(const char* str, int* ptr)
{
    long L;
    char* e;

    errno = 0;
    L = strtol(str, &e, 10);

    if (!errno && *e == '\0')
        if (INT_MIN <= L && L <= INT_MAX)
        {
            *ptr = (int)L;
            return 0;
        }
        else
            return -1;
    else
        return -1;
}

int parser(string str)
{
    string substr, num;
    int filters = -1, flag = -1, dort;

    if(str.compare(0, 6, "select") == 0)
    {
        flag = 1;

        if(str.size() == 6)
            filters = 0;
        else if (str[6] != ' ')
            flag = -1;
    } else
    {
        flag = -1;
    }

    if(flag != -1 && filters != 0)
    {
        size_t pos = 6;
        substr = str.substr(pos);
        filters = 0;

        while(substr.compare(0, 7, " date=[") == 0 || substr.compare(0, 7, " time=[") == 0)
        {
            if(substr.compare(0, 7, " date=[") == 0)
                dort = 1;
            else
                dort = 2;

            pos += 7;
            substr = str.substr(pos);

            if(dort == 1)
            {
                if(isDate(substr) == -1)
                    flag = -1;

                pos += 10;
            }
            else
            {
                if(isTime(substr) == -1)
                    flag = -1;

                pos += 5;
            }

            if(str[pos] != ',')
            {
                flag = -1;
                break;
            }
            substr = str.substr(pos + 1);
            pos++;

            if(dort == 1)
            {
                if(isDate(substr) == -1)
                    flag = -1;

                pos += 10;
            }
            else
            {
                if(isTime(substr) == -1)
                    flag = -1;

                pos += 5;
            }

            if(str[pos] != ']')
            {
                flag = -1;
                break;
            }

            filters++;
            pos++;
            substr = str.substr(pos);
        }
    }

    if(substr.size() == 1 && substr[0] == ' ')
        flag = flag;
    else if(substr.size() != 0)
        flag = -1;

    if (flag == 1)
        return filters;
    else
        return -1;
}

int isDate(string str)
{
    string num;
    int n;

    if(str.size() < 10)
        return -1;

    if(str[0] < '0' || str[0] > '9')
        return -1;
    if(str[1] < '0' || str[1] > '9')
        return -1;

    num = str.substr(0, 2);
    n = stoi(num);
    if (n <= 0 || 31 < n)
        return -1;

    if(str[2] != '.')
        return  -1;

    if(str[3] < '0' || str[3] > '9')
        return -1;
    if(str[4] < '0' || str[4] > '9')
        return -1;

    num = str.substr(3, 2);
    n = stoi(num);
    if (n <= 0 || 12 < n)
        return -1;

    if(str[5] != '.')
        return  -1;

    if(str[6] < '0' || str[6] > '9')
        return -1;
    if(str[7] < '0' || str[7] > '9')
        return -1;
    if(str[8] < '0' || str[8] > '9')
        return -1;
    if(str[9] < '0' || str[9] > '9')
        return -1;

    num = str.substr(6, 4);
    n = stoi(num);
    if (n <= 0 || 2023 < n)
        return -1;

    return 0;
}

int isTime(string str)
{
    string num;
    int n;

    if (str.size() < 5)
        return -1;

    if(str[0] < '0' || str[0] > '9')
        return -1;
    if(str[1] < '0' || str[1] > '9')
        return -1;

    num = str.substr(0, 2);
    n = stoi(num);
    if (n < 0 || 23 < n)
        return -1;

    if(str[2] != '.')
        return  -1;

    if(str[3] < '0' || str[3] > '9')
        return -1;
    if(str[4] < '0' || str[4] > '9')
        return -1;

    num = str.substr(3, 2);
    n = stoi(num);
    if (n < 0 || 59 < n)
        return -1;

    return 0;
}
