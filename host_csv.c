#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>
#define MAX_ITEMS 100
#define BUFFER_SIZE 512
int setup_serial(const char *portname, int baudrate) {
    int fd = open(portname, O_RDWR | O_NOCTTY | O_SYNC | O_NONBLOCK);
    if (fd < 0) {
        printf("Error %d opening %s: %s\n", errno, portname, strerror(errno));
        return -1;
    }
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);
    struct termios tty;
    if (tcgetattr(fd, &tty) != 0) {
        printf("Error %d from tcgetattr: %s\n", errno, strerror(errno));
        return -1;
    }
    speed_t speed;
    switch(baudrate) {
        case 9600: speed = B9600; break;
        case 115200: speed = B115200; break;
        default: speed = B9600; break;
    }
    cfsetospeed(&tty, speed);
    cfsetispeed(&tty, speed);
    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;     
    tty.c_iflag &= ~IGNBRK;         
    tty.c_lflag = 0;                
    tty.c_oflag = 0;                
    tty.c_cc[VMIN]  = 0;            
    tty.c_cc[VTIME] = 5;            
    tty.c_iflag &= ~(IXON | IXOFF | IXANY); 
    tty.c_cflag |= (CLOCAL | CREAD);
    tty.c_cflag &= ~(PARENB | PARODD);      
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CRTSCTS;
    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        printf("Error %d from tcsetattr: %s\n", errno, strerror(errno));
        return -1;
    }
    return fd;
}
int read_serial_line(int fd, char *buffer, int max_len) {
    int pos = 0;
    char c;
    while (pos < max_len - 1) {
        int n = read(fd, &c, 1);
        if (n > 0) {
            if (c == '\n') break;
            if (c != '\r') {
                buffer[pos++] = c;
            }
        } else {
            break;
        }
    }
    buffer[pos] = '\0';
    return pos;
}
void wait_for_ready(int fd) {
    char buf[BUFFER_SIZE];
    for (int i=0; i<20; i++) {
        if (read_serial_line(fd, buf, sizeof(buf)) > 0) {
            if (strstr(buf, "System ready")) return;
        }
        usleep(3500000);
    }
}
typedef struct {
    char id[32];
    char name[64];
    char cat[32];
    char stock[16];
    char loc[32];
    char status[32];
    char owner[32];
    char pic[32];
} Item;
void trim_newline(char *str) {
    int len = strlen(str);
    if (len > 0 && str[len-1] == '\n') str[len-1] = '\0';
    if (len > 1 && str[len-2] == '\r') str[len-2] = '\0';
}
void do_export(const char *port, const char *csv_file) {
    printf("Connecting to %s to export to %s...\n", port, csv_file);
    int fd = setup_serial(port, 9600);
    if (fd < 0) return;
    sleep(2); 
    tcflush(fd, TCIFLUSH);
    write(fd, "EXPORT\n", 7);
    usleep(500000);
    FILE *f = fopen(csv_file, "w");
    if (!f) {
        printf("Error opening %s\n", csv_file);
        close(fd);
        return;
    }
    fprintf(f, "id,name,category,stock,location,status,owner,pic\n");
    char buf[BUFFER_SIZE];
    int exported_count = 0;
    while (read_serial_line(fd, buf, sizeof(buf)) > 0) {
        if (strncmp(buf, "EXPORT_DONE", 11) == 0) {
            break;
        }
        int commas = 0;
        for(int i=0; buf[i]; i++) {
            if(buf[i] == ',') commas++;
        }
        if (commas >= 7) {
            fprintf(f, "%s\n", buf);
            exported_count++;
            printf("Exported item: %s\n", buf);
        }
    }
    fclose(f);
    close(fd);
    printf("Exported %d items successfully.\n", exported_count);
}
void do_import(const char *port, const char *csv_file) {
    printf("Connecting to %s to import from %s...\n", port, csv_file);
    int fd = setup_serial(port, 9600);
    if (fd < 0) return;
    printf("Menunggu Arduino selesai booting (Reset DTR)...\n");
    sleep(3); 
    tcflush(fd, TCIFLUSH);
    FILE *f = fopen(csv_file, "r");
    if (!f) {
        printf("Error opening %s\n", csv_file);
        close(fd);
        return;
    }
    char line[BUFFER_SIZE];
    if (fgets(line, sizeof(line), f) == NULL) {
        printf("Error: CSV file is empty!\n");
        fclose(f); close(fd);
        return;
    }
    while (fgets(line, sizeof(line), f)) {
        trim_newline(line);
        if (strlen(line) == 0) continue;
        char cmd[BUFFER_SIZE + 10];
        sprintf(cmd, "ADD %s\n", line); 
        printf("Sending: %s", cmd);
        write(fd, cmd, strlen(cmd));
        char buf[BUFFER_SIZE];
        int command_done = 0;
        while (!command_done) {
            if (read_serial_line(fd, buf, sizeof(buf)) > 0) {
                printf("  > %s\n", buf);
                if (strstr(buf, "MASTER(EEPROM) OK") || 
                    strstr(buf, "SLAVE(I2C) OK") ||
                    strstr(buf, "memory full") || 
                    strstr(buf, "duplicate ID") || 
                    strstr(buf, "missing parameters") ||
                    strstr(buf, "insufficient fields") ||
                    strstr(buf, "allocation failed") ||
                    strstr(buf, "connection error") ||
                    strstr(buf, "all nodes full") ||
                    strstr(buf, "not found")) {
                    command_done = 1;
                }
            } else {
                usleep(10000); 
            }
        }
        usleep(50000); 
    }
    fclose(f);
    close(fd);
    printf("Import completed successfully.\n");
}
int main(int argc, char **argv) {
    if (argc < 4) {
        printf("Usage:\n");
        printf("  %s export <port> <csv_file>\n", argv[0]);
        printf("  %s import <port> <csv_file>\n", argv[0]);
        printf("Example:\n");
        printf("  %s export /dev/ttyUSB0 data.csv\n", argv[0]);
        return 1;
    }
    const char *mode = argv[1];
    const char *port = argv[2];
    const char *csv = argv[3];
    if (strcmp(mode, "export") == 0) {
        do_export(port, csv);
    } else if (strcmp(mode, "import") == 0) {
        do_import(port, csv);
    } else {
        printf("Unknown mode: %s\n", mode);
    }
    return 0;
}
