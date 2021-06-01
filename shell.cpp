#include <iostream>
#include <vector>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>    
#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
using namespace std;

static char firstOld[256];
static char lastOld[256];
static bool visited;
static vector<pid_t> backProc;
static vector<string> nameProc;
static pid_t pd;

string trim (string input){
    int i=0;
    while (i < input.size() && input [i] == ' ')
        i++;
    if (i < input.size())
        input = input.substr (i);
    else{
        return "";
    }
    
    i = input.size() - 1;
    while (i>=0 && input[i] == ' ')
        i--;
    if (i >= 0)
        input = input.substr (0, i+1);
    else
        return "";
    
    return input;
    

}

vector<string> split (string line, char separator=' '){
    vector<string> result;
    string segment="";
    bool inQuote = false;
    bool inBrace = false;
    for (int found=0; found<line.size(); found++)
    {
        if (line.at(found) == '"' || line.at(found) == '\'')
        {
            //segment += line.at(found);
            inQuote = !inQuote;
        }
        else if (line.at(found) == separator)
        {
            if(inQuote)
            {
                segment += line.at(found);
            }
            else
            {
                segment = trim(segment);
                result.push_back(segment);
                
                segment = "";
            }
        }
        else if (line.at(found) == '{')
        {
            segment += line.at(found);
            inBrace = true;
        }
        else if (line.at(found) == '}')
        {
            segment += line.at(found);
            inBrace = false;
        }
        else if (line.at(found) == '<')
        {
            if(inQuote)
            {
                segment += '\a';
            }
            else
            {
                segment += line.at(found);
            }
        }
        else if (line.at(found) == '>')
        {
            if(inQuote)
            {
                segment += '\v';
            }
            else
            {
                segment += line.at(found);
            }
        }
        else
        {
            if (inBrace)
            {
                if (line.at(found) != ' ')
                    segment += line.at(found);
            }
            else
                segment += line.at(found);
        }
    }
    segment = trim(segment);
    if (segment.length())
        result.push_back(segment);
    //cout << result.size() << endl;
    //for (int i=0; i<result.size(); i++)
    //    cout << result[i] << endl;
    return result;
    //vector<string> results;
    //return results; 
}

char** vec_to_char_array (vector<string> parts){
    char ** result = new char * [parts.size() + 1]; // add 1 for the NULL at the end
    for (int i=0; i<parts.size(); i++){
        // allocate a big enough string
        result [i] = new char [parts [i].size() + 1]; // add 1 for the NULL byte
        strcpy (result [i], parts[i].c_str());
    }
    result [parts.size()] = NULL;
    return result;
}

string decode (string com)
{
    string ret = "";
    for (int f=0; f<com.size(); f++)
    {
        if (com.at(f) == '\a')
        {
            ret += '<';
        }
        else if (com.at(f) == '\v')
        {
            ret += '>';
        }
        else
        {
            ret += com.at(f);
        }
    }
    return ret;
}

