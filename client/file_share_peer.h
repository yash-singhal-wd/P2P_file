#include "peerheader.h"
#include <dirent.h>
#include "calcsha.h"
#define buff_size 1024
#define chunk_size 524288
#define true_login "User logged in Successfully"
int tracker_sd;
string getpath(string filename)
{
    char cwd[1024];
    getcwd(cwd, sizeof(cwd));
    string path = cwd;
    path += "/" + filename;
    return path;
}
struct addr_info
{
    int PORT;
    string IP;
    int socket;
};

struct meta_data
{
    string full_hash;
    long long file_size;
    vector<addr_info> seeders;
    vector<pair<addr_info, string>> leeches;
};

// struct thread_args
// {
//     string f_name;
//     string peer;
//     string d_path;
//     long long chunk_no;
//     long long last_chunk_size;
//     long long no_of_chunks;
//     bool *status;
// };

// deque<string> started_downloads;
// deque<string> partial_downloads;
// deque<string> completed_downloads;

unordered_map<string, vector<string>> started_downloads;   // [GroupID] = [file1,file2,file3]
unordered_map<string, vector<string>> partial_downloads;   // [GroupID] = [file1,file2,file3]
unordered_map<string, vector<string>> completed_downloads; // [GroupID] = [file1,file2,file3]

unordered_map<string, bool> connected_to;

struct file_info
{
    string f_name;
    string f_path;
    long long f_size;
    string hash;
    long long no_of_chunks;
    pair<vector<bool>, int> bitmp; ///{{1,1,0,1,1,0},521};  {{chunks_present},sizeoflastchunk}
    vector<string> chunk_hashes;
    bool seeder;
};
struct argv
{
    vector<string> peers_list;
    string f_name;
    string d_path;
    string g_id;
    long long chunk_no;
    long long last_chunk_size;
    long long no_of_chunks;
    file_info *f_info;
    bool *downloaded_first_chunk;
    bool *downloaded_all;
    // bool *status;
};
vector<file_info> files;
bool str_cmp(string str1, string str2)
{
    return (strcmp(str1.c_str(), str2.c_str()) == 0);
}

// long long getfilesize(string path)
// {
//     // curr working directory
//     // string path = getpath(filename);

//     struct stat st;
//     int rc = stat(path.c_str(), &st);
//     if (rc == 0)
//     {
//         return st.st_size;
//     }
//     else
//     {
//         return -1;
//     }
// }

vector<string> tokenize(string str)
{
    string delm = " ";
    vector<string> tokens;
    string token;

    for (unsigned i = 0; i < str.length(); i++)
    {
        if (str[i] == delm[0])
        {
            tokens.push_back(token);

            token = "";
        }
        else
        {
            token += str[i];
        }
    }
    tokens.push_back(token);

    return tokens;
}

vector<string> extract_ip_port(string str)
{
    string delm = ":";
    vector<string> tokens;
    string token;

    for (unsigned i = 0; i < str.length(); i++)
    {
        if (str[i] == delm[0])
        {
            tokens.push_back(token);

            token = "";
        }
        else
        {
            token += str[i];
        }
    }
    tokens.push_back(token);

    return tokens;
}

meta_data proccess_meta_data(string response)
{
    vector<string> tokens = tokenize(response);
    meta_data m;
    m.full_hash = tokens[tokens.size() - 1];
    tokens.pop_back();

    for (unsigned i = 0; i < tokens.size(); i++)
    {
        vector<string> ip_port = extract_ip_port(tokens[i]);
        addr_info a;
        a.IP = ip_port[0];
        a.PORT = stoi(ip_port[1]);
        m.seeders.push_back(a);
    }

    return m;
}

void create_file(long long file_size, string f_name, string d_path)
{
    // creating file in d_path and writing NULL in it
    string f_path = d_path + "/" + f_name;
    // cout << "Creating file " << f_path << endl;
    // cout << "File size: " << file_size << endl;
    ofstream f(f_path, ios::binary);
    for (long long i = 0; i < file_size; i++)
    {
        f << '0';
    }

    f.close();
}
// void createfile(long long fileSize, string f_name, string d_path)
// {

//     // string filePath = "./output.txt";
//     // int fileSize = 32;
//     string filePath = d_path + "/" + f_name;
//     ifstream f(filePath);
//     if (!f.good())
//     {
//         // create empty file
//         vector<char> empty(fileSize, '0');
//         ofstream ofs(filePath, std::ios::out);

