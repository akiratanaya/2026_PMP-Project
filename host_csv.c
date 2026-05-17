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

    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;     // 8-bit chars
    tty.c_iflag &= ~IGNBRK;         // disable break processing
    tty.c_lflag = 0;                // no signaling chars, no echo, no canonical processing
    tty.c_oflag = 0;                // no remapping, no delays
    tty.c_cc[VMIN]  = 0;            // read doesn't block
    tty.c_cc[VTIME] = 5;            // 0.5 seconds read timeout

    tty.c_iflag &= ~(IXON | IXOFF | IXANY); // shut off xon/xoff ctrl
    tty.c_cflag |= (CLOCAL | CREAD);// ignore modem controls, enable reading
    tty.c_cflag &= ~(PARENB | PARODD);      // shut off parity
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
        usleep(100000);
    }
}

// Struct for inventory items
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

    sleep(2); // Wait for Arduino reset
    tcflush(fd, TCIFLUSH);

    write(fd, "LIST\n", 5);
    usleep(500000);

    char buf[BUFFER_SIZE];
    char ids[MAX_ITEMS][32];
    int id_count = 0;

    // Parse IDs from LIST
    while (read_serial_line(fd, buf, sizeof(buf)) > 0) {
        if (strncmp(buf, "ID:", 3) == 0) {
            strncpy(ids[id_count], buf + 3, sizeof(ids[id_count])-1);
            id_count++;
        }
    }

    if (id_count == 0) {
        printf("No items found on Arduino.\n");
        close(fd);
        return;
    }

    FILE *f = fopen(csv_file, "w");
    if (!f) {
        printf("Error opening %s\n", csv_file);
        close(fd);
        return;
    }

    fprintf(f, "id,name,category,stock,location,status,owner,pic\n");

    for (int i = 0; i < id_count; i++) {
        printf("Fetching ID: %s...\n", ids[i]);
        char cmd[64];
        sprintf(cmd, "FIND %s\n", ids[i]);
        write(fd, cmd, strlen(cmd));
        usleep(200000);

        Item item;
        memset(&item, 0, sizeof(Item));
        strcpy(item.id, ids[i]);

        while (read_serial_line(fd, buf, sizeof(buf)) > 0) {
            if (strncmp(buf, "Name:", 5) == 0) strcpy(item.name, buf+5);
            else if (strncmp(buf, "Cat:", 4) == 0) strcpy(item.cat, buf+4);
            else if (strncmp(buf, "Stock:", 6) == 0) strcpy(item.stock, buf+6);
            else if (strncmp(buf, "Location:", 9) == 0) strcpy(item.loc, buf+9);
            else if (strncmp(buf, "Status:", 7) == 0) strcpy(item.status, buf+7);
            else if (strncmp(buf, "Owner:", 6) == 0) strcpy(item.owner, buf+6);
            else if (strncmp(buf, "PIC:", 4) == 0) strcpy(item.pic, buf+4);
        }

        fprintf(f, "%s,%s,%s,%s,%s,%s,%s,%s\n", 
                item.id, item.name, item.cat, item.stock, 
                item.loc, item.status, item.owner, item.pic);
    }

    fclose(f);
    close(fd);
    printf("Exported %d items successfully.\n", id_count);
}

void do_import(const char *port, const char *csv_file) {
    printf("Connecting to %s to import from %s...\n", port, csv_file);
    int fd = setup_serial(port, 9600);
    if (fd < 0) return;

    sleep(2); // Wait for Arduino reset
    tcflush(fd, TCIFLUSH);

    FILE *f = fopen(csv_file, "r");
    if (!f) {
        printf("Error opening %s\n", csv_file);
        close(fd);
        return;
    }

    char line[BUFFER_SIZE];
    // Read header
    if (fgets(line, sizeof(line), f) == NULL) {
        printf("Error: CSV file is empty!\n");
        fclose(f); close(fd);
        return;
    }

    while (fgets(line, sizeof(line), f)) {
        trim_newline(line);
        if (strlen(line) == 0) continue;

        // Simple CSV parser for expected format (no quotes)
        char cmd[BUFFER_SIZE + 10];
        sprintf(cmd, "ADD %s\n", line); // ADD takes comma separated values

        printf("Sending: %s", cmd);
        write(fd, cmd, strlen(cmd));
        usleep(500000); // Wait for Arduino to process and save

        char buf[BUFFER_SIZE];
        while (read_serial_line(fd, buf, sizeof(buf)) > 0) {
            printf("  > %s\n", buf);
        }
    }

    fclose(f);
    close(fd);
    printf("Import completed.\n");
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
