#include "testsan/testsan.h"
#include <unistd.h>


using namespace __sanitizer;


INTERCEPTOR(void*, malloc, uptr size) {
 return REAL(malloc)(size);
}

// INTERCEPTOR(void *, malloc, uptr size) {
// 	return testsan_Malloc(size); 

// }

struct NewHookArgs {
	char* className;
	long classSize;
};

//Define our own private namespace for our runtime implementation
void testsan_InitInterceptors() {
		//InitializeCommonInterceptors();
		//Should be NOP on mac
    INTERCEPT_FUNCTION(malloc);
}
namespace __testsan {

	static struct {
		int shadow_mem_size; 
		char * shadow_mem_start; 
		char * shadow_mem_end; 
	} shadow_memory_storage;

	//Just a hashmap can i do this?
} //end of __testsan

//use our namespace and interface with the rest of the toolchain
using namespace __testsan; 

//Dont think we need to export this
//Sanitizer interface attribute sets the visability of the symbol so we can export it
void testsan_AllocateShadowMemory() { 
	shadow_memory_storage.shadow_mem_size = 1000 * sizeof(*shadow_memory_storage.shadow_mem_start);

	//Allocate shadow memory of a certain size or die. MmapNoReserveOrDie is the builtin to get shadow memory
	shadow_memory_storage.shadow_mem_start = (char*)MmapNoReserveOrDie(shadow_memory_storage.shadow_mem_size, "Simple Shadow Memory");
	shadow_memory_storage.shadow_mem_end = shadow_memory_storage.shadow_mem_start + shadow_memory_storage.shadow_mem_size; 
//	memset(shadow_memory_storage.shadow_mem_start, 0, shadow_memory_storage.shadow_mem_size);
	
	//Optional: Report to stdout shadow region, useful for debugging. 
	VReport(1, "Shadow mem at %zx .. %zx\n", shadow_memory_storage.shadow_mem_start, shadow_memory_storage.shadow_mem_start + shadow_memory_storage.shadow_mem_size);
}


extern "C" SANITIZER_INTERFACE_ATTRIBUTE 
void testsan_AfterMalloc(char * value) {
	//Printf is sanitizer internal printf
	Printf("Malloc returned address %x\n", value); 
	return;
}
 
extern "C" SANITIZER_INTERFACE_ATTRIBUTE
void testsan_HelloFunction(char * func_name) {
	//puts("Printing function name!"); 	
	return;
}

extern "C" SANITIZER_INTERFACE_ATTRIBUTE
void testsan_HandleNew(NewHookArgs* args, uptr Pointer) {
	Printf("RUNTIME: %s address %x classsize = %d \n", args->className, Pointer, args->classSize);
}

extern "C" SANITIZER_INTERFACE_ATTRIBUTE
void testsan_EndOfMain() {
		//internal strlen is from the sanitizer runtimes libc imp. 
		write(1, "End of main function!\n", internal_strlen("End of main function!\n")); 
	return; 
}

void * testsan_Malloc(uptr size) {
	//This is how you call real malloc 
	void * ret = REAL(malloc)(size); 
		write(1, "Hooked malloc!\n", internal_strlen("Hooked malloc!\n")); 
	//You don't have to return the real address, but program will crash 
	return (void*)0xdeadbeef; 
}


extern "C" SANITIZER_INTERFACE_ATTRIBUTE
#if !SANITIZER_CAN_USE_PREINIT_ARRAY
// For non Linux platforms this will run this function when the RT library is loaded. 
// this adds the consturctor attribute to the init function, running it at load time. 
__attribute__((constructor(0)))
#endif
	void testsan_Init() {
		//Set sanitizer tool name, not required. 
		SanitizerToolName = "testsan";

			/*
		Sanitizers have a lot of flags, theres a built in function 
		to set everything to default. 
		The flags are located in sanitizer_flags.inc
		*/ 
		SetCommonFlagsDefaults();
	
		testsan_InitInterceptors(); 

		//Try to allocate shadowmem, have it store 500 elements. 
		// testsan_AllocateShadowMemory();
		VReport(2, "Initialized testsan runtime!\n"); 
	}
#if SANITIZER_CAN_USE_PREINIT_ARRAY

// For Linux platforms we use the .preinit_array ELF magic.
extern "C" {
	__attribute__((section(".preinit_array"),
				used)) void (*testsan_init_ptr)(void) = testsan_Init;
}
#endif
