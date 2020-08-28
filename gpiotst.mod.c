#include <linux/module.h>
#define INCLUDE_VERMAGIC
#include <linux/build-salt.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

BUILD_SALT;

MODULE_INFO(vermagic, VERMAGIC_STRING);
MODULE_INFO(name, KBUILD_MODNAME);

__visible struct module __this_module
__section(.gnu.linkonce.this_module) = {
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

static const struct modversion_info ____versions[]
__used __section(__versions) = {
	{ 0x1d914e87, "module_layout" },
	{ 0x2d6fcc06, "__kmalloc" },
	{ 0x6efda526, "gpiod_direction_output" },
	{ 0xf81bc5a9, "gpio_to_desc" },
	{ 0x3c3ff9fd, "sprintf" },
	{ 0xd697e69a, "trace_hardirqs_on" },
	{ 0x75efc6fa, "misc_register" },
	{ 0xc5850110, "printk" },
	{ 0x8e865d3c, "arm_delay_ops" },
	{ 0x37a0cba, "kfree" },
	{ 0xefd6cf06, "__aeabi_unwind_cpp_pr0" },
	{ 0xe9bc675f, "gpiod_set_value" },
	{ 0xec3d2e1b, "trace_hardirqs_off" },
	{ 0xf133abf5, "misc_deregister" },
};

MODULE_INFO(depends, "");


MODULE_INFO(srcversion, "641A5D6EF6C188C4DB30D80");
