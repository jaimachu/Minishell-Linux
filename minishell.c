#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "parser.h"

//Cabeceras de las funciones
void manejador(int sig);
void manejadorProcesos(int sig, tline *line);
void recursion(tline *line, int pos);
void comprobarProceso();

//Declaración de variables
typedef struct{
        int posicion;
        int pid;
        char *estado;
        tline *line;
} tJobs;
tJobs *jobs; //Lista de procesos en segundo plano
tline *mandatoAnt; //Mandato que se ejecuta para guardarlo en el jobs
int tamaño = 0; //Posicion que ocupa el mandato
int pidGlobal;
tline *lineGlobal;
int esForeground = 0;

int main(void) {
        int EntradaAux = dup(STDIN_FILENO);
        int SalidaAux = dup(STDOUT_FILENO);
        int ErrorAux = dup(STDERR_FILENO);

        pidGlobal = getpid();
        char buf[1024];
        int mascara = 0022; //Máscara por defecto
        tline * line;
        jobs = calloc(10, sizeof(tJobs)); //La lista del jobs tendrá 10 posiciones como máximo
        int i;
        int pid;
        int status;
        signal(SIGINT, manejador);
        printf("msg> "); 
        while (fgets(buf, 1024, stdin)) {
                line = tokenize(buf);
                lineGlobal = line;
                //manejador(SIGINT);
                if (line==NULL) {
                        continue;
                }
                else if (line->redirect_input != NULL) {
                        char *nombreFichero = line->redirect_input;
                        int fichero = open(nombreFichero,O_RDONLY); //Solo lee
                        dup2(fichero, STDIN_FILENO); //Redirigo  la entrada estandar
                }
                else if (line->redirect_output != NULL) {
                        char *nombreFichero = line->redirect_output;
                        int fichero = open(nombreFichero, O_WRONLY | O_CREAT | O_TRUNC); //Solo escribe o si no está creado se crea el fichero. Se sobreescribe
                        dup2(fichero, STDOUT_FILENO); //Redirigo la salida estandar
                }
                else if (line->redirect_error != NULL) {
                        char *nombreFichero = line->redirect_error;
                        int fichero = open(nombreFichero, O_WRONLY | O_CREAT); //Solo escribe o si no está creado se crea el fichero
                        dup2(fichero, STDERR_FILENO); //Redirigo la salida de error
                        fprintf(stderr, "%s: No se encuentra el mandato\n", line->commands[0].argv[0]);
                }
                else if (line->background) {
                        int pid;
                        pid = fork();
                        if (pid < 0){ //Error
                                fprintf(stderr, "Falló el fork()\n");
                                exit(2);
                        }
                        else if (pid == 0){ //Proceso hijo
                                pidGlobal = pid;
                                execvp(line->commands[0].argv[0], line->commands[0].argv);
                        }
                        else{
                                jobs[tamaño].posicion = tamaño;
                                jobs[tamaño].line = line;
                                jobs[tamaño].pid = pid;
                                jobs[tamaño].estado = "Running";
                                printf("[%d]    %d\n", jobs[tamaño].posicion, jobs[tamaño].pid);
                                tamaño = tamaño + 1;
                                waitpid(-1, NULL, WNOHANG); //Wait no bloqueante. El proceso se queda como zombie
                        }
                }
                for (i=0; i<line->ncommands; i++) {
                        char *mandato = line->commands[i].argv[0];
                        char *ruta = line->commands[i].filename;
                        if (line->background){
                                break;
                        }
                        if ((ruta == NULL) && (line->ncommands < 2)){
                                if (strcmp(mandato,"exit") == 0){
                                        for (int ini = 0; ini < tamaño; ini++){
                                                if (jobs[ini].estado != NULL){
                                                        printf("[%d]    done\n", ini);
                                                        kill(jobs[ini].pid, SIGKILL);
                                                }
                                        }
                                        exit(0);
                                }
                                else if (strcmp(line->commands[i].argv[0], "cd") == 0){
                                        char *directorio;
                                        if (line->commands[i].argc > 2){
                                                fprintf(stderr,"Uso: %s directorio\n",line->commands[i].argv[0]);
                                        }
                                        else if (line->commands[i].argc == 1){
                                                directorio = getenv("HOME");
                                                if (directorio == NULL){
                                                        fprintf(stderr,"No existe la variable $HOME\n"); 
                                                }
                                        }
                                        else{
                                                directorio = line->commands[i].argv[1];
                                        }
                                        printf("El directorio actual es: %s\n", directorio);
                                        chdir(directorio);
                                }
                                else if (strcmp(line->commands[i].argv[0], "umask") == 0){
                                        if (line->commands[i].argc == 1){
                                                printf("%04o\n", mascara); // Introducir por la izquierda los 0s que falten
                                        }
                                        else if (line->commands[i].argc == 2){
                                                char *numero = line->commands[i].argv[1];
                                                if (strlen(numero) != 4){
                                                        fprintf(stderr,"ERROR: El número de dígitos de la máscara debe de ser de 4\n");
                                                }
                                                else{
                                                        int n = atoi(line->commands[i].argv[1]);
                                                        scanf("%o", &n);
                                                        mascara = n;
                                                        umask(mascara);
                                                }
                                        }
                                        else{
                                                fprintf(stderr,"ERROR: Numero de parametros incorrecto\n");
                                        }
                                }
                                else if(strcmp(line->commands[i].argv[0], "jobs") == 0){
                                        int z;
                                        for(z = 0; z < tamaño; z++){
                                                if (jobs[z].estado != NULL){
                                                        char *argv1 = jobs[z].line->commands[i].argv[0];
                                                        printf("[%d] %s         %s\n", jobs[z].posicion, jobs[z].estado, argv1);
                                                }
                                        }
                                }
                                else if (strcmp(line->commands[i].argv[0], "fg") == 0){
                                        if(line->commands[i].argc == 2){
                                                int numero = atoi(line->commands[i].argv[1]);
                                                int ini = 0;
                                                for (ini = 0; ini < tamaño; ini++){
                                                        if (ini == numero){
                                                                int pid = jobs[ini].pid;
                                                                esForeground = 1;
                                                                pidGlobal = getpid();
                                                                manejador(SIGINT);
                                                                if (jobs[ini].pid == waitpid(pid, NULL, WUNTRACED | WCONTINUED)){
                                                                        jobs[ini].estado = NULL;
                                                                }
                                                        }
                                                }
                                        }
                                        else{
                                                fprintf(stderr,"Error al introducir el fg\n");
                                        }
                                }
                                else{
                                        printf("%s: No se encuentra el mandato\n",line->commands[i].argv[0]);
                                }
                        }
                        else if (line->ncommands < 2){
                                pid = fork();
                                if (pid < 0){ //Error
                                        fprintf(stderr, "Falló el fork()\n");
                                        exit(2);
                                }
                                else if (pid == 0){ //Proceso hijo
                                        pidGlobal = pid;
                                        signal(SIGINT, manejador);
                                        execvp(mandato,line->commands[i].argv);
                                        printf("Error al ejecutar el mandato; %s\n", mandato);
                                        exit(1); 
                                }
                                else{ //Proceso padre
                                        wait(&status);
                                        dup2(EntradaAux, STDIN_FILENO);
                                        dup2(SalidaAux, STDOUT_FILENO);
                                        dup2(ErrorAux, STDERR_FILENO);
                                }
                        }
                        else{
                                recursion(line, 0);
                                dup2(EntradaAux,STDIN_FILENO );
                                dup2(SalidaAux,STDOUT_FILENO );
                        }
                }
                comprobarProceso();
                printf("msg> ");
        }
        free(jobs);
        return 0;
}
void comprobarProceso(){
        int ini;
        for (ini = 0; ini < tamaño; ini++){
                if (waitpid(jobs[ini].pid, NULL, WNOHANG) == jobs[ini].pid){
                        printf("[%d]    done    \n", ini);
                        jobs[ini].estado = NULL;
                }
        }
}
void recursion(tline *line, int pos){
        tline *copia = line;
        int pid;
        int fd[2];
        pipe(fd);
        if (copia->ncommands == 1){ //CASO BASE
                pid = fork();
                if (pid < 0){ //Error
                       fprintf(stderr, "Falló el fork()\n");
                       exit(2);
                }
                else if (pid == 0){
                        execvp(copia->commands[pos].argv[0], copia->commands[pos].argv);
                }
                else{
                        wait(NULL);
                }
        }
        else{ //CASO RECURSIVO
                pid = fork();
                if (pid < 0){ //Error
                        fprintf(stderr, "Falló el fork()\n");
                        exit(2);
                }
                else if (pid == 0){
                        close(fd[0]);
                        dup2(fd[1], STDOUT_FILENO);
                        execvp(copia->commands[pos].argv[0], copia->commands[pos].argv);
                        close(fd[1]);
                }else{
                        close(fd[1]);
                        dup2(fd[0], STDIN_FILENO);
                        copia->ncommands = copia->ncommands - 1; //Para cada llamada recursiva se decrementa
                        recursion(copia, pos+1); //La posicion del mandato a ejecutar se aumenta
                        close(fd[0]);
                        wait(NULL);
                }
    }

}

void manejador(int sig){
        if (esForeground == 1){
                kill(pidGlobal, sig);
        }
        else if ((pidGlobal == 0) && (lineGlobal->background == 0)){ //Estamos en el proceso hijo
                kill(pidGlobal,sig);
        }
        else{
                signal(SIGINT, SIG_IGN);
        }
}




