extern int stardust_layout_entry(int argc, char **argv, char **envp);

int cleonos_app_main(int argc, char **argv, char **envp) {
    return stardust_layout_entry(argc, argv, envp);
}
