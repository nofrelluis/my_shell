/*Autores:
    Nofre Lluis Bibiloni Clar
    Aina Galiana Rodriguez
    Michael Alexander Garcia Moreira
*/

#define USE_READLINE
#define _POSIX_C_SOURCE 200112L
#define COMMAND_LINE_SIZE 1024
#define ARGS_SIZE 64
#define N_JOBS 10
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
#include <inttypes.h> //new
#include <sys/file.h> //new
#include <unistd.h>
#include <stdlib.h> //new
#include <errno.h> //new
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/types.h> 
#include <sys/stat.h> 
#include <fcntl.h> 
#ifdef USE_READLINE
#include <readline/readline.h>
#include <readline/history.h>
#endif

int execute_line(char *line);
int n_pids = 0; //suponemos no cuenta el foreground;
char *prompt[3];
char *my_shell = "./my_shell";

struct info_process {
    pid_t pid;
    char status; // ’E’(Ejecutando), ‘D’(Detenido), ‘F’(Finalizado)
    char *command_line; // Guarda el comando
};

static struct info_process *jobs_list[N_JOBS]; //Irá guardando los distintos procesos

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
			ANSI_COLOR_RESET "%s",prompt[0],prompt[1],prompt[2]);//imprime con codigo de colores
    fflush(stdout);
}

//Método para leer la instrucción que se escriba en la consola y la assigna a line
char *read_line(char *line){
    #ifdef USE_READLINE 
		/*char *prompt[3];
		obtener_prompt(prompt);*/
		imprimir_prompt();
        static char *line_read = (char *)NULL; 
        /*line_read = readline (prompt);
		ptr = readline (prompt);*/
        if (line_read){ 
			free(line_read);
			line_read = (char *)NULL;
        }
		line_read = readline(" "); //Leer linia
		if (line_read && *line_read){
			add_history(line_read); //Si hay instrucción se añade al historial
		}
		strcpy(line, line_read);
        return line;
    #else
		char *ptr = malloc(COMMAND_LINE_SIZE);
        imprimir_prompt();
        ptr = fgets(line, COMMAND_LINE_SIZE, stdin); // leer linea
        if (!ptr && !feof(stdin)) {
            // ptr==0 && feof(stdin)==0
            // si no se incluye la 2ª condición no se consigue salir del shell con Ctrl+D
            ptr = line; // Si se omite, al pulsar Ctrl+C produce "Violación de segmento (`core' generado)"
            ptr[0] = 0; // Si se omite esta línea aparece error ejecución ": no se encontró la orden"
        }
		return ptr;
    #endif
}

/*Separa la instrucción en tokens para poder tratar las partes
 de la instrucción por separado, los separadores son el espacio,
 el salto de linea y las tabulaciones, el resultado se guarda en
 el array args[] */ 
int parse_args(char **args, char *line){
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

/* Sirve para entrar y salir de directorios, funciona de la misma
forma que en el shell*/
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
    /*char cwd[ARGS_SIZE]; 
    printf("[%s\n]",getcwd(cwd, ARGS_SIZE));*/
	return 0;
}

//Sirve para ejecutar comandos que se encuentren en archivos de texto
int internal_source(char **args){
    char linea[ARGS_SIZE];
    FILE *fitxer = fopen(args[1],"r"); //Coje dirección del fichero
    if (fitxer != NULL){ //Bucle para leer las instrucciones
        while(fgets(linea,ARGS_SIZE,fitxer) != NULL) {
			execute_line(linea); //Ejecuta la instrucción actual
            fflush(fitxer); // Fuerza vaciado del buffer.
        }
        fclose(fitxer);
    } else {
        perror("Atención error");
    } 
	return 0; 
}

/*Sirve para cambiar el nombre de las variables de Sistema. El formato
de la instrucción es identico a la original de shell*/
int internal_export(char **args){
    char *nomVal[2]; 
    int e = 0;
    nomVal[0] = strtok(args[1],"="); 
    nomVal[1] = strtok(NULL, " \n\t\r"); //Se separan los dos nombres y se assignan a nomVal[]  
    if ((nomVal[1]!=NULL)&&(nomVal[0] != NULL)){ //Comprueba Sintaxis correcta
        //printf( "[%s\n]",getenv(nomVal[0]));
        e = setenv(nomVal[0],nomVal[1],1);
        if (e ==-1){
            perror("Error de sintaxis");
        } else {
            //fprintf(stdout, "[%s\n]",getenv(nomVal[0]));
        }
    } else {
        printf("Error de sintaxis: Faltan argumentos \n");
    }
   	return 0;
}

