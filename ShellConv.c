#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#define tmps "\""

int copyArgs(char ***p_argvDest, char **argvSrc, int argc)
{
	char **argvDest = *p_argvDest;
	int prDelims = 0;
	argvDest = (char **)malloc((argc + 1) * sizeof(char *));
	int i;
	for(i = 0; i < argc; i++)
		if(!strcmp(argvSrc[i], tmps))
		{
			prDelims++;
			argvDest[i] = NULL;
		}
		else
			argvDest[i] = argvSrc[i];
    argvDest[i] = NULL;
	*p_argvDest = argvDest;
	return prDelims;
}

void redir(FILE *f, int *fd) //перенаправляет ввод/вывод в нужный канал и закрывает другой
{
	if(f == stdin)
		dup2(fd[0], 0);
	else if(f == stdout)
		dup2(fd[1], 1);
	else
		perror("Не могу перенаправить");
	close(fd[0]);
	close(fd[1]);
}

void killall(int *pidMas, int pidNum)
{
    int i;
    for(i = 0; i < pidNum; i++)
    {
        kill(pidMas[i], SIGKILL);
        waitpid(pidMas[i], NULL, 0);
    }
}

void closeall(int **pipeMas, int pipeNum)
{
	int i;
	for(i = 0; i < pipeNum; i++)
	{
		close(*(pipeMas[i] + 0));
		close(*(pipeMas[i] + 1));
	}
}

//ошибка (выход по exit с кодом не 0)
#define EXIT_ERROR( status ) (!((WIFEXITED((status)) && !WEXITSTATUS((status))) || !WIFEXITED((status))))

int main(int argc, char **argv) //принимает на вход имена программ (pr1, pr2, ...)
								//с разделителями и выполняет pr1|pr2|... .
{
	int **pipeMas, *pidMas;
	int pid, pidNum = 0, i, j, pipeNum = 0, prNum = 1, status, exitcode = 0;
	char **argv2;
	int prDelims = copyArgs(&argv2, argv, argc);
	if(argc == 1) //нет программ для выполнения
		exit(1);
	else
	{
		pipeMas = (int **)malloc(prDelims * sizeof(int *)); //массив каналов между соседними программами
		for(i = 0; i < prDelims; i++)
			pipeMas[i] = (int *)malloc(2 * sizeof(int));
		if(pipe(pipeMas[pipeNum++]) < 0) //не создался первый канал
			exit(2);
		if((pid = fork()) < 0) //не запустился первый процесс
			exit(3);
		else if(pid == 0)
		{
			redir(stdout, pipeMas[0]); //перенаправить станд. вывод
			execvp(argv2[prNum], argv2 + prNum);
			close(1);
			exit(4);
		}
		else
		{
			pidMas = (int *)malloc((prDelims + 1) * sizeof(int));
			pidMas[pidNum++] = pid;
			waitpid(pid, &status, WNOHANG);
            if(EXIT_ERROR(status))
            {
                closeall(pipeMas, pipeNum);
                killall(pidMas, pidNum);
                fprintf(stderr, "Процесс %s завершен с ошибкой\n", argv2[prNum]);
                exit(5);
            }
			while(argv2[prNum++] != NULL); //переход к следующей программе
			for(j = 1; j <= prDelims - 1; j++)
			{
				if(pipe(pipeMas[pipeNum++]) < 0) //создать новый канал
				{
					closeall(pipeMas, pipeNum - 1);
					killall(pidMas, pidNum);
					perror("Не могу создать канал");
					exit(2);
				}
				if((pid = fork()) < 0)
				{
					closeall(pipeMas, pipeNum); //закрыть все каналы
					killall(pidMas, pidNum); //завершить все предыдущие процессы
					perror("Не могу запустить процесс по fork()");
					exit(4);
				}
				else if(pid == 0) //процесс-потомок
				{
					redir(stdin, pipeMas[pipeNum - 2]); //перенаправить ввод на fd[0] старого канала
					redir(stdout, pipeMas[pipeNum - 1]); //вывод на fd[1] нового канала
					closeall(pipeMas, pipeNum - 2); //закрыть все остальные каналы в потомке
					execvp(argv2[prNum], argv2 + prNum);
					close(1);
					close(0);
					exit(5);
				}
				else //процесс-родитель
				{
					waitpid(pid, &status, WNOHANG);
                    if(EXIT_ERROR(status))
                    {
                        closeall(pipeMas, pipeNum);
                        killall(pidMas, pidNum);
                        fprintf(stderr, "Процесс %s завершен с ошибкой\n", argv2[prNum]);
                        exit(5);
                    }
					while(argv2[prNum++] != NULL); //переход к следующей программе
					pidMas[pidNum++] = pid; //включить в таблицу новый pid
				}
			}
			if((pid = fork()) < 0) //последний процесс не запустился
			{
				closeall(pipeMas, pipeNum);
				killall(pidMas, pidNum);
				fprintf(stderr, "Не могу запустить процесс %s\n", argv2[prNum]);
				exit(5);
			}
			else if(pid == 0) //процесс-потомок
			{
				redir(stdin, pipeMas[pipeNum - 1]); //перенаправить ввод
				closeall(pipeMas, pipeNum - 1); //закрыть все остальные каналы
				execvp(argv2[prNum], argv2 + prNum);
				close(0);
				exit(5);
			}
			else //процесс-родитель
			{
                waitpid(pid, &status, WNOHANG);
                if(EXIT_ERROR(status))
                {
                    closeall(pipeMas, pipeNum);
                    killall(pidMas, pidNum);
                    fprintf(stderr, "Процесс %s завершен с ошибкой\n", argv2[prNum]);
                    exit(5);
                }
				pidMas[pidNum++] = pid;
            }
		}
		for(i = 0; i < pidNum; i++)
		{
			pid = waitpid(pidMas[i], &status, 0);
			exitcode |= WEXITSTATUS(status);
			if(i != pidNum - 1)
			{
                close(*(pipeMas[i] + 0));
                close(*(pipeMas[i] + 1));
            }
			//printf("Conv[%d]+  Done  PID = %d  Code = %d\n", i, pidMas[i], WEXITSTATUS(status));
		}
		free(argv2);
		for(i = 0; i < prDelims; i++)
			free(pipeMas[i]);
        free(pipeMas);
        free(pidMas);
	}
	return exitcode;
}
