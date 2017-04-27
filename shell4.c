#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>

typedef struct node *PNode;
typedef struct node{
	char *str;
	PNode next, prev;
} Node;
typedef unsigned char uchar;

void addWord(PNode *p_P, char *s)
{
    PNode P = *p_P;
    if(P == NULL)
    {
        P = (PNode)malloc(sizeof(Node));
        P -> next = P -> prev = NULL;
        P -> str = (char *)malloc(strlen(s) + 1);
        strcpy(P -> str, s);
    }
    else
    {
        P -> next = (PNode)malloc(sizeof(Node));
        (P -> next) -> str = (char *)malloc(strlen(s) + 1);
        strcpy((P -> next) -> str, s);
		(P -> next) -> prev = P;
		P = P -> next;
		P -> next = NULL;
	}
	*p_P = P;
}

int isAmpOk()
{
	int nextc = getchar();
	ungetc(nextc, stdin);
	if(nextc == '\n' || nextc == EOF)
		return 1;
	else
		return 0;
}

int isspace1(int c) //символы, считающиеся пробельными
{
	char *s = "|<&";
	return (isspace(c) || strchr(s, c) != NULL);
}

uchar divLineToWords(PNode *p_P)
{
	char *line = NULL, *tmp;
	int c, inQuote = 0, inWord = 0, isInConv = 0, wrdbeg = 0;
	unsigned n = 80, size = n, i = 0;
	uchar status = 0;
	#define tmps "\"" //особое слово, используется для разделения программ внутри конвейера
	#define ERREOF 128
	#define ERRAMP 64
	#define ERRQ 32
	#define ERRCONV 16
	#define INPREDIR 4
	#define CONV 2
	#define BACKGR 1
	printf("$ ");
	if((c=getchar())!=EOF)
	{
		if(!(c == '\n' || (c == '&') || (c == '|'))) //амперсанд в начале строки либо пустая стр. = пустая команда
													//черта в начале - ошибка, двойной амперсанд или черта - ошибка
		{
			line = (char*)malloc(n);
			inQuote = (c == '"');
			if(!isspace1(c))
			{
				inWord = 1;
				wrdbeg = i;
				if(c != '"')
				{
					line[i] = c;
					i++;
				}
			}
			while((c = getchar())!='\n' || inQuote || isInConv) //пока не закрыты кавычки
															//или не закрыт конвейер
			{
				if(c == EOF) //конец файла в середине строки
				{
					if(inQuote) //конец файла при незакрытой кавычке
					{
						free(line);
						return ERRQ;
					}
					else if(isInConv) //конец файла при незакрытом конвейере
					{
						free(line);
						return ERRCONV;
					}
					break; //слово завершилось EOF, а не \n
				}
				if(c == '&' && !inQuote)
				{
					if(isAmpOk()) //команда завершилась амперсандом
					{
						status |= BACKGR;
						getchar(); //считать символ пустой строки
						break;
					}
					else //амперсанд вне кавычек и в неправильном месте
					{
						free(line);
						return ERRAMP;
					}
				}
				else if(c == '\n')
					printf("> ");
				else if(c == '"')
					inQuote = !inQuote;
				else if(c == '<' && !inQuote)
				{
					c = ' ';
					status |= INPREDIR;
				}
				else if(c == '|' && !inQuote) //если | вне кавычек, то это конвейер
				{
					if(isInConv) //два символа конвейера без команды между ними
					{
						free(line);
						return ERRCONV;
					}
					status |= CONV;
					isInConv = 1;
					if(!inWord) //нужно добавить разделитель после последнего параметра
						addWord(p_P, tmps); //для отделения команды вместе с параметрами от других команд
				}
				if(!inQuote && (isspace1(c)) && inWord) //конец слова
				{
					line[i] = '\0';
					i++;
					inWord = 0;
					addWord(p_P, line + wrdbeg);
					if(c == '|')
						addWord(p_P, tmps);
				}
				else if(!isspace1(c) && !inWord) //начало нового слова
				{
					if(isInConv)
						isInConv = 0;
					inWord = 1;
					wrdbeg = i;
					if(c != '"')
					{
						line[i] = c;
						i++;
					}
				}
				else if(inWord && c != '"') //внутри слова
				{
						line[i] = c;
						i++;
				}
				if(i > size - 4)
				{
					size += n;
					while((tmp = (char*)realloc(line, size)) == NULL);
					line = tmp;
				}
			}
			*(line + i) = '\0';
			if(inWord)
                addWord(p_P, line + wrdbeg);
			free(line);
			return status;
		}
		else
		{
			*p_P = NULL;
			if(c == '&' && isAmpOk())
                getchar();
            else if(c == '&')
				return ERRAMP;
            else if(c == '|')
            {
                getchar();
				return ERRCONV;
            }
			return 0;
		}
	}
	return ERREOF;
}