//         for (int i = 0; i < fileSize; i++)
//         {
//             if (!ofs.write(&empty[0], 1))
//             {
//                 exit(EXIT_FAILURE);
//             }
//         }
//         ofs.close();
//     }
// }
int connect_to(string IP, int PORT)
{
    int client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket < 0)
    {
        cout << "Error in creating socket" << endl;
        return -1;
    }

    struct sockaddr_in server_address;
    bzero((char *)&server_address, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons((PORT));
    server_address.sin_addr.s_addr = inet_addr(IP.c_str());

    int connection_status = connect(client_socket, (struct sockaddr *)&server_address, sizeof(server_address));
    if (connection_status < 0)
    {
        cout << "Error in invoking connecting call Please Give Correct IP & Port" << endl;
        return -1;
    }
    string ip_port = IP + ":" + to_string(PORT);
    // cout << "Connected to " << ip_port << endl;
    connected_to[ip_port] = true;
    return client_socket;
}
void close_connection(int client_socket, string ip, int port)
{
    close(client_socket);
    string ip_port = ip + ":" + to_string(port);
    // cout << "Disconnected from " << ip_port << endl;
    connected_to[ip_port] = false;
    return;
}

vector<bool> extract_bitmap(char *buffer, long long no_of_chunks)
{
    vector<bool> bitmap(no_of_chunks, false);
    for (long long i = 0; i < no_of_chunks; i++)
    {
        if (buffer[i] == '1')
        {
            bitmap.push_back(true);
        }
        else if (buffer[i] == '0')
        {
            bitmap.push_back(false);
        }
        else
        {
            break;
        }
    }
    return bitmap;
}

void update_freq(char *buffer, unordered_map<int, pair<vector<string>, int>> &chunks_freq, long long no_of_chunks, string IP, int PORT)
{
    vector<bool> chunks = extract_bitmap(buffer, no_of_chunks);
    for (unsigned i = 0; i < chunks.size(); i++)
    {
        if (chunks[i])
        {

            chunks_freq[i].first.push_back(IP + ":" + to_string(PORT));
            chunks_freq[i].second++;
            // chunks_freq[i].first.push_back(IP + ":" + to_string(PORT));
            // chunks_freq[i].second++;
        }
    }
    return;
}

// comparator for sorting chunks_freq
// bool cmp(pair<vector<string>, int> p1, pair<vector<string>, int> p2)
// {
//     if (p1.second == p2.second)
//     {
//         return p1.first.size() < p2.first.size();
//     }
//     return p1.second < p2.second;
// }

int count_connected_peers(vector<string> peers_list)
{
    int count = 0;
    for (auto it : peers_list)
    {
        if (connected_to[it])
        {
            count++;
        }
    }

    return count;
}

bool cmp(pair<int, pair<vector<string>, int>> a, pair<int, pair<vector<string>, int>> b)
{
    if (a.second.second == b.second.second)
    {
        return count_connected_peers(a.second.first) > count_connected_peers(b.second.first);
    }
    return a.second.second > b.second.second;
}

void sorting_map(unordered_map<int, pair<vector<string>, int>> &chunks_freq)
{
    vector<pair<int, pair<vector<string>, int>>> vec;
    for (auto it = chunks_freq.begin(); it != chunks_freq.end(); it++)
    {
        vec.push_back(make_pair(it->first, it->second));
    }
    sort(vec.begin(), vec.end(), cmp);
    chunks_freq.clear();
    for (auto it = vec.begin(); it != vec.end(); it++)
    {
        chunks_freq[it->first] = it->second;
    }
}