//Muestra por consola los procesos actuales.
int internal_jobs(){
    int cont =1;
    while (jobs_list[cont] != 0){ //Mientras haya procesos
		if (jobs_list[cont] ->pid != 0){ // Y no sean nulos, se imprimen
			printf("[%d] PID:%d STATUS:%c COMMAND_LINE:%s\n",cont,jobs_list[cont] ->pid,
            	jobs_list[cont] ->status, jobs_list[cont] ->command_line);
		}
        cont++;
    }
    return 0;
}

//Para el minishell
int internal_exit(){
	exit(0);
	return 0;
}

/*Comprueba que el comando sea de minishell y ejecuta la instrucción correspondiente
si la instrucción es del minishell retorna 1 si no retorna 0*/
int check_internal(char **args){
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
        internal_exit();
    } else {
        internal =0;
    }
    return internal;
}

//Busca un pid especifico en la lista de procesos, devuelve el numero del proceso en la lista
int jobs_list_find(pid_t pid){
    int cont = 1; 
    while ((cont < n_pids) && (jobs_list[cont] ->pid != pid)) {
        cont++;
    }
    return cont;
}

/*Borra el proceso en la posición indicada por parametro y sustituye el último proceso de la lista
por el proceso de la posición que se quiere borrar*/
int  jobs_list_remove(int pos){
    jobs_list[pos] ->pid = jobs_list[n_pids] ->pid;
	jobs_list[pos] ->status = jobs_list[n_pids] ->status;
	strcpy(jobs_list[pos] ->command_line, jobs_list[n_pids] ->command_line); //Cambia los procesos
	//Se borra el proceso
	jobs_list[n_pids] ->pid = 0;
    jobs_list[n_pids] ->status = 'F';
    jobs_list[n_pids] ->command_line = NULL;
    n_pids--; //Decrementa numero de procesos activos
    return 0;
}

/*Añade un proceso al final de la lista, si no se pueden añadir
más procesos se emite un mensaje de error y retorna -1*/
int jobs_list_add(pid_t pid, char status, char *command_line){
	if (n_pids < N_JOBS){
		n_pids++;
		jobs_list[n_pids]=malloc(sizeof(jobs_list));
  		jobs_list[n_pids] ->pid = pid;
    	jobs_list[n_pids] ->status = status;
    	jobs_list[n_pids] ->command_line = command_line;
		fprintf(stdout,"[%d]    Pid: %d Status: %c Command_line: %s\n",
                        n_pids,
                        jobs_list[n_pids] ->pid,
                        jobs_list[n_pids] ->status,
                        jobs_list[n_pids] ->command_line);
		return 0;
	} else {
		fprintf(stderr,"No se pueden añadir más de %d procesos\n",N_JOBS);
		return -1;
	}
}

//Entierra los procesos que terminan
void reaper(int signum){
    signal(SIGCHLD,reaper); //Permite que se llame a reaper() al finalizar un proceso
	pid_t pidchld;
    while ((pidchld = waitpid(-1,&signum,WNOHANG))>0){ //Espera a que un proceso termine y pone el pid en pidchld
    if (jobs_list[0] ->pid == pidchld){ //SI pidchld es el primero resetea valores          
        jobs_list[0] ->pid = 0;
        jobs_list[0] ->status = 'F';
        jobs_list[0] ->command_line = NULL;
    }else if((pidchld != 0)&&(jobs_list[1] != 0)){ // Si no es el primero y no es primer proceso en bakcground
        int pchild= jobs_list_find(pidchld); //Busca el proceso que ha terminado
        fprintf(stdout,"Ha terminado el proceso: [%d]   Pid: %d Status: %c Command_line: %s\n",
                        pchild,
                        jobs_list[pchild] ->pid,
                        jobs_list[pchild] ->status,
                        jobs_list[pchild] ->command_line); //Lo imprime por pantalla
        jobs_list_remove(pchild); //Se borra
    }else if(pidchld == 0){ //pidchid no puede ser el proceso en foreground
		perror("Atención error");
    }
	}
}

//Mata al proceso en foreground
void ctrlc(int signum){
    signal(SIGINT,ctrlc); //Permite que se llame a ctrlc() al pulsar ctrl+C
    if (jobs_list[0] ->pid > 0){ //Si el proceso no es nulo 
        if(jobs_list[0] -> command_line != my_shell){ //Si el proceso no es miniShell
            kill(jobs_list[0] ->pid,SIGTERM); //Se mata al proceso
        } else {
            printf("No se puede enviar Ctrl+C al minishell\n");
			imprimir_prompt();
        }
    } else {
        printf("No hay proceso en foreground\n");
		imprimir_prompt();
    }
}