void writeArgs(PNode P, char ***p_argv) //пишет в argv все слова из P, и в конец пишет NULL
{
	int i = 0, argnum = 0;
	PNode Q = P;
	char **argv = *p_argv;
	if(P != NULL)
	{
		while(Q != NULL)
		{
			Q = Q -> next;
			argnum++;
		}
		argv = (char **)calloc(argnum + 1, sizeof(char *));
		for(i = 0; i < argnum; i++)
		{
			*(argv + i) = (char *)malloc(strlen(P -> str) + 1);
			strcpy(*(argv + i), P -> str);
			P = P -> next;
		}
		*(argv + argnum) = NULL;
	}
	*p_argv = argv;
}

void clean(PNode *p_P)
{
	PNode P = *p_P;
	if(P != NULL)
	{
		while(P -> prev != NULL)
		{
			free(P -> str);
			P = P -> prev;
			free(P -> next);
		}
		free(P);
		*p_P = NULL;
	}
}

PNode gotoBeg(PNode P)
{
	if(P != NULL)
		while(P -> prev != NULL)
			P = P -> prev;
	return P;
}

//ошибка (выход по exit с кодом не 0)
#define EXIT_ERROR( status ) (!((WIFEXITED((status)) && !WEXITSTATUS((status))) || !WIFEXITED((status))))