void apply_piece_selection(unordered_map<int, pair<vector<string>, int>> &chunks_freq)
{
    sorting_map(chunks_freq);

    // printing chunks_freq and connected_to
    // for (auto it = chunks_freq.begin(); it != chunks_freq.end(); it++)
    // {
    //     cout << it->first << " " << it->second.second << " " << count_connected_peers(it->second.first) << endl;
    // }

    return;
}
bool connect_and_download(string ip_port, string f_name, string d_path, unsigned chunk_no, long long last_chunk_size, long long no_of_chunks)
{

    vector<string> ip_port_vec = extract_ip_port(ip_port);
    string IP = ip_port_vec[0];
    int PORT = stoi(ip_port_vec[1]);
    string f_path = d_path + "/" + f_name;
    int peer_sd = connect_to(IP, PORT);
    if (peer_sd == -1)
    {
        return false;
    }

    string request = "send_chunk " + f_name + " " + to_string(chunk_no);
    send(peer_sd, request.c_str(), request.length(), 0);
    if (chunk_no == no_of_chunks - 1)
    {
        char *buffer = new char[last_chunk_size];
        bzero(buffer, last_chunk_size);
        char *chunk = new char[last_chunk_size];
        long long bytes_read = 0;
        while (bytes_read < last_chunk_size)
        {
            long long n = recv(peer_sd, buffer, last_chunk_size, 0);
            if (n <= 0)
            {
                break;
            }

            for (long long i = bytes_read; i < bytes_read + n; i++)
            {
                chunk[i] = buffer[i - bytes_read];
            }
            bytes_read += n;
            // chunk += buffer;
            bzero(buffer, last_chunk_size);
        }
        // cout << "Bytes read out of" << last_chunk_size << " " << bytes_read << endl;
        delete[] buffer;

        if (bytes_read == last_chunk_size)
        {
            // ofstream fout;
            // fout.open(f_path, ios::out | ios::binary | ios::in);
            // fout.seekp(chunk_no * chunk_size + 1);
            // fout.write(chunk, last_chunk_size);
            // fout.close();
            // using pwrite instead
            // cout << "Last Writing chunk " << chunk_no << " " << chunk << endl;
            int fd = open(f_path.c_str(), O_WRONLY | O_CREAT, 0777);
            pwrite(fd, chunk, last_chunk_size, chunk_no * chunk_size);
            int t = close(fd);
            // cout << "closed last chunk file" << endl;

            if (t < 0)
            {
                cout << "Error in closing file" << endl;
                cout << "Error code = " << errno << endl;
                cout << "Error name = " << strerror(errno) << endl;
            }
            delete[] chunk;
            close_connection(peer_sd, IP, PORT);

            return true;
        }
        else
        {
            close_connection(peer_sd, IP, PORT);
            // delete[] chunk;
            return false;
        }
    }
    else
    {
        char *buffer = new char[chunk_size];
        bzero(buffer, chunk_size);
        char *msg = new char[chunk_size];
        long long bytes_read = 0;
        while (bytes_read < chunk_size)
        {
            long long bytes = recv(peer_sd, buffer, chunk_size, 0);
            if (bytes <= 0)
            {
                break;
            }
            for (long long i = bytes_read; i < bytes_read + bytes; i++)
            {
                msg[i] = buffer[i - bytes_read];
            }

            bytes_read += bytes;
            bzero(buffer, chunk_size);
        }
        delete[] buffer;
        // cout << "Bytes read out of" << chunk_size << " " << bytes_read << endl;
        // recv(peer_sd, buffer, chunk_size, 0);
        if (bytes_read == chunk_size)
        {

            int fd = open(f_path.c_str(), O_WRONLY | O_CREAT, 0777);
            pwrite(fd, msg, chunk_size, chunk_no * chunk_size);

            int t = close(fd);
            // cout << "closed file" << endl;
            if (t < 0)
            {
                cout << "Error in closing file" << endl;
                cout << "Error code = " << errno << endl;
                cout << "Error name = " << strerror(errno) << endl;
            }

            close_connection(peer_sd, IP, PORT);
            delete[] msg;
            return true;
        }
        else
        {
            close_connection(peer_sd, IP, PORT);
            // delete[] msg;
            return false;
        }

        // ofstream f(f_path, ios::binary);
        // f.seekp(chunk_no * chunk_size);
        // f.write(msg.data(), chunk_size);
        // f.close();
    }
}

