#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sched.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <limits.h>
#include <sys/mount.h>

#define STACK_SIZE (1024 * 1024)
#define CLONE_FLAGS (CLONE_NEWUTS | CLONE_NEWCGROUP | CLONE_NEWIPC \
        | CLONE_NEWNET | CLONE_NEWUSER | CLONE_NEWPID | CLONE_NEWNS \
        | SIGCHLD)

#define ERR_EXIT(msg) do { perror(msg);exit(EXIT_FAILURE);\
}while(0)

struct container_t {
    char stack[STACK_SIZE];
    char *init_args[2];
    char *cname;
    int syncfd[2];
};
static int container_init(struct container_t *container) {
    sethostname(container->cname,strlen(container->cname));

    //mount("proc","/proc","proc",MS_PRIVATE,NULL);
    system("mount -t proc proc /proc");
    if(execv(container->init_args[0],container->init_args) == -1){
        ERR_EXIT("execv container error.");
    }

    return 0;
}
static int container_run(void *arg){
    struct container_t *container = (struct container_t*)arg;
    pid_t pid = getpid();

    close(container->syncfd[1]);
    char ch;
    if(read(container->syncfd[0],&ch,1) != 0){
        ERR_EXIT("contaienr:failed read syncfd");
    }
    close(container->syncfd[0]);
    return container_init(container);
}

void update_map(char *map,char *path) {
    size_t len;
    int fd;
    len = strlen(map);
    fd = open(path,O_RDWR);
    if(fd == -1){
        ERR_EXIT("open mapping file error.");
    }
    if(write(fd,map,len)!=len){
        ERR_EXIT("write mappiing file error.");
    }
}

void update_ugid_map(pid_t cpid){
    const int MAP_BUF_SIZE = 100;
    char map_buf[MAP_BUF_SIZE];
    char map_path[PATH_MAX];

    snprintf(map_buf,MAP_BUF_SIZE,"0 %ld 1",(long)getuid());
    snprintf(map_path,PATH_MAX,"/proc/%ld/uid_map",(long)cpid);
    update_map(map_buf,map_path);

    snprintf(map_buf,MAP_BUF_SIZE,"0 %ld 1",(long)getgid());
    snprintf(map_path,PATH_MAX,"/proc/%ld/gid_map",(long)cpid);
    update_map(map_buf,map_path);
}
int run_container(){
    struct container_t container = {
        .cname = "container_001",
        .init_args = {
            "/bin/bash",
            NULL,
        },
    };
    if(pipe(container.syncfd) == -1)
        ERR_EXIT("create syncfd error.");

    pid_t cpid = clone(container_run,container.stack + STACK_SIZE,CLONE_FLAGS,&container);
    if (cpid == -1){
        ERR_EXIT("clone child error.");
    }
    printf("main: cloned container pid:%d\n",cpid);
    update_ugid_map(cpid); 
    close(container.syncfd[1]); 

    return waitpid(cpid,NULL,0);

}
int exec_container(pid_t container_pid) {

    int nstype_size = 6;
    char *nstypes[] = {
        "user",
        "net",
        "ipc",
        "uts",
        "pid",
        "mnt",
        "cgroup",
    };
    for(int i=0;i<nstype_size;i++){
        char ns_path[PATH_MAX];
        int ns_fd;
        snprintf(ns_path,PATH_MAX,"/proc/%d/ns/%s",container_pid,nstypes[i]);
        if((ns_fd = open(ns_path,O_RDONLY)) == -1){
            printf("open fd %s error.\n",ns_path);
            continue;
        }
        if(setns(ns_fd,0) == -1){
            perror("setns error.");
            continue;
        }
    }
    pid_t cpid = fork();
    if(cpid == -1){
        ERR_EXIT("failed fork child");
    }
    if(cpid == 0){
        char *args[]= {
            "/bin/bash",
            NULL,
        };
        execv(args[0],args);
    }else{
      return waitpid(cpid,NULL,0); 
    }
    return 0;
}

int main(int argc,char **argv){
    if(argc < 2)
        ERR_EXIT("Usage: enoc run|exec container_pid");
    if(strcmp(argv[1],"run") == 0){
        return run_container();
    }
    if(strcmp(argv[1],"exec") == 0){
        pid_t container_pid = atoi(argv[2]);
        return exec_container(container_pid);
    }
    return 0;
}
