#include "peerheader.h"
#include "file_share_peer.h"

int idx = 0;
int down = 0;
#define buff_size 1024

struct sd_and_cmnd
{
    int client_sd;
    string cmnd;
};

string get_path(string f_name)
{
    char cwd[1024];
    getcwd(cwd, sizeof(cwd));
    string path = cwd;
    path += "/test/" + f_name;
    return path;
}

bool send_chunk(int client_sd, file_info f, int chunkno)
{
    // cout << "sending chunk"
    //      << " " << f.f_name << chunkno << endl;
    string f_path = f.f_path;
    usleep(1000);

    int fd_from = open(f_path.c_str(), O_RDONLY);
    if (fd_from < 0)
    {

        usleep(10000);

        return false;
    }
    char *buffer = new char[chunk_size];
    bzero(buffer, chunk_size);
    int read_bytes = pread(fd_from, buffer, chunk_size, chunkno * chunk_size);
    if (read_bytes < 0)
    {
        close(fd_from);

        cout << "Error in reading file" << endl;
        return false;
    }
    int t = close(fd_from);
    // cout << "closed file" << endl;
    if (t < 0)
    {

        return false;
        // exit(0);
    }

    int sent_bytes = send(client_sd, buffer, read_bytes, 0);

    if (sent_bytes < 0)
    {
        close(fd_from);

        cout << "Error in sending Chunkno:" << chunkno << endl;
        return false;
    }

    delete[] buffer;
    if (sent_bytes == read_bytes)
        return true;
    else
        return false;
}

string process_cmnd(string cmnd, int client_sd)
{
    vector<string> tkns = tokenize(cmnd);
    string res = "";
    string f_name = tkns[1];

    if (str_cmp("send_bitmap", tkns[0]))
    {
        string f_name = tkns[1];
        for (auto it : files)
        {
            if (str_cmp(it.f_name, f_name))
            {
                vector<bool> bitmap = it.bitmp.first;
                string bitmap_str = "";
                for (auto it : bitmap)
                {
                    if (it)
                        bitmap_str += "1";
                    else
                        bitmap_str += "0";
                }
                int r = send(client_sd, bitmap_str.c_str(), chunk_size + 20, 0);
                if (r < 0)
                {
                    cout << "Error in sending bitmap" << endl;
                    return "Error in sending bitmap";
                }
                else
                {
                    cout << "Bitmap sent successfully" << endl;
                    return "Bitmap sent successfully";
                }
            }
        }
    }

    else if (str_cmp("get_last_chunk_size", tkns[0]))
    {
        string f_name = tkns[1];
        for (auto it : files)
        {
            if (str_cmp(it.f_name, f_name))
            {
                // cout << "Last chunk size: " << it.bitmp.second << endl;
                int last_chunk_size = it.bitmp.second;
                string last_chunk_size_str = to_string(last_chunk_size);
                // cout << "Last chunk size string: " << last_chunk_size_str << endl;

                int r = send(client_sd, last_chunk_size_str.c_str(), last_chunk_size_str.length(), 0);
                if (r < 0)
                {
                    cout << "Error in sending last chunk size" << endl;
                    return "Error in sending last chunk size";
                }
                else
                {
                    // cout << "Last chunk size sent successfully" << endl;
                    return "Last chunk size sent successfully";
                }
            }
        }
    }
    else if (str_cmp("send_chunk", tkns[0]))
    {
        string f_name = tkns[1];
        int chunkno = stoi(tkns[2]);
        for (auto it : files)
        {
            if (str_cmp(it.f_name, f_name))
            {
                bool status = send_chunk(client_sd, it, chunkno);
                if (!status)
                {
                    cerr << "Error in sending chunk" << endl;
                    return "Error in sending chunk";
                }
                else
                {
                    return "true";
                }
            }
        }
    }

    else if (str_cmp("get_filesize", tkns[0]))
    {
        string f_name = tkns[1];
        for (auto it : files)
        {
            if (str_cmp(it.f_name, f_name))
            {
                long long f_size = getfilesize(it.f_path);
                // cout << "File size: " << f_size << endl;
                string f_size_str = to_string(f_size);
                // cout << "File size string: " << f_size_str << endl;
                int r = send(client_sd, f_size_str.c_str(), sizeof(f_size_str), 0);
                if (r < 0)
                {
                    cout << "Error in sending file size" << endl;
                    return "Error in sending file size";
                }
                else
                {
                    // cout << "File size sent successfully" << endl;
                    return "File size sent successfully";
                }
            }
        }
    }
    else
    {
        res = "cannot recognize command";
    }

    return res;
}
bool sendack(string ack, int client_sd)
{
    cout << ack << endl;
    int status = send(client_sd, ack.data(), buff_size, 0);
    if (status <= 0)
    {
        return false;
    }
    else
    {
        return true;
    }
}

// void *client_multithreading(void *sd_and_cmnd)
// {
//     int client_sd = ((int *)sd_and_cmnd->client_sd);
//     string cmnd = ((string *)sd_and_cmnd)[1];
//     string res = process_cmnd(cmnd, client_sd);
//     sendack(res, client_sd);
//     close(client_sd);
//     pthread_exit(NULL);
// }