void handle_update_leeches(string f_name, file_info *f, long long chunk_no, long long no_of_chunks, long long last_chunk_size, string d_path, string g_id)
{

    if (chunk_no < no_of_chunks)
    {
        f->f_name = f_name;
        f->f_path = d_path + "/" + f_name;
        f->no_of_chunks = no_of_chunks;
        f->bitmp.first.resize(f->no_of_chunks, 0);
        f->bitmp.first[chunk_no] = 1;
        f->bitmp.second = last_chunk_size;
        f->chunk_hashes.resize(f->no_of_chunks);
        // f->chunk_hashes[chunk_no] = get_hash(f->f_path, chunk_no, f->bitmp.second);
        f->seeder = false;

        // sending info to tracker
        for (auto i = started_downloads[g_id].begin(); i < started_downloads[g_id].end(); i++)
        {
            if (str_cmp(*i, f_name))
            {
                started_downloads[g_id].erase(i);
                partial_downloads[g_id].push_back(f_name);
                break;
            }
        }

        files.push_back(*f);
    }

    string request = "update_leeches " + g_id + " " + f_name;
    int n = send(tracker_sd, request.c_str(), request.length(), 0);
    if (n <= 0)
    {
        cout << "Error in updating download status to tracker" << endl;
        return;
    }
}

void handle_update_bitmap(string f_name, long long chunk_no)
{
    for (auto &f : files)
    {
        if (str_cmp(f.f_name, f_name))
        {
            f.bitmp.first[chunk_no] = 1;
            // f.chunk_hashes[chunk_no] = get_hash(f.f_path, chunk_no, f.bitmp.second);
            break;
        }
    }
}

void *connect_and_download_multithreaded(void *args)
{

    struct argv *t_args = (struct argv *)args;
    vector<string> peers_list = t_args->peers_list;

    for (auto it : peers_list)
    {
        // cout << "Downloading from " << it << endl;
        string ip_port = it;
        string f_name = t_args->f_name;
        string d_path = t_args->d_path;
        unsigned chunk_no = t_args->chunk_no;
        long long last_chunk_size = t_args->last_chunk_size;
        long long no_of_chunks = t_args->no_of_chunks;
        bool state = connect_and_download(ip_port, f_name, d_path, chunk_no, last_chunk_size, no_of_chunks);
        *(t_args->downloaded_all) = *(t_args->downloaded_all) && state;

        if (state)
        {
            if (*(t_args->downloaded_first_chunk) == false)
            {
                *(t_args->downloaded_first_chunk) = true;
                handle_update_leeches(f_name, t_args->f_info, chunk_no, no_of_chunks, last_chunk_size, d_path, t_args->g_id); // first_chunk_downloaded update leeeches and create file_info push it to files vector
            }
            else
            {
                handle_update_bitmap(f_name, chunk_no);
            }

            return NULL;
        }
    }
    return NULL;
}

void handle_update_seeder(string f_name, string d_path, string g_id, long long no_of_chunks, long long last_chunk_size, long long f_size)
{

    // cout << "Seeder " << f_name << " " << d_path << " " << g_id << " " << no_of_chunks << " " << last_chunk_size << endl;

    // for (auto it : partial_downloads[g_id])
    // {
    //     if (str_cmp(it, f_name))
    //     {

    //         partial_downloads[g_id].erase(it);
    //         completed_downloads[g_id].push_back(f_name);
    //         break;
    //     }
    // }
    cout << "File_Name: " << f_name << endl;
    string request = "update_seeder " + g_id + " " + f_name;
    send(tracker_sd, request.c_str(), buff_size, 0);

    for (auto it = partial_downloads[g_id].begin(); it < partial_downloads[g_id].end(); it++)
    {
        if (str_cmp(*it, f_name))
        {
            partial_downloads[g_id].erase(it);
            completed_downloads[g_id].push_back(f_name);
            break;
        }
    }

    for (auto it : files)
    {
        if (str_cmp(it.f_name, f_name))
        {
            it.f_size = f_size;
            it.seeder = true;
            break;
        }
    }
}

void start_downloading_chunks(unordered_map<int, pair<vector<string>, int>> &chunks_freq, string f_name, string d_path, long long last_chunk_size, long long no_of_chunks)
{
    // cout << "Dowloading chunks..." << endl;

    string f_path = d_path + "/" + f_name;
    cout << "Downloading file to:- " << f_path << endl;
    // int j = 0;
    bool res = true;
    for (auto it = chunks_freq.begin(); it != chunks_freq.end(); it++)
    {
        int chunk_no = it->first;
        vector<string> peers_list = it->second.first;
        bool flag = 0;
        for (auto it2 = peers_list.begin(); it2 != peers_list.end(); it2++)
        {
            // have to do multii-threading here
            // cout << "Downloading chunk " << chunk_no << " from " << *it2 << endl;
            cout << "Starting Download of Chunk:" << chunk_no << endl;
            if (connect_and_download(*it2, f_name, d_path, chunk_no, last_chunk_size, no_of_chunks))
            {
                // cout << "Downloaded chunk " << chunk_no << " from " << *it2 << endl;

                flag = 1;
                break;
            }
            else
            {
                cout << "Failed to download chunk " << chunk_no << " from " << *it2 << endl;
                cout << " Trying to connect other peers..." << endl;
            }

            // multithreading
        }
        res = res && flag;
    }
    if (res)
    {
        cout << "File downloaded successfully" << endl;
    }
    else
    {
        cout << "File download failed" << endl;
    }
}

