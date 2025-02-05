#include <linux/module.h>

// this modules contains the defination of the macro that are used in a kernel 
// never include any user space library in a kernel module like C stdlib, no user space library is linked to kernel space
// most of the header lives in <kernel src code>/linux/<header>

//while writing a kernel module there are two function 1) module initialization function 2) module cleanup initialization function

static int __init kernel_module_init(void){
	// static as it is never expected to be called by any other module
	// returns 0 for success else it will not be loaded into kernel
	// called during boot time for statick module and called during instertion fon dynamic

	pr_info(" Hello Test driver!!! \n");
	return 0;
}

//module clean-up function

static void __exit kernel_module_cleanup(void){

	//entry point when a module is removed
	//for a static module it is never called, so if it is even there, it will be removed during build if it sees a __exit tag 
	pr_info(" Good bye 'Hello Test Driver!!!' \n");
}

// here __init and __exit are called function section attributes (compiler directive -> directs the compiler to keep the data/code in a section called .init.txt 
//  #define __init __section(.init.txt)


//module entry registration point 

module_init(kernel_module_init);
module_exit(kernel_module_cleanup);

// Module discription 

MODULE_LICENSE("GPL");
MODULE_AUTHOR("DLSHAD KHAN");
MODULE_DESCRIPTION("A kernel module to print Hello World");
MODULE_INFO(board," Test board BBB");