int main()
{
	#define ISCODE( status ) ((code & (status)) != 0)
	PNode P = NULL;
	int pid, i, status, bckgrNum = 0, inpfd = 0;
	uchar code, tmp;
	char **argv, *filename;
	int fd[2];
	setvbuf(stdout, NULL, _IONBF, BUFSIZ);
WrdInp:
	while((code = divLineToWords(&P)) < 16) //пока нет ошибок при вводе
	{
		while((pid = waitpid(0, &status, WNOHANG)) > 0)
		{
			printf("[%d]+  Done  PID = %d  Code = %d\n", bckgrNum, pid, WEXITSTATUS(status));
			bckgrNum--;
		}
		if(ISCODE(INPREDIR)) //задано перенаправление ввода (последнее слово в списке)
		{
			if(!strcmp(P -> str, ".") || !strcmp(P -> str, ".."))
			{
				code &= (~INPREDIR);
				fprintf(stderr, "Задано имя директории!\n");
				free(P -> str);
				if(P -> prev != NULL)
				{
					P = P -> prev;
					free(P -> next);
					P -> next = NULL;
				}
				else
				{
					free(P);
					P = NULL;
				}
			}
			else
			{
				filename = P -> str;
				if(P -> prev != NULL)
				{
					P = P -> prev;
					free(P -> next);
					P -> next = NULL;
				}
				else
				{
					free(filename); //задано перенаправление без команды
					free(P);
					P = NULL;
				}
			}
		}
		if(P != NULL) //если команда не пустая
		{
			P = gotoBeg(P);
			if(!strcmp(P -> str, "cd"))
			{
				if(P -> next != NULL) //если у cd есть параметры
				{
					if(!strcmp((P -> next) -> str, "~"))
						chdir(getenv("HOME"));
					else
						chdir((P -> next) -> str);
                    clean(&P);
                }
                else //иначе напечатать текущую директорию
                {
                    pid = fork();
                    if(pid == 0)
                    {
                        execlp("pwd", "pwd", NULL);
                        exit(1);
                    }
                    else if(pid > 0)
                    {
                        wait(NULL);
                        clean(&P);
                    }
                }
			}
			else if(!strcmp(P -> str, "exit"))
				break;
			else if(ISCODE(BACKGR) && !ISCODE(CONV)) //если задано выполнение одной программы в фоне
			{
				while(pipe(fd) == -1);
				bckgrNum++;
				pid = fork();
				if(pid < 0)
				{
					perror("Не удалось создать процесс");
					close(fd[0]);
                    close(fd[1]);
					clean(&P);
					bckgrNum--;
					exit(3);
				}
				if(pid)
				{
					read(fd[0], &tmp, sizeof(tmp)); //не очищать список (пока что)
					close(fd[0]);
                    close(fd[1]);
                    printf("[%d]  PID = %d\n", bckgrNum, pid);
                    clean(&P);
                }
				else
				{
					writeArgs(P, &argv); //аргументы из P переписаны в argv
					write(fd[1], &tmp, sizeof(tmp)); //продолжить род. процесс
					close(fd[0]);
                    close(fd[1]);
                    if(ISCODE(INPREDIR)) //задано перенаправление
					{
						inpfd = open(filename, O_RDONLY, 0666);
						free(filename);
						if(inpfd != -1)
						{
							dup2(inpfd, 0);
							close(inpfd);
						}
						else
							perror("Файл не существует, читаю со стандартного ввода");
					}
					execvp(*argv, argv);
					if(ISCODE(INPREDIR) && inpfd != -1)
						close(inpfd);
					for(i = 0; argv[i] != NULL; i++)
						free(argv[i]);
					free(argv);
					perror("Не удалось выполнить команду");
					exit(2);
				}
			}
			else if(ISCODE(CONV)) //конвейерное выполнение
			{
				char *s = "./ShellConv"; //программа получает имена программ и исполняет
										//их конвейером
				P -> prev = (PNode)malloc(sizeof(Node)); //запись в список имени вспом. программы
				(P -> prev) -> str = s;
				(P -> prev) -> next = P;
				P = P -> prev;
				P -> prev = NULL;
				if(ISCODE(BACKGR))
				{
					while(pipe(fd) == -1);
					bckgrNum++;
				}
				if((pid = fork()) < 0)
				{
					perror("Не удалось создать процесс");
					if(ISCODE(BACKGR))
					{
						close(fd[0]);
						close(fd[1]);
						bckgrNum--;
					}
					clean(&P);
					exit(4);
				}
				else if(pid > 0) //процесс-родитель
				{
					if(!ISCODE(BACKGR))
					{
						waitpid(pid, &status, 0);
						if(EXIT_ERROR(status))
							fprintf(stderr, "Конвейер завершен неуспешно\n");
						clean(&P);
					}
					else
					{
						read(fd[0], &tmp, sizeof(tmp)); //не очищать список (пока что)
						close(fd[0]);
						close(fd[1]);
						printf("[%d]  PID = %d\n", bckgrNum, pid);
						clean(&P);
					}
				}
				else //процесс-потомок запускает вспомогательную программу
				{
                    writeArgs(P, &argv);
					if(ISCODE(BACKGR))
					{
						write(fd[1], &tmp, sizeof(tmp)); //продолжить род. процесс
						close(fd[0]);
						close(fd[1]);
					}
					if(ISCODE(INPREDIR)) //задано перенаправление
					{
						inpfd = open(filename, O_RDONLY, 0666);
						free(filename);
						if(inpfd != -1)
						{
							dup2(inpfd, 0);
							close(inpfd);
						}
						else
							perror("Файл не существует, читаю со стандартного ввода");
					}
					execvp(*argv, argv);
					perror("Не удалось запустить конвейер");
					if(ISCODE(INPREDIR) && inpfd != -1)
						close(inpfd);
					if(ISCODE(BACKGR))
					{
						for(i = 0; argv[i] != NULL; i++)
							free(argv[i]);
						free(argv);
					}
					exit(5);
				}
			}
			else
			{
				writeArgs(P, &argv);
				pid = fork();
				if(pid < 0)
				{
					perror("Не удалось создать процесс");
					clean(&P);
					for(i = 0; argv[i] != NULL; i++)
						free(argv[i]);
					free(argv);
					exit(1);
				}
				if(pid)
				{
					wait(NULL);
					clean(&P);
					for(i = 0; argv[i] != NULL; i++)
						free(argv[i]);
					free(argv);
				}
				else
				{
					if(ISCODE(INPREDIR)) //задано перенаправление
					{
						inpfd = open(filename, O_RDONLY, 0666);
						free(filename);
						if(inpfd != -1)
						{
							dup2(inpfd, 0);
							close(inpfd);
						}
						else
							perror("Файл не существует, читаю со стандартного ввода");
					}
					execvp(*argv, argv);
					perror("Не удалось выполнить команду");
					if(ISCODE(INPREDIR) && inpfd != -1)
						close(inpfd);
					exit(2);
				}
			}
		}
	}
	if(code == ERRQ)
	{
		fprintf(stderr, "%s", "unexpected EOF while looking for matching '\"'\n");
		clean(&P);
		goto WrdInp;
	}
	else if(code == ERRAMP || code == ERRCONV)
	{
		char c = (code == ERRAMP)?'&':'|';
		fprintf(stderr, "syntax error: unexpected token '%c'\n", c);
		clean(&P);
		goto WrdInp;
	}
	return 0;
}
