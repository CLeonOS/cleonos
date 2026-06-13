extern int cleonos_rust_shell_main(int argc, char **argv, char **envp);

int cleonos_app_main(int argc, char **argv, char **envp) {
    return cleonos_rust_shell_main(argc, argv, envp);
}