void start_downloading_chunks_multithreaded(unordered_map<int, pair<vector<string>, int>> &chunks_freq, string f_name, string d_path, long long last_chunk_size, long long no_of_chunks, string g_id, long long f_size, vector<string> seeders)
{
    string f_path = d_path + "/" + f_name;
    file_info f;
    // int j = 0;
    bool downloaded_all = true;
    bool first_download = false;

    for (long long i = 0; i < no_of_chunks; i++)
    {
        struct argv *args = new argv;
        if (chunks_freq.find(i) != chunks_freq.end())
        {
            args->peers_list = chunks_freq[i].first;
            args->f_name = f_name;
            args->d_path = d_path;
            args->chunk_no = i;
            args->last_chunk_size = last_chunk_size;
            args->no_of_chunks = no_of_chunks;
            args->f_info = &f;
            args->downloaded_first_chunk = &first_download;
            args->downloaded_all = &downloaded_all;
            args->g_id = g_id;
        }
        else
        {
            args->peers_list = seeders;
            args->f_name = f_name;
            args->d_path = d_path;
            args->chunk_no = i;
            args->last_chunk_size = last_chunk_size;
            args->no_of_chunks = no_of_chunks;
            args->f_info = &f;
            args->downloaded_first_chunk = &first_download;
            args->downloaded_all = &downloaded_all;
            args->g_id = g_id;
        }

        pthread_t thread_id;
         (&thread_id, NULL, connect_and_download_multithreaded, (void *)args);
        usleep(10000);
    }

    if (downloaded_all)
    {
        handle_update_seeder(f_name, d_path, g_id, no_of_chunks, last_chunk_size, f_size);
        cout << "File downloaded successfully" << endl;
    }
    else
    {
        cout << "File download failed:No Peers Available at the moment" << endl;
    }
}

long long get_last_chunk_size(string f_name, vector<addr_info> seeders)
{
    long long last_chunk_size = 0;
    for (auto it : seeders)
    {
        string ip_port = it.IP + ":" + to_string(it.PORT);
        int sd = connect_to(it.IP, it.PORT);
        if (sd == -1)
        {
            continue;
        }
        string rqst = "get_last_chunk_size " + f_name;
        send(sd, rqst.c_str(), rqst.length(), 0);
        char *buffer = new char[chunk_size + 20];
        int bytes_read = 0;
        // cout << "Waiting for last chunk size" << endl;
        bytes_read = recv(sd, buffer, chunk_size + 20, 0);
        if (bytes_read == -1)
        {
            cout << "Error in receiving last chunk size" << endl;
        }
        else
        {
            last_chunk_size = atoi(buffer);
            // cout << "Last chunk size: " << last_chunk_size << endl;
        }
        // while ((bytes_read = recv(sd, buffer, chunk_size, 0)) > 0)
        // {
        //     last_chunk_size = stoll(buffer);
        //     break;
        // }

        close_connection(sd, it.IP, it.PORT);
    }
    // cout << last_chunk_size << endl;
    return last_chunk_size;
}

