#include "modules.h"
#include <stdio.h>

int main(int argc, char **argv)
{
    (void)argc;

    /* 1. Configuration du module racine pour capturer les options globales */
    module_option_t opt_verbose = {
        .s_opt = 'v',
        .l_opt = "verbose",
        .arg_name = NULL,
        .desc = "Enable verbose output"};

    module_option_t opt_help = {
        .s_opt = 'h',
        .l_opt = "help",
        .arg_name = NULL,
        .desc = "Show help message"};

    module_option_t *root_opts[] = {&opt_verbose, &opt_help};

    /* Configure le module racine AVANT le parsing pour gérer '-v', '-h', etc.
     */
    set_root_module("kernforgex", "[options] <command>", root_opts, 2);

    /* 2. Options pour le module de réseau "net" */
    module_option_t opt_interface = {
        .s_opt = 'i',
        .l_opt = "interface",
        .arg_name = "IFACE",
        .desc = "Network interface to use"};

    module_option_t *net_opts[] = {&opt_interface};

    /* Création du module de niveau 0 */
    module_t *net_mod =
        add_module("net", "[options] <subcommand>", net_opts, 1);

    add_module("nett", "[options] <subcommand>", net_opts, 1);

    /* 3. Options pour le sous-module "ip" (net -> ip) */
    module_option_t opt_v6 = {
        .s_opt = '6',
        .l_opt = "ipv6",
        .arg_name = NULL,
        .desc = "Force IPv6 resolution"};

    module_option_t *ip_opts[] = {&opt_v6};

    /* Création du sous-module attaché à "net" */
    add_submodule(net_mod, "ip", "[options] <address>", ip_opts, 1);

    /* 4. Execution du parser CLI (À APPELER APRES LA DÉCLARATION) */
    int ret = launch_cli(argv);

    printf("retour = %d\n", ret);

    return ret;
}