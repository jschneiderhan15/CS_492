#include <sys/syscall.h>
#include <unistd.h>
#include <stdio.h>

int main(){
	char test1[] = "hello";
	printf("%s\n", test1);

	printf("%d\n", syscall(548, test1));

	printf("%s\n", test1);

	char test2[] = "supercalifragilisticexpialidocious";

	printf("%s\n", test2);

	printf("%d\n", syscall(548, test2));

	printf("%s\n", test2);
	
	return 0;
}



