#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <stdbool.h>
#include <dirent.h>
#include <string.h>

typedef struct _ProcessInfo {
    int pid;
    int ppid;
    char name[20];
    char version[20];
    struct _ProcessInfo* child;
    struct _ProcessInfo* next;
} ProcessInfo;

bool is_pid(const char* str) {
    const char* p = str;
    while (*p) {
        if (*p > '9' || *p < '0') return false;
        p++;
    }
    return true;
}

bool get_all_pid(int** pids, int* pid_cnt) {
    DIR* dir = opendir("/proc/");
    if (!dir) {
        printf("Error in reading dir /proc/");
        return false;
    }
    
    // get pid cnt
    struct dirent* dire;
    int cnt = 0;
    while (dire = readdir(dir)) {
        if (!is_pid(dire->d_name)) continue;
        cnt++;
    }
    *pid_cnt = cnt;
    if (cnt <= 0) {
        closedir(dir);
        return false;
    }

    // get all pid
    int* buf = (int*) malloc(sizeof(int) * cnt);
    *pids = buf;
    rewinddir(dir);
    cnt = 0;
    while (dire = readdir(dir)) {
        if (!is_pid(dire->d_name)) continue;
        buf[cnt++] = atoi(dire->d_name);
    }
    closedir(dir);
    return true;
}

bool get_process_info(ProcessInfo* proc, int pid) {
    char buf[256];
    sprintf(buf, "/proc/%d/status", pid);
    FILE* file = fopen(buf, "r");
    if (!file) return false;
    while (fgets(buf, 256, file)) {
        if (strncmp("Name", buf, strlen("Name")) == 0) {
            sscanf(buf, "Name: %s", proc->name);
        } else if (strncmp("Pid", buf, strlen("Pid")) == 0) {
            sscanf(buf, "Pid: %d", &proc->pid);
        } else if (strncmp("PPid", buf, strlen("PPid")) == 0) {
            sscanf(buf, "PPid: %d", &proc->ppid);
        } else {
            continue;
        }
    }
    proc->child = proc->next = NULL;
    return true;
}
bool get_all_process_info(ProcessInfo* proc, int* pids, int pid_cnt) {
    for (int i = 0; i < pid_cnt; i++) {
        if (!get_process_info(&proc[i], pids[i])) return false;
    }
    return true;
}

ProcessInfo* find_proc(ProcessInfo* proc, int pid_cnt, int pid) {
    for (int i = 0; i < pid_cnt; i++) {
        if (proc[i].pid == pid) return &proc[i];
    }
    return NULL;
}
void get_all_childs(ProcessInfo* proc, int pid_cnt, ProcessInfo* root, bool sort) {
    ProcessInfo* head = NULL;
    ProcessInfo* tail = NULL;
    for (int i = 0; i < pid_cnt; i++) {
        if (proc[i].ppid != root->pid) continue;
        if (!head) {
            head = tail = &proc[i];
        } else {
            if (!sort || tail->pid < proc[i].pid) {
                tail->next = &proc[i];
                tail = tail->next;
            } else if (head->pid > proc[i].pid) {
                proc[i].next = head;
                head = &proc[i];
            } else {
                ProcessInfo* curr = head;
                while (curr->next->pid < proc[i].pid) curr = curr->next;
                proc[i].next = curr->next;
                curr->next = &proc[i];
            }
        }
    }
    ProcessInfo* curr = head;
    while (curr) {
        get_all_childs(proc, pid_cnt, curr, sort);
        curr = curr->next;
    }
    root->child = head;
}
ProcessInfo* build_tree(ProcessInfo* proc, int pid_cnt, bool sort) {
    ProcessInfo* root = find_proc(proc, pid_cnt, 1);
    if (!root) return NULL;

    get_all_childs(proc, pid_cnt, root, sort);
}
const int SPACES = 4;
const char* TABSTR = "    ";
void show_pre(int level, bool is_last) {
    for (int i = 0; i < level; i++) {
        printf("%s", TABSTR);
        if (i < level - 1) printf("│ ");
        else {
            if (is_last) printf("└─");
            else printf("├─");
        }
    }
}
void show_info(ProcessInfo* item, bool show_pid) {
    if (show_pid) printf("%s (%d)", item->name, item->pid);
    else printf("%s", item->name);
}
void show_item(ProcessInfo* item, int level, bool show_pid, bool islast) {
    if (level) show_pre(level, islast);
    show_info(item, show_pid);
    printf("\n");
    ProcessInfo* child = item->child;
    while (child) {
        show_item(child, level + 1, show_pid, child->next == NULL);
        child = child->next;
    }
}
void show_tree(ProcessInfo* root, bool show_pid) {
    show_item(root, 0, show_pid, true);
}

int main(int argc, char* argv[]) {
    char opts[] = "pnV";
    static struct option longopts[] = {
        {"show-pids", no_argument, 0, 'p'},
        {"numeric-sort", no_argument, 0, 'n'},
        {"version", no_argument, 0, 'V'},
    };
    int longopt_id = 0;
    int opt = 0;
    bool show_pid = false, show_version = false, numeric_sort = false;
    while (1) {
        int opt = getopt_long(argc, argv, opts, longopts, &longopt_id);
        if (opt == -1) break;
        switch (opt)
        {
        case 'p':
            show_pid = true;
            break;
        case 'n':
            numeric_sort = true;
            break;
        case 'V':
            show_version = true;
            break;
        default:
            printf("usage: %s [-p | --show-pids] [-n | --numeric-sort] [-V | --version]\n",
            argv[0]);
            exit(EXIT_FAILURE);
            break;
        }
    }
    int* pids = NULL;
    int pid_cnt = 0;
    if (!get_all_pid(&pids, &pid_cnt)) {
        printf("Error in geting pids\n");
        exit(EXIT_FAILURE);
    }
    ProcessInfo* proc = (ProcessInfo*) malloc(sizeof(ProcessInfo) * pid_cnt);
    if (!get_all_process_info(proc, pids, pid_cnt)) {
        printf("Error in getting process info\n");
        exit(EXIT_FAILURE);
    }
    ProcessInfo* root = build_tree(proc, pid_cnt, numeric_sort);
    show_tree(root, show_pid);
    return 0;
}