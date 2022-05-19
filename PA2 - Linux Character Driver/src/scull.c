#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <pthread.h>

#include "scull.h"

#define CDEV_NAME "/dev/scull"

/* Quantum command line option */
static int g_quantum;

static void usage(const char *cmd)
{
	printf("Usage: %s <command>\n"
	       "Commands:\n"
	       "  R          Reset quantum\n"
	       "  S <int>    Set quantum\n"
	       "  T <int>    Tell quantum\n"
	       "  G          Get quantum\n"
	       "  Q          Query quantum\n"
	       "  X <int>    Exchange quantum\n"
	       "  H <int>    Shift quantum\n"
	       "  h          Print this message\n"
	       "  i          Info about the quantum\n"
	       "  p          Run child processes\n"
	       "  t          Run threads\n",	       
	       cmd);
}

typedef int cmd_t;

static cmd_t parse_arguments(int argc, const char **argv)
{
	cmd_t cmd;

	if (argc < 2) {
		fprintf(stderr, "%s: Invalid number of arguments\n", argv[0]);
		cmd = -1;
		goto ret;
	}

	/* Parse command and optional int argument */
	cmd = argv[1][0];
	switch (cmd) {
	case 'S':
	case 'T':
	case 'H':
	case 'X':
		if (argc < 3) {
			fprintf(stderr, "%s: Missing quantum\n", argv[0]);
			cmd = -1;
			break;
		}
		g_quantum = atoi(argv[2]);
		break;
	case 'R':
	case 'G':
	case 'Q':
	case 'i':
	case 'p':
	case 't':
	case 'h':
		break;
	default:
		fprintf(stderr, "%s: Invalid command\n", argv[0]);
		cmd = -1;
	}

ret:
	if (cmd < 0 || cmd == 'h') {
		usage(argv[0]);
		exit((cmd == 'h')? EXIT_SUCCESS : EXIT_FAILURE);
	}
	return cmd;
}

//thread friendly function for use in pthread_create
void *thread (void *arg){
	//creating an arbitrary task_info
	struct task_info threadTask;
	//calling ioctl and printing the information 2x
	ioctl(*(int *) arg, SCULL_IOCIQUANTUM, &threadTask);
	printf("state %ld, stack %p, cpu %u, prio %d, sprio %d, nprio %d, rtprio %d, pid %d, tgid %d, nv %ld, niv %ld\n", threadTask.state, threadTask.stack, threadTask.cpu, threadTask.prio, threadTask.static_prio, threadTask.normal_prio, threadTask.rt_priority, threadTask.pid, threadTask.tgid, threadTask.nvcsw, threadTask.nivcsw);
	ioctl(*(int *) arg, SCULL_IOCIQUANTUM, &threadTask);
	printf("state %ld, stack %p, cpu %u, prio %d, sprio %d, nprio %d, rtprio %d, pid %d, tgid %d, nv %ld, niv %ld\n", threadTask.state, threadTask.stack, threadTask.cpu, threadTask.prio, threadTask.static_prio, threadTask.normal_prio, threadTask.rt_priority, threadTask.pid, threadTask.tgid, threadTask.nvcsw, threadTask.nivcsw);
	
	//exiting safely
	pthread_exit(0);
}
	
