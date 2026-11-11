#include <grub/command.h>
#include <grub/term.h>
#include <grub/dl.h>
#include <grub/misc.h>

GRUB_MOD_LICENSE ("GPLv3+");

static grub_command_t b_test; 

/**
 * @brief defines the command of bro_test, this function is simply for test.
 * @param cmd used to tell which grub command will be executed.
 * @param argc stores the number of input the user gave during `bro_test`.
 * @param args stores the all the inputs which the user gave during `bro_test`
 *
 * if user types, `bro_test hello world` then,
 * 
 * argc = 3 
 * 
 * args = ["bro_test", "hello", "world"]
 * 
 * #### __attribute__((unused)) is used just to tell the compiler that this param will not be used in the function
*/
static grub_err_t
test(grub_command_t cmd __attribute__((unused)), int argc __attribute__((unused)), char **argv __attribute__((unused)))
{
    grub_term_normal_color = 0x03;
    grub_setcolorstate(GRUB_TERM_COLOR_NORMAL);
    grub_printf("This is super BRO TEST from your bro!\n");
    grub_setcolorstate(GRUB_TERM_COLOR_STANDARD);
    return GRUB_ERR_NONE;
}
GRUB_MOD_INIT(bro_test){
    b_test = grub_register_command("bro_test", test, "This is just for a test tbh", "same as summary.");
}

GRUB_MOD_FINI(bro_test){}