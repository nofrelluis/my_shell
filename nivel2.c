#define _POSIX_C_SOURCE 200112L
#define COMMAND_LINE_SIZE 1024
#define ARGS_SIZE 64
#define PROMPT "€"
#define CD "cd"
#define EXPORT "export"
#define SOURCE "source"
#define JOBS "jobs"
#define EXIT "exit"
#define ANSI_COLOR_CYAN "\x1b[36m"
#define ANSI_COLOR_GREEN "\x1b[32m"
#define ANSI_COLOR_RESET "\x1b[0m"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

char *prompt[3];

//Recopila los datos necessarios para el prompt y los assigna a prompt[].
char **obtener_prompt(char **prompt){
	char cwd[COMMAND_LINE_SIZE];
    prompt[0] = getenv("USER");
	prompt[1] = getcwd(cwd, COMMAND_LINE_SIZE);
	prompt[2] = PROMPT;
    return prompt;
}

//Crea el prompt que se imprimirá por pantalla, de la forma USER:Directorio actualPROMPT
void imprimir_prompt(){
	obtener_prompt(prompt);
	fprintf(stdout,ANSI_COLOR_CYAN "%s:" ANSI_COLOR_GREEN "%s"
			ANSI_COLOR_RESET "%s ",prompt[0],prompt[1],prompt[2]);//imprime con codigo de colores
    fflush(stdout);
}

char *read_line(char *line){
    printf("Llego a read_line");
    imprimir_prompt();
    char *ptr = malloc(COMMAND_LINE_SIZE);
    ptr = fgets(line, COMMAND_LINE_SIZE, stdin);
    return ptr;
}

int parse_args(char **args, char *line){
    printf("Llego a parse_args");
    const char specialchar[] = " \n\r\t";
    char *token;
    int cont = 0;
    token = strtok(line, specialchar);
    while (token != NULL) {
        args[cont]= token;  
        token = strtok(NULL, specialchar);
        cont++;
    }
    args[cont] = NULL;
    return cont;
}

int internal_cd(char **args){
     int e = 0;
    if (args[1]==NULL){
        //fprintf(stdout, "%s\n",getenv("HOME"));
        chdir(getenv("HOME"));
    } else {
        e = chdir(args[1]);
        if (e == -1){
            perror("Atención error");
        }
    }
    char cwd[ARGS_SIZE]; 
    printf("[%s\n]",getcwd(cwd, ARGS_SIZE));
    return 0;
}

int internal_export(char **args){
    char *nomVal[2]; 
    int e = 0;
    nomVal[0] = strtok(args[1],"="); 
    nomVal[1] = strtok(NULL, " \n\t\r"); //Se separan los dos nombres y se assignan a nomVal[]  
    if ((nomVal[1]!=NULL)&&(nomVal[0] != NULL)){ //Comprueba Sintaxis correcta
        printf( "[%s\n]",getenv(nomVal[0]));
        e = setenv(nomVal[0],nomVal[1],1);
        if (e ==-1){
            perror("Error de sintaxis");
        } else {
            fprintf(stdout, "[%s\n]",getenv(nomVal[0]));
        }
    } else {
        printf("Error de sintaxis: Faltan argumentos \n");
    }
    return 0;
}

int internal_source(char **args){
    return 0;
}

int internal_jobs(){
    return 0;
}

int check_internal(char **args){
    printf("Llego a check_internal");
    int internal =1;
    if (strcmp(args[0],CD)==0){
        internal_cd(args);
    } else if (strcmp(args[0],SOURCE)==0){
        internal_source(args);
    } else if (strcmp(args[0],EXPORT)==0){
        internal_export(args);
    } else if (strcmp(args[0],JOBS)==0){
        internal_jobs();
    } else if (strcmp(args[0],EXIT)==0){
        exit(0);
    } else {
        internal =0;
    }
    return internal;
}

int execute_line(char *line){
    printf("Llego a execute line\n");
    char *args[ARGS_SIZE];
    parse_args(args,line);
    if(check_internal(args)==0){
        printf("El comando no es un comando interno");
    }
    return 0;
}

int main () {
    char *line = malloc(COMMAND_LINE_SIZE);
  printf("PWD : %s\n", getenv("PWD"));
  printf("HOME : %s\n", getenv("HOME"));
  printf("LANGUAGE : %s\n", getenv("LANGUAGE"));
  printf("USER : %s\n", getenv("USER"));
  while (read_line(line)) {
        printf("Llego a execute_line en main\n");
        execute_line(line);
    }
    return 0;
}
