#include <param.h>
#include <file.h>
#include <unistd.h>

int i;

int main(int argc, char **argv) {
    printf("hello!\n");
    for(i=0; i<argc; i++){
        printf("%s\n", argv[i]);
    }
    return 0;
}
