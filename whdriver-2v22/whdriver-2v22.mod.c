#include <linux/build-salt.h>
#include <linux/module.h>
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
	{ 0x45cd1b70, "module_layout" },
	{ 0xd0df5f00, "class_unregister" },
	{ 0xa43edd50, "device_destroy" },
	{ 0x2b68bd2f, "del_timer" },
	{ 0xc6f46339, "init_timer_key" },
	{ 0xdb3c72dc, "class_destroy" },
	{ 0xca42ba96, "device_create" },
	{ 0x6bc3fbc0, "__unregister_chrdev" },
	{ 0xf7c80868, "__class_create" },
	{ 0x6a5fd0e8, "__register_chrdev" },
	{ 0xdecd0b29, "__stack_chk_fail" },
	{ 0x51a910c0, "arm_copy_to_user" },
	{ 0x1000e51, "schedule" },
	{ 0x8f678b07, "__stack_chk_guard" },
	{ 0xc38c83b8, "mod_timer" },
	{ 0x526c3a6c, "jiffies" },
	{ 0x92d5838e, "request_threaded_irq" },
	{ 0x550257fc, "gpiod_to_irq" },
	{ 0x7bac67ed, "gpio_to_desc" },
	{ 0x1d37eeed, "ioremap" },
	{ 0x5f754e5a, "memset" },
	{ 0xc5850110, "printk" },
	{ 0xedc03953, "iounmap" },
	{ 0xc1514a3b, "free_irq" },
	{ 0xf9a482f9, "msleep" },
	{ 0x2660f6b5, "wake_up_process" },
	{ 0xb1ad28e0, "__gnu_mcount_nc" },
};

MODULE_INFO(depends, "");


MODULE_INFO(srcversion, "0DD4C444A9B0C3FEF8AEB03");