//Detiene el proceso en foreground
void ctrlz(int signum){
    signal(SIGTSTP,ctrlz); //Permite que se llame a ctrlz() al pulsar ctrl+Z
    if (jobs_list[0] ->pid > 0){ //Si el pid no es nulo
         if(jobs_list[0] -> command_line != my_shell){ //Si el proceso no es miniShell
            kill(jobs_list[0] ->pid,SIGTSTP);
            jobs_list[0] ->status = 'D'; //Se detiene el proceso
            jobs_list_add(jobs_list[0] ->pid, 
                jobs_list[0] ->status,
                jobs_list[0] ->command_line); //Se crea y seañade el proceso a la lista
            jobs_list[0] ->pid = 0;
            jobs_list[0] ->status = 'F';
            jobs_list[0] ->command_line = NULL; //Se borra el proceso anterior 
        } else {
            perror("Señal SIGTSTP, no enviada debido a proceso es minishell\n");
			imprimir_prompt();
        }
    } else {
        perror("Señal SIGTSTP, no enviada debido a que no hay proceso en foreground\n");
		imprimir_prompt();
    }
}

/*Detecta si el comando tiene un &, indicando que se quiere ejecutar
 la instrucción en background*/
int is_background(char **args){
    int cont =0;
    while(args[cont] != NULL){
        if(*args[cont] == '&'){
            args[cont] = NULL;
            return 0;
        }
        cont++;
    }
    return 1;
} 

/*Mira si el comando tiene un >, que indica que el resultado del comando se
escriba en el fichero que se escribaa después de >, si no existe se crea*/
int is_output_redirection (char **args){
    int cont =0;
    while(args[cont] != NULL){ 
        if(*args[cont] == '>'){ //Detecta el signo >
            args[cont] = NULL;
            int file = open(args[cont+1],  O_WRONLY|O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR);
            if(file != -1){ //No hay error
                dup2 (file, 1);
                close (file);
            } else{ //Ha ocurrido un error
                perror("Atención error");
            }
            return 1;
        }
        cont++;
    }
    return 0;
}

/*Detecta si el comando tiene un #, indicando que se quiere comentar
 el resto de texto que venga despues de #*/
int is_comment(char **args) {
	  int cont =0;
    while(args[cont] != NULL){ 
        if(args[cont][0] == '#'){
            args[cont] = NULL;
		}    
        cont++;
    }
    return 0;
}

/*Ejecuta la instrucción que se pasa por parametro y se decide si el
proceso es hijo o es padre, o si se trata de un error*/ 
int execute_line(char *line){
    char *args[ARGS_SIZE];
	char *com_lin = malloc(COMMAND_LINE_SIZE);
	strcpy(com_lin, line);
    //int cont = parse_args(args,line);
	parse_args(args,line);
	is_comment(args);
    int is_back = is_background(args);	
    if ((check_internal(args)) == 0){
        pid_t pid;
        pid = fork(); //Crea proceso hijo, devuelve 0 si es hijo, devuelve el pid si es Padre
        if (pid == 0) { //Es HIJO
            signal(SIGINT, SIG_IGN);
            if (is_back == 0){
				//signal(SIGINT, NULL); //Si es hijo ignora ctrl+C
                signal(SIGTSTP,NULL); //Si es hijo en background ignora ctrl+Z
            } else {
                signal(SIGTSTP, ctrlz);
				//signal(SIGINT, ctrlc);
            }
            signal(SIGCHLD, SIG_DFL);
            is_output_redirection(args);
            execvp(args[0],args); //Ejecuta la instrucción
            perror("Atención error");
            exit(0);  
        } else if (pid > 0) { //Es PADRE
            char status = 'E';
            if (is_back == 0){  //El proceso hijo se ejecuta en background
				signal(SIGINT,NULL);
                jobs_list_add(pid, status, com_lin);
            }else if(is_back == 1){ //El proceso hijo no se ejecuta en background
				signal(SIGINT,ctrlc);
                jobs_list[0] ->pid = pid;
                jobs_list[0] ->status = status;
                jobs_list[0] ->command_line = com_lin;
                while (jobs_list[0] ->pid != 0){
                    pause();
                }
            }
        } else if (pid < 0) { //Es ERROR
            perror("Atención error");
            exit(3);
        } 
    }
    return 0;
}

int main() {
    char *line = malloc(COMMAND_LINE_SIZE);
    signal(SIGINT,ctrlc);
    signal(SIGTSTP,ctrlz);
    signal(SIGCHLD,reaper); //Habilitación de señales 
	//Evita que si el primer proceso es background el programa termine
	jobs_list[0] =malloc(sizeof(struct info_process));
    jobs_list[0] ->pid=0;
    while (read_line(line)) {
        execute_line(line);
    }
    return 0;
}