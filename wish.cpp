
#include <iostream>
#include <vector>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <string>
#include <cstring>
#include <fstream>
#include <sstream>
#include <algorithm>

using namespace std;

const string error_message = "An error has occurred\n";
vector<string> paths = {"/bin"}; // Default path where executables are searched

// Simple function to print the error message just so I dont need to keep writing it
void print_error() {
    write(STDERR_FILENO, error_message.c_str(), error_message.length());
}

// Helper function to trim whitespace from a string allowing for function calls with and without whitespace
string trim(const string &str) {
    size_t first = str.find_first_not_of(" \t"); // Skips over all spaces and tabs
    if (first == string::npos) return "";  // String is all whitespace
    size_t last = str.find_last_not_of(" \t");  // Find index of last character that is not whitespace
    return str.substr(first, last - first + 1); // Return string with no whitespace
}

// Function to parse user input into tokens (words)
vector<string> parse_input(const string &input) {
    vector<string> tokens;
    string token;
    istringstream tokenStream(input);
    while (tokenStream >> token) {
        tokens.push_back(token);
    }
    return tokens;
}

// Function to handle redirection parsing where `>` can have spaces or not
bool handle_redirection(const vector<string> &tokens, vector<string> &command_tokens, string &output_file) {
    bool redirection_found = false;

    for (size_t i = 0; i < tokens.size(); ++i) {
        string token = tokens[i];
        size_t redir_pos = token.find('>');

        if (redir_pos != string::npos) {
            
            string command_part = token.substr(0, redir_pos);  // Part of token before `>`
            string file_part = token.substr(redir_pos + 1);    // Part of token after `>`

            if (!command_part.empty()) {
                // Trim command part and push into command_tokens
                command_tokens.push_back(trim(command_part));
            }

            // If there is no command before '>' (handles calls which only contain '>'), print an error
            if (command_tokens.empty()) {
                print_error();  // No command before redirection
                return false;
            }

            if (!file_part.empty()) {
                output_file = trim(file_part);
                redirection_found = true;
            } else if (i + 1 < tokens.size()) {
                // Handle case like "command > file"
                if (i + 2 < tokens.size()) {  // More than one token after `>`
                    print_error();
                    return false;  // Invalid redirection: too many arguments after `>`
                }
                output_file = trim(tokens[i + 1]);
                redirection_found = true;
                break;
            } else {
                print_error();
                return false;  // Invalid redirection
            }
        } else if (token == ">") {
            // Handle case where redirection is separated by spaces "command > file"
            if (command_tokens.empty()) {
                print_error();  // No command before redirection
                return false;
            }
            if (i + 1 < tokens.size()) {
                if (i + 2 < tokens.size()) {  // More than one token after `>`
                    print_error();
                    return false;  // Invalid redirection: too many arguments after `>`
                }
                output_file = trim(tokens[i + 1]);
                redirection_found = true;
                break;
            } else {
                print_error();
                return false;  // Invalid redirection: no file specified
            }
        } else {
            // Normal command, push into the command_tokens vector
            command_tokens.push_back(token);
        }
    }

    return redirection_found || output_file.empty();
}

// Function to execute built-in commands (cd, path, exit)
bool execute_builtin_command(const vector<string> &tokens) {
    if (tokens.empty()) return false;

    // Handle `cd` command
    if (tokens[0] == "cd") {

        // If trying to execute cd command, we need to make sure that there are 2 arguments (cd and the directory you want to go to)
        if (tokens.size() != 2) {
            print_error();
        } else if (chdir(tokens[1].c_str()) != 0) {
            // If changing directory fails, print error
            print_error();
        }
        return true;  // Built-in command executed
    }

    // Handle `path` command
    if (tokens[0] == "path") {
        paths.clear();  // If trying to execute a path command, clear the default path
        for (size_t i = 1; i < tokens.size(); ++i) {
            paths.push_back(tokens[i]);  // Add each argument as a directory in the path
        }
        return true;  // Built-in command executed
    }

    // Handle `exit` command
    if (tokens[0] == "exit") {
        // When executing exit command, there can be no arguments so if tokens size > 1, print error
        if (tokens.size() > 1) {
            print_error();
        } else {
            exit(0);  // Exit the shell
        }
        return true;  // Built-in command executed
    }

    return false;  // Not a built-in command
}