void execute (string command){
    size_t LTidx = command.find("<");
    size_t GTidx = command.find(">");
    if(LTidx != string::npos && GTidx != string::npos)
    {
        if(LTidx < GTidx)
        {
            string fileTo = command.substr(GTidx + 1);
            string fileFrom = command.substr(LTidx + 1, GTidx-LTidx-1);
            fileTo = trim(fileTo);
            fileFrom = trim(fileFrom);
            command = command.substr(0, LTidx);

            int fdOut = open(fileTo.c_str(), O_CREAT|O_WRONLY|O_TRUNC,S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
            dup2 (fdOut, 1);
            int fdIn = open(fileFrom.c_str(), O_RDONLY);
            dup2 (fdIn, 0);
        }
        else
        {
            string fileTo = command.substr(LTidx + 1);
            string fileFrom = command.substr(GTidx + 1, LTidx-GTidx-1);
            fileTo = trim(fileTo);
            fileFrom = trim(fileFrom);
            command = command.substr(0, GTidx);

            int fdOut = open(fileTo.c_str(), O_CREAT|O_WRONLY|O_TRUNC,S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
            dup2 (fdOut, 1);
            int fdIn = open(fileFrom.c_str(), O_RDONLY);
            dup2 (fdIn, 0);
        }
    }
    else if(LTidx != string::npos)
    {
        string fileFrom = command.substr(LTidx + 1);
        fileFrom = trim(fileFrom);
        command = command.substr(0, LTidx);

        int fdIn = open(fileFrom.c_str(), O_RDONLY);
        dup2 (fdIn, 0);
    }
    else if(GTidx != string::npos)
    {
        string fileTo = command.substr(GTidx + 1);
        fileTo = trim(fileTo);
        command = command.substr(0, GTidx);

        int fdOut = open(fileTo.c_str(), O_CREAT|O_WRONLY|O_TRUNC,S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
        dup2 (fdOut, 1);
    }

    command = decode(command);

    vector<string> argstrings = split (command, ' '); // split the command into space-separated parts
    char** args = vec_to_char_array (argstrings);// convert vec<string> into an array of char*
    if(argstrings[0] == "cd")
    {
        getcwd(firstOld, sizeof(firstOld)); //put curr directiory into buffer
        if(argstrings[1] == "-")
        {
            if(visited)
                chdir(lastOld);
            else
                cout << "OLDPWD not set" << endl;
        }
        else
        {
            chdir(args[1]);
            visited = true;
        }
        strcpy(lastOld, firstOld);
    }
    if(argstrings[0] == "jobs")
    {
        cout << "Jobs:" << endl;
        if(nameProc.size()>0)
        {
            for(int j=0; j<nameProc.size(); j++)
                cout << backProc[j] << " - " << nameProc[j] << endl;
            //cout << backProc.size() << endl;
        }
        else
            cout << "No jobs." << endl;
    }
    else
    {
        //cout << args[0];
        execvp (args[0], args);
        //cout << "we executed";
    }
}

int main (){
    int orig_stdin = dup(0);
    int orig_stdout = dup(1);
    visited = false;
    while (true){ // repeat this loop until the user presses Ctrl + C
        bool amp = false;
        time_t my_time = time(NULL); //get the time
        char* date = asctime(localtime(&my_time)); //convert into char*
        date[strlen(date) - 1] = '\0'; //trim off the next line
        char buf[256]; //getcwd requires a buffer
        printf("%s %s:~%s$ ",date,getenv("USER"),getcwd(buf, sizeof(buf)));
        string commandline = "";/*get from STDIN, e.g., "ls  -la |   grep Jul  | grep . | grep .cpp" */
        getline(cin, commandline);
        //cout << commandline;
        // split the command by the "|", which tells you the pipe levels
        vector<string> tparts = split (commandline, '|');
        // for each pipe, do the following:
        if(tparts[0].find("&") != string::npos)
        {
            amp = true;
            tparts[0].erase(tparts[0].find("&"));
        }
        for (int i=0; i<tparts.size(); i++){
            // make pipe
            int fd[2];
            pipe(fd);
            pd = fork();
			if (!pd){
                // redirect output to the next level
                // unless this is the last level
                if (i < tparts.size() - 1){
                    // redirect STDOUT to fd[1], so that it can write to the other side
                    dup2(fd[1], 1);
                    close (fd[1]);   // STDOUT already points fd[1], which can be closed
                }
                //execute function that can split the command by spaces to 
                // find out all the arguments, see the definition
                execute (tparts [i]); // this is where you execute
            }else{
                if (!amp)//when not &
                {
                    //wait(0);            // wait for the child process
                    waitpid(pd, 0, 0);
                }
				// then do other redirects
                dup2(fd[0], 0);
                close(fd[1]);
            }
        }
        usleep(50000);
        dup2(orig_stdin, 0);
        dup2(orig_stdout, 1);
        if(amp)
        {
            backProc.push_back(pd);
            nameProc.push_back(commandline);
        }
        for (int p=0; p<backProc.size(); p++)
        {
            pid_t status = waitpid(backProc[p], 0, WNOHANG);
            //cout << status << endl;
            if(status != 0)
            {
                backProc.erase(backProc.begin()+p);
                nameProc.erase(nameProc.begin()+p);
                //p--;
            }
        }
    }
}