bool download_file(meta_data &m, string f_name, string d_path, string g_id)
{
    long long no_of_chunks = m.file_size / chunk_size;
    if (m.file_size % chunk_size != 0)
        no_of_chunks++;

    // create_file(m.file_size, f_name, d_path);
    // cout << "File created " << d_path << "/" << f_name << endl;
    // vector<pair<vector<string>, int>> chunks_freq(no_of_chunks, {{}, 0});
    unordered_map<int, pair<vector<string>, int>> chunks_freq;
    vector<addr_info> seeders = m.seeders;
    vector<pair<addr_info, string>> leeches = m.leeches;

    for (auto it : leeches)
    {
        cout << "Leech: " << it.first.IP << ":" << it.first.PORT << endl;
        string cmnd = "send_bitmap " + f_name;
        addr_info leecher = it.first;
        int leecher_socket = connect_to(leecher.IP, leecher.PORT);
        send(leecher_socket, cmnd.c_str(), cmnd.length(), 0);
        char *buffer = new char[no_of_chunks + 20];
        memset(buffer, 0, no_of_chunks + 20);
        int n = recv(leecher_socket, buffer, no_of_chunks, 0);
        if (n > 0)
        {
            close_connection(leecher_socket, leecher.IP, leecher.PORT);
            update_freq(buffer, chunks_freq, no_of_chunks, leecher.IP, leecher.PORT);
        }
        delete[] buffer;
    }
    /// Piece selection algorithm ----------------------------
    // for (long long i = 0; i < no_of_chunks; i++)
    // {

    //     for (auto it : seeders)
    //     {
    //         string ip_port = it.IP + ":" + to_string(it.PORT);
    //         chunks_freq[i].first.push_back(ip_port);
    //         chunks_freq[i].second++;
    //     }
    // }
    // for (int i = 0; i < seeders.size(); i++)
    // {
    //     string ip_port = seeders[i].IP + ":" + to_string(seeders[i].PORT);
    //     set<int> usd;
    //     chunks_freq[i].first.push_back(ip_port);
    //     chunks_freq[i].second++;
    // }

    long long last_chunk_size = get_last_chunk_size(f_name, seeders);
    cout << "Appling piece selection algorithm" << endl;
    apply_piece_selection(chunks_freq);
    vector<string> seeds;
    for (auto it : seeders)
    {
        string ip_port = it.IP + ":" + to_string(it.PORT);
        seeds.push_back(ip_port);
    }
    cout << "Seeds: " << seeds.size() << endl;
    cout << "Starting Multithreaded Download" << endl;
    start_downloading_chunks_multithreaded(chunks_freq, f_name, d_path, last_chunk_size, no_of_chunks, g_id, m.file_size, seeds);

    return true;
}

string extract_fname(string path)
{
    string delm = "/";
    vector<string> tokens;
    string token;

    for (unsigned i = 0; i < path.length(); i++)
    {
        if (str_cmp(path.substr(i, 1), delm))
        {
            tokens.push_back(token);

            token = "";
        }
        else
        {
            token += path[i];
        }
    }
    tokens.push_back(token);

    return tokens[tokens.size() - 1];
}
// upload_file <file_path> <group_id>
void handle_upload_files(int client_sd, string input)
{

    vector<string> tkns = tokenize(input);

    if (tkns.size() != 3)
    {
        cout << "Expected 2 arguments <file_path> <group_id>" << endl;
        return;
    }

    string hash = calcsha1(tkns[1]);
    if (strcmp(hash.c_str(), "-1") == 0)
    {
        cout << "Recieved Response:-File Doesnot Exists" << endl;
        return;
    }

    string f_name = extract_fname(tkns[1]);
    string f_path = tkns[1];
    string s = tkns[0] + " " + f_name + " " + tkns[2] + " " + hash;

    bool cmnd_send_status = send(client_sd, s.data(), s.size(), 0);

    char buff[buff_size];

    if (cmnd_send_status == false)
    {
        cout << "Error in sending message" << endl;
        return;
    }

    bzero(buff, buff_size);
    cout << s << endl;
    recv(client_sd, buff, buff_size, 0);

    cout << buff << endl;
    cout << str_cmp(buff, "File Uploaded Successfully") << endl;
    if (strcmp(buff, "File Uploaded Successfully") == 0)
    {
        file_info f;
        f.f_name = f_name;
        f.f_path = tkns[1]; // f_path;
        f.f_size = getfilesize(tkns[1]);
        cout << "file size = " << f.f_size << endl;
        f.hash = hash;
        f.seeder = true;
        long long no_of_chunks = f.f_size / chunk_size;
        long long last_chunk_size = f.f_size % chunk_size;

        if (last_chunk_size != 0)
            no_of_chunks++;
        f.no_of_chunks = no_of_chunks;
        for (int i = 0; i < no_of_chunks; i++)
        {
            f.bitmp.first.push_back(true);
        }
        f.bitmp.second = f.f_size % chunk_size;
        cout << "last chunk size = " << f.bitmp.second << endl;
        cout << "No of chunks = " << no_of_chunks << endl;
        // storing hash of each chunk in a vector

        for (int i = 0; i < no_of_chunks; i++)
        {
            if (i == no_of_chunks - 1)
            {
                f.chunk_hashes.push_back(calcsha1_chunk(f.f_path, i * chunk_size, last_chunk_size));
            }
            else
            {
                f.chunk_hashes.push_back(calcsha1_chunk(f.f_path, i * chunk_size, chunk_size));
            }
        }

        files.push_back(f);
    }
}