// Function to execute a single external command
void execute_single_command(const vector<string> &tokens, const string &output_file = "") {
    if (tokens.empty()) return;

    int out_fd = STDOUT_FILENO;  // Default to standard output
    if (!output_file.empty()) {
        out_fd = open(output_file.c_str(), O_CREAT | O_WRONLY | O_TRUNC, S_IRWXU);
        if (out_fd < 0) {
            print_error();
            return;
        }
    }

    pid_t pid = fork();
    if (pid == 0) {
        // Child process
        if (!output_file.empty()) {
            // Redirect standard output and error to the output file 
            dup2(out_fd, STDOUT_FILENO);
            dup2(out_fd, STDERR_FILENO); 
        }

        // Prepare arguments for execv

        // Establish C-string array of size tokens+1 since it must be Null-terminated
        char *args[tokens.size() + 1];

        // Iterate through tokens and convert tokens elements to c-string and add to args 
        for (size_t i = 0; i < tokens.size(); ++i) {
            args[i] = const_cast<char *>(tokens[i].c_str());
        }
        args[tokens.size()] = nullptr;  // Null-terminate the arguments array

        // Iterate paths vector and store command we want to execute at the end of path then send to execv
        for (const auto &dir : paths) {
            string full_path = dir + "/" + tokens[0];
            execv(full_path.c_str(), args);
        }

        // If execv returns, an error occurred
        print_error();
        if (!output_file.empty()) close(out_fd);
        exit(1);
    } else if (pid > 0) {
        // In the parent process, wait for the child to finish
        wait(nullptr);
    } else {
        print_error();  // Fork failed
    }

    if (!output_file.empty()) {
        close(out_fd);  // Close the output file descriptor in the parent process
    }
}

// Function to execute a single command with redirection
void execute_command_with_redirection(const string &command) {
    vector<string> tokens = parse_input(command);
    vector<string> command_tokens;
    string output_file = "";

    if (!handle_redirection(tokens, command_tokens, output_file)) {
        return;  // Return if redirection is invalid
    }

    // Check if it's a built-in command
    if (execute_builtin_command(command_tokens)) {
        return;  // Built-in command executed
    }

    // Execute external command
    execute_single_command(command_tokens, output_file);
}

// Function to handle parallel commands
void execute_parallel_commands(const vector<string> &commands) {
    vector<pid_t> pids;
    for (const string &command : commands) {
        // Skip empty commands
        if (trim(command).empty()) {
            continue;
        }

        pid_t pid = fork();
        if (pid == 0) {
            // Child process: execute the command with redirection
            execute_command_with_redirection(command);
            exit(0);
        } else if (pid > 0) {
            // Parent process: collect child PIDs
            pids.push_back(pid);
        } else {
            print_error();  // Fork failed
        }
    }

    // Wait for all child processes to finish
    for (pid_t pid : pids) {
        waitpid(pid, nullptr, 0);
    }
}

// Main loop to run the shell
void run_shell(istream &input_stream, bool interactive) {
    string line;
    while (true) {
        if (interactive) {
            cout << "wish> ";
        }

        if (!getline(input_stream, line)) {
            // End of input
            break;
        }

        line = trim(line);  // Remove whitespace

        if (line == "&" || line.empty()) {
            continue;  // Skip if only argument is "&" or empty input
        }

        vector<string> commands;    // Vector that will store all commands from the getline
        stringstream ss(line);
        string command;
        while (getline(ss, command, '&')) {
            // Uses '&' as a delimiter, extracting all strings except for '&'
            if (!trim(command).empty()) {
                commands.push_back(trim(command));
            }
        }

        if (!commands.empty()) {
            if (commands.size() > 1) {
                execute_parallel_commands(commands);  // Handle parallel execution
            } else {
                // Single command case
                execute_command_with_redirection(commands[0]);
            }
        }
    }
}


int main(int argc, char *argv[]) {
    if (argc == 1) {
        run_shell(cin, true);  // Interactive mode
    } else if (argc == 2) {
        ifstream batch_file(argv[1]);
        if (!batch_file) {
            print_error();
            exit(1);
        }
        run_shell(batch_file, false);  // Batch mode
    } else {
        print_error();
        exit(1);
    }
    return 0;
}