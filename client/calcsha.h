
#include <openssl/sha.h>

#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string>
#include <iostream>

using namespace std;
long long getfilesize(string path)
{
    // curr working directory
    // string path = getpath(filename);

    struct stat st;
    int rc = stat(path.c_str(), &st);
    if (rc == 0)
    {
        return st.st_size;
    }
    else
    {
        return -1;
    }
}
string calcsha1(string path)
{

    long long f_size = getfilesize(path);
    char *buffer = new char[f_size];

    int fd_from = open(path.c_str(), O_RDONLY);
    read(fd_from, buffer, f_size);
    close(fd_from);
    unsigned char hash[SHA_DIGEST_LENGTH];
    bzero(hash, SHA_DIGEST_LENGTH);
    SHA1((unsigned char *)buffer, f_size, hash);
    char mdString[SHA_DIGEST_LENGTH * 2 + 1];
    for (int i = 0; i < SHA_DIGEST_LENGTH; i++)
        sprintf(&mdString[i * 2], "%02x", hash[i]);

    string res = "";
    for (int i = 0; i < SHA_DIGEST_LENGTH; i++)
    {
        if (mdString[i] != '\0')
            res += mdString[i];
    }
    delete[] buffer;

    return res;
}

string calcsha1_chunk(string f_path, long long off, long long chunk_size)
{

    char *buffer = new char[chunk_size];
    int fd_from = open(f_path.c_str(), O_RDONLY);
    pread(fd_from, buffer, chunk_size, off);
    close(fd_from);

    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1((unsigned char *)buffer, chunk_size, hash);
    char mdString[SHA_DIGEST_LENGTH * 2 + 1];
    for (int i = 0; i < SHA_DIGEST_LENGTH; i++)
        sprintf(&mdString[i * 2], "%02x", hash[i]);

    string res = "";
    for (int i = 0; i < SHA_DIGEST_LENGTH; i++)
    {
        res += mdString[i];
    }
    delete[] buffer;
    return res;
}