bool ask_filesize(string IP, int PORT, string f_name, meta_data &m)
{
    // usleep(100000);
    int peer_sd = connect_to(IP, (PORT));
    if (peer_sd == -1)
    {
        cout << "Error in creating socket" << endl;
        return false;
    }
    string cmnd = "get_filesize " + f_name;
    bool cmnd_send_status = send(peer_sd, cmnd.data(), cmnd.size(), 0);
    if (cmnd_send_status == false)
    {
        cout << "Error in sending message" << endl;
        return false;
    }
    char buff[buff_size];
    bzero(buff, buff_size);
    // usleep(1000);

    recv(peer_sd, buff, buff_size, 0);
    long long f_size = stoll(buff);
    // cout << "Recivied File Size = " << f_size << endl;
    m.file_size = f_size;
    if (f_size <= 0)
    {
        cout << "File not present to peer" << endl;
        return false;
    }
    return true;
}

bool start_download(meta_data &m, string f_name, string d_path, string g_id)
{

    bool res = false;
    for (unsigned i = 0; i < m.seeders.size(); i++)
    {

        bool status = ask_filesize(m.seeders[i].IP, m.seeders[i].PORT, f_name, m);

        if (status == true)
        {
            cout << "File Size Recieved" << m.file_size << endl;
            res = download_file(m, f_name, d_path, g_id);
            break;
        }
        else
        {
            cout << "Could not get filesize from seeder" << endl;
        }
    }

    if (res == true)
        return true;

    return false;
}

// download_file <group_id> <file_name>
// <destination_path>
void store_leeches(meta_data &m, string group_id, string f_name)
{
    string request = "send_leeches " + group_id + " " + f_name;
    int n = send(tracker_sd, request.data(), request.size(), 0);
    if (n < 0)
    {
        return;
    }
    char buff[buff_size];
    bzero(buff, buff_size);

    recv(tracker_sd, buff, buff_size, 0);
    string response = buff;
    if (str_cmp(buff, "no_leeches"))
    {
        cout << "No leeches at moment" << endl;
        return;
    }
    cout << response << endl;
    cout << endl;
    vector<string> ip_port = tokenize(response);

    for (unsigned i = 0; i < ip_port.size(); i += 2)
    {
        addr_info a;
        vector<string> ip_port = extract_ip_port(ip_port[i]);
        if (ip_port.size() == 2)
        {

            a.IP = ip_port[0];
            a.PORT = stoi(ip_port[1]);

            m.leeches.push_back({a, "abcd"});
        }
        cout << "Leeches Recieved" << endl;
        return;
    }
}

void handle_download_files(int client_sd, string s)
{
    // cout << "Inside handle_download" << endl;
    // cout << s << endl;

    vector<string> tkns = tokenize(s);
    if (tkns.size() != 4)
    {
        cout << "Expected 3 arguments <group_id> <file_name> <destination_path>" << endl;
        return;
    }
    string d_path = tkns[3];
    string f_name = tkns[2];
    string g_id = tkns[1];
    string cmnd = tkns[0] + " " + g_id + " " + f_name;

    bool cmnd_send_status = send(client_sd, cmnd.data(), cmnd.size(), 0);

    if (cmnd_send_status == false)
    {
        cout << "Error in sending message" << endl;
        return;
    }
    char buff[buff_size];
    bzero(buff, buff_size);
    recv(client_sd, buff, buff_size, 0);
    // cout << buff << endl;
    if (str_cmp(buff, "File Found"))
    {
        // cout << " True downloads" << endl;
        bzero(buff, buff_size);
        recv(client_sd, buff, buff_size, 0);
        // cout << buff << endl;
        meta_data m = proccess_meta_data(buff);
        store_leeches(m, g_id, f_name);
        // cout << "Full Hash: " << m.full_hash << endl;
        // cout << "Seeders: " << endl;
        started_downloads[g_id].push_back(f_name);
        start_download(m, f_name, d_path, g_id);

        // for (unsigned i = 0; i < m.seeders.size(); i++)
        // {
        //     cout << m.seeders[i].IP << ":" << m.seeders[i].PORT << endl;
        // }
    }
    else
    {
        cout << buff << endl;
    }
}

