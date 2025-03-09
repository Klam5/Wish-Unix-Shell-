### Basic Shell

Simple shell that constantly runs in a while loop asking for a command which it will execute.
Enter exit when you want to stop.

Made use of `fork()`, `exec()`, and `wait()/waitpid()` to help execute commands.



* `exit`: When the user types `exit`, your shell should simply call the `exit`
  system call with 0 as a parameter. It is an error to pass any arguments to
  `exit`. 

* `cd`: `cd` always take one argument (0 or >1 args should be signaled as an
error). To change directories, use the `chdir()` system call with the argument
supplied by the user; if `chdir` fails, that is also an error.

* `path`: The `path` command takes 0 or more arguments, with each argument
  separated by whitespace from the others. A typical usage would be like this:
  `wish> path /bin /usr/bin`, which would set `/bin` and `/usr/bin` as the
  search path of the shell. If the user sets path to be empty, then the shell
  should not be able to run any programs (except built-in commands). The
  `path` command always overwrites the old path with the newly specified
  path. 

### Redirection

This shell is capable of redirecting outputs of one file and sending it as an
input to another. For example, if a user types `ls -la /tmp > output`, 
instead of printing the output, the standard output of the `ls` 
program is rerouted to the file `output`.

If the `output` file exists before you run your program, it will overwrite the file. 


### Parallel Commands

Supports parallel executions with the use of "&" :

```
wish> cmd1 & cmd2 args1 args2 & cmd3 args1
```

each of these commands will run with their respective parameters in parallel
instead of waiting for one to finish before running the next command