void *client_multithreading(void *sd_and_cmnd)
{
    struct sd_and_cmnd *sd_cmnd = (struct sd_and_cmnd *)sd_and_cmnd;
    int client_sd = sd_cmnd->client_sd;
    string cmnd = sd_cmnd->cmnd;
    string ack = process_cmnd(cmnd, client_sd);
    if (str_cmp(ack, "true"))
    {
        // cout << "Downloaded chunk Successfully" << endl;
        return NULL;
    }
    sendack(ack, client_sd);
    close(client_sd);
    pthread_exit(NULL);
}

void *handle_multiple_clients(void *client_sd)
{
    int client_socket = *(int *)client_sd;
    char cmnd[buff_size] = {0};
    int count = 0;
    pthread_t threadid[1000];
    int i = 0;
    while (1)
    {
        count++;

        // string fil_name = "abc.txt";

        // int fil_size = 4918;
        // bool status = savefile(fil_name, fil_size, client_socket);
        // status ? cout << "File Received" << endl : cout << "File not Received: Try Again" << endl;
        bzero(cmnd, buff_size);

        int n = recv(client_socket, cmnd, buff_size, 0);
        if (n <= 0)
        {
            // cout << "Client Disconnected" << endl;
            break;
        }
        // cout << "Command Recived: " << cmnd << endl;

        struct sd_and_cmnd *sd_and_cmnd = new struct sd_and_cmnd;
        sd_and_cmnd->client_sd = client_socket;
        sd_and_cmnd->cmnd = cmnd;
        // usleep(10000);
        // pthread_t threadid;
        pthread_create(&threadid[i++], NULL, client_multithreading, (void *)sd_and_cmnd);
        if (i > 900)
        {
            i = 0;
        }
        // pthread_detach(threadid);
        usleep(10000);
        // pthread_detach(tid);
    }
    return NULL;
}

void run_server(int server_sd)
{
    // cout << "Inside run_server" << endl;
    sockaddr_in client_addr;
    socklen_t client_addr_size = sizeof(client_addr);

    // int i = 0;

    while (1)
    {

        int client_sd = accept(server_sd, (sockaddr *)&client_addr, &client_addr_size);
        if (client_sd < 0)
        {
            cout << "Error in accepting client" << endl;
            return;
        }
        // cout << "Peer connected" << endl;
        // usleep(1000);
        pthread_t thread_id;
        pthread_create(&thread_id, NULL, handle_multiple_clients, (void *)&client_sd);
        pthread_detach(thread_id);
        // if (i >= 1000)
        // {
        //     for (int j = 0; j < i; j++)
        //     {
        //         pthread_join(thread_id[j], NULL);
        //     }
        // }

        // char cmnd[1024];
        // bzero(cmnd, 1024);
        // int n = recv(client_sd, cmnd, 1024, 0);

        // if (n <= 0)
        // {
        //     cout << "Error in receiving command" << endl;
        //     break;
        // }

        // // Handling multiple commands from other peers:-
        //  if (strcmp(cmnd, "send_file") == 0)
        //  {
        //      if (pthread_create(&thread_id[i], NULL, handle_multiple_clients, &client_sd) != 0)
        //      {
        //          cout << "Error in creating thread" << endl;
        //          break;
        //      }
        //      i++;
        //  }
        //  else
        //  {
        //      cout << "Invalid command" << endl;
        //      break;
        //  }

        //     if (client_sd < 0)
        //     {
        //         cout << "Error in accepting client" << endl;
        //         // break;
        //     }
        //     cout << "Client " << idx << " connected" << endl;
        //     idx++;
        //     pthread_create(&thread_id[i++], NULL, handle_multiple_clients, &client_sd);
    }

    close(server_sd);
}

void *server(void *p)
{
    int port = *(int *)p;
    sockaddr_in server_addr;
    bzero((char *)&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(port);

    int server_sd = socket(AF_INET, SOCK_STREAM, 0);

    if (server_sd < 0)
    {
        cerr << "Cannot Acquire Socket" << endl;
        exit(0);
    }
    int bind_status = bind(server_sd, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (bind_status < 0)
    {
        cerr << "Cannot Bind Socket" << endl;
        exit(0);
    }

    listen(server_sd, 15);

    run_server(server_sd);
    return NULL;
}
void extract_port(vector<string> &tracker_info, string path)
{
    fstream file;
    file.open(path.c_str(), ios::in);
    string t;
    while (getline(file, t))
    {
        tracker_info.push_back(t);
    }
    file.close();
}

int main(int argc, char const *argv[])
{
    if (argc != 3)
    {
        cerr << "Enter port number";
        exit(0);
    }
    string peer_ip_port = argv[1];
    string tracker_path = argv[2];

    vector<string> p_ip_port = extract_ip_port(peer_ip_port);

    int listening_port = stoi(p_ip_port[1]);

    // server(port);
    pthread_t t;
    pthread_create(&t, NULL, server, &listening_port);

    int fd = open(tracker_path.c_str(), O_RDONLY);
    if (fd < 0)
    {
        cout << "Error in opening tracker file" << endl;
        exit(0);
    }
    char tracker_ip_port[100];
    bzero(tracker_ip_port, 100);
    read(fd, tracker_ip_port, 100);
    close(fd);
    string t_ip_port = tracker_ip_port;

    vector<string> t_ip_port_vec = extract_ip_port(t_ip_port);

    int server_port = stoi(t_ip_port_vec[1]);
    string servr_ip = t_ip_port_vec[0];
    init_peer(server_port, servr_ip, listening_port);

    pthread_join(t, NULL);
}