void handle_show_downloads(int client_sd)
{
    for (auto it = started_downloads.begin(); it != started_downloads.end(); it++)
    {

        for (auto it2 = it->second.begin(); it2 != it->second.end(); it2++)
        {
            cout << "[D] " << it->first << " " << *it2 << endl;
        }
    }
    for (auto it = partial_downloads.begin(); it != partial_downloads.end(); it++)
    {
        for (auto it2 = it->second.begin(); it2 != it->second.end(); it2++)
        {
            cout << "[D] " << it->first << " " << *it2 << endl;
        }
    }

    for (auto it = completed_downloads.begin(); it != completed_downloads.end(); it++)
    {

        for (auto it2 = it->second.begin(); it2 != it->second.end(); it2++)
        {
            cout << "[C] " << it->first << " " << *it2 << endl;
        }
    }
}

addr_info sd_to_addr(int client_sd)
{
    addr_info addr;
    struct sockaddr_in addr_in;
    socklen_t addr_size = sizeof(struct sockaddr_in);
    getpeername(client_sd, (struct sockaddr *)&addr_in, &addr_size);
    addr.IP = inet_ntoa(addr_in.sin_addr);
    addr.PORT = ntohs(addr_in.sin_port);
    addr.socket = client_sd;
    return addr;
}

void run_peer(int client_sd, int listening_port)
{
    // cout << "Inside run_peer" << endl;

    string input;
    while (strcmp(input.data(), "exit") != 0)
    {
        cout << "Enter command:"
             << ">";
        getline(cin, input);
        cout << endl;

        if (str_cmp(input.substr(0, 11), "upload_file"))
        {
            handle_upload_files(client_sd, input);
            continue;
        }
        else if (str_cmp(input.substr(0, 13), "download_file"))
        {
            handle_download_files(client_sd, input);
            continue;
        }
        else if (str_cmp(input, "show_downloads"))
        {
            handle_show_downloads(client_sd);
            continue;
        }

        else if (str_cmp(input.substr(0, 5), "login"))
        {
            input += " " + to_string(listening_port);
        }
        else if (str_cmp(input, "peer_list_files"))
        {
            // cout << "Inside list_files" << endl;
            // cout << "Files size" << files.size() << endl;
            for (auto it : files)
            {
                cout << it.f_name << " " << it.f_path << " " << it.f_size << " " << it.hash << " " << it.bitmp.second << endl;
            }
        }

        bool cmnd_send_status = send(client_sd, input.data(), input.size(), 0);
        char buff[buff_size];
        if (cmnd_send_status == false)
        {
            cout << "Error in sending message" << endl;
            continue;
        }
        else
        {

            bzero(buff, buff_size);
            recv(client_sd, buff, buff_size, 0);
            cout << "Recieved Response:-";
            cout << buff << endl;
            cout << endl;
            bzero(buff, buff_size);
        }
        input = "";
    }
}

int init_peer(int port, string ip, int listening_port)
{
    // struct hostent *servr_ip = gethostbyname(ip.c_str());
    struct sockaddr_in commn_sd;
    commn_sd.sin_family = AF_INET;
    commn_sd.sin_port = htons(port);

    if (inet_pton(AF_INET, ip.c_str(), &commn_sd.sin_addr) <= 0)
    {
        cout << "Invalid address/ Address not supported" << endl;
        return -1;
    }

    int client_sd;
    if ((client_sd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        printf("\n Socket creation error \n");
        return -1;
    }
    int status;
    if ((status = connect(client_sd, (struct sockaddr *)&commn_sd,
                          sizeof(commn_sd))) < 0)
    {
        printf("\nConnection Failed \n");
        return -1;
    }
    tracker_sd = client_sd;
    run_peer(client_sd, listening_port);
    return 1;
}
