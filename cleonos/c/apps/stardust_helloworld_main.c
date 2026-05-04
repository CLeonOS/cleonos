extern int stardust_helloworld_entry(int argc, char **argv, char **envp);

int cleonos_app_main(int argc, char **argv, char **envp) {
    return stardust_helloworld_entry(argc, argv, envp);
}
