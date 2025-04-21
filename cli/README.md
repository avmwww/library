# Simple minimalistic cli interface 


## Example usage 

```
// Create CLI
struct cli *cli = cli_create();
cli_set_prompt(cli, "my-cli> ");

// Register user command
int my_command_handler(void *ctx, int argc, char **argv, void *priv) {
    struct cliq *cq = priv;
    cli_printf(cq, "Hello, %s!", argv[1]);
    return 0;
}

cli_cmd_register(cli, "greet", my_command_handler, NULL, "Say hello");

// Use telnet client
cli_connect(cli, selector, client_fd, -1);
```

Or see <example> dir 
