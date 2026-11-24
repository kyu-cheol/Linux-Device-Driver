#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define __NR_enqueue_item 464
#define __NR_dequeue_item 465

int main(int argc, char **argv){
	int output;
	
	if(argc > 3 || argc < 2){
		perror("argument is invaild.\n");
		return -1;
	}

	if(!strcmp(argv[1], "enqueue")){
		if(syscall(__NR_enqueue_item, atoi(argv[2]))){
			perror("enqueue_item error.\n");
			return -1;
		}
		printf("enqueue %d\n", atoi(argv[2]));
	}

	if(!strcmp(argv[1], "dequeue")){
		output = syscall(__NR_dequeue_item);
		if(output == -1){
			perror("dequeue_item error.\n");
			return -1;
		}
		printf("output : %d\n", output);
	}

	return 0;
}
