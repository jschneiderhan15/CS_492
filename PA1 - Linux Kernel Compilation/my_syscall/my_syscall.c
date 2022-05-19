#include <linux/syscalls.h>
SYSCALL_DEFINE1(john_syscall, char *, string){
	long copy, count, i;
	char buf[32];
	if(strnlen_user(string, 32) > sizeof(buf)){
		return -1;
	}
	copy = copy_from_user(buf, string, strnlen_user(string,32));
	count = 0;
	if(copy < 0){
		return -1;
	}
	if(copy > sizeof(buf)){
		return -1;
	}
	printk(KERN_ALERT "before: %s\n", buf);
	for(i = 0; i < sizeof(buf); i++){
		if(buf[i] == 'a' || buf[i] == 'e' || buf[i] == 'i' || buf[i] == 'o' || buf[i] == 'u' 			|| buf[i] == 'y'){
				buf[i] = 'X';
				count = count + 1;
				continue;
			}
	}
	
	printk(KERN_ALERT "after: %s\n", buf);

	if(copy_to_user(string, buf, strnlen_user(string,32))){
		return -1;
	}
	return count;
}