static int do_op(int fd, cmd_t cmd)
{
	int ret, q, i;
	struct task_info passTask;
	pthread_t tids[4] = {0, 0, 0, 0};
	switch (cmd) {
	case 'R':
		ret = ioctl(fd, SCULL_IOCRESET);
		if (ret == 0)
			printf("Quantum reset\n");
		break;
	case 'Q':
		q = ioctl(fd, SCULL_IOCQQUANTUM);
		printf("Quantum: %d\n", q);
		ret = 0;
		break;
	case 'G':
		ret = ioctl(fd, SCULL_IOCGQUANTUM, &q);
		if (ret == 0)
			printf("Quantum: %d\n", q);
		break;
	case 'T':
		ret = ioctl(fd, SCULL_IOCTQUANTUM, g_quantum);
		if (ret == 0)
			printf("Quantum set\n");
		break;
	case 'S':
		q = g_quantum;
		ret = ioctl(fd, SCULL_IOCSQUANTUM, &q);
		if (ret == 0)
			printf("Quantum set\n");
		break;
	case 'X':
		q = g_quantum;
		ret = ioctl(fd, SCULL_IOCXQUANTUM, &q);
		if (ret == 0)
			printf("Quantum exchanged, old quantum: %d\n", q);
		break;
	case 'H':
		q = ioctl(fd, SCULL_IOCHQUANTUM, g_quantum);
		printf("Quantum shifted, old quantum: %d\n", q);
		ret = 0;
		break;
	case 'i':
		//calling ioctl to populate fields in the passTask
		ret = ioctl(fd, SCULL_IOCIQUANTUM, &passTask);
		//printing information about the task
		printf("state %ld, stack %p, cpu %u, prio %d, sprio %d, nprio %d, rtprio %d, pid %d, tgid %d, nv %ld, niv %ld\n", passTask.state, passTask.stack, passTask.cpu, passTask.prio, passTask.static_prio,passTask.normal_prio, passTask.rt_priority, passTask.pid, passTask.tgid, passTask.nvcsw,passTask.nivcsw);
		break;
	case 'p':
		//looping four times for four children
		for(i = 0; i < 4; i++)
		{
			//forking
			int child = fork();
			//if the fork fails, exit with an error
			if(child == -1){
				printf("fork() has failed!\n");
				exit(1);
			}
			//if the process is in the child
			else if(child == 0){
				//call ioctl and print info 2x
				ioctl(fd, SCULL_IOCIQUANTUM, &passTask);
				printf("state %ld, stack %p, cpu %u, prio %d, sprio %d, nprio %d, rtprio %d, pid %d, tgid %d, nv %ld, niv %ld\n", passTask.state, passTask.stack, passTask.cpu, passTask.prio, passTask.static_prio, passTask.normal_prio, passTask.rt_priority, passTask.pid, passTask.tgid, passTask.nvcsw, passTask.nivcsw);
				ioctl(fd, SCULL_IOCIQUANTUM, &passTask);
				printf("state %ld, stack %p, cpu %u, prio %d, sprio %d, nprio %d, rtprio %d, pid %d, tgid %d, nv %ld, niv %ld\n", passTask.state, passTask.stack, passTask.cpu, passTask.prio, passTask.static_prio, passTask.normal_prio, passTask.rt_priority, passTask.pid, passTask.tgid, passTask.nvcsw, passTask.nivcsw);
				exit(0);
			}
		}
		//waiting for each forked process
		for(i = 0; i < 4; i++){
			wait(NULL);
		}
		break;		
	case 't':
		//for loop to loop four times for four threads
		for(i = 0; i < 4; i++)
		{
			//adding tids to an array for later use
			pthread_t tid;
			//creating the thread
			pthread_create(&tid,NULL,thread, &fd);
			tids[i] = tid;
		}
		//looping through each thread to join them
		for(i = 0; i < 4; i++){
			pthread_join(tids[i], NULL);
		}
		break;
	default:
		/* Should never occur */
		abort();
		ret = -1; /* Keep the compiler happy */
	}

	if (ret != 0)
		perror("ioctl");
	return ret;
}

int main(int argc, const char **argv)
{
	int fd, ret;
	cmd_t cmd;

	cmd = parse_arguments(argc, argv);

	fd = open(CDEV_NAME, O_RDONLY);
	if (fd < 0) {
		perror("cdev open");
		return EXIT_FAILURE;
	}

	printf("Device (%s) opened\n", CDEV_NAME);

	ret = do_op(fd, cmd);

	if (close(fd) != 0) {
		perror("cdev close");
		return EXIT_FAILURE;
	}

	printf("Device (%s) closed\n", CDEV_NAME);

	return (ret != 0)? EXIT_FAILURE : EXIT_SUCCESS;
}
