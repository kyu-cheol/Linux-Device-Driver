#include <gpiod.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#define A 0
#define B 1
#define C 2
#define D 3
#define E 4
#define F 5
#define G 6

void move_cur(int x, int y) {
	printf("\033[%dd\033[%dG", y, x);
}

int main(int argc, char **argv) {
	struct gpiod_chip *chip = gpiod_chip_open("/dev/gpiochip1");
	struct gpiod_line *line[7];
	int num;
	int i;

	if (argc != 2) {
		printf("wrong argument.\n");
		return -1;
	}
	/*
	if (argv[1] < 48 || argv[1] > 57) {
		printf("wrong range argument.\n");
		return -1;
	}
	*/
	
	for (i = 0; i < sizeof(line) / sizeof(*line); i++) {
		line[i] = gpiod_chip_get_line(chip, i);
	}

	gpiod_line_request_output(line[A], NULL, 0);    // LED A OFF
	gpiod_line_request_output(line[B], NULL, 0);    // LED B OFF
	gpiod_line_request_output(line[C], NULL, 0);    // LED C OFF
	gpiod_line_request_output(line[D], NULL, 0);    // LED D OFF
	gpiod_line_request_output(line[E], NULL, 0);    // LED E OFF
	gpiod_line_request_output(line[F], NULL, 0);    // LED F OFF
	gpiod_line_request_output(line[G], NULL, 0);    // LED G OFF

	system("clear");
	
	num = atoi(argv[1]);
	
	switch(num) {
	case 0:
		gpiod_line_set_value(line[A], 1);
		gpiod_line_set_value(line[B], 1);
		gpiod_line_set_value(line[C], 1);
		gpiod_line_set_value(line[D], 1);
		gpiod_line_set_value(line[E], 1);
		gpiod_line_set_value(line[F], 1);
		break;
	case 1:
		gpiod_line_set_value(line[B], 1);
		gpiod_line_set_value(line[C], 1);
		break;
	case 2:
		gpiod_line_set_value(line[A], 1);
		gpiod_line_set_value(line[B], 1);
		gpiod_line_set_value(line[G], 1);
		gpiod_line_set_value(line[E], 1);
		gpiod_line_set_value(line[D], 1);
		break;
	case 3:
		gpiod_line_set_value(line[A], 1);
		gpiod_line_set_value(line[B], 1);
		gpiod_line_set_value(line[G], 1);
		gpiod_line_set_value(line[C], 1);
		gpiod_line_set_value(line[D], 1);
		break;
	case 4:
		gpiod_line_set_value(line[F], 1);
		gpiod_line_set_value(line[G], 1);
		gpiod_line_set_value(line[B], 1);
		gpiod_line_set_value(line[C], 1);
		break;
	case 5:
		gpiod_line_set_value(line[A], 1);
		gpiod_line_set_value(line[F], 1);
		gpiod_line_set_value(line[G], 1);
		gpiod_line_set_value(line[C], 1);
		gpiod_line_set_value(line[D], 1);
		break;
	case 6:
		gpiod_line_set_value(line[A], 1);
		gpiod_line_set_value(line[F], 1);
		gpiod_line_set_value(line[G], 1);
		gpiod_line_set_value(line[C], 1);
		gpiod_line_set_value(line[D], 1);
		gpiod_line_set_value(line[E], 1);
		break;
	case 7:
		gpiod_line_set_value(line[F], 1);
		gpiod_line_set_value(line[A], 1);
		gpiod_line_set_value(line[B], 1);
		gpiod_line_set_value(line[C], 1);
		break;
	case 8:
		for (i = 0; i < 7; i++) {
			gpiod_line_set_value(line[i], 1);
		}
		break;
	case 9:
		gpiod_line_set_value(line[G], 1);
		gpiod_line_set_value(line[F], 1);
		gpiod_line_set_value(line[A], 1);
		gpiod_line_set_value(line[B], 1);
		gpiod_line_set_value(line[C], 1);
		gpiod_line_set_value(line[D], 1);
		break;
	}

	move_cur(1, 24);
	
	return 0;
}
