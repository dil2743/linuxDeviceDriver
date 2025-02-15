#include <linux/module.h>
#define INCLUDE_VERMAGIC
#include <linux/build-salt.h>
#include <linux/elfnote-lto.h>
#include <linux/export-internal.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

#ifdef CONFIG_UNWINDER_ORC
#include <asm/orc_header.h>
ORC_HEADER;
#endif

BUILD_SALT;
BUILD_LTO_INFO;

MODULE_INFO(vermagic, VERMAGIC_STRING);
MODULE_INFO(name, KBUILD_MODNAME);

__visible struct module __this_module
__section(".gnu.linkonce.this_module") = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};

#ifdef CONFIG_RETPOLINE
MODULE_INFO(retpoline, "Y");
#endif



static const char ____versions[]
__used __section("__versions") =
	"\x10\x00\x00\x00\x7e\x3a\x2c\x12"
	"_printk\0"
	"\x24\x00\x00\x00\xab\x9e\x6c\x3a"
	"__platform_driver_register\0\0"
	"\x24\x00\x00\x00\x1e\xfb\x8b\x5f"
	"platform_driver_unregister\0\0"
	"\x1c\x00\x00\x00\x48\x9f\xdb\x88"
	"__check_object_size\0"
	"\x20\x00\x00\x00\x28\xe1\xa4\x12"
	"__arch_copy_from_user\0\0\0"
	"\x10\x00\x00\x00\xad\x64\xb7\xdc"
	"memset\0\0"
	"\x24\x00\x00\x00\x52\x3f\x0a\x4b"
	"gic_nonsecure_priorities\0\0\0\0"
	"\x1c\x00\x00\x00\xef\x6d\x5c\xa6"
	"alt_cb_patch_nops\0\0\0"
	"\x1c\x00\x00\x00\x54\xfc\xbb\x6c"
	"__arch_copy_to_user\0"
	"\x18\x00\x00\x00\x0e\x1e\x89\xec"
	"module_layout\0\0\0"
	"\x00\x00\x00\x00\x00\x00\x00\x00";

MODULE_INFO(depends, "");


MODULE_INFO(srcversion, "70BA3624BBA3FFDBC8ED673");
