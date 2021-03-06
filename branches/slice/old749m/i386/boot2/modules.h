/*
 * Module Loading functionality
 * Copyright 2009 Evan Lojewski. All rights reserved.
 *
 */

#include <mach-o/loader.h>
#include <mach-o/nlist.h>


// There is a bug with the module system / rebasing / binding
// that causes static variables to be incorrectly rebased or bound
// Disable static variables for the moment
// #define static

#ifndef __BOOT_MODULES_H
#define __BOOT_MODULES_H

#define SYMBOLS_MODULE "Symbols.dylib"

#define SYMBOL_LOOKUP_SYMBOL	"_lookup_symbol"
#define STUB_ENTRY_SIZE	6

#define SECT_NON_LAZY_SYMBOL_PTR	"__nl_symbol_ptr"
#define SECT_SYMBOL_STUBS			"__symbol_stub"


#define VALID_FUNCTION(__x__)	(__x__ && (void*)__x__ != (void*)0xFFFFFFFF)

extern unsigned long long textAddress;
extern unsigned long long textSection;


typedef struct symbolList_t
{
	char* symbol;
	unsigned int addr;
	struct symbolList_t* next;
} symbolList_t;

typedef struct moduleList_t
{
	char* module;
	unsigned int version;
	unsigned int compat;
	struct moduleList_t* next;
} moduleList_t;

typedef struct callbackList_t
{
	void(*callback)(void*, void*, void*, void*);
	struct callbackList_t* next;
} callbackList_t;

typedef struct moduleHook_t
{
	const char* name;
	callbackList_t* callbacks;
	struct moduleHook_t* next;
} moduleHook_t;



int init_module_system();
void load_all_modules();



int load_module(char* module);
int is_module_loaded(const char* name);
void module_loaded(const char* name/*, UInt32 version, UInt32 compat*/);




/********************************************************************************/
/*	Symbol Functions															*/
/********************************************************************************/
long long		add_symbol(char* symbol, long long addr, char is64);
unsigned int	lookup_all_symbols(const char* name);



/********************************************************************************/
/*	Macho Parser																*/
/********************************************************************************/
void*			parse_mach(void* binary, 
							int(*dylib_loader)(char*),
							long long(*symbol_handler)(char*, long long, char));
unsigned int	handle_symtable(UInt32 base,
							 struct symtab_command* symtabCommand,
							 long long(*symbol_handler)(char*, long long, char),
							 char is64);
void			rebase_macho(void* base, char* rebase_stream, UInt32 size);
inline void		rebase_location(UInt32* location, char* base, int type);
void			bind_macho(void* base, char* bind_stream, UInt32 size);
inline void		bind_location(UInt32* location, char* value, UInt32 addend, int type);




/********************************************************************************/
/*	Module Interface														*/
/********************************************************************************/
int				replace_function(const char* symbol, void* newAddress);
int				execute_hook(const char* name, void*, void*, void*, void*);
void			register_hook_callback(const char* name, void(*callback)(void*, void*, void*, void*));
moduleHook_t*	hook_exists(const char* name);

#if DEBUG_MODULES
void			print_hook_list();
#endif

/********************************************************************************/
/*	dyld Interface																*/
/********************************************************************************/
void dyld_stub_binder();

#endif /* __BOOT_MODULES_H */