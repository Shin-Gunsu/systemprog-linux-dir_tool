//2020136151 신건수
#include <signal.h>
#include<time.h>
#include<stdio.h>
#include<dirent.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include<sys/stat.h>
#include<sys/types.h>
#include<pwd.h>
#include<grp.h>
#define MAX_CMD_SIZE 128
#define MAX_COM_ARG_SIZE 1024
void readProcessInfo(const char *pid);
static char *chroot_path = "/tmp/test";
static char *current_dir;
extern char **environ;
static void get_realpath(char *usr_path, char *result)
{
    char *stack[32];
    int   index = 0;
    char  fullpath[128];
    char *tok;
    int   i;
#define PATH_TOKEN   "/"

    if (usr_path[0] == '/') {
        strncpy(fullpath, usr_path, sizeof(fullpath)-1);
    } else {
        snprintf(fullpath, sizeof(fullpath)-1, "%s/%s", current_dir + strlen(chroot_path), usr_path);
    }

    /* parsing */
    tok = strtok(fullpath, PATH_TOKEN);
    if (tok == NULL) {
        goto out;
    }

    do {
        if (strcmp(tok, ".") == 0 || strcmp(tok, "") == 0) {
            ; // skip
        } else if (strcmp(tok, "..") == 0) {
            if (index > 0) index--;
        } else {
            stack[index++] = tok;
        }
    } while ((tok = strtok(NULL, PATH_TOKEN)) && (index < 32));

out:
    strcpy(result, chroot_path);

    // TODO: boundary check
    for (i = 0; i < index; i++) {
        strcat(result, "/");
        strcat(result, stack[i]);
    }
}
//디렉토리 구조체의 d_type을 읽어 파일의 속성타입별로 출력
const char *get_file_type(unsigned char d_type){
    switch (d_type) {
        case DT_REG:
            return "File";
        case DT_DIR:
            return "Directory";
        case DT_FIFO:
            return "Named Pipe";
       case DT_SOCK:
            return "Socket";
        case DT_CHR:
            return "Character Device";
        case DT_BLK:
            return "Block Device";
        case DT_LNK:
            return "Symbolic Link";
        default:
            return "Other";
    }
}
void sig_handler(int signo){
	printf("\nIgnore Ctrl+C\n");
}
int main(int argc, char **argv){
	char *command, *tok_str;

	struct stat file_info;
	struct passwd *pw;
	struct group *grp;
	struct dirent *entry;	//디렉토리 엔트리 구조체 
	DIR *dir;
	char* com_arg[MAX_COM_ARG_SIZE];	//명령어 뒤의 인자를 저장.

	int comd_num=0;			//명령어 넘버, cd=1, mkdir=2, rmdir=3, rename=4, ls=5 이 외의 명령어를 입력하면 help.
	int idx=0;				//명령어 뒤의 인자가 몇개인지 카운트. 명령어가 받을 수 있는 인자를 초과하면 Too many arguments 출력
	pw=getpwuid(getuid());
	grp = getgrgid(getgid());

	command = (char*)malloc(MAX_CMD_SIZE);
	if (command == NULL) {
		perror("malloc");
		exit(1);
	}
    current_dir = (char*) malloc(MAX_CMD_SIZE);
    if (current_dir == NULL) {
        perror("current_dir malloc");
        free(command);
        exit(1);
    }

    if (chdir(chroot_path) < 0) {
        mkdir(chroot_path, 0755);
        chdir(chroot_path);
    }
	void(*hand)(int);
	hand=signal(SIGINT,sig_handler);
	if(hand==SIG_ERR){
		perror("signal");
		exit(1);
	}

	do{
	
		if (getcwd(current_dir, MAX_CMD_SIZE) == NULL) {
            perror("getcwd");
            break;
        }

        if (strlen(current_dir) == strlen(chroot_path)) {
            printf("/"); // for root path
        }
        printf("%s $ ", current_dir + strlen(chroot_path));
		if(fgets(command,MAX_CMD_SIZE-1,stdin)==NULL)break;	//사용자로부터 입력받음

		tok_str=strtok(command," \n");		//문자열을 나눈다.
		if(tok_str==NULL)continue;			//사용자가 아무것도 입력하지 않고 Enter를 입력했을시 Segment fault오류가 발생하여 tok_str==NULL이라면 반복문 시작점으로 돌아와 다시 입력받음.
		if(strcmp(tok_str,"quit")==0){		//입력받은 문자가 quit일시 반복문 종료
			break;
		}else{
			printf("your command:%s\n",tok_str);	//입력받은 명령어 출력
			printf("and arguments is ");			
			if(strcmp(tok_str,"cd")==0){			//각 명령어에 따라 고유넘버 부여
				comd_num=1;
			}else if(strcmp(tok_str,"mkdir")==0){
				comd_num=2;
			}else if(strcmp(tok_str,"rmdir")==0){
				comd_num=3;
			}else if(strcmp(tok_str,"rename")==0){
				comd_num=4;
			}else if(strcmp(tok_str,"ls")==0){
				comd_num=5;
			}else if(strcmp(tok_str,"ln")==0){
				comd_num=6;
			}else if(strcmp(tok_str,"rm")==0){
				comd_num=7;
			}else if(strcmp(tok_str,"chmod")==0){
				comd_num=8;
			}else if(strcmp(tok_str,"cat")==0){
				comd_num=9;
			}else if(strcmp(tok_str,"cp")==0){
				comd_num=10;
			}else if(strcmp(tok_str,"ps")==0){
				comd_num=11;
			}else if(strcmp(tok_str,"env")==0){
				comd_num=12;
			}else if(strcmp(tok_str,"unset")==0){
				comd_num=13;		
			}else if(strcmp(tok_str,"export")==0){
				comd_num=14;
			}else if(strcmp(tok_str,"echo")==0){
				comd_num=15;
			}else if(strcmp(tok_str,"kill")==0){
				comd_num=16;
			}else{
				comd_num=0;
			}
			idx=0;	//인자 카운트 0으로 초기화.
			if(comd_num!=15){
				while(tok_str=strtok(NULL," \n")){		//입력받은 문자열의 나머지 인자부분의 문자를 나눔
					if(tok_str==NULL){					//인자가 없다면 NULL출력
						printf("NULL");
					}else{
						printf("%s  ",tok_str);			//나머지 인자 출력
						if(idx<MAX_COM_ARG_SIZE){					
							com_arg[idx++]=tok_str;		//인자 저장
						}
						else {
							idx++;
						}
					}
				}
			}else{
				tok_str=strtok(NULL,"\n");
				com_arg[idx++]=tok_str;
			}
		
			printf("\n");

			struct stat statbuf; 
			switch(comd_num){						//명령어 넘버에 따라 함수 실행.
				case 1:
					if(idx<2){	
						    char rpath[128];
                    get_realpath(com_arg[0],rpath);
						if(chdir(rpath)==-1){       //인자가 1개 이하일 시 chdir명령어 실행
							perror("cd");	
						}
					}else{							//인자가 2개 이상일 시 Too many arguments 출력
						printf("Too many arguments!!\n");
					}
					break;
				case 2:
					if(idx<2){						//인자가 1개 이하일 시 mkdir명령어 실행
                       char rpath[128];
                     	get_realpath(com_arg[0], rpath);
						if(mkdir(rpath,0755)==-1){	//디렉토리 생성 권한은 0755
							perror("mkdir");
						}
					}else{
						printf("Too many arguments!!\n");
					}
					break;
				case 3:
					if(idx<2){						//인자가 1개 이하일 시 rmdir실행
                         char rpath[128];
                     get_realpath(com_arg[0], rpath);
						if(rmdir(rpath)==-1){	
							perror("rmdir");
						}
					}else{
						printf("Too many arguments!!\n");
					}	
					break;
				case 4:
					if(idx<3){						//인자가 2개 이하일 시 rename실행
						 char rpath1[128];
						  char rpath2[128];
						   get_realpath(com_arg[0], rpath1);
						    get_realpath(com_arg[1], rpath2);
						if(rename(rpath1,rpath2)==-1){
							perror("rename");
						}
					}else{
						printf("Too many argumants!!\n");
					}
					break;
				case 5:
					dir = opendir(".");		//현재 디렉토리 열기
					if(idx<1){				//인자가 없어야 실행
						if (dir == NULL) {	//디렉토리 열기 실패
							perror("opendir");
						}
						while ((entry = readdir(dir)) != NULL) {	//디렉토리를 읽어와 entry 구조체에 저장
							char *file_name;
							file_name=entry->d_name;
							stat(file_name,&statbuf);
							printf("Type: %-17s UID: %-10s  GID: %-10s Inode:%d Name: %s SIZE: %d Permission: %o Nlink: %o \nAtime: %sMtime: %sCtime: %s \n",get_file_type(entry->d_type),pw->pw_name,grp->gr_name,(int)entry->d_ino, entry->d_name,(int)statbuf.st_size,(statbuf.st_mode&07777),(unsigned int)statbuf.st_nlink,ctime(&statbuf.st_atime),ctime(&statbuf.st_mtim.tv_sec),ctime(&statbuf.st_ctim.tv_sec));	 //entry구조체의 정보를 가져와 타입, inode, 이름, UID, GID, 파일 사이즈, 권한, 하드링크 수, 파일의 접근, 수정, 생성 시간을 출력한다.
							
						}
					}else{
						printf("Too many arguments!!\n");
					}
					closedir(dir);
					break;
				case 6:
					if(strcmp(com_arg[0],"-s")!=0){	//심볼릭 링크와 하드링크 명령을 나누는 기준으로 두번째 인자가 -s일 시 symlink가 실행되고, 아니면 link실행.
						if(idx<3){
							char rpath1[128];
							char rpath2[128];
							get_realpath(com_arg[0],rpath1);
							get_realpath(com_arg[1],rpath2);
							if(link(rpath1,rpath2)==-1){
								perror("link");
							}
						}else{
							printf("Too many arguments!!\n");
						}
					}else{
						if(idx<4){
							char rpath1[128];
							char rpath2[128];
							get_realpath(com_arg[1],rpath1);
							get_realpath(com_arg[2],rpath2);
							if(symlink(rpath1,rpath2)==-1){
								perror("symlink");
							}
						}else{
							printf("Too many arguments!!\n");
						}
					}
					break;
				case 7:	//rm명령어 실행.
					if(idx<2){
						char rpath[128];
						get_realpath(com_arg[0],rpath);
						if(unlink(rpath)==-1){
							perror("rm");
						}
					}else{
						printf("Too many arguments!!\n");
					}
					break;
				case 8:	//chmod명령어 실행. 인자로 받은 문자열을 8진수로 바꾸어 실행한다.
					if(idx<3){
						char rpath1[128];
						char rpath2[128];
						get_realpath(com_arg[0],rpath1);
						get_realpath(com_arg[1],rpath2);
						if(chmod(rpath2,strtol(rpath1,NULL,8))==-1){
							perror("chmod");
						}
					}else{
						printf("Too many arguments!!\n");
					}
					break;
				case 9:	//cat명령어 실행. 파일을 읽어와 문자를 화면에 출력해준다.
					if(idx<2){
						char rpath[128];
						get_realpath(com_arg[0],rpath);
						FILE *fp=fopen(rpath,"r");
						char ch;
						if(fp==NULL){
							perror(rpath);
						}else{
							while((ch=fgetc(fp))!=EOF){
								fputc(ch,stdout);
							}
						}
						fclose(fp);
					}else{
						printf("Too many arguments!!\n");
					}
					break;
				case 10:	//cp명령어 실행. 원본파일을 읽어와 새 파일에 입력하여 파일을 복사한다.
					if(idx<3){
						char rpath1[128];
						char rpath2[128];
						get_realpath(com_arg[0],rpath1);
						get_realpath(com_arg[1],rpath2);

						int source_fd = open(rpath1, O_RDONLY);	//파일 디스크럽터에 소스 파일 할당
						if (source_fd == -1) {
							perror("open source file");
						}

						struct stat st;	//stat구조체에 할당하여 파일정보를 읽어옴 
						if (fstat(source_fd, &st) == -1) {		
							perror("fstat");
							close(source_fd);
						}

						int dest_fd = open(rpath2, O_RDWR | O_CREAT | O_TRUNC, 0666);	//파일을 읽기 및 쓰기로 열기
						if (dest_fd == -1) {
							perror("open destination file");
							close(source_fd);
						}

						void *source_map = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, source_fd, 0); //소스파일을 메모리에 매핑
						if (source_map == MAP_FAILED) {
							perror("mmap source file");
							close(source_fd);
							close(dest_fd);
						}

						if (ftruncate(dest_fd, st.st_size) == -1) {	//파일 기술자를 사용하여 목적 파일의 크기를 확장한다.
							perror("ftruncate");
							munmap(source_map, st.st_size);
							close(source_fd);
							close(dest_fd);
						}

						void *dest_map = mmap(NULL, st.st_size, PROT_WRITE, MAP_SHARED, dest_fd, 0); //목적파일에 메모리 매핑
						if (dest_map == MAP_FAILED) {
							perror("mmap destination file");
							munmap(source_map, st.st_size);
							close(source_fd);
							close(dest_fd);
						}

						memcpy(dest_map, source_map, st.st_size); //소스파일에서 목적파일로 데이터 복사

						if (msync(dest_map, st.st_size, MS_SYNC) == -1) {	//매핑된 메모리를 동기화
							perror("msync");
							munmap(source_map, st.st_size);
							munmap(dest_map, st.st_size);
							close(source_fd);
							close(dest_fd);
						}

						munmap(source_map, st.st_size);	//메모리 매핑 해제
						munmap(dest_map, st.st_size);

						close(source_fd);
						close(dest_fd);

					}else{
						printf("Too many arguments!!\n");
					}
					break;
				case 11:	//ps 명령어 실행, 프로세스 목록을 읽어와 출력한다.
					if(idx<1){
						dir = opendir("/proc");
    					if (dir == NULL) {
       			 			perror("opendir");
        					return 1;
    					}

    					while ((entry = readdir(dir)) != NULL) {
        					if (entry->d_type == DT_DIR) {
            					if (atoi(entry->d_name) != 0) {
                					readProcessInfo(entry->d_name);	//프로세스 목록을 읽어오는 함수
            					}
        					}
    					}			
					}else{
						printf("Too mney arguments!!\n");
					}
                    break;
					case 12:	//모든 환경변수를 읽어오는 함수
					if(idx<1){
   						char **env;
  						env=environ;
   						while(*env){
							printf("%s\n",*env);
							env++;
   						}
					}
					break;
					case 13:	//unset 실행
						if(idx<2){
							if(unsetenv(com_arg[0])!=0){
								perror(com_arg[0]);
							}
						}else{
						printf("Too mney arguments!!\n");
					}
					break;
					case 14:	//환경 변수를 변경하는 명령어, export를 입력하고 path=/tmp/test를 입력하게 된다면 =을 기준으로 나누어 path의 환경변수에 /tmp/test의 값을 설정해 준다.
						if(idx<2){
							char* spltok[2];
							spltok[0] = strtok(com_arg[0], "=");
							 if (spltok[0] != NULL) {
       							 spltok[1] = strtok(NULL, "=");

        						if (spltok[1] != NULL) {
            						if(setenv(spltok[0],spltok[1],1)!=0){
										perror("setenv");
									}
        						} else {
           							 perror(spltok[1]);
       							}
    						} else {
       							 perror(spltok[0]);
    						}
						}else{
						printf("Too mney arguments!!\n");
					}
						break;
					case 15:	//echo함수 구현, echo명령어를 입력받으면 뒤에 인자들을 나누지 않고 그 문자 그대로 출력한다.
						if(idx<2){
							printf("%s\n",com_arg[0]);
						}else{
						printf("Too mney arguments!!\n");
					}
				break;
				case 16:	//kill 명령 구현
					if(idx<3){
						int pid = atoi(com_arg[1]);
						int sig = atoi(com_arg[0]);
						kill(pid,sig);
					}else{
						printf("Too mney arguments!!\n");
					}
					break;
				default:		//cd, mkdir, rmdir, rename, ls명령어를 제외한 명령어를 입력하면 사용가능한 명령어와 그 설명을 출력해준다.
					printf("\n--command list--\n\n");
					printf("help : Shows a list of commands and a description of the function.\n\n");
					printf("cd <path>: Moves the current directory to the <path>.\n\n");
					printf("mkdir <path>: Create a directory corresponding to the path.\n\n");
					printf("rmdir <path>: Delete the directory corresponding to path.\n\n");
					printf("rename <source> <target>: Change source directory to target name.\n\n");
					printf("ls: Shows the contents of the current directory (a list of files and subdirectories).\n\n");
					printf("cp <source> <target> : Copy source directory to target directory\n\n");
					
					break;
					
			}
		}
	}while(1);
	
	free(command);
    free(current_dir);
	return 0;
	
}
	
void readProcessInfo(const char *pid) {	//프로세스 목록 읽어오는 함수, 각 proc디렉토의 밑의 프로세스 디렉토리들을 방문하면서 cmdline과 이름, PID를 출력한다.
    char path[MAX_CMD_SIZE];
    char cmdline[MAX_CMD_SIZE];
    char name[MAX_CMD_SIZE];
    FILE *fp;

    snprintf(path, sizeof(path), "/proc/%s/cmdline", pid);
    fp = fopen(path, "r");
    if (fp) {
        if (fgets(cmdline, sizeof(cmdline), fp)) {
            fclose(fp);

            snprintf(path, sizeof(path), "/proc/%s/status", pid);
            fp = fopen(path, "r");
            if (fp) {
                while (fgets(name, sizeof(name), fp)) {
                    if (strncmp(name, "Name:", 5) == 0) {
                        printf("PID: %s\nName: %sCmdline: %s\n\n", pid, name + 6, cmdline);
                        break;
                    }
                }
                fclose(fp);
            }
        } else {
            fclose(fp);
        }
    }
}